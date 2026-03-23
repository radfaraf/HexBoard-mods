#include "SequencerMode.h"

#include <Adafruit_NeoPixel.h>
#include <LittleFS.h>
#include <cctype>
#include <cstdio>
#include <cstring>

extern void rebootToBootloader();
extern bool fileSystemExists;
extern GEM_u8g2 menu;
extern GEMPage menuPageSynthSequencer;
extern GEMPage menuPageSequencer;
extern GEMPage menuPageSequencerBrowser;
extern GEMPage menuPageSequencerFiles;
extern U8G2_SH1107_SEEED_128X128_F_HW_I2C u8g2;
extern bool screenSaverOn;
extern uint64_t screenTime;
extern uint64_t runTime;
extern Adafruit_NeoPixel strip;

int sequencerConfirmHue = 250;
byte sequencerConfirmSaturation = 255;
byte sequencerConfirmValue = 211;

namespace {
constexpr byte SEQUENCER_STEP_COUNT = 32;
constexpr byte SEQUENCER_TRANSPORT_BUTTON_INDEX = 9;
constexpr byte SEQUENCER_OVERVIEW_BUTTON_INDEX = 18;
constexpr byte SEQUENCER_CONFIRM_BUTTON_INDEX = 19;
constexpr byte SEQUENCER_MAX_NOTES_PER_STEP = 4;
constexpr byte SEQUENCER_OVERVIEW_STEPS_PER_PAGE = 8;
constexpr byte SEQUENCER_OVERLAY_CONTRAST = 63;
constexpr byte SEQUENCER_NO_NOTE = 255;
constexpr uint64_t SEQUENCER_NOTE_CONFIRM_MICROS = 2000000ULL;
constexpr uint64_t SEQUENCER_CLEAR_HOLD_MICROS = 1000000ULL;
constexpr uint64_t SEQUENCER_SELECTED_ON_MICROS = 1000000ULL;
constexpr uint64_t SEQUENCER_SELECTED_OFF_MICROS = 200000ULL;
constexpr byte SEQUENCER_TRANSPORT_STOP = 0;
constexpr byte SEQUENCER_TRANSPORT_PLAY = 1;
constexpr byte SEQUENCER_TAP_PREVIEW_OFF = 0;
constexpr byte SEQUENCER_TAP_PREVIEW_ON = 1;
constexpr byte SEQUENCER_PLAY_TYPE_MIDI = 0;
constexpr byte SEQUENCER_PLAY_TYPE_OB_SYNTH = 1;
constexpr byte SEQUENCER_DIRECTION_FORWARD = 0;
constexpr byte SEQUENCER_DIRECTION_BACKWARD = 1;
constexpr byte SEQUENCER_DIRECTION_PING_PONG = 2;
constexpr byte SEQUENCER_DIRECTION_RANDOM = 3;
constexpr byte SEQUENCER_DIRECTION_BROWNIAN = 4;
constexpr byte SEQUENCER_DIRECTION_DRUNK = 5;
constexpr const char* SEQUENCER_STORAGE_ROOT = "/Sequences";
constexpr const char* SEQUENCER_CURRENT_PATH_FILE = "/Sequences/.current";
constexpr const char* SEQUENCER_LEGACY_STORAGE_PATH = "/sequence.hbseq";
constexpr const char* SEQUENCER_FILE_EXTENSION = ".hbseq";
constexpr byte SEQUENCER_GATE_CHOICE_COUNT = 12;
constexpr byte SEQUENCER_MAX_ACTIVE_PLAYBACK_GROUPS = 16;
constexpr byte SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT = 6;
constexpr byte SEQUENCER_BROWSER_MAX_ENTRIES = 24;
constexpr size_t SEQUENCER_MAX_PATH_LENGTH = 255;
constexpr size_t SEQUENCER_BROWSER_TITLE_LENGTH = 28;
constexpr size_t SEQUENCER_NAME_EDIT_MAX_LENGTH = 20;

byte sequencerStepMidiNotes[SEQUENCER_STEP_COUNT][SEQUENCER_MAX_NOTES_PER_STEP] = {};
byte sequencerStepNoteCount[SEQUENCER_STEP_COUNT] = {};
uint16_t sequencerStepGatePercent[SEQUENCER_STEP_COUNT] = {};

enum class SequencerOverlayMode : uint8_t {
  Hidden = 0,
  AwaitingNote = 1,
  NoteAssigned = 2,
  StepCleared = 3,
  StatusMessage = 4,
  LengthEdit = 5,
  Overview = 6,
  Naming = 7
};

struct SequencerPlaybackGroup {
  bool active = false;
  byte midiNotes[SEQUENCER_MAX_NOTES_PER_STEP] = {
    SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE
  };
  byte noteCount = 0;
  uint64_t noteOffAt = 0;
};

enum class SequencerBrowserMode : uint8_t {
  None = 0,
  Load = 1,
  SaveNew = 2,
  DeleteFile = 3,
  DeleteFolder = 4,
  RenameFile = 5,
  RenameFolder = 6,
  CreateFolder = 7
};

enum class SequencerNamingTarget : uint8_t {
  None = 0,
  Sequence = 1,
  Folder = 2,
  RenameSequence = 3,
  RenameFolder = 4
};

enum class SequencerNamingAction : uint8_t {
  InsertChar = 0,
  Backspace = 1,
  Cancel = 2
};

struct SequencerBrowserEntry {
  bool isDirectory = false;
  char title[SEQUENCER_BROWSER_TITLE_LENGTH] = "";
  char path[SEQUENCER_MAX_PATH_LENGTH] = "";
};

struct SequencerNamingKey {
  byte buttonIndex = 0;
  SequencerNamingAction action = SequencerNamingAction::InsertChar;
  char character = '\0';
};

int8_t sequencerSelectedStep = -1;
SequencerOverlayMode sequencerOverlayMode = SequencerOverlayMode::Hidden;
uint64_t sequencerOverlayUntil = 0;
bool sequencerOverlayVisible = false;
bool sequencerOverlayDirty = false;
char sequencerStatusLineOne[24] = "";
char sequencerStatusLineTwo[24] = "";
byte sequencerEditMidiNotes[SEQUENCER_MAX_NOTES_PER_STEP] = {
  SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE
};
byte sequencerEditNoteCount = 0;
byte sequencerUndoMidiNotes[SEQUENCER_MAX_NOTES_PER_STEP] = {
  SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE
};
byte sequencerUndoNoteCount = 0;
byte sequencerAuditionHeldNoteCounts[128] = {};
byte sequencerPlaybackHeldNoteCounts[128] = {};
int8_t sequencerPlayingStep = -1;
SequencerPlaybackGroup sequencerPlaybackGroups[SEQUENCER_MAX_ACTIVE_PLAYBACK_GROUPS] = {};
uint64_t sequencerNextStepAt = 0;
uint64_t sequencerCurrentStepStartedAt = 0;
uint64_t sequencerConfirmPressedAt = 0;
bool sequencerConfirmHeld = false;
byte sequencerStepPlayCount = SEQUENCER_STEP_COUNT;
byte sequencerTapPreview = SEQUENCER_TAP_PREVIEW_ON;
byte sequencerPlayType = SEQUENCER_PLAY_TYPE_MIDI;
byte sequencerDirection = SEQUENCER_DIRECTION_FORWARD;
int8_t sequencerPingPongDelta = 1;
byte sequencerTempo = 120;
byte sequencerTransportState = 0;
bool sequencerDirty = false;
bool sequencerStorageInitialized = false;
uint16_t sequencerLengthPercentDisplay = 100;
byte sequencerOverviewPage = 0;
char sequencerCurrentSequencePath[SEQUENCER_MAX_PATH_LENGTH] = "";
char sequencerBrowserPath[SEQUENCER_MAX_PATH_LENGTH] = "";
SequencerBrowserMode sequencerBrowserMode = SequencerBrowserMode::None;
SequencerBrowserEntry sequencerBrowserEntries[SEQUENCER_BROWSER_MAX_ENTRIES] = {};
byte sequencerBrowserEntryCount = 0;
byte sequencerBrowserOffset = 0;
char sequencerBrowserPageTitle[20] = "Sequences";
char sequencerBrowserEntryTitles[SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT][SEQUENCER_BROWSER_TITLE_LENGTH] = {};
SequencerNamingTarget sequencerNamingTarget = SequencerNamingTarget::None;
char sequencerNamingBuffer[SEQUENCER_NAME_EDIT_MAX_LENGTH + 1] = "";
byte sequencerNamingLength = 0;
char sequencerRenameSourcePath[SEQUENCER_MAX_PATH_LENGTH] = "";

void showSequencerStatusMessage(const char* lineOne, const char* lineTwo);
void refreshSequencerBrowserMenu(bool resetSelection = true);
void sequencerBrowserNewFolderCallback();
bool isSequencerNamingActive();
void openSequencerDeleteFileBrowser();
void openSequencerDeleteFolderBrowser();
void openSequencerRenameFileBrowser();
void openSequencerRenameFolderBrowser();
void openSequencerCreateFolderBrowser();
void sequencerBrowserDeleteFolderCallback();
void sequencerBrowserRenameFolderCallback();

const char* sequencerChromaticNames[12] = {
  "C", "C#", "D", "Eb", "E", "F",
  "F#", "G", "G#", "A", "Bb", "B"
};
const uint16_t sequencerGateChoices[SEQUENCER_GATE_CHOICE_COUNT] = {
  0, 25, 50, 75, 100, 150, 200, 250, 300, 350, 400, 1000
};

byte sequencerGateChoiceIndex(uint16_t gatePercent);
void sortSequencerBrowserEntriesRange(byte startIndex, byte endExclusive);
void startSequencerNaming(SequencerNamingTarget target, const char* initialText);

const SequencerNamingKey sequencerNamingKeys[] = {
  { 1, SequencerNamingAction::InsertChar, 'A' },
  { 2, SequencerNamingAction::InsertChar, 'B' },
  { 3, SequencerNamingAction::InsertChar, 'C' },
  { 4, SequencerNamingAction::InsertChar, 'D' },
  { 5, SequencerNamingAction::InsertChar, 'E' },
  { 6, SequencerNamingAction::InsertChar, 'F' },
  { 7, SequencerNamingAction::InsertChar, 'G' },
  { 8, SequencerNamingAction::InsertChar, 'H' },
  { 10, SequencerNamingAction::InsertChar, 'I' },
  { 11, SequencerNamingAction::InsertChar, 'J' },
  { 12, SequencerNamingAction::InsertChar, 'K' },
  { 13, SequencerNamingAction::InsertChar, 'L' },
  { 14, SequencerNamingAction::InsertChar, 'M' },
  { 15, SequencerNamingAction::InsertChar, 'N' },
  { 16, SequencerNamingAction::InsertChar, 'O' },
  { 17, SequencerNamingAction::InsertChar, 'P' },
  { 21, SequencerNamingAction::InsertChar, 'Q' },
  { 22, SequencerNamingAction::InsertChar, 'R' },
  { 23, SequencerNamingAction::InsertChar, 'S' },
  { 24, SequencerNamingAction::InsertChar, 'T' },
  { 25, SequencerNamingAction::InsertChar, 'U' },
  { 26, SequencerNamingAction::InsertChar, 'V' },
  { 27, SequencerNamingAction::InsertChar, 'W' },
  { 28, SequencerNamingAction::InsertChar, 'X' },
  { 30, SequencerNamingAction::InsertChar, 'Y' },
  { 31, SequencerNamingAction::InsertChar, 'Z' },
  { 32, SequencerNamingAction::InsertChar, ' ' },
  { 33, SequencerNamingAction::InsertChar, '-' },
  { 34, SequencerNamingAction::InsertChar, '1' },
  { 35, SequencerNamingAction::InsertChar, '2' },
  { 36, SequencerNamingAction::InsertChar, '3' },
  { 41, SequencerNamingAction::Backspace, '\0' },
  { 42, SequencerNamingAction::Cancel, '\0' }
};

void copySequencerString(char* destination, size_t destinationSize, const char* source) {
  if (destinationSize == 0) {
    return;
  }
  snprintf(destination, destinationSize, "%s", (source != nullptr) ? source : "");
}

bool sequencerPathIsRoot(const char* path) {
  return path != nullptr && strcmp(path, SEQUENCER_STORAGE_ROOT) == 0;
}

bool sequencerPathHasExtension(const char* path, const char* extension) {
  if (path == nullptr || extension == nullptr) {
    return false;
  }
  size_t pathLength = strlen(path);
  size_t extensionLength = strlen(extension);
  return pathLength > extensionLength && strcmp(path + pathLength - extensionLength, extension) == 0;
}

bool isSequencerFilePath(const char* path) {
  return sequencerPathHasExtension(path, SEQUENCER_FILE_EXTENSION);
}

bool sequencerPathStartsWith(const char* path, const char* prefix) {
  if (path == nullptr || prefix == nullptr) {
    return false;
  }
  size_t prefixLength = strlen(prefix);
  return strncmp(path, prefix, prefixLength) == 0;
}

bool sequencerPathIsWithinFolder(const char* path, const char* folderPath) {
  if (!sequencerPathStartsWith(path, folderPath)) {
    return false;
  }
  size_t folderLength = strlen(folderPath);
  return path[folderLength] == '/' || path[folderLength] == '\0';
}

void joinSequencerPath(const char* directoryPath, const char* leafName, char* out, size_t outSize) {
  if (outSize == 0) {
    return;
  }
  if (directoryPath == nullptr || directoryPath[0] == '\0') {
    snprintf(out, outSize, "%s", (leafName != nullptr) ? leafName : "");
    return;
  }
  if (leafName == nullptr || leafName[0] == '\0') {
    snprintf(out, outSize, "%s", directoryPath);
    return;
  }
  if (strcmp(directoryPath, "/") == 0) {
    snprintf(out, outSize, "/%s", leafName);
    return;
  }
  snprintf(out, outSize, "%s/%s", directoryPath, leafName);
}

void extractSequencerLeafName(const char* path, char* out, size_t outSize) {
  if (outSize == 0) {
    return;
  }
  if (path == nullptr || path[0] == '\0') {
    out[0] = '\0';
    return;
  }
  const char* slash = strrchr(path, '/');
  const char* leaf = (slash != nullptr) ? slash + 1 : path;
  snprintf(out, outSize, "%s", leaf);
}

void stripSequencerFileExtension(char* text) {
  if (text == nullptr) {
    return;
  }
  size_t textLength = strlen(text);
  size_t extensionLength = strlen(SEQUENCER_FILE_EXTENSION);
  if (textLength > extensionLength &&
      strcmp(text + textLength - extensionLength, SEQUENCER_FILE_EXTENSION) == 0) {
    text[textLength - extensionLength] = '\0';
  }
}

void extractSequencerDisplayName(const char* path, char* out, size_t outSize) {
  extractSequencerLeafName(path, out, outSize);
  stripSequencerFileExtension(out);
}

void extractSequencerParentPath(const char* path, char* out, size_t outSize) {
  if (outSize == 0) {
    return;
  }
  if (path == nullptr || path[0] == '\0' || sequencerPathIsRoot(path)) {
    copySequencerString(out, outSize, SEQUENCER_STORAGE_ROOT);
    return;
  }

  char working[SEQUENCER_MAX_PATH_LENGTH];
  copySequencerString(working, sizeof(working), path);
  char* slash = strrchr(working, '/');
  if (slash == nullptr || slash == working) {
    copySequencerString(out, outSize, SEQUENCER_STORAGE_ROOT);
    return;
  }
  *slash = '\0';
  copySequencerString(out, outSize, working);
}

void extractSequencerDirectoryPath(const char* filePath, char* out, size_t outSize) {
  if (filePath == nullptr || filePath[0] == '\0') {
    copySequencerString(out, outSize, SEQUENCER_STORAGE_ROOT);
    return;
  }
  if (!isSequencerFilePath(filePath)) {
    copySequencerString(out, outSize, filePath);
    return;
  }
  extractSequencerParentPath(filePath, out, outSize);
}

int compareSequencerStringsIgnoreCase(const char* left, const char* right) {
  while (*left != '\0' && *right != '\0') {
    int leftValue = tolower(static_cast<unsigned char>(*left));
    int rightValue = tolower(static_cast<unsigned char>(*right));
    if (leftValue != rightValue) {
      return leftValue - rightValue;
    }
    left++;
    right++;
  }
  return tolower(static_cast<unsigned char>(*left)) - tolower(static_cast<unsigned char>(*right));
}

void formatSequencerBrowserEntryTitle(const char* path, bool isDirectory, char* out, size_t outSize) {
  char displayName[SEQUENCER_BROWSER_TITLE_LENGTH];
  extractSequencerDisplayName(path, displayName, sizeof(displayName));
  if (displayName[0] == '\0') {
    copySequencerString(displayName, sizeof(displayName), isDirectory ? "Folder" : "Sequence");
  }
  snprintf(out, outSize, "%s%s", displayName, isDirectory ? "/" : "");
}

void clearSequencerBrowserEntries() {
  sequencerBrowserEntryCount = 0;
  for (byte index = 0; index < SEQUENCER_BROWSER_MAX_ENTRIES; index++) {
    sequencerBrowserEntries[index] = SequencerBrowserEntry{};
  }
}

bool ensureSequencerStorageRoot() {
  if (!fileSystemExists) {
    return false;
  }
  if (LittleFS.exists(SEQUENCER_STORAGE_ROOT)) {
    return true;
  }
  return LittleFS.mkdir(SEQUENCER_STORAGE_ROOT);
}

bool rememberSequencerCurrentPath() {
  if (!fileSystemExists || !ensureSequencerStorageRoot()) {
    return false;
  }
  if (sequencerCurrentSequencePath[0] == '\0') {
    if (LittleFS.exists(SEQUENCER_CURRENT_PATH_FILE)) {
      LittleFS.remove(SEQUENCER_CURRENT_PATH_FILE);
    }
    return true;
  }

  File f = LittleFS.open(SEQUENCER_CURRENT_PATH_FILE, "w");
  if (!f) {
    return false;
  }
  f.println(sequencerCurrentSequencePath);
  f.close();
  return true;
}

void setSequencerCurrentPath(const char* path) {
  copySequencerString(sequencerCurrentSequencePath, sizeof(sequencerCurrentSequencePath), path);
  rememberSequencerCurrentPath();
}

bool loadRememberedSequencerCurrentPath() {
  sequencerCurrentSequencePath[0] = '\0';
  if (!fileSystemExists || !LittleFS.exists(SEQUENCER_CURRENT_PATH_FILE)) {
    return false;
  }

  File f = LittleFS.open(SEQUENCER_CURRENT_PATH_FILE, "r");
  if (!f) {
    return false;
  }

  String path = f.readStringUntil('\n');
  f.close();
  path.trim();
  if (path.length() == 0 || !path.startsWith(SEQUENCER_STORAGE_ROOT) || !isSequencerFilePath(path.c_str())) {
    return false;
  }

  copySequencerString(sequencerCurrentSequencePath, sizeof(sequencerCurrentSequencePath), path.c_str());
  return true;
}

bool addSequencerBrowserEntry(const char* path, bool isDirectory) {
  if (sequencerBrowserEntryCount >= SEQUENCER_BROWSER_MAX_ENTRIES) {
    return false;
  }
  SequencerBrowserEntry& entry = sequencerBrowserEntries[sequencerBrowserEntryCount++];
  entry.isDirectory = isDirectory;
  copySequencerString(entry.path, sizeof(entry.path), path);
  formatSequencerBrowserEntryTitle(path, isDirectory, entry.title, sizeof(entry.title));
  return true;
}

void sortSequencerBrowserEntries() {
  if (sequencerBrowserEntryCount < 2) {
    return;
  }
  sortSequencerBrowserEntriesRange(0, sequencerBrowserEntryCount);
}

void sortSequencerBrowserEntriesRange(byte startIndex, byte endExclusive) {
  if (endExclusive <= startIndex + 1) {
    return;
  }
  for (byte i = startIndex; i + 1 < endExclusive; i++) {
    for (byte j = static_cast<byte>(i + 1); j < endExclusive; j++) {
      if (compareSequencerStringsIgnoreCase(sequencerBrowserEntries[j].title, sequencerBrowserEntries[i].title) < 0) {
        SequencerBrowserEntry temp = sequencerBrowserEntries[i];
        sequencerBrowserEntries[i] = sequencerBrowserEntries[j];
        sequencerBrowserEntries[j] = temp;
      }
    }
  }
}

void scanSequencerBrowserEntries(bool includeDirectories, bool includeFiles) {
  Dir dir = LittleFS.openDir(sequencerBrowserPath);
  while (dir.next() && sequencerBrowserEntryCount < SEQUENCER_BROWSER_MAX_ENTRIES) {
    String fileName = dir.fileName();
    if (fileName.length() == 0 || fileName.startsWith(".")) {
      continue;
    }

    if (dir.isDirectory()) {
      if (!includeDirectories) {
        continue;
      }
      char childPath[SEQUENCER_MAX_PATH_LENGTH];
      joinSequencerPath(sequencerBrowserPath, fileName.c_str(), childPath, sizeof(childPath));
      addSequencerBrowserEntry(childPath, true);
    } else if (includeFiles && isSequencerFilePath(fileName.c_str())) {
      char childPath[SEQUENCER_MAX_PATH_LENGTH];
      joinSequencerPath(sequencerBrowserPath, fileName.c_str(), childPath, sizeof(childPath));
      addSequencerBrowserEntry(childPath, false);
    }
  }
}

void rebuildSequencerBrowserEntries() {
  clearSequencerBrowserEntries();
  if (!fileSystemExists || !ensureSequencerStorageRoot()) {
    return;
  }

  if (sequencerBrowserPath[0] == '\0' || !LittleFS.exists(sequencerBrowserPath)) {
    copySequencerString(sequencerBrowserPath, sizeof(sequencerBrowserPath), SEQUENCER_STORAGE_ROOT);
  }

  scanSequencerBrowserEntries(true, false);
  sortSequencerBrowserEntriesRange(0, sequencerBrowserEntryCount);
  if (sequencerBrowserMode == SequencerBrowserMode::Load ||
      sequencerBrowserMode == SequencerBrowserMode::DeleteFile ||
      sequencerBrowserMode == SequencerBrowserMode::RenameFile) {
    byte directoryCount = sequencerBrowserEntryCount;
    scanSequencerBrowserEntries(false, true);
    sortSequencerBrowserEntriesRange(directoryCount, sequencerBrowserEntryCount);
  }

  if (sequencerBrowserEntryCount == 0) {
    sequencerBrowserOffset = 0;
  } else if (sequencerBrowserOffset >= sequencerBrowserEntryCount) {
    sequencerBrowserOffset =
      static_cast<byte>(((sequencerBrowserEntryCount - 1) / SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT) *
                        SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT);
  }
}

bool generateSequencerAutoPath(const char* directoryPath, char* out, size_t outSize) {
  if (!fileSystemExists || !ensureSequencerStorageRoot()) {
    return false;
  }

  char candidateName[32];
  for (unsigned index = 1; index <= 9999; index++) {
    snprintf(candidateName, sizeof(candidateName), "Sequence %03u%s", index, SEQUENCER_FILE_EXTENSION);
    joinSequencerPath(directoryPath, candidateName, out, outSize);
    if (!LittleFS.exists(out)) {
      return true;
    }
  }
  return false;
}

bool generateSequencerAutoFolderPath(const char* directoryPath, char* out, size_t outSize) {
  if (!fileSystemExists || !ensureSequencerStorageRoot()) {
    return false;
  }

  char candidateName[32];
  for (unsigned index = 1; index <= 9999; index++) {
    snprintf(candidateName, sizeof(candidateName), "Folder %03u", index);
    joinSequencerPath(directoryPath, candidateName, out, outSize);
    if (!LittleFS.exists(out)) {
      return true;
    }
  }
  return false;
}

void clearSequencerCurrentPathIfDeleted(const char* path, bool isDirectory) {
  if (sequencerCurrentSequencePath[0] == '\0' || path == nullptr || path[0] == '\0') {
    return;
  }

  bool shouldClear = false;
  if (isDirectory) {
    shouldClear = sequencerPathIsWithinFolder(sequencerCurrentSequencePath, path);
  } else {
    shouldClear = strcmp(sequencerCurrentSequencePath, path) == 0;
  }

  if (shouldClear) {
    sequencerCurrentSequencePath[0] = '\0';
    rememberSequencerCurrentPath();
  }
}

bool deleteSequencerFolderRecursive(const char* folderPath) {
  if (!fileSystemExists || folderPath == nullptr || folderPath[0] == '\0' || sequencerPathIsRoot(folderPath)) {
    return false;
  }

  Dir dir = LittleFS.openDir(folderPath);
  while (dir.next()) {
    String entryName = dir.fileName();
    if (entryName.length() == 0 || entryName.startsWith(".")) {
      continue;
    }

    char childPath[SEQUENCER_MAX_PATH_LENGTH];
    joinSequencerPath(folderPath, entryName.c_str(), childPath, sizeof(childPath));
    if (dir.isDirectory()) {
      if (!deleteSequencerFolderRecursive(childPath)) {
        return false;
      }
    } else {
      if (!LittleFS.remove(childPath)) {
        return false;
      }
    }
  }

  return LittleFS.rmdir(folderPath);
}

void startSequencerRename(SequencerNamingTarget target, const char* sourcePath) {
  char initialName[SEQUENCER_NAME_EDIT_MAX_LENGTH + 1];
  extractSequencerDisplayName(sourcePath, initialName, sizeof(initialName));
  copySequencerString(sequencerRenameSourcePath, sizeof(sequencerRenameSourcePath), sourcePath);
  startSequencerNaming(target, initialName);
}

bool isSequencerNamingActive() {
  return sequencerOverlayMode == SequencerOverlayMode::Naming && sequencerNamingTarget != SequencerNamingTarget::None;
}

const SequencerNamingKey* getSequencerNamingKey(byte buttonIndex) {
  for (const SequencerNamingKey& key : sequencerNamingKeys) {
    if (key.buttonIndex == buttonIndex) {
      return &key;
    }
  }
  return nullptr;
}

void setSequencerNamingBuffer(const char* text) {
  copySequencerString(sequencerNamingBuffer, sizeof(sequencerNamingBuffer), text);
  sequencerNamingLength = static_cast<byte>(strlen(sequencerNamingBuffer));
  if (sequencerNamingLength > SEQUENCER_NAME_EDIT_MAX_LENGTH) {
    sequencerNamingLength = SEQUENCER_NAME_EDIT_MAX_LENGTH;
    sequencerNamingBuffer[sequencerNamingLength] = '\0';
  }
}

void startSequencerNaming(SequencerNamingTarget target, const char* initialText) {
  sequencerNamingTarget = target;
  setSequencerNamingBuffer(initialText);
  sequencerOverlayMode = SequencerOverlayMode::Naming;
  sequencerOverlayUntil = 0;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = true;
}

void exitSequencerNaming(bool redrawBrowser = true) {
  sequencerNamingTarget = SequencerNamingTarget::None;
  sequencerNamingBuffer[0] = '\0';
  sequencerNamingLength = 0;
  sequencerRenameSourcePath[0] = '\0';
  sequencerOverlayMode = SequencerOverlayMode::Hidden;
  sequencerOverlayUntil = 0;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = false;
  if (redrawBrowser) {
    menu.setMenuPageCurrent(menuPageSequencerBrowser);
    menu.drawMenu();
  }
}

void insertSequencerNamingChar(char character) {
  if (sequencerNamingLength >= SEQUENCER_NAME_EDIT_MAX_LENGTH) {
    return;
  }
  sequencerNamingBuffer[sequencerNamingLength++] = character;
  sequencerNamingBuffer[sequencerNamingLength] = '\0';
}

void backspaceSequencerNamingChar() {
  if (sequencerNamingLength == 0) {
    return;
  }
  sequencerNamingLength--;
  sequencerNamingBuffer[sequencerNamingLength] = '\0';
}

int8_t buttonIndexToSequencerStep(byte buttonIndex) {
  if (buttonIndex >= 1 && buttonIndex <= 8) {
    return static_cast<int8_t>(buttonIndex - 1);
  }
  if (buttonIndex >= 10 && buttonIndex < 18) {
    return static_cast<int8_t>(8 + (buttonIndex - 10));
  }
  if (buttonIndex >= 20 && buttonIndex < 28) {
    return static_cast<int8_t>(16 + (buttonIndex - 20));
  }
  if (buttonIndex >= 30 && buttonIndex < 38) {
    return static_cast<int8_t>(24 + (buttonIndex - 30));
  }
  return -1;
}

int8_t sequencerStepToButtonIndex(byte stepIndex) {
  if (stepIndex < 8) {
    return static_cast<int8_t>(stepIndex + 1);
  }
  if (stepIndex < 16) {
    return static_cast<int8_t>(10 + (stepIndex - 8));
  }
  if (stepIndex < 24) {
    return static_cast<int8_t>(20 + (stepIndex - 16));
  }
  if (stepIndex < SEQUENCER_STEP_COUNT) {
    return static_cast<int8_t>(30 + (stepIndex - 24));
  }
  return -1;
}

bool handleSequencerRotaryTurnInternal(int8_t direction) {
  if (sequencerSelectedStep < 0 || direction == 0) {
    return false;
  }

  int8_t buttonIndex = sequencerStepToButtonIndex(static_cast<byte>(sequencerSelectedStep));
  if (buttonIndex < 0 || !isBoardButtonPressed(static_cast<byte>(buttonIndex))) {
    return false;
  }

  uint16_t currentGate = sequencerStepGatePercent[sequencerSelectedStep];
  byte gateIndex = sequencerGateChoiceIndex(currentGate);
  int nextIndex = static_cast<int>(gateIndex) + direction;
  if (nextIndex < 0) {
    nextIndex = 0;
  } else if (nextIndex >= SEQUENCER_GATE_CHOICE_COUNT) {
    nextIndex = SEQUENCER_GATE_CHOICE_COUNT - 1;
  }

  uint16_t newGate = sequencerGateChoices[nextIndex];
  if (newGate == currentGate) {
    return true;
  }

  sequencerStepGatePercent[sequencerSelectedStep] = newGate;
  sequencerLengthPercentDisplay = newGate;
  sequencerDirty = true;
  sequencerOverlayMode = SequencerOverlayMode::LengthEdit;
  sequencerOverlayUntil = runTime + SEQUENCER_NOTE_CONFIRM_MICROS;
  sequencerOverlayDirty = true;
  return true;
}

byte sequencerGateChoiceIndex(uint16_t gatePercent) {
  for (byte i = 0; i < SEQUENCER_GATE_CHOICE_COUNT; i++) {
    if (sequencerGateChoices[i] == gatePercent) {
      return i;
    }
  }
  byte nearestIndex = 0;
  uint16_t nearestDistance = 65535;
  for (byte i = 0; i < SEQUENCER_GATE_CHOICE_COUNT; i++) {
    uint16_t distance = static_cast<uint16_t>(abs(static_cast<int>(sequencerGateChoices[i]) - static_cast<int>(gatePercent)));
    if (distance < nearestDistance) {
      nearestDistance = distance;
      nearestIndex = i;
    }
  }
  return nearestIndex;
}

void formatSequencerStepNote(char* out, size_t outSize, byte midiNote) {
  if (midiNote >= 128) {
    snprintf(out, outSize, "--");
    return;
  }
  const char* label = sequencerChromaticNames[midiNote % 12];
  int octave = (midiNote / 12) - 1;
  snprintf(out, outSize, "%s%d", label, octave);
}

void clearSequencerNoteBuffer(byte* notes, byte& count) {
  count = 0;
  for (byte i = 0; i < SEQUENCER_MAX_NOTES_PER_STEP; i++) {
    notes[i] = SEQUENCER_NO_NOTE;
  }
}

void sortSequencerNoteBuffer(byte* notes, byte count) {
  if (count < 2) {
    return;
  }

  for (byte i = 0; i + 1 < count; i++) {
    for (byte j = static_cast<byte>(i + 1); j < count; j++) {
      if (notes[j] < notes[i]) {
        byte temp = notes[i];
        notes[i] = notes[j];
        notes[j] = temp;
      }
    }
  }
}

byte findNoteInBuffer(const byte* notes, byte count, byte midiNote) {
  for (byte i = 0; i < count; i++) {
    if (notes[i] == midiNote) {
      return i;
    }
  }
  return SEQUENCER_MAX_NOTES_PER_STEP;
}

void loadEditBufferFromStep(byte stepIndex) {
  clearSequencerNoteBuffer(sequencerEditMidiNotes, sequencerEditNoteCount);
  byte count = sequencerStepNoteCount[stepIndex];
  for (byte i = 0; i < count && i < SEQUENCER_MAX_NOTES_PER_STEP; i++) {
    sequencerEditMidiNotes[i] = sequencerStepMidiNotes[stepIndex][i];
  }
  sequencerEditNoteCount = count;
  sortSequencerNoteBuffer(sequencerEditMidiNotes, sequencerEditNoteCount);
}

void snapshotUndoBufferFromStep(byte stepIndex) {
  clearSequencerNoteBuffer(sequencerUndoMidiNotes, sequencerUndoNoteCount);
  byte count = sequencerStepNoteCount[stepIndex];
  for (byte i = 0; i < count && i < SEQUENCER_MAX_NOTES_PER_STEP; i++) {
    sequencerUndoMidiNotes[i] = sequencerStepMidiNotes[stepIndex][i];
  }
  sequencerUndoNoteCount = count;
}

void saveEditBufferToStep(byte stepIndex) {
  sortSequencerNoteBuffer(sequencerEditMidiNotes, sequencerEditNoteCount);
  clearSequencerNoteBuffer(sequencerStepMidiNotes[stepIndex], sequencerStepNoteCount[stepIndex]);
  for (byte i = 0; i < sequencerEditNoteCount && i < SEQUENCER_MAX_NOTES_PER_STEP; i++) {
    sequencerStepMidiNotes[stepIndex][i] = sequencerEditMidiNotes[i];
  }
  sequencerStepNoteCount[stepIndex] = sequencerEditNoteCount;
}

void toggleEditBufferNote(byte midiNote) {
  byte index = findNoteInBuffer(sequencerEditMidiNotes, sequencerEditNoteCount, midiNote);
  if (index < SEQUENCER_MAX_NOTES_PER_STEP) {
    for (byte i = index; i + 1 < sequencerEditNoteCount; i++) {
      sequencerEditMidiNotes[i] = sequencerEditMidiNotes[i + 1];
    }
    if (sequencerEditNoteCount > 0) {
      sequencerEditNoteCount--;
      sequencerEditMidiNotes[sequencerEditNoteCount] = SEQUENCER_NO_NOTE;
    }
    return;
  }

  if (sequencerEditNoteCount < SEQUENCER_MAX_NOTES_PER_STEP) {
    sequencerEditMidiNotes[sequencerEditNoteCount++] = midiNote;
  }
  sortSequencerNoteBuffer(sequencerEditMidiNotes, sequencerEditNoteCount);
}

byte sequencerPrimaryMidiNote(byte stepIndex) {
  if (stepIndex >= SEQUENCER_STEP_COUNT) {
    return SEQUENCER_NO_NOTE;
  }
  if (sequencerSelectedStep == stepIndex && sequencerOverlayMode == SequencerOverlayMode::AwaitingNote) {
    return (sequencerEditNoteCount > 0) ? sequencerEditMidiNotes[0] : SEQUENCER_NO_NOTE;
  }
  return (sequencerStepNoteCount[stepIndex] > 0) ? sequencerStepMidiNotes[stepIndex][0] : SEQUENCER_NO_NOTE;
}

bool isSequencerSelectionLit() {
  uint64_t cycleMicros = SEQUENCER_SELECTED_ON_MICROS + SEQUENCER_SELECTED_OFF_MICROS;
  if (cycleMicros == 0) {
    return true;
  }
  return (runTime % cycleMicros) < SEQUENCER_SELECTED_ON_MICROS;
}

void fillOverlayNoteLines(char* lineOne, size_t lineOneSize, char* lineTwo, size_t lineTwoSize) {
  lineOne[0] = '\0';
  lineTwo[0] = '\0';

  const byte* sourceNotes = sequencerEditMidiNotes;
  byte sourceCount = sequencerEditNoteCount;
  if (sequencerOverlayMode == SequencerOverlayMode::NoteAssigned && sequencerSelectedStep >= 0) {
    sourceNotes = sequencerStepMidiNotes[sequencerSelectedStep];
    sourceCount = sequencerStepNoteCount[sequencerSelectedStep];
  }

  if (sourceCount == 0) {
    snprintf(lineOne, lineOneSize, "--");
    return;
  }

  char noteLabel[12];
  for (byte i = 0; i < sourceCount && i < SEQUENCER_MAX_NOTES_PER_STEP; i++) {
    formatSequencerStepNote(noteLabel, sizeof(noteLabel), sourceNotes[i]);
    if (lineOne[0] != '\0') {
      strncat(lineOne, " ", lineOneSize - strlen(lineOne) - 1);
    }
    strncat(lineOne, noteLabel, lineOneSize - strlen(lineOne) - 1);
  }
}

void fillOverviewStepLine(byte stepIndex, char* lineOut, size_t lineOutSize) {
  if (stepIndex >= SEQUENCER_STEP_COUNT || lineOutSize == 0) {
    return;
  }

  snprintf(lineOut, lineOutSize, "%02u ", static_cast<unsigned>(stepIndex + 1));

  if (sequencerStepNoteCount[stepIndex] == 0) {
    strncat(lineOut, "_", lineOutSize - strlen(lineOut) - 1);
    return;
  }

  char noteLabel[8];
  for (byte noteIndex = 0; noteIndex < sequencerStepNoteCount[stepIndex] &&
                           noteIndex < SEQUENCER_MAX_NOTES_PER_STEP; noteIndex++) {
    formatSequencerStepNote(noteLabel, sizeof(noteLabel), sequencerStepMidiNotes[stepIndex][noteIndex]);
    if (noteIndex > 0) {
      strncat(lineOut, " ", lineOutSize - strlen(lineOut) - 1);
    }
    strncat(lineOut, noteLabel, lineOutSize - strlen(lineOut) - 1);
  }
}

void hideSequencerOverlay() {
  sequencerOverlayMode = SequencerOverlayMode::Hidden;
  sequencerOverlayUntil = 0;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = false;
}

void showSequencerOverviewPage(bool advancePage) {
  constexpr byte pageCount =
    (SEQUENCER_STEP_COUNT + SEQUENCER_OVERVIEW_STEPS_PER_PAGE - 1) / SEQUENCER_OVERVIEW_STEPS_PER_PAGE;
  if (advancePage) {
    sequencerOverviewPage = static_cast<byte>((sequencerOverviewPage + 1) % pageCount);
  } else {
    sequencerOverviewPage = 0;
  }
  sequencerOverlayMode = SequencerOverlayMode::Overview;
  sequencerOverlayUntil = 0;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = true;
}

uint64_t sequencerStepDurationMicros() {
  byte tempo = (sequencerTempo == 0) ? 1 : sequencerTempo;
  return 60000000ULL / static_cast<uint64_t>(tempo) / 4ULL;
}

void sendSequencerManagedNoteOn(byte midiNote, bool playbackNote) {
  if (midiNote >= 128) {
    return;
  }
  byte& heldCount = playbackNote ? sequencerPlaybackHeldNoteCounts[midiNote] : sequencerAuditionHeldNoteCounts[midiNote];
  if (heldCount == 0 && sequencerPlaybackHeldNoteCounts[midiNote] == 0 && sequencerAuditionHeldNoteCounts[midiNote] == 0) {
    if (sequencerPlayType == SEQUENCER_PLAY_TYPE_OB_SYNTH) {
      sendBoardPreviewSynthNote(midiNote, true);
    } else {
      sendBoardPreviewMidiNote(midiNote, true);
    }
  }
  if (heldCount < 255) {
    heldCount++;
  }
}

void sendSequencerManagedNoteOff(byte midiNote, bool playbackNote) {
  if (midiNote >= 128) {
    return;
  }
  byte& heldCount = playbackNote ? sequencerPlaybackHeldNoteCounts[midiNote] : sequencerAuditionHeldNoteCounts[midiNote];
  if (heldCount > 0) {
    heldCount--;
  }
  if (sequencerPlaybackHeldNoteCounts[midiNote] == 0 && sequencerAuditionHeldNoteCounts[midiNote] == 0) {
    if (sequencerPlayType == SEQUENCER_PLAY_TYPE_OB_SYNTH) {
      sendBoardPreviewSynthNote(midiNote, false);
    } else {
      sendBoardPreviewMidiNote(midiNote, false);
    }
  }
}

byte sequencerActiveStepCount() {
  byte activeStepCount = sequencerStepPlayCount;
  if (activeStepCount < 1) {
    return 1;
  }
  if (activeStepCount > SEQUENCER_STEP_COUNT) {
    return SEQUENCER_STEP_COUNT;
  }
  return activeStepCount;
}

int8_t nextSequencerStep(byte activeStepCount) {
  if (activeStepCount <= 1) {
    return 0;
  }

  switch (sequencerDirection) {
    case SEQUENCER_DIRECTION_BACKWARD:
      if (sequencerPlayingStep < 0) {
        return static_cast<int8_t>(activeStepCount - 1);
      }
      return static_cast<int8_t>((sequencerPlayingStep + activeStepCount - 1) % activeStepCount);

    case SEQUENCER_DIRECTION_PING_PONG:
      if (sequencerPlayingStep < 0) {
        sequencerPingPongDelta = 1;
        return 0;
      }
      if (sequencerPlayingStep >= activeStepCount - 1) {
        sequencerPingPongDelta = -1;
      } else if (sequencerPlayingStep <= 0) {
        sequencerPingPongDelta = 1;
      }
      return static_cast<int8_t>(sequencerPlayingStep + sequencerPingPongDelta);

    case SEQUENCER_DIRECTION_RANDOM:
      return static_cast<int8_t>(random(activeStepCount));

    case SEQUENCER_DIRECTION_BROWNIAN:
      // Brownian wanders to a neighboring step each tick, creating a slow random walk.
      if (sequencerPlayingStep < 0) {
        return 0;
      }
      if (sequencerPlayingStep <= 0) {
        return 1;
      }
      if (sequencerPlayingStep >= activeStepCount - 1) {
        return static_cast<int8_t>(activeStepCount - 2);
      }
      return static_cast<int8_t>(sequencerPlayingStep + (random(2) == 0 ? -1 : 1));

    case SEQUENCER_DIRECTION_DRUNK:
      // Drunk usually stays put or stumbles one step left or right for a looser wandering rhythm.
      if (sequencerPlayingStep < 0) {
        return 0;
      }
      {
        int8_t candidate = sequencerPlayingStep + static_cast<int8_t>(random(3)) - 1;
        if (candidate < 0) {
          candidate = 0;
        } else if (candidate >= activeStepCount) {
          candidate = static_cast<int8_t>(activeStepCount - 1);
        }
        return candidate;
      }

    case SEQUENCER_DIRECTION_FORWARD:
    default:
      if (sequencerPlayingStep < 0) {
        return 0;
      }
      return static_cast<int8_t>((sequencerPlayingStep + 1) % activeStepCount);
  }
}

void stopSequencerPlaybackNote() {
  for (byte groupIndex = 0; groupIndex < SEQUENCER_MAX_ACTIVE_PLAYBACK_GROUPS; groupIndex++) {
    SequencerPlaybackGroup& group = sequencerPlaybackGroups[groupIndex];
    if (!group.active) {
      continue;
    }
    for (byte noteIndex = 0; noteIndex < group.noteCount; noteIndex++) {
      if (group.midiNotes[noteIndex] < 128) {
        sendSequencerManagedNoteOff(group.midiNotes[noteIndex], true);
      }
      group.midiNotes[noteIndex] = SEQUENCER_NO_NOTE;
    }
    group.noteCount = 0;
    group.noteOffAt = 0;
    group.active = false;
  }
}

void serviceSequencerPlaybackGroups() {
  for (byte groupIndex = 0; groupIndex < SEQUENCER_MAX_ACTIVE_PLAYBACK_GROUPS; groupIndex++) {
    SequencerPlaybackGroup& group = sequencerPlaybackGroups[groupIndex];
    if (!group.active || runTime < group.noteOffAt) {
      continue;
    }
    for (byte noteIndex = 0; noteIndex < group.noteCount; noteIndex++) {
      if (group.midiNotes[noteIndex] < 128) {
        sendSequencerManagedNoteOff(group.midiNotes[noteIndex], true);
      }
      group.midiNotes[noteIndex] = SEQUENCER_NO_NOTE;
    }
    group.noteCount = 0;
    group.noteOffAt = 0;
    group.active = false;
  }
}

void startSequencerPlaybackGroup(byte stepIndex, uint64_t stepDuration) {
  uint16_t gatePercent = sequencerStepGatePercent[stepIndex];
  byte noteCount = sequencerStepNoteCount[stepIndex];
  if (noteCount == 0 || gatePercent == 0) {
    return;
  }

  int freeGroupIndex = -1;
  for (byte groupIndex = 0; groupIndex < SEQUENCER_MAX_ACTIVE_PLAYBACK_GROUPS; groupIndex++) {
    if (!sequencerPlaybackGroups[groupIndex].active) {
      freeGroupIndex = groupIndex;
      break;
    }
  }
  if (freeGroupIndex < 0) {
    freeGroupIndex = 0;
    for (byte groupIndex = 1; groupIndex < SEQUENCER_MAX_ACTIVE_PLAYBACK_GROUPS; groupIndex++) {
      if (sequencerPlaybackGroups[groupIndex].noteOffAt < sequencerPlaybackGroups[freeGroupIndex].noteOffAt) {
        freeGroupIndex = groupIndex;
      }
    }
    SequencerPlaybackGroup& oldestGroup = sequencerPlaybackGroups[freeGroupIndex];
    for (byte noteIndex = 0; noteIndex < oldestGroup.noteCount; noteIndex++) {
      if (oldestGroup.midiNotes[noteIndex] < 128) {
        sendSequencerManagedNoteOff(oldestGroup.midiNotes[noteIndex], true);
      }
    }
  }

  SequencerPlaybackGroup& group = sequencerPlaybackGroups[freeGroupIndex];
  group.active = true;
  group.noteCount = 0;
  uint64_t playbackStartedAt = runTime;
  group.noteOffAt = playbackStartedAt + ((stepDuration * gatePercent) / 100ULL);
  for (byte noteIndex = 0; noteIndex < SEQUENCER_MAX_NOTES_PER_STEP; noteIndex++) {
    group.midiNotes[noteIndex] = SEQUENCER_NO_NOTE;
  }

  for (byte noteIndex = 0; noteIndex < noteCount && noteIndex < SEQUENCER_MAX_NOTES_PER_STEP; noteIndex++) {
    byte midiNote = sequencerStepMidiNotes[stepIndex][noteIndex];
    if (midiNote >= 128) {
      continue;
    }
    sendSequencerManagedNoteOn(midiNote, true);
    group.midiNotes[group.noteCount++] = midiNote;
  }
}

void previewSequencerStep(byte stepIndex) {
  if (stepIndex >= SEQUENCER_STEP_COUNT) {
    return;
  }
  startSequencerPlaybackGroup(stepIndex, sequencerStepDurationMicros());
}

void clearSelectedSequencerStep() {
  if (sequencerSelectedStep < 0) {
    return;
  }
  clearSequencerNoteBuffer(sequencerEditMidiNotes, sequencerEditNoteCount);
  saveEditBufferToStep(static_cast<byte>(sequencerSelectedStep));
  sequencerDirty = true;
  sequencerOverlayMode = SequencerOverlayMode::StepCleared;
  sequencerOverlayDirty = true;
  sequencerConfirmHeld = false;
  sequencerConfirmPressedAt = 0;
}

void resetSequencerState() {
  for (byte step = 0; step < SEQUENCER_STEP_COUNT; step++) {
    clearSequencerNoteBuffer(sequencerStepMidiNotes[step], sequencerStepNoteCount[step]);
    sequencerStepGatePercent[step] = 100;
  }
  memset(sequencerAuditionHeldNoteCounts, 0, sizeof(sequencerAuditionHeldNoteCounts));
  memset(sequencerPlaybackHeldNoteCounts, 0, sizeof(sequencerPlaybackHeldNoteCounts));
  for (byte groupIndex = 0; groupIndex < SEQUENCER_MAX_ACTIVE_PLAYBACK_GROUPS; groupIndex++) {
    sequencerPlaybackGroups[groupIndex] = SequencerPlaybackGroup{};
  }
  clearSequencerNoteBuffer(sequencerEditMidiNotes, sequencerEditNoteCount);
  clearSequencerNoteBuffer(sequencerUndoMidiNotes, sequencerUndoNoteCount);
  sequencerSelectedStep = -1;
  sequencerPlayingStep = -1;
  sequencerStepPlayCount = SEQUENCER_STEP_COUNT;
  sequencerTapPreview = SEQUENCER_TAP_PREVIEW_ON;
  sequencerPlayType = SEQUENCER_PLAY_TYPE_MIDI;
  sequencerDirection = SEQUENCER_DIRECTION_FORWARD;
  sequencerPingPongDelta = 1;
  sequencerTempo = 120;
  sequencerConfirmHeld = false;
  sequencerConfirmPressedAt = 0;
  sequencerNextStepAt = 0;
  sequencerCurrentStepStartedAt = 0;
  sequencerOverviewPage = 0;
  hideSequencerOverlay();
  stopSequencerPlaybackNote();
}

void parseSequencerStepNotes(byte stepIndex, const String& value) {
  clearSequencerNoteBuffer(sequencerStepMidiNotes[stepIndex], sequencerStepNoteCount[stepIndex]);
  if (value.length() == 0) {
    return;
  }

  int start = 0;
  while (start <= value.length() && sequencerStepNoteCount[stepIndex] < SEQUENCER_MAX_NOTES_PER_STEP) {
    int commaIndex = value.indexOf(',', start);
    String token = (commaIndex >= 0) ? value.substring(start, commaIndex) : value.substring(start);
    token.trim();
    if (token.length() > 0) {
      int noteValue = token.toInt();
      if (noteValue >= 0 && noteValue < 128) {
        sequencerStepMidiNotes[stepIndex][sequencerStepNoteCount[stepIndex]++] = static_cast<byte>(noteValue);
      }
    }
    if (commaIndex < 0) {
      break;
    }
    start = commaIndex + 1;
  }
  sortSequencerNoteBuffer(sequencerStepMidiNotes[stepIndex], sequencerStepNoteCount[stepIndex]);
}

bool loadSequencerFromFlash() {
  resetSequencerState();
  if (!fileSystemExists) {
    sequencerDirty = false;
    return false;
  }

  File f = LittleFS.open(SEQUENCER_LEGACY_STORAGE_PATH, "r");
  if (!f) {
    sequencerDirty = false;
    return false;
  }

  bool sawFormat = false;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line.startsWith("#")) {
      continue;
    }

