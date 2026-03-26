#include "UsbBackup.h"

#include <LittleFS.h>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <vector>

extern bool fileSystemExists;
extern bool debugMessages;
extern std::atomic<bool> flashWriteInProgress;

namespace {

constexpr uint32_t USB_BACKUP_TRANSFER_TIMEOUT_MS = 15000;
constexpr size_t USB_BACKUP_COMMAND_BUFFER_SIZE = 320;
constexpr size_t USB_BACKUP_PATH_BUFFER_SIZE = 256;
constexpr size_t USB_BACKUP_TEMP_PATH_BUFFER_SIZE = 288;
constexpr size_t USB_BACKUP_STATUS_LINE_SIZE = 24;
constexpr size_t USB_BACKUP_IO_BUFFER_SIZE = 128;
constexpr const char* USB_BACKUP_ROOT = "/Sequences";
constexpr const char* USB_BACKUP_FILE_EXTENSION = ".hbseq";
constexpr const char* USB_BACKUP_PROTOCOL = "HBK1";

enum class UsbBackupReceiveState : uint8_t {
  Command = 0,
  PutPayload = 1
};

bool usbBackupActive = false;
bool usbBackupDebugMessagesBeforeSession = false;
bool usbBackupUiRefreshRequested = false;
char usbBackupStatusLineOne[USB_BACKUP_STATUS_LINE_SIZE] = "USB Backup Off";
char usbBackupStatusLineTwo[USB_BACKUP_STATUS_LINE_SIZE] = "Host tool idle";
UsbBackupReceiveState usbBackupReceiveState = UsbBackupReceiveState::Command;
char usbBackupCommandBuffer[USB_BACKUP_COMMAND_BUFFER_SIZE] = {};
size_t usbBackupCommandLength = 0;
uint32_t usbBackupLastIoAt = 0;
File usbBackupIncomingFile;
char usbBackupIncomingFinalPath[USB_BACKUP_PATH_BUFFER_SIZE] = {};
char usbBackupIncomingTempPath[USB_BACKUP_TEMP_PATH_BUFFER_SIZE] = {};
size_t usbBackupIncomingBytesRemaining = 0;

void setUsbBackupStatus(const char* lineOne, const char* lineTwo) {
  snprintf(usbBackupStatusLineOne, sizeof(usbBackupStatusLineOne), "%s", (lineOne != nullptr) ? lineOne : "");
  snprintf(usbBackupStatusLineTwo, sizeof(usbBackupStatusLineTwo), "%s", (lineTwo != nullptr) ? lineTwo : "");
  usbBackupUiRefreshRequested = true;
}

void sendProtocolLine(const char* text) {
  Serial.print(text);
  Serial.print('\n');
}

void sendErrorLine(const char* text) {
  Serial.print("ERR ");
  Serial.print(text);
  Serial.print('\n');
}

bool pathHasExtension(const char* path, const char* extension) {
  if (path == nullptr || extension == nullptr) {
    return false;
  }
  size_t pathLength = strlen(path);
  size_t extensionLength = strlen(extension);
  return pathLength > extensionLength && strcmp(path + pathLength - extensionLength, extension) == 0;
}

bool pathStartsWithRoot(const char* path) {
  if (path == nullptr) {
    return false;
  }
  size_t rootLength = strlen(USB_BACKUP_ROOT);
  if (strncmp(path, USB_BACKUP_ROOT, rootLength) != 0) {
    return false;
  }
  return path[rootLength] == '\0' || path[rootLength] == '/';
}

bool pathContainsInvalidSegments(const char* path) {
  if (path == nullptr || path[0] != '/') {
    return true;
  }

  const char* segment = path + 1;
  while (*segment != '\0') {
    const char* slash = strchr(segment, '/');
    size_t segmentLength = (slash != nullptr) ? static_cast<size_t>(slash - segment) : strlen(segment);
    if (segmentLength == 0) {
      return true;
    }
    if (segment[0] == '.' || (segmentLength == 1 && segment[0] == '.') ||
        (segmentLength == 2 && segment[0] == '.' && segment[1] == '.')) {
      return true;
    }
    for (size_t index = 0; index < segmentLength; index++) {
      unsigned char c = static_cast<unsigned char>(segment[index]);
      if (c < 32 || c == '\\') {
        return true;
      }
    }
    if (slash == nullptr) {
      break;
    }
    segment = slash + 1;
  }
  return false;
}

int hexValue(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  return -1;
}

bool decodePathToken(const char* token, char* out, size_t outSize) {
  if (token == nullptr || out == nullptr || outSize == 0) {
    return false;
  }

  size_t outIndex = 0;
  for (size_t index = 0; token[index] != '\0'; index++) {
    char c = token[index];
    if (c == '%') {
      if (token[index + 1] == '\0' || token[index + 2] == '\0') {
        return false;
      }
      int hi = hexValue(token[index + 1]);
      int lo = hexValue(token[index + 2]);
      if (hi < 0 || lo < 0) {
        return false;
      }
      c = static_cast<char>((hi << 4) | lo);
      index += 2;
    }
    if (outIndex + 1 >= outSize) {
      return false;
    }
    out[outIndex++] = c;
  }
  out[outIndex] = '\0';
  return true;
}

bool encodePathToken(const char* path, char* out, size_t outSize) {
  if (path == nullptr || out == nullptr || outSize == 0) {
    return false;
  }

  size_t outIndex = 0;
  for (size_t index = 0; path[index] != '\0'; index++) {
    unsigned char c = static_cast<unsigned char>(path[index]);
    bool safe = (std::isalnum(c) != 0) || c == '/' || c == '-' || c == '_' || c == '.';
    if (safe) {
      if (outIndex + 1 >= outSize) {
        return false;
      }
      out[outIndex++] = static_cast<char>(c);
    } else {
      if (outIndex + 3 >= outSize) {
        return false;
      }
      snprintf(out + outIndex, outSize - outIndex, "%%%02X", c);
      outIndex += 3;
    }
  }
  out[outIndex] = '\0';
  return true;
}

bool ensureBackupRoot() {
  if (!fileSystemExists) {
    return false;
  }
  if (LittleFS.exists(USB_BACKUP_ROOT)) {
    return true;
  }
  return LittleFS.mkdir(USB_BACKUP_ROOT);
}

bool pathExistsAndIsDirectory(const char* path) {
  File f = LittleFS.open(path, "r");
  if (!f) {
    return false;
  }
  bool isDirectory = f.isDirectory();
  f.close();
  return isDirectory;
}

bool pathExistsAndIsFile(const char* path) {
  File f = LittleFS.open(path, "r");
  if (!f) {
    return false;
  }
  bool isFile = f.isFile();
  f.close();
  return isFile;
}

void joinBackupPath(const char* directoryPath, const char* leafName, char* out, size_t outSize) {
  if (outSize == 0) {
    return;
  }
  if (strcmp(directoryPath, "/") == 0) {
    snprintf(out, outSize, "/%s", leafName);
  } else {
    snprintf(out, outSize, "%s/%s", directoryPath, leafName);
  }
}

bool validateDirectoryPath(const char* path) {
  return path != nullptr &&
         pathStartsWithRoot(path) &&
         !pathContainsInvalidSegments(path);
}

bool validateFilePath(const char* path) {
  return validateDirectoryPath(path) &&
         pathHasExtension(path, USB_BACKUP_FILE_EXTENSION);
}

bool parentDirectoryExists(const char* path) {
  char parentPath[USB_BACKUP_PATH_BUFFER_SIZE];
  snprintf(parentPath, sizeof(parentPath), "%s", path);
  char* slash = strrchr(parentPath, '/');
  if (slash == nullptr || slash == parentPath) {
    snprintf(parentPath, sizeof(parentPath), "%s", USB_BACKUP_ROOT);
  } else {
    *slash = '\0';
  }
  return pathExistsAndIsDirectory(parentPath);
}

void clearPendingIncomingState() {
  if (usbBackupIncomingFile) {
    usbBackupIncomingFile.close();
  }
  if (usbBackupIncomingTempPath[0] != '\0' && LittleFS.exists(usbBackupIncomingTempPath)) {
    LittleFS.remove(usbBackupIncomingTempPath);
  }
  usbBackupIncomingFinalPath[0] = '\0';
  usbBackupIncomingTempPath[0] = '\0';
  usbBackupIncomingBytesRemaining = 0;
  usbBackupReceiveState = UsbBackupReceiveState::Command;
  flashWriteInProgress.store(false, std::memory_order_relaxed);
}

void finishIncomingFileWrite() {
  if (usbBackupIncomingFile) {
    usbBackupIncomingFile.close();
  }

  bool success = true;
  if (LittleFS.exists(usbBackupIncomingFinalPath) && !LittleFS.remove(usbBackupIncomingFinalPath)) {
    success = false;
  }
  if (success && !LittleFS.rename(usbBackupIncomingTempPath, usbBackupIncomingFinalPath)) {
    success = false;
  }

  flashWriteInProgress.store(false, std::memory_order_relaxed);

  if (!success) {
    if (LittleFS.exists(usbBackupIncomingTempPath)) {
      LittleFS.remove(usbBackupIncomingTempPath);
    }
    sendErrorLine("WRITE_FAILED");
    setUsbBackupStatus("Backup Error", "Write failed");
  } else {
    sendProtocolLine("OK PUT");
    setUsbBackupStatus("Restore OK", "File written");
  }

  usbBackupIncomingFinalPath[0] = '\0';
  usbBackupIncomingTempPath[0] = '\0';
  usbBackupIncomingBytesRemaining = 0;
  usbBackupReceiveState = UsbBackupReceiveState::Command;
}

bool deleteDirectoryRecursive(const char* folderPath) {
  if (!validateDirectoryPath(folderPath) || strcmp(folderPath, USB_BACKUP_ROOT) == 0) {
    return false;
  }

  std::vector<String> childPaths;
  {
    Dir dir = LittleFS.openDir(folderPath);
    while (dir.next()) {
      String entryName = dir.fileName();
      if (entryName.length() == 0 || entryName.startsWith(".")) {
        continue;
      }

      char childPath[USB_BACKUP_PATH_BUFFER_SIZE];
      joinBackupPath(folderPath, entryName.c_str(), childPath, sizeof(childPath));
      childPaths.push_back(String(childPath));
    }
  }

  for (const String& childPath : childPaths) {
    if (pathExistsAndIsDirectory(childPath.c_str())) {
      if (!deleteDirectoryRecursive(childPath.c_str())) {
        return false;
      }
    } else if (pathExistsAndIsFile(childPath.c_str())) {
      if (!LittleFS.remove(childPath.c_str())) {
        return false;
      }
    }
  }

  if (!LittleFS.exists(folderPath)) {
    return true;
  }
  return LittleFS.rmdir(folderPath);
}

size_t tokenizeCommand(char* line, char** tokens, size_t maxTokens) {
  size_t count = 0;
  char* current = line;
  while (*current != '\0' && count < maxTokens) {
    while (*current == ' ') {
      current++;
    }
    if (*current == '\0') {
      break;
    }
    tokens[count++] = current;
    while (*current != '\0' && *current != ' ') {
      current++;
    }
    if (*current == '\0') {
      break;
    }
    *current++ = '\0';
  }
  return count;
}

void handleHelloCommand() {
  if (!ensureBackupRoot()) {
    sendErrorLine("FILESYSTEM_UNAVAILABLE");
    return;
  }
  Serial.print("OK HELLO ");
  Serial.print(USB_BACKUP_PROTOCOL);
  Serial.print(" ROOT ");
  Serial.print(USB_BACKUP_ROOT);
  Serial.print('\n');
  setUsbBackupStatus("Session Active", "Host connected");
}

void handleListCommand(const char* encodedPath) {
  char path[USB_BACKUP_PATH_BUFFER_SIZE];
  if (!decodePathToken(encodedPath, path, sizeof(path)) ||
      !validateDirectoryPath(path) ||
      !pathExistsAndIsDirectory(path)) {
    sendErrorLine("BAD_PATH");
    return;
  }

  Dir dir = LittleFS.openDir(path);
  char encodedChild[USB_BACKUP_PATH_BUFFER_SIZE * 3];
  while (dir.next()) {
    String entryName = dir.fileName();
    if (entryName.length() == 0 || entryName.startsWith(".")) {
      continue;
    }

    char childPath[USB_BACKUP_PATH_BUFFER_SIZE];
    joinBackupPath(path, entryName.c_str(), childPath, sizeof(childPath));
    bool includeEntry = dir.isDirectory() || pathHasExtension(entryName.c_str(), USB_BACKUP_FILE_EXTENSION);
    if (!includeEntry || !encodePathToken(childPath, encodedChild, sizeof(encodedChild))) {
      continue;
    }

    Serial.print("ENTRY ");
    Serial.print(dir.isDirectory() ? 'D' : 'F');
    Serial.print(' ');
    Serial.print(encodedChild);
    Serial.print(' ');
    Serial.print(dir.isDirectory() ? 0 : dir.fileSize());
    Serial.print('\n');
  }

  sendProtocolLine("DONE LIST");
  setUsbBackupStatus("Backup Ready", "Listed files");
}

void handleGetCommand(const char* encodedPath) {
  char path[USB_BACKUP_PATH_BUFFER_SIZE];
  if (!decodePathToken(encodedPath, path, sizeof(path)) ||
      !validateFilePath(path) ||
      !pathExistsAndIsFile(path)) {
    sendErrorLine("BAD_PATH");
    return;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    sendErrorLine("OPEN_FAILED");
    return;
  }

  size_t fileSize = file.size();
  Serial.print("OK SIZE ");
  Serial.print(fileSize);
  Serial.print('\n');

  uint8_t buffer[USB_BACKUP_IO_BUFFER_SIZE];
  while (file.available()) {
    size_t bytesRead = file.read(buffer, sizeof(buffer));
    if (bytesRead == 0) {
      break;
    }
    Serial.write(buffer, bytesRead);
  }
  file.close();
  sendProtocolLine("DONE GET");
  setUsbBackupStatus("Backup Sent", "File transferred");
}

void handlePutCommand(const char* encodedPath, const char* sizeToken) {
  char path[USB_BACKUP_PATH_BUFFER_SIZE];
  if (!decodePathToken(encodedPath, path, sizeof(path)) || !validateFilePath(path)) {
    sendErrorLine("BAD_PATH");
    return;
  }

  unsigned long sizeValue = strtoul(sizeToken, nullptr, 10);
  if (sizeValue == 0 && strcmp(sizeToken, "0") != 0) {
    sendErrorLine("BAD_SIZE");
    return;
  }

  char parentPath[USB_BACKUP_PATH_BUFFER_SIZE];
  snprintf(parentPath, sizeof(parentPath), "%s", path);
  char* slash = strrchr(parentPath, '/');
  if (slash == nullptr || slash == parentPath) {
    snprintf(parentPath, sizeof(parentPath), "%s", USB_BACKUP_ROOT);
  } else {
    *slash = '\0';
  }

  if (!pathExistsAndIsDirectory(parentPath)) {
    sendErrorLine("MISSING_PARENT");
    return;
  }

  snprintf(usbBackupIncomingFinalPath, sizeof(usbBackupIncomingFinalPath), "%s", path);
  if (snprintf(usbBackupIncomingTempPath, sizeof(usbBackupIncomingTempPath), "%s.tmp", path) >=
      static_cast<int>(sizeof(usbBackupIncomingTempPath))) {
    sendErrorLine("PATH_TOO_LONG");
    usbBackupIncomingFinalPath[0] = '\0';
    return;
  }

  if (LittleFS.exists(usbBackupIncomingTempPath)) {
    LittleFS.remove(usbBackupIncomingTempPath);
  }

  flashWriteInProgress.store(true, std::memory_order_relaxed);
  usbBackupIncomingFile = LittleFS.open(usbBackupIncomingTempPath, "w");
  if (!usbBackupIncomingFile) {
    flashWriteInProgress.store(false, std::memory_order_relaxed);
    usbBackupIncomingFinalPath[0] = '\0';
    usbBackupIncomingTempPath[0] = '\0';
    sendErrorLine("OPEN_FAILED");
    return;
  }

  usbBackupIncomingBytesRemaining = static_cast<size_t>(sizeValue);
  usbBackupReceiveState = UsbBackupReceiveState::PutPayload;
  usbBackupLastIoAt = millis();
  sendProtocolLine("READY");
  setUsbBackupStatus("Restore Busy", "Receiving file");

  if (usbBackupIncomingBytesRemaining == 0) {
    finishIncomingFileWrite();
  }
}

void handleMkdirCommand(const char* encodedPath) {
  char path[USB_BACKUP_PATH_BUFFER_SIZE];
  if (!decodePathToken(encodedPath, path, sizeof(path)) ||
      !validateDirectoryPath(path) ||
      strcmp(path, USB_BACKUP_ROOT) == 0) {
    sendErrorLine("BAD_PATH");
    return;
  }

  if (LittleFS.exists(path)) {
    if (pathExistsAndIsDirectory(path)) {
      sendProtocolLine("OK MKDIR");
      return;
    }
    sendErrorLine("PATH_EXISTS");
    return;
  }

  if (!LittleFS.mkdir(path)) {
    sendErrorLine("MKDIR_FAILED");
    return;
  }
  sendProtocolLine("OK MKDIR");
  setUsbBackupStatus("Restore OK", "Folder created");
}

void handleDeleteCommand(const char* encodedPath) {
  char path[USB_BACKUP_PATH_BUFFER_SIZE];
  if (!decodePathToken(encodedPath, path, sizeof(path)) ||
      !validateFilePath(path) ||
      !pathExistsAndIsFile(path)) {
    sendErrorLine("BAD_PATH");
    return;
  }

  if (!LittleFS.remove(path)) {
    sendErrorLine("DELETE_FAILED");
    return;
  }
  sendProtocolLine("OK DELETE");
  setUsbBackupStatus("Restore OK", "File deleted");
}

void handleRmdirCommand(const char* encodedPath) {
  char path[USB_BACKUP_PATH_BUFFER_SIZE];
  if (!decodePathToken(encodedPath, path, sizeof(path)) ||
      !validateDirectoryPath(path) ||
      strcmp(path, USB_BACKUP_ROOT) == 0 ||
      !pathExistsAndIsDirectory(path)) {
    sendErrorLine("BAD_PATH");
    return;
  }

  if (!deleteDirectoryRecursive(path)) {
    sendErrorLine("RMDIR_FAILED");
    return;
  }
  sendProtocolLine("OK RMDIR");
  setUsbBackupStatus("Restore OK", "Folder deleted");
}

void handleRenameCommand(const char* encodedSourcePath, const char* encodedTargetPath) {
  char sourcePath[USB_BACKUP_PATH_BUFFER_SIZE];
  char targetPath[USB_BACKUP_PATH_BUFFER_SIZE];
  if (!decodePathToken(encodedSourcePath, sourcePath, sizeof(sourcePath)) ||
      !decodePathToken(encodedTargetPath, targetPath, sizeof(targetPath))) {
    sendErrorLine("BAD_PATH");
    return;
  }

  bool sourceIsFile = pathExistsAndIsFile(sourcePath);
  bool sourceIsDirectory = pathExistsAndIsDirectory(sourcePath);
  if (!sourceIsFile && !sourceIsDirectory) {
    sendErrorLine("BAD_PATH");
    return;
  }

  if (sourceIsFile) {
    if (!validateFilePath(sourcePath) || !validateFilePath(targetPath)) {
      sendErrorLine("BAD_PATH");
      return;
    }
  } else {
    if (!validateDirectoryPath(sourcePath) ||
        !validateDirectoryPath(targetPath) ||
        strcmp(sourcePath, USB_BACKUP_ROOT) == 0 ||
        strcmp(targetPath, USB_BACKUP_ROOT) == 0) {
      sendErrorLine("BAD_PATH");
      return;
    }
  }

  if (!parentDirectoryExists(targetPath)) {
    sendErrorLine("MISSING_PARENT");
    return;
  }

  if (strcmp(sourcePath, targetPath) == 0) {
    sendProtocolLine("OK RENAME");
    return;
  }

  if (LittleFS.exists(targetPath)) {
    sendErrorLine("PATH_EXISTS");
    return;
  }

  if (!LittleFS.rename(sourcePath, targetPath)) {
    sendErrorLine("RENAME_FAILED");
    return;
  }

  sendProtocolLine("OK RENAME");
  setUsbBackupStatus("Restore OK", sourceIsFile ? "File renamed" : "Folder renamed");
}

void processCommandLine(char* line) {
  char* tokens[4] = {};
  size_t tokenCount = tokenizeCommand(line, tokens, 4);
  if (tokenCount == 0) {
    return;
  }

  if (strcmp(tokens[0], "HELLO") == 0) {
    handleHelloCommand();
  } else if (strcmp(tokens[0], "LIST") == 0 && tokenCount >= 2) {
    handleListCommand(tokens[1]);
  } else if (strcmp(tokens[0], "GET") == 0 && tokenCount >= 2) {
    handleGetCommand(tokens[1]);
  } else if (strcmp(tokens[0], "PUT") == 0 && tokenCount >= 3) {
    handlePutCommand(tokens[1], tokens[2]);
  } else if (strcmp(tokens[0], "MKDIR") == 0 && tokenCount >= 2) {
    handleMkdirCommand(tokens[1]);
  } else if (strcmp(tokens[0], "DELETE") == 0 && tokenCount >= 2) {
    handleDeleteCommand(tokens[1]);
  } else if (strcmp(tokens[0], "RMDIR") == 0 && tokenCount >= 2) {
    handleRmdirCommand(tokens[1]);
  } else if (strcmp(tokens[0], "RENAME") == 0 && tokenCount >= 3) {
    handleRenameCommand(tokens[1], tokens[2]);
  } else if (strcmp(tokens[0], "PING") == 0) {
    sendProtocolLine("OK PONG");
  } else {
    sendErrorLine("UNKNOWN_COMMAND");
  }
}

void serviceCommandInput() {
  while (Serial.available() > 0) {
    int incoming = Serial.read();
    if (incoming < 0) {
      break;
    }
    char c = static_cast<char>(incoming);
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      usbBackupCommandBuffer[usbBackupCommandLength] = '\0';
      processCommandLine(usbBackupCommandBuffer);
      usbBackupCommandLength = 0;
      usbBackupCommandBuffer[0] = '\0';
      usbBackupLastIoAt = millis();
      continue;
    }
    if (usbBackupCommandLength + 1 >= sizeof(usbBackupCommandBuffer)) {
      usbBackupCommandLength = 0;
      usbBackupCommandBuffer[0] = '\0';
      sendErrorLine("COMMAND_TOO_LONG");
      continue;
    }
    usbBackupCommandBuffer[usbBackupCommandLength++] = c;
  }
}