    int equalsIndex = line.indexOf('=');
    if (equalsIndex < 0) {
      continue;
    }

    String key = line.substring(0, equalsIndex);
    String value = line.substring(equalsIndex + 1);
    key.trim();
    value.trim();

    if (key == "format") {
      sawFormat = (value == "HBSEQ");
    } else if (key == "tempo") {
      int tempoValue = value.toInt();
      if (tempoValue >= 1 && tempoValue <= 255) {
        sequencerTempo = static_cast<byte>(tempoValue);
      }
    } else if (key == "steps") {
      int stepCount = value.toInt();
      if (stepCount >= 1 && stepCount <= SEQUENCER_STEP_COUNT) {
        sequencerStepPlayCount = static_cast<byte>(stepCount);
      }
    } else if (key == "tapPreview") {
      int tapPreviewValue = value.toInt();
      sequencerTapPreview = (tapPreviewValue == SEQUENCER_TAP_PREVIEW_ON) ? SEQUENCER_TAP_PREVIEW_ON : SEQUENCER_TAP_PREVIEW_OFF;
    } else if (key == "playType") {
      int playTypeValue = value.toInt();
      sequencerPlayType = (playTypeValue == SEQUENCER_PLAY_TYPE_OB_SYNTH) ? SEQUENCER_PLAY_TYPE_OB_SYNTH : SEQUENCER_PLAY_TYPE_MIDI;
    } else if (key == "direction") {
      int directionValue = value.toInt();
      if (directionValue >= SEQUENCER_DIRECTION_FORWARD && directionValue <= SEQUENCER_DIRECTION_DRUNK) {
        sequencerDirection = static_cast<byte>(directionValue);
      }
    } else if (key.startsWith("step")) {
      int stepNumber = key.substring(4).toInt();
      if (stepNumber >= 1 && stepNumber <= SEQUENCER_STEP_COUNT) {
        parseSequencerStepNotes(static_cast<byte>(stepNumber - 1), value);
      }
    } else if (key.startsWith("gate")) {
      int stepNumber = key.substring(4).toInt();
      int gateValue = value.toInt();
      if (stepNumber >= 1 && stepNumber <= SEQUENCER_STEP_COUNT && gateValue >= 0 && gateValue <= 1000) {
        sequencerStepGatePercent[stepNumber - 1] = static_cast<uint16_t>(gateValue);
      }
    }
  }

  f.close();
  sequencerDirty = false;
  return sawFormat;
}