void serviceIncomingFilePayload() {
  uint8_t buffer[USB_BACKUP_IO_BUFFER_SIZE];
  while (usbBackupIncomingBytesRemaining > 0 && Serial.available() > 0) {
    size_t desired = usbBackupIncomingBytesRemaining;
    if (desired > sizeof(buffer)) {
      desired = sizeof(buffer);
    }
    size_t available = static_cast<size_t>(Serial.available());
    if (desired > available) {
      desired = available;
    }
    if (desired == 0) {
      break;
    }
    size_t bytesRead = Serial.readBytes(reinterpret_cast<char*>(buffer), desired);
    if (bytesRead == 0) {
      break;
    }
    size_t bytesWritten = usbBackupIncomingFile.write(buffer, bytesRead);
    if (bytesWritten != bytesRead) {
      clearPendingIncomingState();
      sendErrorLine("WRITE_FAILED");
      setUsbBackupStatus("Backup Error", "Write failed");
      return;
    }
    usbBackupIncomingBytesRemaining -= bytesRead;
    usbBackupLastIoAt = millis();
  }

  if (usbBackupIncomingBytesRemaining == 0) {
    finishIncomingFileWrite();
    return;
  }

  if ((millis() - usbBackupLastIoAt) > USB_BACKUP_TRANSFER_TIMEOUT_MS) {
    clearPendingIncomingState();
    sendErrorLine("TIMEOUT");
    setUsbBackupStatus("Backup Error", "Transfer timeout");
  }
}

}  // namespace

void setupUsbBackup() {
  setUsbBackupStatus("USB Backup Off", "Host tool idle");
}

bool enterUsbBackupMode() {
  if (usbBackupActive) {
    setUsbBackupStatus("Session Active", "Host tool ready");
    return true;
  }
  if (!ensureBackupRoot()) {
    setUsbBackupStatus("Backup Error", "FS unavailable");
    return false;
  }

  usbBackupActive = true;
  usbBackupDebugMessagesBeforeSession = debugMessages;
  debugMessages = false;
  usbBackupReceiveState = UsbBackupReceiveState::Command;
  usbBackupCommandLength = 0;
  usbBackupIncomingBytesRemaining = 0;
  usbBackupIncomingFinalPath[0] = '\0';
  usbBackupIncomingTempPath[0] = '\0';
  usbBackupLastIoAt = millis();
  while (Serial.available() > 0) {
    Serial.read();
  }
  setUsbBackupStatus("Session Active", "Run host tool");
  return true;
}

void exitUsbBackupMode() {
  if (!usbBackupActive) {
    setUsbBackupStatus("USB Backup Off", "Host tool idle");
    return;
  }

  clearPendingIncomingState();
  usbBackupActive = false;
  usbBackupCommandLength = 0;
  usbBackupCommandBuffer[0] = '\0';
  debugMessages = usbBackupDebugMessagesBeforeSession;
  setUsbBackupStatus("USB Backup Off", "Host tool idle");
}

bool isUsbBackupActive() {
  return usbBackupActive;
}

void serviceUsbBackup() {
  if (!usbBackupActive) {
    return;
  }

  if (usbBackupReceiveState == UsbBackupReceiveState::PutPayload) {
    serviceIncomingFilePayload();
  } else {
    serviceCommandInput();
  }
}

void getUsbBackupStatusLines(char* lineOneOut, size_t lineOneSize,
                             char* lineTwoOut, size_t lineTwoSize) {
  if (lineOneOut != nullptr && lineOneSize > 0) {
    snprintf(lineOneOut, lineOneSize, "%s", usbBackupStatusLineOne);
  }
  if (lineTwoOut != nullptr && lineTwoSize > 0) {
    snprintf(lineTwoOut, lineTwoSize, "%s", usbBackupStatusLineTwo);
  }
}

bool consumeUsbBackupUiRefreshRequested() {
  bool requested = usbBackupUiRefreshRequested;
  usbBackupUiRefreshRequested = false;
  return requested;
}