bool loadSequencerFromPath(const char* path) {
  resetSequencerState();
  if (!fileSystemExists || path == nullptr || path[0] == '\0') {
    sequencerDirty = false;
    return false;
  }

  File f = LittleFS.open(path, "r");
  if (!f) {
    sequencerDirty = false;
    return false;
  }

  bool sawFormat = false;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line.startsWith("#")) {
      continue;
    }

    int equalsIndex = line.indexOf('=');
    if (equalsIndex < 0) {
      continue;
    }

    String key = line.substring(0, equalsIndex);
    String value = line.substring(equalsIndex + 1);
    key.trim();
    value.trim();

    if (key == "format") {
      sawFormat = (value == "HBSEQ");
    } else if (key == "tempo") {
      int tempoValue = value.toInt();
      if (tempoValue >= 1 && tempoValue <= 255) {
        sequencerTempo = static_cast<byte>(tempoValue);
      }
    } else if (key == "steps") {
      int stepCount = value.toInt();
      if (stepCount >= 1 && stepCount <= SEQUENCER_STEP_COUNT) {
        sequencerStepPlayCount = static_cast<byte>(stepCount);
      }
    } else if (key == "tapPreview") {
      int tapPreviewValue = value.toInt();
      sequencerTapPreview = (tapPreviewValue == SEQUENCER_TAP_PREVIEW_ON) ? SEQUENCER_TAP_PREVIEW_ON : SEQUENCER_TAP_PREVIEW_OFF;
    } else if (key == "playType") {
      int playTypeValue = value.toInt();
      sequencerPlayType = (playTypeValue == SEQUENCER_PLAY_TYPE_OB_SYNTH) ? SEQUENCER_PLAY_TYPE_OB_SYNTH : SEQUENCER_PLAY_TYPE_MIDI;
    } else if (key == "direction") {
      int directionValue = value.toInt();
      if (directionValue >= SEQUENCER_DIRECTION_FORWARD && directionValue <= SEQUENCER_DIRECTION_DRUNK) {
        sequencerDirection = static_cast<byte>(directionValue);
      }
    } else if (key.startsWith("step")) {
      int stepNumber = key.substring(4).toInt();
      if (stepNumber >= 1 && stepNumber <= SEQUENCER_STEP_COUNT) {
        parseSequencerStepNotes(static_cast<byte>(stepNumber - 1), value);
      }
    } else if (key.startsWith("gate")) {
      int stepNumber = key.substring(4).toInt();
      int gateValue = value.toInt();
      if (stepNumber >= 1 && stepNumber <= SEQUENCER_STEP_COUNT && gateValue >= 0 && gateValue <= 1000) {
        sequencerStepGatePercent[stepNumber - 1] = static_cast<uint16_t>(gateValue);
      }
    }
  }

  f.close();
  sequencerDirty = false;
  return sawFormat;
}

bool saveSequencerToPath(const char* path) {
  if (!fileSystemExists || path == nullptr || path[0] == '\0' || !ensureSequencerStorageRoot()) {
    return false;
  }

  char tempPath[SEQUENCER_MAX_PATH_LENGTH];
  snprintf(tempPath, sizeof(tempPath), "%s.tmp", path);
  if (LittleFS.exists(tempPath)) {
    LittleFS.remove(tempPath);
  }

  File f = LittleFS.open(tempPath, "w");
  if (!f) {
    return false;
  }

  f.println("format=HBSEQ");
  f.println("version=1");
  f.print("tempo=");
  f.println(sequencerTempo);
  f.print("steps=");
  f.println(sequencerStepPlayCount);
  f.print("tapPreview=");
  f.println(sequencerTapPreview);
  f.print("playType=");
  f.println(sequencerPlayType);
  f.print("direction=");
  f.println(sequencerDirection);

  for (byte step = 0; step < SEQUENCER_STEP_COUNT; step++) {
    f.print("step");
    f.print(step + 1);
    f.print('=');
    for (byte noteIndex = 0; noteIndex < sequencerStepNoteCount[step]; noteIndex++) {
      if (noteIndex > 0) {
        f.print(',');
      }
      f.print(sequencerStepMidiNotes[step][noteIndex]);
    }
    f.println();
    f.print("gate");
    f.print(step + 1);
    f.print('=');
    f.println(sequencerStepGatePercent[step]);
  }

  f.close();
  if (LittleFS.exists(path)) {
    LittleFS.remove(path);
  }
  if (!LittleFS.rename(tempPath, path)) {
    LittleFS.remove(tempPath);
    return false;
  }
  sequencerDirty = false;
  return true;
}

bool saveSequencerToCurrentPath() {
  if (!fileSystemExists || !ensureSequencerStorageRoot()) {
    return false;
  }

  char targetPath[SEQUENCER_MAX_PATH_LENGTH];
  if (sequencerCurrentSequencePath[0] == '\0') {
    if (!generateSequencerAutoPath(SEQUENCER_STORAGE_ROOT, targetPath, sizeof(targetPath))) {
      return false;
    }
  } else {
    copySequencerString(targetPath, sizeof(targetPath), sequencerCurrentSequencePath);
  }

  if (!saveSequencerToPath(targetPath)) {
    return false;
  }

  setSequencerCurrentPath(targetPath);
  return true;
}

bool saveSequencerAsNewInDirectory(const char* directoryPath, char* savedPath, size_t savedPathSize) {
  if (savedPathSize == 0 || !fileSystemExists || !ensureSequencerStorageRoot()) {
    return false;
  }

  char targetPath[SEQUENCER_MAX_PATH_LENGTH];
  if (!generateSequencerAutoPath(directoryPath, targetPath, sizeof(targetPath))) {
    return false;
  }
  if (!saveSequencerToPath(targetPath)) {
    return false;
  }

  setSequencerCurrentPath(targetPath);
  copySequencerString(savedPath, savedPathSize, targetPath);
  return true;
}

bool loadSequencerAtStartup() {
  resetSequencerState();
  if (!fileSystemExists || !ensureSequencerStorageRoot()) {
    sequencerDirty = false;
    return false;
  }

  if (loadRememberedSequencerCurrentPath() && sequencerCurrentSequencePath[0] != '\0') {
    if (loadSequencerFromPath(sequencerCurrentSequencePath)) {
      return true;
    }
    sequencerCurrentSequencePath[0] = '\0';
    rememberSequencerCurrentPath();
  }

  return loadSequencerFromFlash();
}

void showSequencerPathStatusMessage(const char* lineOne, const char* path) {
  char displayName[24];
  extractSequencerDisplayName(path, displayName, sizeof(displayName));
  if (displayName[0] == '\0') {
    copySequencerString(displayName, sizeof(displayName), "Sequence");
  }
  showSequencerStatusMessage(lineOne, displayName);
}

void saveSequencerMenuCallback() {
  if (saveSequencerToCurrentPath()) {
    showSequencerPathStatusMessage("Saved", sequencerCurrentSequencePath);
  } else {
    showSequencerStatusMessage("Error Saving", "Flash write failed");
  }
}

void revertSequencerMenuCallback() {
  if (sequencerCurrentSequencePath[0] != '\0' && loadSequencerFromPath(sequencerCurrentSequencePath)) {
    showSequencerPathStatusMessage("Reverted", sequencerCurrentSequencePath);
  } else if (LittleFS.exists(SEQUENCER_LEGACY_STORAGE_PATH) && loadSequencerFromFlash()) {
    showSequencerStatusMessage("Reverted", "Legacy sequence");
  } else {
    resetSequencerState();
    sequencerDirty = false;
    showSequencerStatusMessage("Reverted", "Blank sequence");
  }
}

void openSequencerBrowser(SequencerBrowserMode browserMode) {
  sequencerBrowserMode = browserMode;
  sequencerBrowserOffset = 0;
  if (sequencerCurrentSequencePath[0] != '\0') {
    extractSequencerDirectoryPath(sequencerCurrentSequencePath, sequencerBrowserPath, sizeof(sequencerBrowserPath));
  } else {
    copySequencerString(sequencerBrowserPath, sizeof(sequencerBrowserPath), SEQUENCER_STORAGE_ROOT);
  }
  refreshSequencerBrowserMenu();
}

void openSequencerLoadBrowser() {
  openSequencerBrowser(SequencerBrowserMode::Load);
}

void openSequencerSaveNewBrowser() {
  openSequencerBrowser(SequencerBrowserMode::SaveNew);
}

void openSequencerDeleteFileBrowser() {
  openSequencerBrowser(SequencerBrowserMode::DeleteFile);
}

void openSequencerDeleteFolderBrowser() {
  openSequencerBrowser(SequencerBrowserMode::DeleteFolder);
}

void openSequencerRenameFileBrowser() {
  openSequencerBrowser(SequencerBrowserMode::RenameFile);
}

void openSequencerRenameFolderBrowser() {
  openSequencerBrowser(SequencerBrowserMode::RenameFolder);
}

void openSequencerCreateFolderBrowser() {
  openSequencerBrowser(SequencerBrowserMode::CreateFolder);
}

void sequencerBrowserUpCallback() {
  extractSequencerParentPath(sequencerBrowserPath, sequencerBrowserPath, sizeof(sequencerBrowserPath));
  sequencerBrowserOffset = 0;
  refreshSequencerBrowserMenu();
}

void sequencerBrowserPrevPageCallback() {
  if (sequencerBrowserOffset >= SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT) {
    sequencerBrowserOffset -= SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT;
  } else {
    sequencerBrowserOffset = 0;
  }
  refreshSequencerBrowserMenu(false);
}

void sequencerBrowserNextPageCallback() {
  if (sequencerBrowserOffset + SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT < sequencerBrowserEntryCount) {
    sequencerBrowserOffset += SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT;
  }
  refreshSequencerBrowserMenu(false);
}

void sequencerBrowserEntryCallback(GEMCallbackData callbackData) {
  int entryIndex = callbackData.valInt;
  if (entryIndex < 0 || entryIndex >= SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT) {
    return;
  }

  byte actualIndex = static_cast<byte>(sequencerBrowserOffset + entryIndex);
  if (actualIndex >= sequencerBrowserEntryCount) {
    return;
  }

  SequencerBrowserEntry& entry = sequencerBrowserEntries[actualIndex];
  if (entry.isDirectory) {
    copySequencerString(sequencerBrowserPath, sizeof(sequencerBrowserPath), entry.path);
    sequencerBrowserOffset = 0;
    refreshSequencerBrowserMenu();
    return;
  }

  if (sequencerBrowserMode == SequencerBrowserMode::Load && loadSequencerFromPath(entry.path)) {
    setSequencerCurrentPath(entry.path);
    menu.setMenuPageCurrent(menuPageSequencer);
    menu.drawMenu();
    showSequencerPathStatusMessage("Loaded", entry.path);
  } else if (sequencerBrowserMode == SequencerBrowserMode::DeleteFile) {
    char deletedPath[SEQUENCER_MAX_PATH_LENGTH];
    copySequencerString(deletedPath, sizeof(deletedPath), entry.path);
    clearSequencerCurrentPathIfDeleted(deletedPath, false);
    if (LittleFS.remove(deletedPath)) {
      refreshSequencerBrowserMenu(false);
      showSequencerPathStatusMessage("Deleted", deletedPath);
    } else {
      menu.setMenuPageCurrent(menuPageSequencerBrowser);
      menu.drawMenu();
      showSequencerStatusMessage("Error Delete", "File failed");
    }
  } else if (sequencerBrowserMode == SequencerBrowserMode::RenameFile) {
    startSequencerRename(SequencerNamingTarget::RenameSequence, entry.path);
  } else {
    menu.setMenuPageCurrent(menuPageSequencer);
    menu.drawMenu();
    showSequencerStatusMessage("Error Loading", "Read failed");
  }
}

void sequencerBrowserSaveHereCallback() {
  if (sequencerBrowserMode == SequencerBrowserMode::CreateFolder) {
    char suggestedPath[SEQUENCER_MAX_PATH_LENGTH];
    char suggestedName[SEQUENCER_NAME_EDIT_MAX_LENGTH + 1];
    if (generateSequencerAutoFolderPath(sequencerBrowserPath, suggestedPath, sizeof(suggestedPath))) {
      extractSequencerDisplayName(suggestedPath, suggestedName, sizeof(suggestedName));
    } else {
      copySequencerString(suggestedName, sizeof(suggestedName), "FOLDER");
    }
    startSequencerNaming(SequencerNamingTarget::Folder, suggestedName);
    return;
  }

  char suggestedPath[SEQUENCER_MAX_PATH_LENGTH];
  char suggestedName[SEQUENCER_NAME_EDIT_MAX_LENGTH + 1];
  if (generateSequencerAutoPath(sequencerBrowserPath, suggestedPath, sizeof(suggestedPath))) {
    extractSequencerDisplayName(suggestedPath, suggestedName, sizeof(suggestedName));
  } else {
    copySequencerString(suggestedName, sizeof(suggestedName), "SEQUENCE");
  }
  startSequencerNaming(SequencerNamingTarget::Sequence, suggestedName);
}

void sequencerBrowserNewFolderCallback() {
  char suggestedPath[SEQUENCER_MAX_PATH_LENGTH];
  char suggestedName[SEQUENCER_NAME_EDIT_MAX_LENGTH + 1];
  if (generateSequencerAutoFolderPath(sequencerBrowserPath, suggestedPath, sizeof(suggestedPath))) {
    extractSequencerDisplayName(suggestedPath, suggestedName, sizeof(suggestedName));
  } else {
    copySequencerString(suggestedName, sizeof(suggestedName), "FOLDER");
  }
  startSequencerNaming(SequencerNamingTarget::Folder, suggestedName);
}

void sequencerBrowserDeleteFolderCallback() {
  if (sequencerBrowserMode != SequencerBrowserMode::DeleteFolder || sequencerPathIsRoot(sequencerBrowserPath)) {
    return;
  }

  char folderPath[SEQUENCER_MAX_PATH_LENGTH];
  char parentPath[SEQUENCER_MAX_PATH_LENGTH];
  copySequencerString(folderPath, sizeof(folderPath), sequencerBrowserPath);
  extractSequencerParentPath(folderPath, parentPath, sizeof(parentPath));

  clearSequencerCurrentPathIfDeleted(folderPath, true);
  if (deleteSequencerFolderRecursive(folderPath)) {
    copySequencerString(sequencerBrowserPath, sizeof(sequencerBrowserPath), parentPath);
    sequencerBrowserOffset = 0;
    refreshSequencerBrowserMenu();
    showSequencerPathStatusMessage("Deleted", folderPath);
  } else {
    menu.setMenuPageCurrent(menuPageSequencerBrowser);
    menu.drawMenu();
    showSequencerStatusMessage("Error Delete", "Folder failed");
  }
}

void sequencerBrowserRenameFolderCallback() {
  if (sequencerBrowserMode != SequencerBrowserMode::RenameFolder || sequencerPathIsRoot(sequencerBrowserPath)) {
    return;
  }
  startSequencerRename(SequencerNamingTarget::RenameFolder, sequencerBrowserPath);
}

bool commitSequencerNaming() {
  if (!isSequencerNamingActive()) {
    return false;
  }
  if (sequencerNamingLength == 0) {
    showSequencerStatusMessage("Name Empty", "Enter a name");
    sequencerOverlayMode = SequencerOverlayMode::Naming;
    sequencerOverlayUntil = 0;
    sequencerOverlayVisible = false;
    sequencerOverlayDirty = true;
    return true;
  }

  char targetPath[SEQUENCER_MAX_PATH_LENGTH];
  if (sequencerNamingTarget == SequencerNamingTarget::Sequence ||
      sequencerNamingTarget == SequencerNamingTarget::RenameSequence) {
    char fileLeaf[SEQUENCER_MAX_PATH_LENGTH];
    snprintf(fileLeaf, sizeof(fileLeaf), "%s%s", sequencerNamingBuffer, SEQUENCER_FILE_EXTENSION);
    char parentPath[SEQUENCER_MAX_PATH_LENGTH];
    if (sequencerNamingTarget == SequencerNamingTarget::RenameSequence) {
      extractSequencerParentPath(sequencerRenameSourcePath, parentPath, sizeof(parentPath));
      joinSequencerPath(parentPath, fileLeaf, targetPath, sizeof(targetPath));
    } else {
      joinSequencerPath(sequencerBrowserPath, fileLeaf, targetPath, sizeof(targetPath));
    }

    if (sequencerNamingTarget == SequencerNamingTarget::RenameSequence &&
        strcmp(targetPath, sequencerRenameSourcePath) == 0) {
      exitSequencerNaming(true);
      return true;
    }

    if (LittleFS.exists(targetPath)) {
      showSequencerStatusMessage("Name Exists", "Pick another");
      sequencerOverlayMode = SequencerOverlayMode::Naming;
      sequencerOverlayUntil = 0;
      sequencerOverlayVisible = false;
      sequencerOverlayDirty = true;
      return true;
    }

    if (sequencerNamingTarget == SequencerNamingTarget::RenameSequence) {
      if (LittleFS.rename(sequencerRenameSourcePath, targetPath)) {
        if (strcmp(sequencerCurrentSequencePath, sequencerRenameSourcePath) == 0) {
          setSequencerCurrentPath(targetPath);
        }
        exitSequencerNaming(false);
        refreshSequencerBrowserMenu(false);
        showSequencerPathStatusMessage("Renamed", targetPath);
        return true;
      }
      showSequencerStatusMessage("Error Rename", "File failed");
      sequencerOverlayMode = SequencerOverlayMode::Naming;
      sequencerOverlayVisible = false;
      sequencerOverlayDirty = true;
      return true;
    } else if (saveSequencerToPath(targetPath)) {
      setSequencerCurrentPath(targetPath);
      sequencerDirty = false;
      sequencerNamingTarget = SequencerNamingTarget::None;
      menu.setMenuPageCurrent(menuPageSequencer);
      menu.drawMenu();
      showSequencerPathStatusMessage("Saved New", targetPath);
      return true;
    }

    showSequencerStatusMessage("Error Saving", "Save New failed");
    sequencerOverlayMode = SequencerOverlayMode::Naming;
    sequencerOverlayVisible = false;
    sequencerOverlayDirty = true;
    return true;
  }

  char parentPath[SEQUENCER_MAX_PATH_LENGTH];
  if (sequencerNamingTarget == SequencerNamingTarget::RenameFolder) {
    extractSequencerParentPath(sequencerRenameSourcePath, parentPath, sizeof(parentPath));
    joinSequencerPath(parentPath, sequencerNamingBuffer, targetPath, sizeof(targetPath));
    if (strcmp(targetPath, sequencerRenameSourcePath) == 0) {
      exitSequencerNaming(true);
      return true;
    }
  } else {
    joinSequencerPath(sequencerBrowserPath, sequencerNamingBuffer, targetPath, sizeof(targetPath));
  }
  if (LittleFS.exists(targetPath)) {
    showSequencerStatusMessage("Name Exists", "Pick another");
    sequencerOverlayMode = SequencerOverlayMode::Naming;
    sequencerOverlayUntil = 0;
    sequencerOverlayVisible = false;
    sequencerOverlayDirty = true;
    return true;
  }
  if (sequencerNamingTarget == SequencerNamingTarget::RenameFolder) {
    if (!LittleFS.rename(sequencerRenameSourcePath, targetPath)) {
      showSequencerStatusMessage("Error Rename", "Folder failed");
      sequencerOverlayMode = SequencerOverlayMode::Naming;
      sequencerOverlayVisible = false;
      sequencerOverlayDirty = true;
      return true;
    }

    if (sequencerPathIsWithinFolder(sequencerCurrentSequencePath, sequencerRenameSourcePath)) {
      char suffix[SEQUENCER_MAX_PATH_LENGTH];
      snprintf(suffix, sizeof(suffix), "%s", sequencerCurrentSequencePath + strlen(sequencerRenameSourcePath));
      snprintf(sequencerCurrentSequencePath, sizeof(sequencerCurrentSequencePath), "%s%s", targetPath, suffix);
      rememberSequencerCurrentPath();
    }
    copySequencerString(sequencerBrowserPath, sizeof(sequencerBrowserPath), targetPath);
    sequencerBrowserOffset = 0;
    exitSequencerNaming(false);
    refreshSequencerBrowserMenu();
    showSequencerPathStatusMessage("Renamed", targetPath);
    return true;
  }

  if (!LittleFS.mkdir(targetPath)) {
    showSequencerStatusMessage("Error Folder", "Create failed");
    sequencerOverlayMode = SequencerOverlayMode::Naming;
    sequencerOverlayVisible = false;
    sequencerOverlayDirty = true;
    return true;
  }

  copySequencerString(sequencerBrowserPath, sizeof(sequencerBrowserPath), targetPath);
  sequencerBrowserOffset = 0;
  sequencerNamingTarget = SequencerNamingTarget::None;
  refreshSequencerBrowserMenu();
  showSequencerPathStatusMessage("Folder Created", targetPath);
  return true;
}

void showSequencerStatusMessage(const char* lineOne, const char* lineTwo) {
  snprintf(sequencerStatusLineOne, sizeof(sequencerStatusLineOne), "%s", lineOne);
  snprintf(sequencerStatusLineTwo, sizeof(sequencerStatusLineTwo), "%s", lineTwo);
  sequencerOverlayMode = SequencerOverlayMode::StatusMessage;
  sequencerOverlayUntil = runTime + SEQUENCER_NOTE_CONFIRM_MICROS;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = true;
}

void setSequencerTransportState(byte newState) {
  byte normalizedState = (newState == SEQUENCER_TRANSPORT_PLAY) ? SEQUENCER_TRANSPORT_PLAY : SEQUENCER_TRANSPORT_STOP;
  sequencerTransportState = normalizedState;
  if (sequencerTransportState == SEQUENCER_TRANSPORT_PLAY) {
    sequencerPlayingStep = -1;
    sequencerPingPongDelta = 1;
    sequencerNextStepAt = runTime;
    sequencerCurrentStepStartedAt = runTime;
  } else {
    stopSequencerPlaybackNote();
    sequencerPlayingStep = -1;
    sequencerNextStepAt = 0;
    sequencerCurrentStepStartedAt = 0;
  }
  menu.drawMenu();
}

void sequencerTransportMenuCallback(GEMCallbackData callbackData) {
  setSequencerTransportState(callbackData.valByte);
}

void sequencerTempoMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  sequencerDirty = true;
}

void sequencerStepPlayCountMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  if (sequencerStepPlayCount < 1) {
    sequencerStepPlayCount = 1;
  } else if (sequencerStepPlayCount > SEQUENCER_STEP_COUNT) {
    sequencerStepPlayCount = SEQUENCER_STEP_COUNT;
  }
  sequencerDirty = true;
}

void sequencerTapPreviewMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  sequencerDirty = true;
}

void sequencerPlayTypeMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  sequencerDirty = true;
}

void sequencerDirectionMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  sequencerPingPongDelta = 1;
  sequencerDirty = true;
}

const GEMSpinnerBoundariesByte spinnerBoundariesSequencerStepPlayCount = { 1, 1, SEQUENCER_STEP_COUNT };
const GEMSpinnerBoundariesByte spinnerBoundariesSequencerTempo = { 1, 1, 255 };

GEMSpinner spinnerSequencerStepPlayCount(spinnerBoundariesSequencerStepPlayCount, GEM_LOOP);
GEMSpinner spinnerSequencerTempo(spinnerBoundariesSequencerTempo, GEM_LOOP);

SelectOptionByte optionByteSequencerTransport[] = { { "Stop", 0 }, { "Play", 1 } };
GEMSelect selectSequencerTransport(sizeof(optionByteSequencerTransport) / sizeof(SelectOptionByte), optionByteSequencerTransport);
SelectOptionByte optionByteSequencerTapPreview[] = { { "Off", SEQUENCER_TAP_PREVIEW_OFF }, { "On", SEQUENCER_TAP_PREVIEW_ON } };
GEMSelect selectSequencerTapPreview(sizeof(optionByteSequencerTapPreview) / sizeof(SelectOptionByte), optionByteSequencerTapPreview);
SelectOptionByte optionByteSequencerPlayType[] = { { "MIDI", SEQUENCER_PLAY_TYPE_MIDI }, { "OB Synth", SEQUENCER_PLAY_TYPE_OB_SYNTH } };
GEMSelect selectSequencerPlayType(sizeof(optionByteSequencerPlayType) / sizeof(SelectOptionByte), optionByteSequencerPlayType);

SelectOptionByte optionByteSequencerDirection[] = {
  { "Forward", SEQUENCER_DIRECTION_FORWARD },
  { "Backward", SEQUENCER_DIRECTION_BACKWARD },
  // Ping-Pong runs to the end of the pattern, then reverses direction until it reaches the start again.
  { "Ping-Pong", SEQUENCER_DIRECTION_PING_PONG },
  // Random chooses any step in the active range on each tick with no memory of the previous position.
  { "Random", SEQUENCER_DIRECTION_RANDOM },
  // Brownian moves only to neighboring steps, so the playhead drifts instead of jumping across the pattern.
  { "Brownian", SEQUENCER_DIRECTION_BROWNIAN },
  // Drunk can stay put or stumble one step left or right, making it even less predictable than Brownian.
  { "Drunk", SEQUENCER_DIRECTION_DRUNK }
};
GEMSelect selectSequencerDirection(sizeof(optionByteSequencerDirection) / sizeof(SelectOptionByte), optionByteSequencerDirection);

GEMItem menuItemEnterKeyboard("Keyboard", enterKeyboardMode);
GEMItem menuGotoSynthFromSequencer("Synth Options", menuPageSynthSequencer);
GEMItem menuGotoSequencerFiles("File Management", menuPageSequencerFiles);
GEMItem menuItemSequencerSave("Save", saveSequencerMenuCallback);
GEMItem menuItemSequencerSaveNew("Save New", openSequencerSaveNewBrowser);
GEMItem menuItemSequencerLoad("Load", openSequencerLoadBrowser);
GEMItem menuItemSequencerCreateFolder("Create Folder", openSequencerCreateFolderBrowser);
GEMItem menuItemSequencerRenameFile("Rename File", openSequencerRenameFileBrowser);
GEMItem menuItemSequencerRenameFolder("Rename Folder", openSequencerRenameFolderBrowser);
GEMItem menuItemSequencerDeleteFile("Delete File", openSequencerDeleteFileBrowser);
GEMItem menuItemSequencerDeleteFolder("Delete Folder", openSequencerDeleteFolderBrowser);
GEMItem menuItemSequencerRevert("Revert", revertSequencerMenuCallback);
GEMItem menuItemSequencerPlayStop("Play/Stop", sequencerTransportState, selectSequencerTransport, sequencerTransportMenuCallback);
GEMItem menuItemSequencerStepPlayCount("Steps", sequencerStepPlayCount, spinnerSequencerStepPlayCount, sequencerStepPlayCountMenuCallback);
GEMItem menuItemSequencerTapPreview("Tap Preview", sequencerTapPreview, selectSequencerTapPreview, sequencerTapPreviewMenuCallback);
GEMItem menuItemSequencerPlayType("Play Type", sequencerPlayType, selectSequencerPlayType, sequencerPlayTypeMenuCallback);
GEMItem menuItemSequencerDirection("Direction", sequencerDirection, selectSequencerDirection, sequencerDirectionMenuCallback);
GEMItem menuItemSequencerTempo("Tempo", sequencerTempo, spinnerSequencerTempo, sequencerTempoMenuCallback);
GEMItem menuItemSequencerFirmwareUpdate("Update Firmware", rebootToBootloader);
GEMItem menuItemSequencerBrowserSaveHere("Save Here", sequencerBrowserSaveHereCallback);
GEMItem menuItemSequencerBrowserNewFolder("New Folder", sequencerBrowserNewFolderCallback);
GEMItem menuItemSequencerBrowserRenameFolder("Rename This Folder", sequencerBrowserRenameFolderCallback);
GEMItem menuItemSequencerBrowserDeleteFolder("Delete This Folder", sequencerBrowserDeleteFolderCallback);
GEMItem menuItemSequencerBrowserUp("..", sequencerBrowserUpCallback);
GEMItem menuItemSequencerBrowserEntry0("", sequencerBrowserEntryCallback, 0);
GEMItem menuItemSequencerBrowserEntry1("", sequencerBrowserEntryCallback, 1);
GEMItem menuItemSequencerBrowserEntry2("", sequencerBrowserEntryCallback, 2);
GEMItem menuItemSequencerBrowserEntry3("", sequencerBrowserEntryCallback, 3);
GEMItem menuItemSequencerBrowserEntry4("", sequencerBrowserEntryCallback, 4);
GEMItem menuItemSequencerBrowserEntry5("", sequencerBrowserEntryCallback, 5);
GEMItem menuItemSequencerBrowserPrev("Prev", sequencerBrowserPrevPageCallback);
GEMItem menuItemSequencerBrowserNext("Next", sequencerBrowserNextPageCallback);
GEMItem* sequencerBrowserEntryItems[SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT] = {
  &menuItemSequencerBrowserEntry0,
  &menuItemSequencerBrowserEntry1,
  &menuItemSequencerBrowserEntry2,
  &menuItemSequencerBrowserEntry3,
  &menuItemSequencerBrowserEntry4,
  &menuItemSequencerBrowserEntry5
};

void refreshSequencerBrowserMenu(bool resetSelection) {
  rebuildSequencerBrowserEntries();

  char folderName[SEQUENCER_BROWSER_TITLE_LENGTH];
  if (sequencerPathIsRoot(sequencerBrowserPath)) {
    copySequencerString(folderName, sizeof(folderName), "Sequences");
  } else {
    extractSequencerDisplayName(sequencerBrowserPath, folderName, sizeof(folderName));
  }

  if (sequencerBrowserMode == SequencerBrowserMode::SaveNew) {
    snprintf(sequencerBrowserPageTitle, sizeof(sequencerBrowserPageTitle), "Save:%s", folderName);
  } else if (sequencerBrowserMode == SequencerBrowserMode::CreateFolder) {
    snprintf(sequencerBrowserPageTitle, sizeof(sequencerBrowserPageTitle), "Folder:%s", folderName);
  } else if (sequencerBrowserMode == SequencerBrowserMode::DeleteFile) {
    snprintf(sequencerBrowserPageTitle, sizeof(sequencerBrowserPageTitle), "DelFile:%s", folderName);
  } else if (sequencerBrowserMode == SequencerBrowserMode::DeleteFolder) {
    snprintf(sequencerBrowserPageTitle, sizeof(sequencerBrowserPageTitle), "DelFold:%s", folderName);
  } else if (sequencerBrowserMode == SequencerBrowserMode::RenameFile) {
    snprintf(sequencerBrowserPageTitle, sizeof(sequencerBrowserPageTitle), "RenFile:%s", folderName);
  } else if (sequencerBrowserMode == SequencerBrowserMode::RenameFolder) {
    snprintf(sequencerBrowserPageTitle, sizeof(sequencerBrowserPageTitle), "RenFold:%s", folderName);
  } else {
    snprintf(sequencerBrowserPageTitle, sizeof(sequencerBrowserPageTitle), "Load:%s", folderName);
  }

  menuPageSequencerBrowser.setTitle(sequencerBrowserPageTitle);

  bool showSaveHere = (sequencerBrowserMode == SequencerBrowserMode::SaveNew);
  bool showCreateHere = (sequencerBrowserMode == SequencerBrowserMode::CreateFolder);
  menuItemSequencerBrowserSaveHere.setTitle(showCreateHere ? "Create Here" : "Save Here");
  menuItemSequencerBrowserSaveHere.hide(!(showSaveHere || showCreateHere));
  menuItemSequencerBrowserNewFolder.hide(!showSaveHere);
  menuItemSequencerBrowserRenameFolder.hide(!(sequencerBrowserMode == SequencerBrowserMode::RenameFolder) ||
                                            sequencerPathIsRoot(sequencerBrowserPath));
  menuItemSequencerBrowserDeleteFolder.hide(!(sequencerBrowserMode == SequencerBrowserMode::DeleteFolder) ||
                                            sequencerPathIsRoot(sequencerBrowserPath));
  menuItemSequencerBrowserUp.hide(sequencerPathIsRoot(sequencerBrowserPath));

  for (byte i = 0; i < SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT; i++) {
    GEMItem* item = sequencerBrowserEntryItems[i];
    byte actualIndex = static_cast<byte>(sequencerBrowserOffset + i);
    if (actualIndex < sequencerBrowserEntryCount) {
      copySequencerString(sequencerBrowserEntryTitles[i], sizeof(sequencerBrowserEntryTitles[i]), sequencerBrowserEntries[actualIndex].title);
      item->setTitle(sequencerBrowserEntryTitles[i]).show();
    } else {
      item->hide();
    }
  }

  menuItemSequencerBrowserPrev.hide(sequencerBrowserOffset == 0);
  menuItemSequencerBrowserNext.hide(sequencerBrowserOffset + SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT >= sequencerBrowserEntryCount);

  if (resetSelection) {
    menuPageSequencerBrowser.setCurrentMenuItemIndex(0);
  }
  menu.setMenuPageCurrent(menuPageSequencerBrowser);
  menu.drawMenu();
}

}  // namespace

bool handleSequencerRotaryTurn(int8_t direction) {
  if (isSequencerNamingActive()) {
    (void)direction;
    return true;
  }
  return handleSequencerRotaryTurnInternal(direction);
}

bool handleSequencerEncoderClick() {
  if (!isSequencerNamingActive()) {
    return false;
  }
  return commitSequencerNaming();
}

GEMPage menuPageSequencer("Sequencer");
GEMPage menuPageSequencerFiles("File Management", menuPageSequencer);
GEMPage menuPageSequencerBrowser("Load", menuPageSequencer);

void handleSequencerButtonEvent(byte buttonIndex, bool pressed) {
  if (isSequencerNamingActive()) {
    if (!pressed) {
      return;
    }

    const SequencerNamingKey* namingKey = getSequencerNamingKey(buttonIndex);
    if (namingKey == nullptr) {
      return;
    }

    if (namingKey->action == SequencerNamingAction::InsertChar) {
      insertSequencerNamingChar(namingKey->character);
    } else if (namingKey->action == SequencerNamingAction::Backspace) {
      backspaceSequencerNamingChar();
    } else if (namingKey->action == SequencerNamingAction::Cancel) {
      exitSequencerNaming(true);
      return;
    }

    sequencerOverlayVisible = false;
    sequencerOverlayDirty = true;
    return;
  }

  if (!pressed) {
    if (buttonIndex == SEQUENCER_OVERVIEW_BUTTON_INDEX) {
      return;
    }
    if (buttonIndex == SEQUENCER_CONFIRM_BUTTON_INDEX) {
      if (sequencerSelectedStep >= 0 && sequencerConfirmHeld) {
        clearSequencerNoteBuffer(sequencerEditMidiNotes, sequencerEditNoteCount);
        for (byte i = 0; i < sequencerUndoNoteCount && i < SEQUENCER_MAX_NOTES_PER_STEP; i++) {
          sequencerEditMidiNotes[i] = sequencerUndoMidiNotes[i];
        }
        sequencerEditNoteCount = sequencerUndoNoteCount;
        saveEditBufferToStep(static_cast<byte>(sequencerSelectedStep));
        sequencerDirty = true;
        sequencerOverlayMode = SequencerOverlayMode::AwaitingNote;
        sequencerOverlayDirty = true;
      }
      sequencerConfirmHeld = false;
      sequencerConfirmPressedAt = 0;
      return;
    }

    byte releasedMidiNote = 0;
    if (getButtonMidiNoteForSequencer(buttonIndex, releasedMidiNote) && releasedMidiNote < 128) {
      sendSequencerManagedNoteOff(releasedMidiNote, false);
    }
    return;
  }

  screenTime = 0;
  if (screenSaverOn) {
    screenSaverOn = false;
    u8g2.setContrast(SEQUENCER_OVERLAY_CONTRAST);
  }

  if (buttonIndex == SEQUENCER_OVERVIEW_BUTTON_INDEX) {
    bool advancePage = (sequencerOverlayMode == SequencerOverlayMode::Overview);
    showSequencerOverviewPage(advancePage);
    return;
  }

  if (sequencerOverlayMode == SequencerOverlayMode::Overview) {
    hideSequencerOverlay();
  }

  if (buttonIndex == SEQUENCER_TRANSPORT_BUTTON_INDEX) {
    setSequencerTransportState(
      (sequencerTransportState == SEQUENCER_TRANSPORT_PLAY) ? SEQUENCER_TRANSPORT_STOP : SEQUENCER_TRANSPORT_PLAY);
    return;
  }

  if (buttonIndex == SEQUENCER_CONFIRM_BUTTON_INDEX) {
    sequencerConfirmHeld = true;
    sequencerConfirmPressedAt = runTime;
    return;
  }

  int8_t stepIndex = buttonIndexToSequencerStep(buttonIndex);
  if (stepIndex >= 0) {
    if (sequencerTapPreview == SEQUENCER_TAP_PREVIEW_ON) {
      previewSequencerStep(static_cast<byte>(stepIndex));
    }
    if (sequencerSelectedStep == stepIndex) {
      sequencerSelectedStep = -1;
      hideSequencerOverlay();
      return;
    }
    sequencerSelectedStep = stepIndex;
    snapshotUndoBufferFromStep(static_cast<byte>(stepIndex));
    loadEditBufferFromStep(static_cast<byte>(stepIndex));
    sequencerOverlayMode = SequencerOverlayMode::AwaitingNote;
    sequencerOverlayUntil = 0;
    sequencerOverlayDirty = true;
    return;
  }

  byte midiNote = 0;
  if (getButtonMidiNoteForSequencer(buttonIndex, midiNote)) {
    if (midiNote < 128) {
      sendSequencerManagedNoteOn(midiNote, false);
    }
    if (sequencerSelectedStep >= 0) {
      toggleEditBufferNote(midiNote);
      saveEditBufferToStep(static_cast<byte>(sequencerSelectedStep));
      sequencerDirty = true;
      sequencerOverlayMode = SequencerOverlayMode::AwaitingNote;
      sequencerOverlayDirty = true;
    }
  }
}

void setupSequencerMenu() {
  if (!sequencerStorageInitialized) {
    ensureSequencerStorageRoot();
    copySequencerString(sequencerBrowserPath, sizeof(sequencerBrowserPath), SEQUENCER_STORAGE_ROOT);
    loadSequencerAtStartup();
    sequencerStorageInitialized = true;
  }

  menuPageSequencer.addMenuItem(menuItemEnterKeyboard);
  menuPageSequencer.addMenuItem(menuGotoSynthFromSequencer);
  menuPageSequencer.addMenuItem(menuGotoSequencerFiles);
  menuPageSequencer.addMenuItem(menuItemSequencerSave);
  menuPageSequencer.addMenuItem(menuItemSequencerSaveNew);
  menuPageSequencer.addMenuItem(menuItemSequencerLoad);
  menuPageSequencer.addMenuItem(menuItemSequencerRevert);
  menuPageSequencer.addMenuItem(menuItemSequencerPlayStop);
  menuPageSequencer.addMenuItem(menuItemSequencerStepPlayCount);
  menuPageSequencer.addMenuItem(menuItemSequencerDirection);
  menuPageSequencer.addMenuItem(menuItemSequencerTempo);
  menuPageSequencer.addMenuItem(menuItemSequencerTapPreview);
  menuPageSequencer.addMenuItem(menuItemSequencerPlayType);
  menuPageSequencer.addMenuItem(menuItemSequencerFirmwareUpdate);

  menuPageSequencerFiles.addMenuItem(menuItemSequencerRenameFile);
  menuPageSequencerFiles.addMenuItem(menuItemSequencerRenameFolder);
  menuPageSequencerFiles.addMenuItem(menuItemSequencerCreateFolder);
  menuPageSequencerFiles.addMenuItem(menuItemSequencerDeleteFile);
  menuPageSequencerFiles.addMenuItem(menuItemSequencerDeleteFolder);

  menuPageSequencerBrowser.addMenuItem(menuItemSequencerBrowserSaveHere);
  menuPageSequencerBrowser.addMenuItem(menuItemSequencerBrowserNewFolder);
  menuPageSequencerBrowser.addMenuItem(menuItemSequencerBrowserRenameFolder);
  menuPageSequencerBrowser.addMenuItem(menuItemSequencerBrowserDeleteFolder);
  menuPageSequencerBrowser.addMenuItem(menuItemSequencerBrowserUp);
  for (byte i = 0; i < SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT; i++) {
    menuPageSequencerBrowser.addMenuItem(*sequencerBrowserEntryItems[i]);
  }
  menuPageSequencerBrowser.addMenuItem(menuItemSequencerBrowserPrev);
  menuPageSequencerBrowser.addMenuItem(menuItemSequencerBrowserNext);

  menuItemSequencerBrowserSaveHere.hide();
  menuItemSequencerBrowserNewFolder.hide();
  menuItemSequencerBrowserRenameFolder.hide();
  menuItemSequencerBrowserDeleteFolder.hide();
  menuItemSequencerBrowserUp.hide();
  menuItemSequencerBrowserPrev.hide();
  menuItemSequencerBrowserNext.hide();
  for (byte i = 0; i < SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT; i++) {
    sequencerBrowserEntryItems[i]->hide();
  }
}

void drawSequencerOverlay() {
  if (sequencerOverlayMode == SequencerOverlayMode::Naming && isSequencerNamingActive()) {
    sequencerOverlayVisible = true;
    sequencerOverlayDirty = false;

    char nameLine[SEQUENCER_NAME_EDIT_MAX_LENGTH + 2];
    copySequencerString(nameLine, sizeof(nameLine), sequencerNamingBuffer);
    bool showCursor = ((runTime / 400000ULL) % 2ULL) == 0ULL;
    if (showCursor && sequencerNamingLength < SEQUENCER_NAME_EDIT_MAX_LENGTH) {
      nameLine[sequencerNamingLength] = '_';
      nameLine[sequencerNamingLength + 1] = '\0';
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(4, 16, nameLine);

    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(4, 36, "A B C D E F G H");
    u8g2.drawStr(4, 50, "I J K L M N O P");
    u8g2.drawStr(4, 64, "Q R S T U V W X");
    u8g2.drawStr(4, 78, "Y Z SPC - 1 2 3");
    u8g2.drawStr(4, 96, "<-  CANCEL");
    u8g2.sendBuffer();
    return;
  }

  bool hasSelectedStepOverlay = (sequencerOverlayMode != SequencerOverlayMode::Hidden &&
                                 sequencerOverlayMode != SequencerOverlayMode::StatusMessage &&
                                 sequencerOverlayMode != SequencerOverlayMode::Overview &&
                                 sequencerSelectedStep >= 0);
  bool hasStatusOverlay = (sequencerOverlayMode == SequencerOverlayMode::StatusMessage);
  bool hasOverviewOverlay = (sequencerOverlayMode == SequencerOverlayMode::Overview);

  if (sequencerOverlayMode == SequencerOverlayMode::Hidden ||
      (!hasSelectedStepOverlay && !hasStatusOverlay && !hasOverviewOverlay)) {
    if (sequencerOverlayVisible) {
      sequencerOverlayVisible = false;
      sequencerOverlayDirty = false;
      menu.drawMenu();
    }
    return;
  }

  if (screenSaverOn) {
    return;
  }

  if ((sequencerOverlayMode == SequencerOverlayMode::NoteAssigned ||
       sequencerOverlayMode == SequencerOverlayMode::LengthEdit ||
       sequencerOverlayMode == SequencerOverlayMode::StatusMessage) &&
      runTime >= sequencerOverlayUntil) {
    if (sequencerOverlayMode == SequencerOverlayMode::NoteAssigned) {
      sequencerSelectedStep = -1;
    } else if (sequencerOverlayMode == SequencerOverlayMode::LengthEdit && sequencerSelectedStep >= 0) {
      sequencerOverlayMode = SequencerOverlayMode::AwaitingNote;
      sequencerOverlayVisible = false;
      sequencerOverlayDirty = true;
      return;
    }
    sequencerOverlayMode = SequencerOverlayMode::Hidden;
    sequencerOverlayVisible = false;
    sequencerOverlayDirty = false;
    menu.drawMenu();
    return;
  }

  if (!sequencerOverlayDirty && sequencerOverlayVisible) {
    return;
  }

  if (sequencerOverlayMode == SequencerOverlayMode::Overview) {
    char lineBuffer[SEQUENCER_OVERVIEW_STEPS_PER_PAGE][24];
    constexpr byte overviewPageCount =
      (SEQUENCER_STEP_COUNT + SEQUENCER_OVERVIEW_STEPS_PER_PAGE - 1) / SEQUENCER_OVERVIEW_STEPS_PER_PAGE;
    if (sequencerOverviewPage >= overviewPageCount) {
      sequencerOverviewPage = 0;
    }
    byte firstStep = static_cast<byte>(sequencerOverviewPage * SEQUENCER_OVERVIEW_STEPS_PER_PAGE);

    sequencerOverlayVisible = true;
    sequencerOverlayDirty = false;

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_tf);

    for (byte row = 0; row < SEQUENCER_OVERVIEW_STEPS_PER_PAGE; row++) {
      byte stepIndex = static_cast<byte>(firstStep + row);
      if (stepIndex >= SEQUENCER_STEP_COUNT) {
        break;
      }
      fillOverviewStepLine(stepIndex, lineBuffer[row], sizeof(lineBuffer[row]));
      int y = 3 + (row * 15);
      u8g2.drawStr(4, y, lineBuffer[row]);
    }

    u8g2.sendBuffer();
    return;
  }

  char headerLabel[20];
  char stepLabel[16];
  char noteLineOne[24];
  char noteLineTwo[24];
  char hintLineOne[24];
  char hintLineTwo[24];
  stepLabel[0] = '\0';
  noteLineOne[0] = '\0';
  noteLineTwo[0] = '\0';
  hintLineOne[0] = '\0';
  hintLineTwo[0] = '\0';

  if (sequencerOverlayMode != SequencerOverlayMode::StatusMessage &&
      sequencerOverlayMode != SequencerOverlayMode::AwaitingNote &&
      sequencerOverlayMode != SequencerOverlayMode::StepCleared) {
    snprintf(stepLabel, sizeof(stepLabel), "Step %02d", sequencerSelectedStep + 1);
    fillOverlayNoteLines(noteLineOne, sizeof(noteLineOne), noteLineTwo, sizeof(noteLineTwo));
  } else if (sequencerOverlayMode == SequencerOverlayMode::AwaitingNote) {
    fillOverlayNoteLines(noteLineOne, sizeof(noteLineOne), noteLineTwo, sizeof(noteLineTwo));
  }

  if (sequencerOverlayMode == SequencerOverlayMode::AwaitingNote) {
    snprintf(headerLabel, sizeof(headerLabel), "Edit #%02d", sequencerSelectedStep + 1);
    snprintf(hintLineOne, sizeof(hintLineOne), "Length %u%%",
             static_cast<unsigned>(sequencerStepGatePercent[sequencerSelectedStep]));
  } else if (sequencerOverlayMode == SequencerOverlayMode::LengthEdit) {
    snprintf(headerLabel, sizeof(headerLabel), "Step Length");
    snprintf(hintLineOne, sizeof(hintLineOne), "Length %u%%", static_cast<unsigned>(sequencerLengthPercentDisplay));
    fillOverlayNoteLines(noteLineOne, sizeof(noteLineOne), noteLineTwo, sizeof(noteLineTwo));
  } else if (sequencerOverlayMode == SequencerOverlayMode::StepCleared) {
    snprintf(headerLabel, sizeof(headerLabel), "Erased #%02d", sequencerSelectedStep + 1);
    snprintf(hintLineOne, sizeof(hintLineOne), "Press blue key");
    snprintf(hintLineTwo, sizeof(hintLineTwo), "to undo");
  } else if (sequencerOverlayMode == SequencerOverlayMode::StatusMessage) {
    snprintf(headerLabel, sizeof(headerLabel), "%s", sequencerStatusLineOne);
    snprintf(hintLineOne, sizeof(hintLineOne), "%s", sequencerStatusLineTwo);
  } else {
    snprintf(headerLabel, sizeof(headerLabel), "Chord Saved");
  }

  sequencerOverlayVisible = true;
  sequencerOverlayDirty = false;

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawStr(20, 18, headerLabel);
  int hintLineOneY = (stepLabel[0] != '\0') ? 54 : 40;
  int hintLineTwoY = (stepLabel[0] != '\0') ? 68 : 54;
  int noteLineOneY = (stepLabel[0] != '\0') ? 96 : 88;
  int noteLineTwoY = (stepLabel[0] != '\0') ? 112 : 104;
  if (stepLabel[0] != '\0') {
    u8g2.drawStr(36, 36, stepLabel);
  }
  if (hintLineOne[0] != '\0') {
    u8g2.drawStr(8, hintLineOneY, hintLineOne);
  }
  if (hintLineTwo[0] != '\0') {
    u8g2.drawStr(4, hintLineTwoY, hintLineTwo);
  }
  u8g2.setFont(u8g2_font_6x13_tf);
  if (noteLineOne[0] != '\0') {
    u8g2.drawStr(12, noteLineOneY, noteLineOne);
  }
  if (noteLineTwo[0] != '\0') {
    u8g2.drawStr(12, noteLineTwoY, noteLineTwo);
  }
  u8g2.sendBuffer();
}

void applySequencerLedOverrides() {
  if (isSequencerNamingActive()) {
    uint32_t activeColor = getSequencerConfirmLedColor();
    uint16_t ledCount = strip.numPixels();
    for (uint16_t buttonIndex = 0; buttonIndex < ledCount; buttonIndex++) {
      strip.setPixelColor(buttonIndex, 0);
    }
    for (const SequencerNamingKey& key : sequencerNamingKeys) {
      if (key.buttonIndex < ledCount) {
        strip.setPixelColor(key.buttonIndex, activeColor);
      }
    }
    return;
  }

  strip.setPixelColor(
    SEQUENCER_TRANSPORT_BUTTON_INDEX,
    getSequencerTransportLedColor(sequencerTransportState == SEQUENCER_TRANSPORT_PLAY));
  strip.setPixelColor(
    SEQUENCER_OVERVIEW_BUTTON_INDEX,
    getSequencerUnsetStepLedColor(sequencerOverlayMode == SequencerOverlayMode::Overview));
  strip.setPixelColor(SEQUENCER_CONFIRM_BUTTON_INDEX, getSequencerConfirmLedColor());

  for (byte step = 0; step < SEQUENCER_STEP_COUNT; step++) {
    int8_t buttonIndex = sequencerStepToButtonIndex(step);
    if (buttonIndex < 0) {
      continue;
    }

    uint32_t colorCode = 0;
    bool selected = (sequencerSelectedStep == step);
    bool playing = (sequencerPlayingStep == step);
    bool selectionLit = !selected || isSequencerSelectionLit();
    byte primaryMidiNote = sequencerPrimaryMidiNote(step);

    if (!selectionLit) {
      strip.setPixelColor(buttonIndex, 0);
      continue;
    }

    if (primaryMidiNote >= 128) {
      strip.setPixelColor(buttonIndex, getSequencerUnsetStepLedColor(selected || playing));
    } else if (getBoardLedColorForMidiNote(primaryMidiNote, playing, colorCode)) {
      strip.setPixelColor(buttonIndex, colorCode);
    }
  }
}

void updateSequencerTransport() {
  if (sequencerConfirmHeld && sequencerSelectedStep >= 0) {
    uint64_t heldMicros = runTime - sequencerConfirmPressedAt;
    if (heldMicros >= SEQUENCER_CLEAR_HOLD_MICROS) {
      clearSelectedSequencerStep();
    }
  }

  serviceSequencerPlaybackGroups();

  if (sequencerTransportState != SEQUENCER_TRANSPORT_PLAY) {
    return;
  }

  if (runTime < sequencerNextStepAt) {
    return;
  }

  uint64_t stepDuration = sequencerStepDurationMicros();
  sequencerCurrentStepStartedAt = sequencerNextStepAt;
  sequencerNextStepAt += stepDuration;
  byte activeStepCount = sequencerActiveStepCount();
  sequencerPlayingStep = nextSequencerStep(activeStepCount);

  byte noteCount = sequencerStepNoteCount[sequencerPlayingStep];
  if (noteCount == 0) {
    return;
  }

  startSequencerPlaybackGroup(static_cast<byte>(sequencerPlayingStep), stepDuration);
}
