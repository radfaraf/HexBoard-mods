#include "SequencerMode.h"
#include "UsbBackup.h"

#include <Adafruit_NeoPixel.h>
#include <LittleFS.h>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <vector>

extern void rebootToBootloader();
extern bool fileSystemExists;
extern GEM_u8g2 menu;
extern GEMPage menuPageSynthSequencer;
extern GEMPage menuPageColorsSequencer;
extern GEMPage menuPageSequencer;
extern GEMPage menuPageSequencerBrowser;
extern GEMPage menuPageSequencerFiles;
extern GEMPage menuPageSequencerPlayback;
extern GEMPage menuPageSequencerMidiSync;
extern GEMPage menuPageSequencerUsbBackup;
extern GEMPage menuPageSequencerUsbBackupExitConfirm;
extern GEMPage menuPageSequencerUsbBackupStopConfirm;
extern GEMPage menuPageSequencerDeleteConfirm;
extern GEMPage menuPageSequencerTargetAction;
extern GEMPage menuPageSequencerLights;
extern U8G2_SH1107_SEEED_128X128_F_HW_I2C u8g2;
extern bool screenSaverOn;
extern uint64_t screenTime;
extern uint64_t runTime;
extern Adafruit_NeoPixel strip;
extern RP2040 rp2040;
extern uint32_t applyNotePixelColor(byte x);
extern void setLEDcolorCodes();
extern bool isKeyboardMode();
extern volatile bool isrProfilingEnabled;
extern volatile uint32_t isrProfileAvgUs;
extern volatile uint32_t isrProfileCount;
extern volatile uint32_t midiMonitorQueueDepth;
extern volatile uint32_t midiMonitorDroppedCount;
extern volatile uint32_t midiMonitorLateCount;
extern void readAndResetISRProfile();
extern void resetMidiMonitorStats();

int sequencerConfirmHue = 250;
byte sequencerConfirmSaturation = 255;
byte sequencerConfirmValue = 211;

namespace {
constexpr byte SEQUENCER_STEP_COUNT = 32;
constexpr byte SEQUENCER_TRANSPORT_BUTTON_INDEX = 9;
constexpr byte SEQUENCER_OVERVIEW_BUTTON_INDEX = 18;
constexpr byte SEQUENCER_CONFIRM_BUTTON_INDEX = 19;
constexpr byte SEQUENCER_FUNCTION_BUTTON_INDEX = 29;
constexpr byte SEQUENCER_FUNCTION_CANCEL_BUTTON_INDEX = 102;
constexpr byte SEQUENCER_MAX_NOTES_PER_STEP = 6;
// Six-note steps plus overlapping playback/tap-preview layers need more held-note headroom.
constexpr byte SEQUENCER_MAX_MANAGED_HELD_NOTES = 128;
constexpr byte SEQUENCER_OVERLAY_CONTRAST = 63;
constexpr int16_t SEQUENCER_NO_PITCH = INT16_MIN;
constexpr byte SEQUENCER_DEFAULT_VELOCITY = 96;
constexpr byte SEQUENCER_DEFAULT_PROBABILITY = 100;
constexpr byte SEQUENCER_VELOCITY_CHOICE_COUNT = 27;
constexpr byte SEQUENCER_PROBABILITY_CHOICE_COUNT = 21;
constexpr uint64_t SEQUENCER_NOTE_CONFIRM_MICROS = 2000000ULL;
constexpr uint64_t SEQUENCER_CLEAR_HOLD_MICROS = 1000000ULL;
constexpr uint64_t SEQUENCER_PERFORMANCE_HOLD_MICROS = 2000000ULL;
constexpr uint64_t SEQUENCER_PERFORMANCE_REFRESH_MICROS = 250000ULL;
constexpr uint64_t SEQUENCER_SELECTED_ON_MICROS = 600000ULL;
constexpr uint64_t SEQUENCER_SELECTED_OFF_MICROS = 200000ULL;
constexpr uint32_t SEQUENCER_AUDIO_ISR_PERIOD_MICROS = 24;
constexpr byte SEQUENCER_TRANSPORT_STOP = 0;
constexpr byte SEQUENCER_TRANSPORT_PLAY = 1;
constexpr byte SEQUENCER_TAP_PREVIEW_OFF = 0;
constexpr byte SEQUENCER_TAP_PREVIEW_ON = 1;
constexpr byte SEQUENCER_PLAY_TYPE_MIDI = 0;
constexpr byte SEQUENCER_PLAY_TYPE_OB_SYNTH = 1;
constexpr byte SEQUENCER_CLOCK_SOURCE_INTERNAL = 0;
constexpr byte SEQUENCER_CLOCK_SOURCE_EXTERNAL_MIDI = 1;
constexpr byte SEQUENCER_SEND_CLOCK_OFF = 0;
constexpr byte SEQUENCER_SEND_CLOCK_ON = 1;
constexpr byte SEQUENCER_SEND_TRANSPORT_OFF = 0;
constexpr byte SEQUENCER_SEND_TRANSPORT_ON = 1;
constexpr byte SEQUENCER_STEP_ACCENT_OFF = 0;
constexpr byte SEQUENCER_STEP_COLOR_NOTE = 0;
constexpr byte SEQUENCER_STEP_COLOR_REGULAR = 1;
constexpr byte SEQUENCER_STEP_LIGHT_OFF = 0;
constexpr byte SEQUENCER_STEP_LIGHT_LOW = 64;
// Keep medium/high above the very low LED region where some hues can look skewed.
constexpr byte SEQUENCER_STEP_LIGHT_MEDIUM = 112;
// High is intentionally about 25% below highest so play-position pop stays visible.
constexpr byte SEQUENCER_STEP_LIGHT_HIGH = 192;
constexpr byte SEQUENCER_STEP_LIGHT_HIGHEST = 255;
constexpr byte SEQUENCER_STEP_HUE_RED = 0;
constexpr byte SEQUENCER_STEP_HUE_ORANGE = 1;
constexpr byte SEQUENCER_STEP_HUE_YELLOW = 2;
constexpr byte SEQUENCER_STEP_HUE_LIME = 3;
constexpr byte SEQUENCER_STEP_HUE_GREEN = 4;
constexpr byte SEQUENCER_STEP_HUE_TEAL = 5;
constexpr byte SEQUENCER_STEP_HUE_CYAN = 6;
constexpr byte SEQUENCER_STEP_HUE_LIGHT_BLUE = 7;
constexpr byte SEQUENCER_STEP_HUE_BLUE = 8;
constexpr byte SEQUENCER_STEP_HUE_INDIGO = 9;
constexpr byte SEQUENCER_STEP_HUE_PURPLE = 10;
constexpr byte SEQUENCER_STEP_HUE_MAGENTA = 11;
constexpr byte SEQUENCER_STEP_HUE_PINK = 12;
constexpr byte SEQUENCER_MIDI_CLOCKS_PER_STEP = 6;
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
constexpr byte SEQUENCER_GATE_CHOICE_COUNT = 17;
constexpr byte SEQUENCER_MAX_ACTIVE_PLAYBACK_GROUPS = 16;
constexpr byte SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT = 8;
constexpr byte SEQUENCER_BROWSER_MAX_ENTRIES = 128;
constexpr size_t SEQUENCER_MAX_PATH_LENGTH = 255;
constexpr size_t SEQUENCER_BROWSER_TITLE_LENGTH = 28;
constexpr size_t SEQUENCER_MENU_TITLE_LENGTH = 48;
constexpr size_t SEQUENCER_NAME_EDIT_MAX_LENGTH = 20;
constexpr int SEQUENCER_OVERVIEW_FIRST_LINE_Y = 12;
constexpr int SEQUENCER_OVERVIEW_MAX_BASELINE_Y = 120;
constexpr int SEQUENCER_OVERVIEW_SECOND_LINE_OFFSET = 12;
constexpr int SEQUENCER_OVERVIEW_SINGLE_STEP_ADVANCE = 14;
constexpr int SEQUENCER_OVERVIEW_DOUBLE_STEP_ADVANCE = 24;

int16_t sequencerStepPitchSteps[SEQUENCER_STEP_COUNT][SEQUENCER_MAX_NOTES_PER_STEP] = {};
byte sequencerStepNoteCount[SEQUENCER_STEP_COUNT] = {};
uint16_t sequencerStepGatePercent[SEQUENCER_STEP_COUNT] = {};
byte sequencerStepVelocity[SEQUENCER_STEP_COUNT] = {};
byte sequencerStepProbability[SEQUENCER_STEP_COUNT] = {};
bool sequencerStepTie[SEQUENCER_STEP_COUNT] = {};

enum class SequencerOverlayMode : uint8_t {
  Hidden = 0,
  AwaitingNote = 1,
  NoteAssigned = 2,
  StepCleared = 3,
  StatusMessage = 4,
  LengthEdit = 5,
  Overview = 6,
  Naming = 7,
  ExactLengthEdit = 8,
  PerformanceMonitor = 9,
  FunctionPicker = 10,
  ExactVelocityEdit = 11,
  ExactProbabilityEdit = 12,
  CopyTargetSelect = 13
};

struct SequencerPlaybackExternalClockState {
  bool pendingGateSync = false;
  bool pendingTieBoundary = false;
  uint16_t gatePercent = 100;
  uint64_t startedAt = 0;
};

struct SequencerPlaybackGroup {
  bool active = false;
  int16_t pitchSteps[SEQUENCER_MAX_NOTES_PER_STEP] = {
    SEQUENCER_NO_PITCH, SEQUENCER_NO_PITCH, SEQUENCER_NO_PITCH,
    SEQUENCER_NO_PITCH, SEQUENCER_NO_PITCH, SEQUENCER_NO_PITCH
  };
  byte noteCount = 0;
  int8_t sourceStep = -1;
  bool transportPlayback = false;
  SequencerPlaybackExternalClockState externalClock = {};
  uint64_t noteOffAt = 0;
};

struct SequencerExternalClockState {
  byte pulseCount = 0;
  uint64_t lastPulseAt = 0;
  uint64_t stepDuration = 0;
  uint64_t pulseMicrosAccum = 0;
  byte pulseIntervalCount = 0;
};

struct SequencerManagedHeldNote {
  bool active = false;
  int16_t pitchSteps = SEQUENCER_NO_PITCH;
  byte auditionCount = 0;
  byte playbackCount = 0;
  uint64_t playbackLedActivatedAt = 0;
  uint32_t playbackLedActivationEpoch = 0;
  SequencerTunedNoteHandle handle = {};
};

enum class SequencerBrowserMode : uint8_t {
  None = 0,
  Load = 1,
  SaveNew = 2,
  CreateFolder = 3,
  RenameOrDelete = 4
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

enum class SequencerToolAction : uint8_t {
  Length = 0,
  Velocity = 1,
  OctaveUp = 2,
  OctaveDown = 3,
  Probability = 4,
  Tie = 5,
  Copy = 6,
  Cancel = 7
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

struct SequencerToolKey {
  byte buttonIndex = 0;
  SequencerToolAction action = SequencerToolAction::Velocity;
};

int8_t sequencerSelectedStep = -1;
SequencerOverlayMode sequencerOverlayMode = SequencerOverlayMode::Hidden;
SequencerOverlayMode sequencerOverlayReturnMode = SequencerOverlayMode::Hidden;
uint64_t sequencerOverlayUntil = 0;
bool sequencerOverlayVisible = false;
bool sequencerOverlayDirty = false;
char sequencerStatusLineOne[24] = "";
char sequencerStatusLineTwo[24] = "";
int16_t sequencerEditPitchSteps[SEQUENCER_MAX_NOTES_PER_STEP] = {
  SEQUENCER_NO_PITCH, SEQUENCER_NO_PITCH, SEQUENCER_NO_PITCH,
  SEQUENCER_NO_PITCH, SEQUENCER_NO_PITCH, SEQUENCER_NO_PITCH
};
byte sequencerEditNoteCount = 0;
int16_t sequencerUndoPitchSteps[SEQUENCER_MAX_NOTES_PER_STEP] = {
  SEQUENCER_NO_PITCH, SEQUENCER_NO_PITCH, SEQUENCER_NO_PITCH,
  SEQUENCER_NO_PITCH, SEQUENCER_NO_PITCH, SEQUENCER_NO_PITCH
};
byte sequencerUndoNoteCount = 0;
uint16_t sequencerUndoGatePercent = 100;
byte sequencerUndoVelocity = SEQUENCER_DEFAULT_VELOCITY;
byte sequencerUndoProbability = SEQUENCER_DEFAULT_PROBABILITY;
bool sequencerUndoTie = false;
SequencerManagedHeldNote sequencerManagedHeldNotes[SEQUENCER_MAX_MANAGED_HELD_NOTES] = {};
uint32_t sequencerTransportPlaybackLedEpoch = 1;
int8_t sequencerPlayingStep = -1;
SequencerPlaybackGroup sequencerPlaybackGroups[SEQUENCER_MAX_ACTIVE_PLAYBACK_GROUPS] = {};
uint64_t sequencerNextStepAt = 0;
uint64_t sequencerCurrentStepStartedAt = 0;
uint64_t sequencerConfirmPressedAt = 0;
bool sequencerConfirmHeld = false;
uint64_t sequencerTransportPressedAt = 0;
bool sequencerTransportHeld = false;
SequencerOverlayMode sequencerOverlayBeforePerformance = SequencerOverlayMode::Hidden;
byte sequencerStepPlayCount = SEQUENCER_STEP_COUNT;
byte sequencerTapPreview = SEQUENCER_TAP_PREVIEW_ON;
byte sequencerPlayType = SEQUENCER_PLAY_TYPE_MIDI;
byte sequencerClockSource = SEQUENCER_CLOCK_SOURCE_INTERNAL;
byte sequencerSendClock = SEQUENCER_SEND_CLOCK_OFF;
byte sequencerSendTransport = SEQUENCER_SEND_TRANSPORT_OFF;
byte sequencerStepAccentEvery = 4;
byte sequencerStepColorMode = SEQUENCER_STEP_COLOR_REGULAR;
byte sequencerStepHue = SEQUENCER_STEP_HUE_INDIGO;
byte sequencerDirection = SEQUENCER_DIRECTION_FORWARD;
int8_t sequencerPingPongDelta = 1;
byte sequencerTempo = 120;
byte sequencerTransportState = 0;
bool sequencerDirty = false;
bool sequencerStorageInitialized = false;
int8_t sequencerCopySourceStep = -1;
uint16_t sequencerLengthPercentDisplay = 100;
uint16_t sequencerExactLengthOriginal = 100;
char sequencerExactLengthBuffer[5] = "";
byte sequencerExactLengthLength = 0;
byte sequencerVelocityDisplay = SEQUENCER_DEFAULT_VELOCITY;
byte sequencerExactVelocityOriginal = SEQUENCER_DEFAULT_VELOCITY;
byte sequencerProbabilityDisplay = SEQUENCER_DEFAULT_PROBABILITY;
byte sequencerExactProbabilityOriginal = SEQUENCER_DEFAULT_PROBABILITY;
byte sequencerOverviewStartStep = 0;
char sequencerCurrentSequencePath[SEQUENCER_MAX_PATH_LENGTH] = "";
char sequencerMenuTitle[SEQUENCER_MENU_TITLE_LENGTH] = "Sequencer";
char sequencerBrowserPath[SEQUENCER_MAX_PATH_LENGTH] = "";
SequencerBrowserMode sequencerBrowserMode = SequencerBrowserMode::None;
SequencerBrowserEntry sequencerBrowserEntries[SEQUENCER_BROWSER_MAX_ENTRIES] = {};
byte sequencerBrowserEntryCount = 0;
byte sequencerBrowserOffset = 0;
char sequencerBrowserPageTitle[20] = "Sequences";
char sequencerBrowserEntryTitles[SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT][SEQUENCER_BROWSER_TITLE_LENGTH] = {};
char sequencerDeletePromptLineOne[SEQUENCER_BROWSER_TITLE_LENGTH] = "";
char sequencerDeletePromptLineTwo[SEQUENCER_BROWSER_TITLE_LENGTH] = "";
char sequencerDeletePromptLineThree[SEQUENCER_BROWSER_TITLE_LENGTH] = "";
char sequencerDeletePromptLineFour[SEQUENCER_BROWSER_TITLE_LENGTH] = "";
char sequencerDeleteTargetPath[SEQUENCER_MAX_PATH_LENGTH] = "";
bool sequencerDeleteTargetIsFolder = false;
char sequencerTargetActionPromptLineOne[SEQUENCER_BROWSER_TITLE_LENGTH] = "";
char sequencerTargetActionPromptLineTwo[SEQUENCER_BROWSER_TITLE_LENGTH] = "";
char sequencerTargetActionPromptLineThree[SEQUENCER_BROWSER_TITLE_LENGTH] = "";
char sequencerTargetActionPromptLineFour[SEQUENCER_BROWSER_TITLE_LENGTH] = "";
char sequencerTargetActionPath[SEQUENCER_MAX_PATH_LENGTH] = "";
bool sequencerTargetActionIsFolder = false;
SequencerNamingTarget sequencerNamingTarget = SequencerNamingTarget::None;
char sequencerNamingBuffer[SEQUENCER_NAME_EDIT_MAX_LENGTH + 1] = "";
byte sequencerNamingLength = 0;
char sequencerRenameSourcePath[SEQUENCER_MAX_PATH_LENGTH] = "";
char sequencerUsbBackupStatusLineOne[SEQUENCER_BROWSER_TITLE_LENGTH] = "USB Backup Off";
char sequencerUsbBackupStatusLineTwo[SEQUENCER_BROWSER_TITLE_LENGTH] = "Host tool idle";
GEMPage* sequencerLastMenuPage = nullptr;
uint64_t sequencerPerformanceLastSampleAt = 0;
uint16_t sequencerPerformanceCpuPercent = 0;
uint32_t sequencerPerformanceCpuAvgUs = 0;
uint32_t sequencerPerformanceHeapUsedBytes = 0;
uint32_t sequencerPerformanceHeapTotalBytes = 0;
uint64_t sequencerPerformanceStorageUsedBytes = 0;
uint64_t sequencerPerformanceStorageTotalBytes = 0;
bool sequencerPerformanceStorageValid = false;
// External-clock playback has two phases: a first-step estimate before a full
// boundary is known, then steady-state timing on real MIDI clock boundaries.
SequencerExternalClockState sequencerExternalClock = {};
uint64_t sequencerNextMidiClockAt = 0;

void showSequencerStatusMessage(const char* lineOne, const char* lineTwo);
void showSequencerStatusMessageAndReturn(const char* lineOne, const char* lineTwo, SequencerOverlayMode returnMode);
void showSequencerPersistentStatusMessage(const char* lineOne, const char* lineTwo);
bool rememberSequencerCurrentPath();
void extractSequencerDisplayName(const char* path, char* out, size_t outSize);
void refreshSequencerMenuTitle();
void resetSequencerClockSyncState();
void openSequencerLightsMenu();
void refreshSequencerLightsMenu(bool redrawMenu = true);
bool sequencerUsesExternalClock();
bool sequencerShouldSendMidiClock();
bool sequencerShouldSendMidiTransport();
uint64_t sequencerCurrentStepDurationMicros();
void advanceSequencerPlaybackStep(uint64_t stepDuration, bool applyProbability = true);
void serviceSequencerInternalMidiClock();
void enterSequencerExactLengthEdit();
void exitSequencerExactLengthEdit(bool saveChanges);
void enterSequencerExactVelocityEdit();
void exitSequencerExactVelocityEdit(bool saveChanges);
void enterSequencerExactProbabilityEdit();
void exitSequencerExactProbabilityEdit(bool saveChanges);
void enterSequencerFunctionPicker();
void exitSequencerFunctionPicker();
void enterSequencerCopyTargetSelect();
void exitSequencerCopyTargetSelect(bool returnToTools = true);
void showSequencerPerformanceMonitor();
void hideSequencerPerformanceMonitor();
void refreshSequencerPerformanceStats(bool forceRefresh);
void refreshSequencerBrowserMenu(bool resetSelection = true);
void sequencerBrowserNewFolderCallback();
bool isSequencerNamingActive();
byte sequencerVelocityChoiceValue(byte choiceIndex);
byte sequencerVelocityChoiceIndex(byte velocity);
byte sequencerProbabilityChoiceValue(byte choiceIndex);
byte sequencerProbabilityChoiceIndex(byte probability);
void snapshotUndoBufferFromStep(byte stepIndex);
void loadEditBufferFromStep(byte stepIndex);
void openSequencerCreateFolderBrowser();
void sequencerBrowserIndicatorCallback();
bool isSequencerAccentStep(byte stepIndex);
int8_t sequencerStepToButtonIndex(byte stepIndex);
int16_t sequencerPrimaryPitchSteps(byte stepIndex);
bool isSequencerProgrammedStep(byte stepIndex);
bool isSequencerSelectionLit();
byte sequencerActiveStepCount();
float sequencerStepHueValue(byte hueSetting);
uint32_t getSequencerAccentedUnsetStepLedColor(bool selected);
byte getSequencerProgrammedStepLightLevel(bool selected, bool playing, bool accented);
uint32_t scaleSequencerLinearColor(uint32_t color, byte level);
uint32_t getSequencerRegularFilledStepLedColor(bool selected, bool playing, bool accented);
uint32_t getSequencerNoteStepLedColor(int16_t pitchSteps, bool selected, bool playing, bool accented);
extern GEMItem menuItemSequencerStepHue;
extern GEMItem menuItemSequencerUsbBackupStatusOne;
extern GEMItem menuItemSequencerUsbBackupStatusTwo;
extern GEMItem menuItemSequencerUsbBackupStart;
extern GEMItem menuItemSequencerUsbBackupStop;
extern GEMItem menuItemSequencerUsbBackupExitPromptOne;
extern GEMItem menuItemSequencerUsbBackupExitPromptTwo;
extern GEMItem menuItemSequencerUsbBackupExitPromptThree;
extern GEMItem menuItemSequencerUsbBackupExitPromptFour;
extern GEMItem menuItemSequencerUsbBackupExitYes;
extern GEMItem menuItemSequencerUsbBackupExitNo;
extern GEMItem menuItemSequencerUsbBackupStopPromptOne;
extern GEMItem menuItemSequencerUsbBackupStopPromptTwo;
extern GEMItem menuItemSequencerUsbBackupStopPromptThree;
extern GEMItem menuItemSequencerUsbBackupStopPromptFour;
extern GEMItem menuItemSequencerUsbBackupStopYes;
extern GEMItem menuItemSequencerUsbBackupStopNo;
extern GEMItem menuItemSequencerDeletePromptOne;
extern GEMItem menuItemSequencerDeletePromptTwo;
extern GEMItem menuItemSequencerDeletePromptThree;
extern GEMItem menuItemSequencerDeletePromptFour;
extern GEMItem menuItemSequencerDeleteYes;
extern GEMItem menuItemSequencerDeleteCancel;
extern GEMItem menuItemSequencerTargetActionPromptOne;
extern GEMItem menuItemSequencerTargetActionPromptTwo;
extern GEMItem menuItemSequencerTargetActionPromptThree;
extern GEMItem menuItemSequencerTargetActionPromptFour;
extern GEMItem menuItemSequencerTargetActionRename;
extern GEMItem menuItemSequencerTargetActionDelete;
extern GEMItem menuItemSequencerTargetActionCancel;
void setSequencerTransportState(byte newState, bool redrawMenu = true, bool sendMidi = true);
void refreshSequencerUsbBackupMenu(bool redrawMenu = true);
void clearSequencerTargetActionState();
void openSequencerTargetActionMenu(const char* targetPath, bool isFolder);
void refreshSequencerTargetActionMenu();
void sequencerTargetActionPromptMenuCallback();
void sequencerTargetActionRenameCallback();
void sequencerTargetActionDeleteCallback();
void sequencerTargetActionCancelCallback();
void openSequencerDeleteConfirm(const char* targetPath, bool isFolder);
void refreshSequencerDeleteConfirmMenu();
void sequencerDeletePromptMenuCallback();
void confirmSequencerDeleteMenuCallback();
void cancelSequencerDeleteMenuCallback();
void usbBackupStatusMenuCallback();
void startUsbBackupMenuCallback();
void stopUsbBackupMenuCallback();
void usbBackupExitPromptMenuCallback();
void confirmUsbBackupExitMenuCallback();
void cancelUsbBackupExitMenuCallback();
void usbBackupStopPromptMenuCallback();
void confirmUsbBackupStopMenuCallback();
void cancelUsbBackupStopMenuCallback();
bool guardSequencerStorageForUsbBackup(const char* actionLineTwo);
void openSequencerSaveNewBrowser();
void openSequencerRenameOrDeleteBrowser();

// Accent grouping always starts at step 1, so indices 0, N, 2N... are emphasized.
bool isSequencerAccentStep(byte stepIndex) {
  if (sequencerStepAccentEvery < 2 || stepIndex >= SEQUENCER_STEP_COUNT) {
    return false;
  }
  return (stepIndex % sequencerStepAccentEvery) == 0;
}

float sequencerStepHueValue(byte hueSetting) {
  return getBoardNamedHue(hueSetting);
}

uint32_t getSequencerAccentedUnsetStepLedColor(bool selected) {
  // Accented empty steps get a visible medium level; selected empty steps still jump to highest.
  const byte stepValue = selected ? SEQUENCER_STEP_LIGHT_HIGHEST : SEQUENCER_STEP_LIGHT_MEDIUM;
  return buildBoardLedColor(0.0f, 0, applyBoardRestLedLevel(stepValue));
}

uint32_t getSequencerMediumBlueActionLedColor() {
  return buildBoardLedColor(
    static_cast<float>(sequencerConfirmHue),
    sequencerConfirmSaturation,
    applyBoardRestLedLevel(SEQUENCER_STEP_LIGHT_MEDIUM));
}

byte getSequencerProgrammedStepLightLevel(bool selected, bool playing, bool accented) {
  if (playing) {
    return SEQUENCER_STEP_LIGHT_HIGHEST;
  }
  if (selected || accented) {
    return SEQUENCER_STEP_LIGHT_HIGH;
  }
  return SEQUENCER_STEP_LIGHT_MEDIUM;
}

uint32_t scaleSequencerLinearColor(uint32_t color, byte level) {
  if (level == SEQUENCER_STEP_LIGHT_OFF) {
    return 0;
  }
  if (level >= SEQUENCER_STEP_LIGHT_HIGHEST) {
    return gammaBoardLedColor(color);
  }

  uint32_t scaled = 0;
  for (byte shift = 0; shift <= 24; shift += 8) {
    uint32_t channel = (color >> shift) & 0xFF;
    channel = (channel * level + 127) / 255;
    scaled |= (channel << shift);
  }
  return gammaBoardLedColor(scaled);
}

// Regular step colors are sequencer-only UI colors and ignore the stored note hue.
uint32_t getSequencerRegularFilledStepLedColor(bool selected, bool playing, bool accented) {
  float hue = sequencerStepHueValue(sequencerStepHue);
  const byte level = getSequencerProgrammedStepLightLevel(selected, playing, accented);
  uint32_t referenceColor = buildBoardLinearLedColor(hue, 255, applyBoardRestLedLevel(SEQUENCER_STEP_LIGHT_HIGHEST));
  return scaleSequencerLinearColor(referenceColor, level);
}

uint32_t getSequencerNoteStepLedColor(int16_t pitchSteps, bool selected, bool playing, bool accented) {
  float hue = 0.0f;
  byte sat = 0;
  if (!getBoardBaseLedColorForPitchSteps(pitchSteps, hue, sat)) {
    return 0;
  }

  const byte level = getSequencerProgrammedStepLightLevel(selected, playing, accented);
  uint32_t referenceColor = buildBoardLinearLedColor(hue, sat, applyBoardRestLedLevel(SEQUENCER_STEP_LIGHT_HIGHEST));
  return scaleSequencerLinearColor(referenceColor, level);
}

bool isSequencerTransportPlaybackPitchActive(int16_t boardPitchSteps) {
  if (sequencerTransportState != SEQUENCER_TRANSPORT_PLAY) {
    return false;
  }

  for (byte heldIndex = 0; heldIndex < SEQUENCER_MAX_MANAGED_HELD_NOTES; heldIndex++) {
    const SequencerManagedHeldNote& heldNote = sequencerManagedHeldNotes[heldIndex];
    if (!heldNote.active || heldNote.playbackCount == 0) {
      continue;
    }
    if (heldNote.pitchSteps == boardPitchSteps) {
      return true;
    }
  }
  return false;
}

const SequencerManagedHeldNote* findSequencerTransportPlaybackLedNoteInternal(byte buttonIndex) {
  if (isKeyboardMode()) {
    return nullptr;
  }

  int16_t pitchSteps = SEQUENCER_NO_PITCH;
  if (!getButtonPitchStepsForSequencer(buttonIndex, pitchSteps)) {
    return nullptr;
  }

  for (byte heldIndex = 0; heldIndex < SEQUENCER_MAX_MANAGED_HELD_NOTES; heldIndex++) {
    const SequencerManagedHeldNote& heldNote = sequencerManagedHeldNotes[heldIndex];
    if (!heldNote.active || heldNote.playbackCount == 0 || heldNote.pitchSteps != pitchSteps) {
      continue;
    }
    return &heldNote;
  }
  return nullptr;
}

bool isSequencerTransportPlaybackLedActiveInternal(byte buttonIndex) {
  return findSequencerTransportPlaybackLedNoteInternal(buttonIndex) != nullptr;
}

bool didSequencerTransportPlaybackLedJustStartInternal(byte buttonIndex) {
  const SequencerManagedHeldNote* heldNote = findSequencerTransportPlaybackLedNoteInternal(buttonIndex);
  return heldNote != nullptr && heldNote->playbackLedActivationEpoch == sequencerTransportPlaybackLedEpoch;
}

uint64_t getSequencerTransportPlaybackLedTimePressedInternal(byte buttonIndex) {
  const SequencerManagedHeldNote* heldNote = findSequencerTransportPlaybackLedNoteInternal(buttonIndex);
  if (heldNote == nullptr) {
    return 0;
  }
  return heldNote->playbackLedActivatedAt;
}

void applySequencerTransportPlaybackNoteLedState() {
  uint16_t ledCount = strip.numPixels();
  for (uint16_t buttonIndex = 0; buttonIndex < ledCount; buttonIndex++) {
    if (!isSequencerTransportPlaybackLedActiveInternal(static_cast<byte>(buttonIndex))) {
      continue;
    }
    strip.setPixelColor(buttonIndex, applyNotePixelColor(static_cast<byte>(buttonIndex)));
  }
}

const uint16_t sequencerGateChoices[SEQUENCER_GATE_CHOICE_COUNT] = {
  0, 25, 50, 75, 100, 150, 200, 250, 300, 350, 400, 500, 600, 700, 800, 900, 1000
};

byte sequencerGateChoiceIndex(uint16_t gatePercent);
void sortSequencerBrowserEntriesRange(byte startIndex, byte endExclusive);
void startSequencerNaming(SequencerNamingTarget target, const char* initialText);
void clearSequencerNoteBuffer(int16_t* notes, byte& count);
void sortSequencerNoteBuffer(int16_t* notes, byte count);
byte findNoteInBuffer(const int16_t* notes, byte count, int16_t pitchSteps);
void saveEditBufferToStep(byte stepIndex);
void copySequencerStepData(byte sourceStep, byte destinationStep);
void restoreSelectedSequencerStepFromUndo();
void previewSequencerStep(byte stepIndex);
void startSequencerPlaybackGroup(
  byte stepIndex,
  uint64_t stepDuration,
  bool applyProbability = true,
  bool transportPlayback = false);
byte sequencerSelectedStepVelocity();
void sendSequencerManagedNoteOff(int16_t pitchSteps, bool playbackNote);
int getVisibleBrowserEntryMenuIndex(bool firstVisible);
const SequencerToolKey* getSequencerToolKey(byte buttonIndex);
int sequencerDisplayedOctaveForPitchSteps(int16_t pitchSteps);
bool canTransposeSelectedSequencerStep(int16_t pitchStepDelta);
bool transposeSelectedSequencerStep(int16_t pitchStepDelta);
void handleSequencerToolAction(SequencerToolAction action);
bool sequencerStepHasPlayableNoteData(byte stepIndex);
int8_t findSequencerTieSourceStep(byte stepIndex, byte activeStepCount);
int findActiveSequencerTransportPlaybackGroup(byte sourceStep);
void continueSequencerTiePlayback(byte stepIndex, uint64_t stepDuration);
void clearSequencerPlaybackGroup(SequencerPlaybackGroup& group, bool stopNotes = true);
bool doesNextSequencerStepContinueSource(byte stepIndex, byte activeStepCount);
bool doesSequencerStepContinuePlaybackGroup(int8_t stepIndex, byte activeStepCount, const SequencerPlaybackGroup& group);
void releaseSequencerPlaybackGroupsForStepBoundary(int8_t nextStepIndex, byte activeStepCount);
void recordSequencerExternalClockPulse();
bool advanceSequencerExternalClockState(uint64_t& stepDuration);
void updatePendingExternalGateSyncEstimate();
void resolvePendingExternalGateSync(uint64_t stepDuration);
void resolvePendingExternalTieBoundaries(byte activeStepCount);
int8_t getSequencerOverlayNoteSourceStep();
bool shouldDisplaySequencerTie(int8_t stepIndex);
bool appendSequencerNoteLabel(char* line, size_t lineSize, const char* label);
byte countSequencerOverviewStepsThatFit(byte firstStep);
bool appendOverviewLabelToLine(char* line, size_t lineSize, const char* label);
bool appendOverlayLabelToLine(char* line, size_t lineSize, const char* label);

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
  { 37, SequencerNamingAction::InsertChar, '4' },
  { 38, SequencerNamingAction::InsertChar, '5' },
  { 41, SequencerNamingAction::Backspace, '\0' },
  { 42, SequencerNamingAction::InsertChar, '6' },
  { 43, SequencerNamingAction::InsertChar, '7' },
  { 44, SequencerNamingAction::InsertChar, '8' },
  { 45, SequencerNamingAction::InsertChar, '9' },
  { 46, SequencerNamingAction::InsertChar, '0' },
  { 82, SequencerNamingAction::Cancel, '\0' }
};

const SequencerNamingKey sequencerExactLengthKeys[] = {
  { 50, SequencerNamingAction::InsertChar, '0' },
  { 51, SequencerNamingAction::InsertChar, '1' },
  { 52, SequencerNamingAction::InsertChar, '2' },
  { 53, SequencerNamingAction::InsertChar, '3' },
  { 54, SequencerNamingAction::InsertChar, '4' },
  { 61, SequencerNamingAction::InsertChar, '5' },
  { 62, SequencerNamingAction::InsertChar, '6' },
  { 63, SequencerNamingAction::InsertChar, '7' },
  { 64, SequencerNamingAction::InsertChar, '8' },
  { 65, SequencerNamingAction::InsertChar, '9' },
  { 70, SequencerNamingAction::Backspace, '\0' },
  { SEQUENCER_FUNCTION_CANCEL_BUTTON_INDEX, SequencerNamingAction::Cancel, '\0' }
};

const SequencerToolKey sequencerToolKeys[] = {
  { 50, SequencerToolAction::Length },
  { 51, SequencerToolAction::Velocity },
  { 52, SequencerToolAction::OctaveUp },
  { 53, SequencerToolAction::OctaveDown },
  { 61, SequencerToolAction::Probability },
  { 62, SequencerToolAction::Tie },
  { 63, SequencerToolAction::Copy },
  { SEQUENCER_FUNCTION_CANCEL_BUTTON_INDEX, SequencerToolAction::Cancel }
};

void copySequencerString(char* destination, size_t destinationSize, const char* source) {
  if (destinationSize == 0) {
    return;
  }
  snprintf(destination, destinationSize, "%s", (source != nullptr) ? source : "");
}

void refreshSequencerMenuTitle() {
  if (sequencerCurrentSequencePath[0] == '\0') {
    copySequencerString(sequencerMenuTitle, sizeof(sequencerMenuTitle), "Sequencer");
  } else {
    char displayName[SEQUENCER_BROWSER_TITLE_LENGTH];
    extractSequencerDisplayName(sequencerCurrentSequencePath, displayName, sizeof(displayName));
    if (displayName[0] == '\0') {
      copySequencerString(displayName, sizeof(displayName), "Sequence");
    }
    snprintf(sequencerMenuTitle, sizeof(sequencerMenuTitle), "Seq-%s%s",
             sequencerDirty ? "*" : "", displayName);
  }
  menuPageSequencer.setTitle(sequencerMenuTitle);
}

void applySequencerStepLedState() {
  byte activeStepCount = sequencerActiveStepCount();
  for (byte step = 0; step < SEQUENCER_STEP_COUNT; step++) {
    int8_t buttonIndex = sequencerStepToButtonIndex(step);
    if (buttonIndex < 0) {
      continue;
    }

    if (step >= activeStepCount) {
      strip.setPixelColor(buttonIndex, 0);
      continue;
    }

    bool selected = (sequencerSelectedStep == step);
    bool playing = (sequencerPlayingStep == step);
    bool accented = isSequencerAccentStep(step);
    bool selectionLit = !selected || isSequencerSelectionLit();
    bool programmedStep = isSequencerProgrammedStep(step);
    int16_t primaryPitchSteps = sequencerPrimaryPitchSteps(step);

    if (!selectionLit) {
      strip.setPixelColor(buttonIndex, 0);
      continue;
    }

    // Sequencer step LEDs split into three visual paths:
    // 1) empty/unset steps, 2) regular step colors, 3) note-colored steps.
    // Bugs in one path may not appear in the others, so debug the matching
    // branch instead of assuming all "white-looking" steps are note colors.
    if (!programmedStep) {
      if (accented) {
        strip.setPixelColor(buttonIndex, getSequencerAccentedUnsetStepLedColor(selected || playing));
      } else {
        const bool highlighted = selected || playing;
        // Sequencer mapping requires empty non-accented steps to stay fully off
        // unless they are selected/playing.
        strip.setPixelColor(buttonIndex, highlighted ? getSequencerUnsetStepLedColor(true) : 0);
      }
      continue;
    }

    if (sequencerStepColorMode == SEQUENCER_STEP_COLOR_REGULAR) {
      strip.setPixelColor(buttonIndex, getSequencerRegularFilledStepLedColor(selected, playing, accented));
      continue;
    }

    if (primaryPitchSteps == SEQUENCER_NO_PITCH) {
      // Tied steps without a usable pitch source still stay visibly programmed.
      strip.setPixelColor(buttonIndex, getSequencerRegularFilledStepLedColor(selected, playing, accented));
      continue;
    }

    strip.setPixelColor(buttonIndex, getSequencerNoteStepLedColor(primaryPitchSteps, selected, playing, accented));
  }
}

void selectSequencerStepForEditing(byte stepIndex, SequencerOverlayMode nextOverlayMode) {
  sequencerSelectedStep = static_cast<int8_t>(stepIndex);
  snapshotUndoBufferFromStep(stepIndex);
  loadEditBufferFromStep(stepIndex);
  sequencerLengthPercentDisplay = sequencerStepGatePercent[stepIndex];
  sequencerVelocityDisplay = sequencerStepVelocity[stepIndex];
  sequencerProbabilityDisplay = sequencerStepProbability[stepIndex];
  sequencerExactLengthOriginal = sequencerLengthPercentDisplay;
  sequencerExactVelocityOriginal = sequencerVelocityDisplay;
  sequencerExactProbabilityOriginal = sequencerProbabilityDisplay;
  sequencerOverlayMode = nextOverlayMode;
  sequencerOverlayUntil = 0;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = true;
}

void setSequencerDirtyState(bool dirty) {
  sequencerDirty = dirty;
  refreshSequencerMenuTitle();
}

void clearSequencerCurrentPath() {
  sequencerCurrentSequencePath[0] = '\0';
  rememberSequencerCurrentPath();
  refreshSequencerMenuTitle();
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
  refreshSequencerMenuTitle();
}

bool loadRememberedSequencerCurrentPath() {
  sequencerCurrentSequencePath[0] = '\0';
  refreshSequencerMenuTitle();
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
  refreshSequencerMenuTitle();
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
      sequencerBrowserMode == SequencerBrowserMode::RenameOrDelete) {
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
    clearSequencerCurrentPath();
  }
}

bool deleteSequencerFolderRecursive(const char* folderPath) {
  if (!fileSystemExists || folderPath == nullptr || folderPath[0] == '\0' || sequencerPathIsRoot(folderPath)) {
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

      char childPath[SEQUENCER_MAX_PATH_LENGTH];
      joinSequencerPath(folderPath, entryName.c_str(), childPath, sizeof(childPath));
      childPaths.push_back(String(childPath));
    }
  }

  for (const String& childPath : childPaths) {
    File child = LittleFS.open(childPath.c_str(), "r");
    if (!child) {
      continue;
    }
    bool childIsDirectory = child.isDirectory();
    child.close();

    if (childIsDirectory) {
      if (!deleteSequencerFolderRecursive(childPath.c_str())) {
        return false;
      }
    } else {
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

void enterSequencerExactLengthEdit() {
  if (sequencerSelectedStep < 0) {
    return;
  }
  sequencerExactLengthOriginal = sequencerStepGatePercent[sequencerSelectedStep];
  sequencerExactLengthBuffer[0] = '\0';
  sequencerExactLengthLength = 0;
  sequencerLengthPercentDisplay = sequencerExactLengthOriginal;
  sequencerOverlayMode = SequencerOverlayMode::ExactLengthEdit;
  sequencerOverlayUntil = 0;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = true;
}

void exitSequencerExactLengthEdit(bool saveChanges) {
  if (sequencerSelectedStep >= 0) {
    uint16_t finalValue = sequencerExactLengthOriginal;
    if (saveChanges && sequencerExactLengthLength > 0) {
      finalValue = static_cast<uint16_t>(atoi(sequencerExactLengthBuffer));
    }
    if (finalValue > 1000) {
      finalValue = 1000;
    }
    if (sequencerStepGatePercent[sequencerSelectedStep] != finalValue) {
      sequencerStepGatePercent[sequencerSelectedStep] = finalValue;
      setSequencerDirtyState(true);
    }
    sequencerLengthPercentDisplay = finalValue;
  }
  sequencerOverlayMode = SequencerOverlayMode::FunctionPicker;
  sequencerOverlayUntil = 0;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = true;
}

void enterSequencerExactVelocityEdit() {
  if (sequencerSelectedStep < 0) {
    return;
  }
  sequencerExactVelocityOriginal = sequencerStepVelocity[sequencerSelectedStep];
  sequencerVelocityDisplay = sequencerExactVelocityOriginal;
  sequencerOverlayMode = SequencerOverlayMode::ExactVelocityEdit;
  sequencerOverlayUntil = 0;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = true;
}

void exitSequencerExactVelocityEdit(bool saveChanges) {
  if (sequencerSelectedStep >= 0) {
    byte finalValue = sequencerExactVelocityOriginal;
    if (saveChanges) {
      finalValue = sequencerVelocityDisplay;
    }
    if (sequencerStepVelocity[sequencerSelectedStep] != finalValue) {
      sequencerStepVelocity[sequencerSelectedStep] = finalValue;
      setSequencerDirtyState(true);
    }
    sequencerVelocityDisplay = finalValue;
  }
  sequencerOverlayMode = SequencerOverlayMode::FunctionPicker;
  sequencerOverlayUntil = 0;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = true;
}

void enterSequencerExactProbabilityEdit() {
  if (sequencerSelectedStep < 0) {
    return;
  }
  sequencerExactProbabilityOriginal = sequencerStepProbability[sequencerSelectedStep];
  sequencerProbabilityDisplay = sequencerExactProbabilityOriginal;
  sequencerOverlayMode = SequencerOverlayMode::ExactProbabilityEdit;
  sequencerOverlayUntil = 0;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = true;
}

void exitSequencerExactProbabilityEdit(bool saveChanges) {
  if (sequencerSelectedStep >= 0) {
    byte finalValue = sequencerExactProbabilityOriginal;
    if (saveChanges) {
      finalValue = sequencerProbabilityDisplay;
    }
    if (sequencerStepProbability[sequencerSelectedStep] != finalValue) {
      sequencerStepProbability[sequencerSelectedStep] = finalValue;
      setSequencerDirtyState(true);
    }
    sequencerProbabilityDisplay = finalValue;
  }
  sequencerOverlayMode = SequencerOverlayMode::FunctionPicker;
  sequencerOverlayUntil = 0;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = true;
}

void enterSequencerCopyTargetSelect() {
  if (sequencerSelectedStep < 0) {
    return;
  }
  sequencerCopySourceStep = sequencerSelectedStep;
  sequencerOverlayMode = SequencerOverlayMode::CopyTargetSelect;
  sequencerOverlayUntil = 0;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = true;
}

void exitSequencerCopyTargetSelect(bool returnToTools) {
  sequencerCopySourceStep = -1;
  sequencerOverlayMode = returnToTools ? SequencerOverlayMode::FunctionPicker : SequencerOverlayMode::Hidden;
  sequencerOverlayUntil = 0;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = true;
}

void enterSequencerFunctionPicker() {
  if (sequencerSelectedStep < 0) {
    return;
  }
  sequencerOverlayMode = SequencerOverlayMode::FunctionPicker;
  sequencerOverlayUntil = 0;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = true;
}

void exitSequencerFunctionPicker() {
  sequencerCopySourceStep = -1;
  if (sequencerSelectedStep >= 0) {
    sequencerOverlayMode = SequencerOverlayMode::AwaitingNote;
  } else {
    sequencerOverlayMode = SequencerOverlayMode::Hidden;
  }
  sequencerOverlayUntil = 0;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = true;
}

const SequencerNamingKey* getSequencerExactLengthKey(byte buttonIndex) {
  for (const SequencerNamingKey& key : sequencerExactLengthKeys) {
    if (key.buttonIndex == buttonIndex) {
      return &key;
    }
  }
  return nullptr;
}

void insertSequencerExactLengthChar(char character) {
  if (character < '0' || character > '9') {
    return;
  }
  if (sequencerExactLengthLength >= 4) {
    return;
  }
  sequencerExactLengthBuffer[sequencerExactLengthLength++] = character;
  sequencerExactLengthBuffer[sequencerExactLengthLength] = '\0';
  uint16_t value = static_cast<uint16_t>(atoi(sequencerExactLengthBuffer));
  if (value > 1000) {
    snprintf(sequencerExactLengthBuffer, sizeof(sequencerExactLengthBuffer), "1000");
    sequencerExactLengthLength = 4;
    value = 1000;
  }
  sequencerLengthPercentDisplay = value;
  sequencerOverlayDirty = true;
}

void backspaceSequencerExactLengthChar() {
  if (sequencerExactLengthLength == 0) {
    return;
  }

  sequencerExactLengthLength--;
  sequencerExactLengthBuffer[sequencerExactLengthLength] = '\0';
  if (sequencerExactLengthLength > 0) {
    sequencerLengthPercentDisplay = static_cast<uint16_t>(atoi(sequencerExactLengthBuffer));
  } else {
    sequencerLengthPercentDisplay = sequencerExactLengthOriginal;
  }
  sequencerOverlayDirty = true;
}

const SequencerToolKey* getSequencerToolKey(byte buttonIndex) {
  for (const SequencerToolKey& key : sequencerToolKeys) {
    if (key.buttonIndex == buttonIndex) {
      return &key;
    }
  }
  return nullptr;
}

int sequencerDisplayedOctaveForPitchSteps(int16_t pitchSteps) {
  const int displayedPitch = static_cast<int>(pitchSteps) + getSequencerCurrentTranspose();
  int cycleLength = static_cast<int>(getSequencerTuningCycleLength());
  if (cycleLength <= 0) {
    cycleLength = 12;
  }
  int stepInCycle = displayedPitch % cycleLength;
  if (stepInCycle < 0) {
    stepInCycle += cycleLength;
  }
  return ((displayedPitch - stepInCycle) / cycleLength) + 4;
}

bool canTransposeSelectedSequencerStep(int16_t pitchStepDelta) {
  if (sequencerSelectedStep < 0 || sequencerEditNoteCount == 0) {
    return false;
  }

  for (byte i = 0; i < sequencerEditNoteCount && i < SEQUENCER_MAX_NOTES_PER_STEP; i++) {
    const int32_t transposed = static_cast<int32_t>(sequencerEditPitchSteps[i]) + pitchStepDelta;
    if (transposed < -32768 || transposed > 32767) {
      return false;
    }

    const int octave = sequencerDisplayedOctaveForPitchSteps(static_cast<int16_t>(transposed));
    if (octave < 0 || octave > 9) {
      return false;
    }
  }

  return true;
}

bool transposeSelectedSequencerStep(int16_t pitchStepDelta) {
  if (sequencerSelectedStep < 0 || sequencerEditNoteCount == 0) {
    return false;
  }
  if (!canTransposeSelectedSequencerStep(pitchStepDelta)) {
    return false;
  }

  int16_t transposedNotes[SEQUENCER_MAX_NOTES_PER_STEP] = {
    SEQUENCER_NO_PITCH, SEQUENCER_NO_PITCH, SEQUENCER_NO_PITCH,
    SEQUENCER_NO_PITCH, SEQUENCER_NO_PITCH, SEQUENCER_NO_PITCH
  };
  byte transposedCount = 0;

  for (byte i = 0; i < sequencerEditNoteCount && i < SEQUENCER_MAX_NOTES_PER_STEP; i++) {
    int32_t transposed = static_cast<int32_t>(sequencerEditPitchSteps[i]) + pitchStepDelta;
    if (transposed < -32768) {
      transposed = -32768;
    } else if (transposed > 32767) {
      transposed = 32767;
    }

    int16_t pitchSteps = static_cast<int16_t>(transposed);
    if (findNoteInBuffer(transposedNotes, transposedCount, pitchSteps) >= SEQUENCER_MAX_NOTES_PER_STEP &&
        transposedCount < SEQUENCER_MAX_NOTES_PER_STEP) {
      transposedNotes[transposedCount++] = pitchSteps;
    }
  }

  sortSequencerNoteBuffer(transposedNotes, transposedCount);

  bool changed = (transposedCount != sequencerEditNoteCount);
  if (!changed) {
    for (byte i = 0; i < transposedCount; i++) {
      if (transposedNotes[i] != sequencerEditPitchSteps[i]) {
        changed = true;
        break;
      }
    }
  }

  if (!changed) {
    return false;
  }

  clearSequencerNoteBuffer(sequencerEditPitchSteps, sequencerEditNoteCount);
  for (byte i = 0; i < transposedCount; i++) {
    sequencerEditPitchSteps[i] = transposedNotes[i];
  }
  sequencerEditNoteCount = transposedCount;
  saveEditBufferToStep(static_cast<byte>(sequencerSelectedStep));
  setSequencerDirtyState(true);
  previewSequencerStep(static_cast<byte>(sequencerSelectedStep));
  return true;
}

void handleSequencerToolAction(SequencerToolAction action) {
  switch (action) {
    case SequencerToolAction::Length:
      enterSequencerExactLengthEdit();
      return;
    case SequencerToolAction::Velocity:
      enterSequencerExactVelocityEdit();
      return;
    case SequencerToolAction::OctaveUp:
      if (sequencerEditNoteCount == 0) {
        showSequencerStatusMessageAndReturn("Step empty", "Add notes first", SequencerOverlayMode::FunctionPicker);
        return;
      }
      if (!canTransposeSelectedSequencerStep(getSequencerTuningCycleLength())) {
        showSequencerStatusMessageAndReturn("Oct+", "Range 0-9", SequencerOverlayMode::FunctionPicker);
        return;
      }
      if (transposeSelectedSequencerStep(getSequencerTuningCycleLength())) {
        sequencerOverlayMode = SequencerOverlayMode::FunctionPicker;
        sequencerOverlayVisible = false;
        sequencerOverlayDirty = true;
      } else {
        showSequencerStatusMessageAndReturn("Oct+", "Step unchanged", SequencerOverlayMode::FunctionPicker);
      }
      return;
    case SequencerToolAction::OctaveDown:
      if (sequencerEditNoteCount == 0) {
        showSequencerStatusMessageAndReturn("Step empty", "Add notes first", SequencerOverlayMode::FunctionPicker);
        return;
      }
      if (!canTransposeSelectedSequencerStep(-static_cast<int16_t>(getSequencerTuningCycleLength()))) {
        showSequencerStatusMessageAndReturn("Oct-", "Range 0-9", SequencerOverlayMode::FunctionPicker);
        return;
      }
      if (transposeSelectedSequencerStep(-static_cast<int16_t>(getSequencerTuningCycleLength()))) {
        sequencerOverlayMode = SequencerOverlayMode::FunctionPicker;
        sequencerOverlayVisible = false;
        sequencerOverlayDirty = true;
      } else {
        showSequencerStatusMessageAndReturn("Oct-", "Step unchanged", SequencerOverlayMode::FunctionPicker);
      }
      return;
    case SequencerToolAction::Probability:
      enterSequencerExactProbabilityEdit();
      return;
    case SequencerToolAction::Tie:
      if (sequencerSelectedStep < 0) {
        showSequencerStatusMessageAndReturn("Tie", "Select step first", SequencerOverlayMode::FunctionPicker);
        return;
      }
      sequencerStepTie[sequencerSelectedStep] = !sequencerStepTie[sequencerSelectedStep];
      setSequencerDirtyState(true);
      sequencerOverlayMode = SequencerOverlayMode::FunctionPicker;
      sequencerOverlayVisible = false;
      sequencerOverlayDirty = true;
      return;
    case SequencerToolAction::Copy:
      enterSequencerCopyTargetSelect();
      return;
    case SequencerToolAction::Cancel:
      exitSequencerFunctionPicker();
      return;
  }
}

int8_t buttonIndexToSequencerStep(byte buttonIndex) {
  if (buttonIndex >= 1 && buttonIndex <= 8) {
    return static_cast<int8_t>(buttonIndex - 1);
  }
  if (buttonIndex >= 10 && buttonIndex < 18) {
    return static_cast<int8_t>(8 + (buttonIndex - 10));
  }
  if (buttonIndex >= 21 && buttonIndex < 29) {
    return static_cast<int8_t>(16 + (buttonIndex - 21));
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
    return static_cast<int8_t>(21 + (stepIndex - 16));
  }
  if (stepIndex < SEQUENCER_STEP_COUNT) {
    return static_cast<int8_t>(30 + (stepIndex - 24));
  }
  return -1;
}

bool handleSequencerRotaryTurnInternal(int8_t direction) {
  if (direction == 0) {
    return false;
  }

  if (sequencerOverlayMode == SequencerOverlayMode::ExactVelocityEdit) {
    byte currentIndex = sequencerVelocityChoiceIndex(sequencerVelocityDisplay);
    int nextIndex = static_cast<int>(currentIndex) + direction;
    if (nextIndex < 0) {
      nextIndex = 0;
    } else if (nextIndex >= SEQUENCER_VELOCITY_CHOICE_COUNT) {
      nextIndex = SEQUENCER_VELOCITY_CHOICE_COUNT - 1;
    }

    byte newVelocity = sequencerVelocityChoiceValue(static_cast<byte>(nextIndex));
    if (newVelocity != sequencerVelocityDisplay) {
      sequencerVelocityDisplay = newVelocity;
      sequencerOverlayDirty = true;
    }
    return true;
  }

  if (sequencerOverlayMode == SequencerOverlayMode::ExactProbabilityEdit) {
    byte currentIndex = sequencerProbabilityChoiceIndex(sequencerProbabilityDisplay);
    int nextIndex = static_cast<int>(currentIndex) + direction;
    if (nextIndex < 0) {
      nextIndex = 0;
    } else if (nextIndex >= SEQUENCER_PROBABILITY_CHOICE_COUNT) {
      nextIndex = SEQUENCER_PROBABILITY_CHOICE_COUNT - 1;
    }

    byte newProbability = sequencerProbabilityChoiceValue(static_cast<byte>(nextIndex));
    if (newProbability != sequencerProbabilityDisplay) {
      sequencerProbabilityDisplay = newProbability;
      sequencerOverlayDirty = true;
    }
    return true;
  }

  if (sequencerOverlayMode == SequencerOverlayMode::ExactLengthEdit) {
    return true;
  }

  if (sequencerSelectedStep < 0) {
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
  setSequencerDirtyState(true);
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

byte sequencerVelocityChoiceValue(byte choiceIndex) {
  if (choiceIndex >= SEQUENCER_VELOCITY_CHOICE_COUNT - 1) {
    return 127;
  }
  return static_cast<byte>(choiceIndex * 5);
}

byte sequencerVelocityChoiceIndex(byte velocity) {
  byte nearestIndex = 0;
  uint16_t nearestDistance = 65535;
  for (byte i = 0; i < SEQUENCER_VELOCITY_CHOICE_COUNT; i++) {
    byte choiceValue = sequencerVelocityChoiceValue(i);
    uint16_t distance = static_cast<uint16_t>(abs(static_cast<int>(choiceValue) - static_cast<int>(velocity)));
    if (distance < nearestDistance) {
      nearestDistance = distance;
      nearestIndex = i;
    }
  }
  return nearestIndex;
}

byte sequencerProbabilityChoiceValue(byte choiceIndex) {
  if (choiceIndex >= SEQUENCER_PROBABILITY_CHOICE_COUNT - 1) {
    return 100;
  }
  return static_cast<byte>(choiceIndex * 5);
}

byte sequencerProbabilityChoiceIndex(byte probability) {
  byte nearestIndex = 0;
  uint16_t nearestDistance = 65535;
  for (byte i = 0; i < SEQUENCER_PROBABILITY_CHOICE_COUNT; i++) {
    byte choiceValue = sequencerProbabilityChoiceValue(i);
    uint16_t distance = static_cast<uint16_t>(abs(static_cast<int>(choiceValue) - static_cast<int>(probability)));
    if (distance < nearestDistance) {
      nearestDistance = distance;
      nearestIndex = i;
    }
  }
  return nearestIndex;
}

void formatSequencerStepNote(char* out, size_t outSize, int16_t pitchSteps) {
  if (pitchSteps == SEQUENCER_NO_PITCH) {
    snprintf(out, outSize, "--");
    return;
  }
  formatBoardPitchStepsForSequencer(pitchSteps, out, outSize);
}

bool appendSequencerNoteLabel(char* line, size_t lineSize, const char* label) {
  if (line == nullptr || lineSize == 0 || label == nullptr || label[0] == '\0') {
    return false;
  }

  size_t lineLength = strlen(line);
  size_t labelLength = strlen(label);
  size_t requiredLength = labelLength + ((lineLength > 0) ? 1 : 0);
  if (lineLength + requiredLength >= lineSize) {
    return false;
  }

  if (lineLength > 0) {
    strncat(line, " ", lineSize - strlen(line) - 1);
  }
  strncat(line, label, lineSize - strlen(line) - 1);
  return true;
}

bool appendOverviewLabelToLine(char* line, size_t lineSize, const char* label) {
  if (line == nullptr || lineSize == 0 || label == nullptr || label[0] == '\0') {
    return false;
  }

  char candidate[24];
  snprintf(candidate, sizeof(candidate), "%s%s%s", line, (line[0] != '\0') ? " " : "", label);
  if (strlen(candidate) >= lineSize) {
    return false;
  }

  constexpr uint8_t overviewUsableWidthPixels = 124;
  if (u8g2.getStrWidth(candidate) > overviewUsableWidthPixels) {
    return false;
  }

  snprintf(line, lineSize, "%s", candidate);
  return true;
}

bool appendOverlayLabelToLine(char* line, size_t lineSize, const char* label) {
  if (line == nullptr || lineSize == 0 || label == nullptr || label[0] == '\0') {
    return false;
  }

  char candidate[24];
  snprintf(candidate, sizeof(candidate), "%s%s%s", line, (line[0] != '\0') ? " " : "", label);
  if (strlen(candidate) >= lineSize) {
    return false;
  }

  u8g2.setFont(u8g2_font_6x13_tf);
  constexpr uint8_t overlayUsableWidthPixels = 116;
  if (u8g2.getStrWidth(candidate) > overlayUsableWidthPixels) {
    return false;
  }

  snprintf(line, lineSize, "%s", candidate);
  return true;
}

void clearSequencerNoteBuffer(int16_t* notes, byte& count) {
  count = 0;
  for (byte i = 0; i < SEQUENCER_MAX_NOTES_PER_STEP; i++) {
    notes[i] = SEQUENCER_NO_PITCH;
  }
}

void sortSequencerNoteBuffer(int16_t* notes, byte count) {
  if (count < 2) {
    return;
  }

  for (byte i = 0; i + 1 < count; i++) {
    for (byte j = static_cast<byte>(i + 1); j < count; j++) {
      if (notes[j] < notes[i]) {
        int16_t temp = notes[i];
        notes[i] = notes[j];
        notes[j] = temp;
      }
    }
  }
}

byte findNoteInBuffer(const int16_t* notes, byte count, int16_t pitchSteps) {
  for (byte i = 0; i < count; i++) {
    if (notes[i] == pitchSteps) {
      return i;
    }
  }
  return SEQUENCER_MAX_NOTES_PER_STEP;
}

void loadEditBufferFromStep(byte stepIndex) {
  clearSequencerNoteBuffer(sequencerEditPitchSteps, sequencerEditNoteCount);
  byte count = sequencerStepNoteCount[stepIndex];
  for (byte i = 0; i < count && i < SEQUENCER_MAX_NOTES_PER_STEP; i++) {
    sequencerEditPitchSteps[i] = sequencerStepPitchSteps[stepIndex][i];
  }
  sequencerEditNoteCount = count;
  sortSequencerNoteBuffer(sequencerEditPitchSteps, sequencerEditNoteCount);
}

void snapshotUndoBufferFromStep(byte stepIndex) {
  clearSequencerNoteBuffer(sequencerUndoPitchSteps, sequencerUndoNoteCount);
  byte count = sequencerStepNoteCount[stepIndex];
  for (byte i = 0; i < count && i < SEQUENCER_MAX_NOTES_PER_STEP; i++) {
    sequencerUndoPitchSteps[i] = sequencerStepPitchSteps[stepIndex][i];
  }
  sequencerUndoNoteCount = count;
  sequencerUndoGatePercent = sequencerStepGatePercent[stepIndex];
  sequencerUndoVelocity = sequencerStepVelocity[stepIndex];
  sequencerUndoProbability = sequencerStepProbability[stepIndex];
  sequencerUndoTie = sequencerStepTie[stepIndex];
}

void saveEditBufferToStep(byte stepIndex) {
  sortSequencerNoteBuffer(sequencerEditPitchSteps, sequencerEditNoteCount);
  clearSequencerNoteBuffer(sequencerStepPitchSteps[stepIndex], sequencerStepNoteCount[stepIndex]);
  for (byte i = 0; i < sequencerEditNoteCount && i < SEQUENCER_MAX_NOTES_PER_STEP; i++) {
    sequencerStepPitchSteps[stepIndex][i] = sequencerEditPitchSteps[i];
  }
  sequencerStepNoteCount[stepIndex] = sequencerEditNoteCount;
}

void copySequencerStepData(byte sourceStep, byte destinationStep) {
  clearSequencerNoteBuffer(sequencerStepPitchSteps[destinationStep], sequencerStepNoteCount[destinationStep]);
  byte sourceCount = sequencerStepNoteCount[sourceStep];
  for (byte i = 0; i < sourceCount && i < SEQUENCER_MAX_NOTES_PER_STEP; i++) {
    sequencerStepPitchSteps[destinationStep][i] = sequencerStepPitchSteps[sourceStep][i];
  }
  sequencerStepNoteCount[destinationStep] = sourceCount;
  sequencerStepGatePercent[destinationStep] = sequencerStepGatePercent[sourceStep];
  sequencerStepVelocity[destinationStep] = sequencerStepVelocity[sourceStep];
  sequencerStepProbability[destinationStep] = sequencerStepProbability[sourceStep];
  sequencerStepTie[destinationStep] = sequencerStepTie[sourceStep];
}

void restoreSelectedSequencerStepFromUndo() {
  if (sequencerSelectedStep < 0) {
    return;
  }

  clearSequencerNoteBuffer(sequencerEditPitchSteps, sequencerEditNoteCount);
  for (byte i = 0; i < sequencerUndoNoteCount && i < SEQUENCER_MAX_NOTES_PER_STEP; i++) {
    sequencerEditPitchSteps[i] = sequencerUndoPitchSteps[i];
  }
  sequencerEditNoteCount = sequencerUndoNoteCount;
  saveEditBufferToStep(static_cast<byte>(sequencerSelectedStep));
  sequencerStepGatePercent[sequencerSelectedStep] = sequencerUndoGatePercent;
  sequencerStepVelocity[sequencerSelectedStep] = sequencerUndoVelocity;
  sequencerStepProbability[sequencerSelectedStep] = sequencerUndoProbability;
  sequencerStepTie[sequencerSelectedStep] = sequencerUndoTie;
  sequencerLengthPercentDisplay = sequencerUndoGatePercent;
  sequencerVelocityDisplay = sequencerUndoVelocity;
  sequencerProbabilityDisplay = sequencerUndoProbability;
  setSequencerDirtyState(true);
}

int8_t getSequencerOverlayNoteSourceStep() {
  if (sequencerOverlayMode == SequencerOverlayMode::CopyTargetSelect && sequencerCopySourceStep >= 0) {
    return sequencerCopySourceStep;
  }
  return sequencerSelectedStep;
}

bool shouldDisplaySequencerTie(int8_t stepIndex) {
  return stepIndex >= 0 &&
         stepIndex < SEQUENCER_STEP_COUNT &&
         sequencerStepTie[stepIndex];
}

void toggleEditBufferNote(int16_t pitchSteps) {
  byte index = findNoteInBuffer(sequencerEditPitchSteps, sequencerEditNoteCount, pitchSteps);
  if (index < SEQUENCER_MAX_NOTES_PER_STEP) {
    for (byte i = index; i + 1 < sequencerEditNoteCount; i++) {
      sequencerEditPitchSteps[i] = sequencerEditPitchSteps[i + 1];
    }
    if (sequencerEditNoteCount > 0) {
      sequencerEditNoteCount--;
      sequencerEditPitchSteps[sequencerEditNoteCount] = SEQUENCER_NO_PITCH;
    }
    return;
  }

  if (sequencerEditNoteCount < SEQUENCER_MAX_NOTES_PER_STEP) {
    sequencerEditPitchSteps[sequencerEditNoteCount++] = pitchSteps;
  }
  sortSequencerNoteBuffer(sequencerEditPitchSteps, sequencerEditNoteCount);
}

int16_t sequencerPrimaryPitchSteps(byte stepIndex) {
  if (stepIndex >= SEQUENCER_STEP_COUNT) {
    return SEQUENCER_NO_PITCH;
  }
  if (sequencerStepTie[stepIndex]) {
    if (sequencerStepNoteCount[stepIndex] > 0) {
      return sequencerStepPitchSteps[stepIndex][0];
    }
    int8_t sourceStep = findSequencerTieSourceStep(stepIndex, sequencerActiveStepCount());
    if (sourceStep >= 0 && sequencerStepNoteCount[sourceStep] > 0) {
      return sequencerStepPitchSteps[sourceStep][0];
    }
    return SEQUENCER_NO_PITCH;
  }
  if (sequencerSelectedStep == stepIndex && sequencerOverlayMode == SequencerOverlayMode::AwaitingNote) {
    return (sequencerEditNoteCount > 0) ? sequencerEditPitchSteps[0] : SEQUENCER_NO_PITCH;
  }
  return (sequencerStepNoteCount[stepIndex] > 0) ? sequencerStepPitchSteps[stepIndex][0] : SEQUENCER_NO_PITCH;
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

  int8_t sourceStepIndex = getSequencerOverlayNoteSourceStep();
  if (shouldDisplaySequencerTie(sourceStepIndex)) {
    snprintf(lineOne, lineOneSize, "T");
    return;
  }

  const int16_t* sourceNotes = sequencerEditPitchSteps;
  byte sourceCount = sequencerEditNoteCount;
  if (sourceStepIndex >= 0 && sourceStepIndex < SEQUENCER_STEP_COUNT) {
    sourceNotes = sequencerStepPitchSteps[sourceStepIndex];
    sourceCount = sequencerStepNoteCount[sourceStepIndex];
  }

  if (sourceCount == 0) {
    snprintf(lineOne, lineOneSize, "--");
    return;
  }

  char noteLabel[12];
  for (byte i = 0; i < sourceCount && i < SEQUENCER_MAX_NOTES_PER_STEP; i++) {
    formatSequencerStepNote(noteLabel, sizeof(noteLabel), sourceNotes[i]);
    if (!appendOverlayLabelToLine(lineOne, lineOneSize, noteLabel)) {
      appendOverlayLabelToLine(lineTwo, lineTwoSize, noteLabel);
    }
  }
}

void fillOverviewStepLines(byte stepIndex, char* lineOne, size_t lineOneSize, char* lineTwo, size_t lineTwoSize) {
  if (stepIndex >= SEQUENCER_STEP_COUNT || lineOneSize == 0 || lineTwoSize == 0) {
    return;
  }

  lineOne[0] = '\0';
  lineTwo[0] = '\0';

  if (shouldDisplaySequencerTie(stepIndex)) {
    snprintf(lineOne, lineOneSize, "%02u T", static_cast<unsigned>(stepIndex + 1));
    return;
  }

  if (sequencerStepNoteCount[stepIndex] == 0) {
    snprintf(lineOne, lineOneSize, "%02u _", static_cast<unsigned>(stepIndex + 1));
    return;
  }

  snprintf(lineOne, lineOneSize, "%02u", static_cast<unsigned>(stepIndex + 1));

  char noteLabel[12];
  byte noteCount = sequencerStepNoteCount[stepIndex];
  for (byte noteIndex = 0; noteIndex < noteCount && noteIndex < SEQUENCER_MAX_NOTES_PER_STEP; noteIndex++) {
    formatSequencerStepNote(noteLabel, sizeof(noteLabel), sequencerStepPitchSteps[stepIndex][noteIndex]);
    if (!appendOverviewLabelToLine(lineOne, lineOneSize, noteLabel)) {
      if (lineTwo[0] == '\0') {
        snprintf(lineTwo, lineTwoSize, "   ");
      }
      appendOverviewLabelToLine(lineTwo, lineTwoSize, noteLabel);
    }
  }
}

byte countSequencerOverviewStepsThatFit(byte firstStep) {
  if (firstStep >= SEQUENCER_STEP_COUNT) {
    return 0;
  }

  u8g2.setFont(u8g2_font_6x13_tf);
  int y = SEQUENCER_OVERVIEW_FIRST_LINE_Y;
  byte count = 0;
  char lineOne[24];
  char lineTwo[24];

  for (byte stepIndex = firstStep; stepIndex < SEQUENCER_STEP_COUNT; stepIndex++) {
    fillOverviewStepLines(stepIndex, lineOne, sizeof(lineOne), lineTwo, sizeof(lineTwo));
    bool usesSecondLine = (lineTwo[0] != '\0');
    int lastBaseline = usesSecondLine ? (y + SEQUENCER_OVERVIEW_SECOND_LINE_OFFSET) : y;
    if (lastBaseline > SEQUENCER_OVERVIEW_MAX_BASELINE_Y) {
      break;
    }

    count++;
    y += usesSecondLine ? SEQUENCER_OVERVIEW_DOUBLE_STEP_ADVANCE : SEQUENCER_OVERVIEW_SINGLE_STEP_ADVANCE;
  }

  return (count > 0) ? count : 1;
}

void hideSequencerOverlay() {
  bool wasVisible = sequencerOverlayVisible;
  sequencerCopySourceStep = -1;
  sequencerOverlayMode = SequencerOverlayMode::Hidden;
  sequencerOverlayUntil = 0;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = false;
  if (wasVisible) {
    u8g2.clearBuffer();
    u8g2.sendBuffer();
  }
}

void formatSequencerUsageLabel(uint64_t bytes, char* out, size_t outSize) {
  if (outSize == 0) {
    return;
  }
  if (bytes >= (1024ULL * 1024ULL)) {
    snprintf(out, outSize, "%lluM", (bytes + (512ULL * 1024ULL)) / (1024ULL * 1024ULL));
  } else if (bytes >= 1024ULL) {
    snprintf(out, outSize, "%lluK", (bytes + 512ULL) / 1024ULL);
  } else {
    snprintf(out, outSize, "%lluB", bytes);
  }
}

void refreshSequencerPerformanceStats(bool forceRefresh) {
  if (sequencerOverlayMode != SequencerOverlayMode::PerformanceMonitor) {
    return;
  }

  if (!forceRefresh &&
      sequencerPerformanceLastSampleAt != 0 &&
      (runTime - sequencerPerformanceLastSampleAt) < SEQUENCER_PERFORMANCE_REFRESH_MICROS) {
    return;
  }

  sequencerPerformanceHeapUsedBytes = static_cast<uint32_t>(rp2040.getUsedHeap());
  sequencerPerformanceHeapTotalBytes = static_cast<uint32_t>(rp2040.getTotalHeap());

  FSInfo storageInfo;
  if (fileSystemExists && LittleFS.info(storageInfo)) {
    sequencerPerformanceStorageUsedBytes = storageInfo.usedBytes;
    sequencerPerformanceStorageTotalBytes = storageInfo.totalBytes;
    sequencerPerformanceStorageValid = true;
  } else {
    sequencerPerformanceStorageUsedBytes = 0;
    sequencerPerformanceStorageTotalBytes = 0;
    sequencerPerformanceStorageValid = false;
  }

  readAndResetISRProfile();
  if (isrProfileCount > 0) {
    sequencerPerformanceCpuAvgUs = isrProfileAvgUs;
    uint32_t percent = static_cast<uint32_t>(
      (static_cast<uint64_t>(sequencerPerformanceCpuAvgUs) * 100ULL + (SEQUENCER_AUDIO_ISR_PERIOD_MICROS / 2ULL)) /
      SEQUENCER_AUDIO_ISR_PERIOD_MICROS);
    sequencerPerformanceCpuPercent = static_cast<uint16_t>((percent > 999U) ? 999U : percent);
  } else {
    sequencerPerformanceCpuAvgUs = 0;
    sequencerPerformanceCpuPercent = 0;
  }

  sequencerPerformanceLastSampleAt = runTime;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = true;
}

void showSequencerPerformanceMonitor() {
  if (sequencerOverlayMode == SequencerOverlayMode::PerformanceMonitor) {
    refreshSequencerPerformanceStats(false);
    return;
  }

  sequencerOverlayBeforePerformance = sequencerOverlayMode;
  sequencerOverlayMode = SequencerOverlayMode::PerformanceMonitor;
  sequencerOverlayUntil = 0;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = true;
  sequencerPerformanceLastSampleAt = 0;
  resetMidiMonitorStats();
  isrProfilingEnabled = true;
  readAndResetISRProfile();
  refreshSequencerPerformanceStats(true);
}

void hideSequencerPerformanceMonitor() {
  if (sequencerOverlayMode != SequencerOverlayMode::PerformanceMonitor) {
    return;
  }

  isrProfilingEnabled = false;
  sequencerOverlayMode = sequencerOverlayBeforePerformance;
  sequencerOverlayUntil = 0;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = true;
  menu.drawMenu();
}

void showSequencerOverviewPage(bool advancePage) {
  if (advancePage) {
    byte displayedCount = countSequencerOverviewStepsThatFit(sequencerOverviewStartStep);
    byte nextStart = static_cast<byte>(sequencerOverviewStartStep + displayedCount);
    sequencerOverviewStartStep = (nextStart < SEQUENCER_STEP_COUNT) ? nextStart : 0;
  } else {
    sequencerOverviewStartStep = 0;
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

void resetSequencerClockSyncState() {
  sequencerExternalClock = SequencerExternalClockState{};
  sequencerNextMidiClockAt = 0;
}

bool sequencerUsesExternalClock() {
  return sequencerClockSource == SEQUENCER_CLOCK_SOURCE_EXTERNAL_MIDI;
}

bool sequencerShouldSendMidiClock() {
  return !sequencerUsesExternalClock() && sequencerSendClock == SEQUENCER_SEND_CLOCK_ON;
}

bool sequencerShouldSendMidiTransport() {
  return !sequencerUsesExternalClock() && sequencerSendTransport == SEQUENCER_SEND_TRANSPORT_ON;
}

uint64_t sequencerCurrentStepDurationMicros() {
  if (sequencerUsesExternalClock() && sequencerExternalClock.stepDuration > 0) {
    return sequencerExternalClock.stepDuration;
  }
  return sequencerStepDurationMicros();
}

byte sequencerSelectedStepVelocity() {
  if (sequencerSelectedStep >= 0) {
    return sequencerStepVelocity[sequencerSelectedStep];
  }
  return SEQUENCER_DEFAULT_VELOCITY;
}

int findSequencerManagedHeldNote(int16_t pitchSteps) {
  for (byte i = 0; i < SEQUENCER_MAX_MANAGED_HELD_NOTES; i++) {
    if (sequencerManagedHeldNotes[i].active && sequencerManagedHeldNotes[i].pitchSteps == pitchSteps) {
      return i;
    }
  }
  return -1;
}

int allocateSequencerManagedHeldNote(int16_t pitchSteps) {
  for (byte i = 0; i < SEQUENCER_MAX_MANAGED_HELD_NOTES; i++) {
    if (!sequencerManagedHeldNotes[i].active) {
      sequencerManagedHeldNotes[i] = SequencerManagedHeldNote{};
      sequencerManagedHeldNotes[i].active = true;
      sequencerManagedHeldNotes[i].pitchSteps = pitchSteps;
      return i;
    }
  }
  return -1;
}

void sendSequencerManagedNoteOn(int16_t pitchSteps, bool playbackNote, byte velocity) {
  if (pitchSteps == SEQUENCER_NO_PITCH) {
    return;
  }

  int slotIndex = findSequencerManagedHeldNote(pitchSteps);
  if (slotIndex < 0) {
    slotIndex = allocateSequencerManagedHeldNote(pitchSteps);
    if (slotIndex < 0) {
      return;
    }
  }

  SequencerManagedHeldNote& heldNote = sequencerManagedHeldNotes[slotIndex];
  if (heldNote.auditionCount == 0 && heldNote.playbackCount == 0) {
    if (!startSequencerTunedNote(
          pitchSteps,
          sequencerPlayType == SEQUENCER_PLAY_TYPE_OB_SYNTH,
          velocity,
          heldNote.handle)) {
      heldNote = SequencerManagedHeldNote{};
      return;
    }
  }

  byte& heldCount = playbackNote ? heldNote.playbackCount : heldNote.auditionCount;
  if (playbackNote && heldCount == 0) {
    heldNote.playbackLedActivatedAt = runTime;
    heldNote.playbackLedActivationEpoch = sequencerTransportPlaybackLedEpoch;
  }
  if (heldCount < 255) {
    heldCount++;
  }
}

void stopSequencerAuditionNotes() {
  for (byte i = 0; i < SEQUENCER_MAX_MANAGED_HELD_NOTES; i++) {
    while (sequencerManagedHeldNotes[i].active && sequencerManagedHeldNotes[i].auditionCount > 0) {
      sendSequencerManagedNoteOff(sequencerManagedHeldNotes[i].pitchSteps, false);
    }
  }
}

void sendSequencerManagedNoteOff(int16_t pitchSteps, bool playbackNote) {
  if (pitchSteps == SEQUENCER_NO_PITCH) {
    return;
  }

  int slotIndex = findSequencerManagedHeldNote(pitchSteps);
  if (slotIndex < 0) {
    return;
  }

  SequencerManagedHeldNote& heldNote = sequencerManagedHeldNotes[slotIndex];
  byte& heldCount = playbackNote ? heldNote.playbackCount : heldNote.auditionCount;
  if (heldCount > 0) {
    heldCount--;
  }

  if (heldNote.playbackCount == 0 && heldNote.auditionCount == 0) {
    stopSequencerTunedNote(heldNote.handle);
    heldNote = SequencerManagedHeldNote{};
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

bool sequencerStepHasPlayableNoteData(byte stepIndex) {
  return stepIndex < SEQUENCER_STEP_COUNT &&
         !sequencerStepTie[stepIndex] &&
         sequencerStepNoteCount[stepIndex] > 0 &&
         sequencerStepGatePercent[stepIndex] > 0;
}

bool isSequencerProgrammedStep(byte stepIndex) {
  return stepIndex < SEQUENCER_STEP_COUNT &&
         (sequencerStepTie[stepIndex] || sequencerStepNoteCount[stepIndex] > 0);
}

int8_t findSequencerTieSourceStep(byte stepIndex, byte activeStepCount) {
  if (stepIndex == 0 || stepIndex >= activeStepCount || !sequencerStepTie[stepIndex]) {
    return -1;
  }

  for (int8_t sourceStep = static_cast<int8_t>(stepIndex - 1); sourceStep >= 0; sourceStep--) {
    if (sequencerStepHasPlayableNoteData(static_cast<byte>(sourceStep))) {
      return sourceStep;
    }
  }

  return -1;
}

int findActiveSequencerTransportPlaybackGroup(byte sourceStep) {
  for (byte groupIndex = 0; groupIndex < SEQUENCER_MAX_ACTIVE_PLAYBACK_GROUPS; groupIndex++) {
    const SequencerPlaybackGroup& group = sequencerPlaybackGroups[groupIndex];
    if (group.active && group.transportPlayback && group.sourceStep == sourceStep) {
      return groupIndex;
    }
  }
  return -1;
}

void clearSequencerPlaybackGroup(SequencerPlaybackGroup& group, bool stopNotes) {
  if (stopNotes) {
    for (byte noteIndex = 0; noteIndex < group.noteCount; noteIndex++) {
      if (group.pitchSteps[noteIndex] != SEQUENCER_NO_PITCH) {
        sendSequencerManagedNoteOff(group.pitchSteps[noteIndex], true);
      }
      group.pitchSteps[noteIndex] = SEQUENCER_NO_PITCH;
    }
  } else {
    for (byte noteIndex = 0; noteIndex < SEQUENCER_MAX_NOTES_PER_STEP; noteIndex++) {
      group.pitchSteps[noteIndex] = SEQUENCER_NO_PITCH;
    }
  }

  group.noteCount = 0;
  group.sourceStep = -1;
  group.transportPlayback = false;
  group.externalClock = SequencerPlaybackExternalClockState{};
  group.noteOffAt = 0;
  group.active = false;
}

bool doesNextSequencerStepContinueSource(byte stepIndex, byte activeStepCount) {
  byte nextStepIndex = static_cast<byte>(stepIndex + 1);
  return nextStepIndex < activeStepCount &&
         findSequencerTieSourceStep(nextStepIndex, activeStepCount) == stepIndex;
}

bool doesSequencerStepContinuePlaybackGroup(int8_t stepIndex, byte activeStepCount, const SequencerPlaybackGroup& group) {
  if (stepIndex < 0 ||
      stepIndex >= activeStepCount ||
      !group.transportPlayback ||
      group.sourceStep < 0 ||
      !sequencerStepTie[static_cast<byte>(stepIndex)]) {
    return false;
  }

  return findSequencerTieSourceStep(static_cast<byte>(stepIndex), activeStepCount) == group.sourceStep;
}

void releaseSequencerPlaybackGroupsForStepBoundary(int8_t nextStepIndex, byte activeStepCount) {
  for (byte groupIndex = 0; groupIndex < SEQUENCER_MAX_ACTIVE_PLAYBACK_GROUPS; groupIndex++) {
    SequencerPlaybackGroup& group = sequencerPlaybackGroups[groupIndex];
    if (!group.active ||
        (group.externalClock.pendingGateSync && group.noteOffAt == 0) ||
        group.externalClock.pendingTieBoundary ||
        runTime < group.noteOffAt) {
      continue;
    }

    // Boundary note-offs should happen before the next regular step retriggers,
    // but ties must keep their carried note alive across that boundary.
    if (doesSequencerStepContinuePlaybackGroup(nextStepIndex, activeStepCount, group)) {
      continue;
    }

    clearSequencerPlaybackGroup(group, true);
  }
}

void continueSequencerTiePlayback(byte stepIndex, uint64_t stepDuration) {
  byte activeStepCount = sequencerActiveStepCount();
  int8_t sourceStep = findSequencerTieSourceStep(stepIndex, activeStepCount);
  if (sourceStep < 0) {
    return;
  }

  int groupIndex = findActiveSequencerTransportPlaybackGroup(sourceStep);
  if (groupIndex < 0) {
    return;
  }

  SequencerPlaybackGroup& group = sequencerPlaybackGroups[groupIndex];
  if (sequencerUsesExternalClock()) {
    group.externalClock.pendingTieBoundary = true;
    group.noteOffAt = 0;
    return;
  }

  uint64_t tieNoteOffAt = runTime + stepDuration;
  if (group.noteOffAt < tieNoteOffAt) {
    group.noteOffAt = tieNoteOffAt;
  }
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
    clearSequencerPlaybackGroup(group, true);
  }
}

void serviceSequencerPlaybackGroups() {
  for (byte groupIndex = 0; groupIndex < SEQUENCER_MAX_ACTIVE_PLAYBACK_GROUPS; groupIndex++) {
    SequencerPlaybackGroup& group = sequencerPlaybackGroups[groupIndex];
    if (!group.active ||
        (group.externalClock.pendingGateSync && group.noteOffAt == 0) ||
        group.externalClock.pendingTieBoundary ||
        runTime < group.noteOffAt) {
      continue;
    }
    clearSequencerPlaybackGroup(group, true);
  }
}

void advanceSequencerPlaybackStep(uint64_t stepDuration, bool applyProbability) {
  byte activeStepCount = sequencerActiveStepCount();
  int8_t nextStepIndex = nextSequencerStep(activeStepCount);

  if (nextStepIndex < 0) {
    return;
  }

  releaseSequencerPlaybackGroupsForStepBoundary(nextStepIndex, activeStepCount);
  sequencerPlayingStep = nextStepIndex;

  byte stepIndex = static_cast<byte>(sequencerPlayingStep);
  if (sequencerStepTie[stepIndex]) {
    continueSequencerTiePlayback(stepIndex, stepDuration);
    return;
  }

  if (sequencerStepNoteCount[stepIndex] == 0) {
    return;
  }

  startSequencerPlaybackGroup(stepIndex, stepDuration, applyProbability, true);
}

void serviceSequencerInternalMidiClock() {
  if (!sequencerShouldSendMidiClock() || sequencerTransportState != SEQUENCER_TRANSPORT_PLAY) {
    return;
  }

  uint64_t clockPulseDuration = sequencerStepDurationMicros() / SEQUENCER_MIDI_CLOCKS_PER_STEP;
  if (clockPulseDuration == 0) {
    clockPulseDuration = 1;
  }

  if (sequencerNextMidiClockAt == 0) {
    sequencerNextMidiClockAt = runTime;
  }

  while (runTime >= sequencerNextMidiClockAt) {
    sendSequencerMidiClockPulse();
    sequencerNextMidiClockAt += clockPulseDuration;
  }
}

void recordSequencerExternalClockPulse() {
  if (sequencerExternalClock.lastPulseAt != 0 && runTime > sequencerExternalClock.lastPulseAt) {
    uint64_t pulseDuration = runTime - sequencerExternalClock.lastPulseAt;
    if (pulseDuration > 0 && sequencerExternalClock.pulseIntervalCount < 255) {
      sequencerExternalClock.pulseMicrosAccum += pulseDuration;
      sequencerExternalClock.pulseIntervalCount++;
    }
  }
  sequencerExternalClock.lastPulseAt = runTime;
}

bool advanceSequencerExternalClockState(uint64_t& stepDuration) {
  updatePendingExternalGateSyncEstimate();

  sequencerExternalClock.pulseCount =
    static_cast<byte>((sequencerExternalClock.pulseCount + 1) % SEQUENCER_MIDI_CLOCKS_PER_STEP);
  if (sequencerExternalClock.pulseCount != 0) {
    return false;
  }

  stepDuration = sequencerCurrentStepDurationMicros();
  if (sequencerCurrentStepStartedAt != 0 && runTime > sequencerCurrentStepStartedAt) {
    // Measure the full time between external step boundaries. Using only the
    // most recent MIDI-clock pulse makes tied holds wobble under USB jitter.
    uint64_t measuredStepDuration = runTime - sequencerCurrentStepStartedAt;
    if (measuredStepDuration > 0) {
      sequencerExternalClock.stepDuration = measuredStepDuration;
      stepDuration = measuredStepDuration;
    }
  }

  resolvePendingExternalGateSync(stepDuration);

  sequencerExternalClock.pulseMicrosAccum = 0;
  sequencerExternalClock.pulseIntervalCount = 0;
  return true;
}

void updatePendingExternalGateSyncEstimate() {
  if (sequencerExternalClock.pulseIntervalCount == 0) {
    return;
  }

  uint64_t estimatedStepDuration =
    (sequencerExternalClock.pulseMicrosAccum * SEQUENCER_MIDI_CLOCKS_PER_STEP) /
    sequencerExternalClock.pulseIntervalCount;
  if (estimatedStepDuration == 0) {
    return;
  }

  for (byte groupIndex = 0; groupIndex < SEQUENCER_MAX_ACTIVE_PLAYBACK_GROUPS; groupIndex++) {
    SequencerPlaybackGroup& group = sequencerPlaybackGroups[groupIndex];
    if (!group.active || !group.externalClock.pendingGateSync) {
      continue;
    }
    group.noteOffAt =
      group.externalClock.startedAt + ((estimatedStepDuration * group.externalClock.gatePercent) / 100ULL);
  }
}

void resolvePendingExternalGateSync(uint64_t stepDuration) {
  for (byte groupIndex = 0; groupIndex < SEQUENCER_MAX_ACTIVE_PLAYBACK_GROUPS; groupIndex++) {
    SequencerPlaybackGroup& group = sequencerPlaybackGroups[groupIndex];
    if (!group.active || !group.externalClock.pendingGateSync) {
      continue;
    }
    group.externalClock.pendingGateSync = false;
    uint64_t resolvedNoteOffAt =
      group.externalClock.startedAt + ((stepDuration * group.externalClock.gatePercent) / 100ULL);
    if (resolvedNoteOffAt > 0) {
      group.noteOffAt = resolvedNoteOffAt;
    }
  }
}

void resolvePendingExternalTieBoundaries(byte activeStepCount) {
  for (byte groupIndex = 0; groupIndex < SEQUENCER_MAX_ACTIVE_PLAYBACK_GROUPS; groupIndex++) {
    SequencerPlaybackGroup& group = sequencerPlaybackGroups[groupIndex];
    if (!group.active || !group.externalClock.pendingTieBoundary) {
      continue;
    }
    if (group.externalClock.startedAt >= sequencerCurrentStepStartedAt) {
      // Notes started on this boundary should be resolved on the next one.
      continue;
    }

    bool currentStepContinuesSource =
      (sequencerPlayingStep >= 0 &&
       sequencerStepTie[static_cast<byte>(sequencerPlayingStep)] &&
       findSequencerTieSourceStep(static_cast<byte>(sequencerPlayingStep), activeStepCount) == group.sourceStep);

    if (currentStepContinuesSource) {
      group.noteOffAt = 0;
      continue;
    }

    group.externalClock.pendingTieBoundary = false;
    group.noteOffAt = runTime;
  }
}

void startSequencerPlaybackGroup(byte stepIndex, uint64_t stepDuration, bool applyProbability, bool transportPlayback) {
  uint16_t gatePercent = sequencerStepGatePercent[stepIndex];
  byte noteCount = sequencerStepNoteCount[stepIndex];
  byte stepVelocity = sequencerStepVelocity[stepIndex];
  byte stepProbability = sequencerStepProbability[stepIndex];
  if (noteCount == 0 || gatePercent == 0) {
    return;
  }
  if (applyProbability) {
    if (stepProbability == 0) {
      return;
    }
    if (stepProbability < 100 && static_cast<byte>(random(100)) >= stepProbability) {
      return;
    }
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
    clearSequencerPlaybackGroup(sequencerPlaybackGroups[freeGroupIndex], true);
  }

  SequencerPlaybackGroup& group = sequencerPlaybackGroups[freeGroupIndex];
  group.active = true;
  group.noteCount = 0;
  group.sourceStep = transportPlayback ? static_cast<int8_t>(stepIndex) : -1;
  group.transportPlayback = transportPlayback;
  uint64_t playbackStartedAt = runTime;
  if (transportPlayback && sequencerCurrentStepStartedAt > 0) {
    // Transport gate timing follows the step boundary rather than the service-loop
    // timestamp so 100% notes release on-grid and same-pitch retriggers do not
    // drift into each other on monosynths.
    playbackStartedAt = sequencerCurrentStepStartedAt;
  }
  group.externalClock = SequencerPlaybackExternalClockState{};
  group.externalClock.gatePercent = gatePercent;
  group.externalClock.startedAt = playbackStartedAt;
  group.noteOffAt = playbackStartedAt + ((stepDuration * gatePercent) / 100ULL);
  if (transportPlayback) {
    byte activeStepCount = sequencerActiveStepCount();
    bool nextStepContinuesSource = doesNextSequencerStepContinueSource(stepIndex, activeStepCount);
    if (sequencerUsesExternalClock() && nextStepContinuesSource) {
      // External ties follow real step boundaries rather than predicted
      // durations so jitter cannot release the note before the next tie step.
      group.externalClock.pendingTieBoundary = true;
      group.noteOffAt = 0;
    } else if (sequencerUsesExternalClock() && sequencerExternalClock.stepDuration == 0) {
      // The first external transport step starts before we know the real step
      // length, so defer its initial gate timing until the first measured
      // boundary arrives.
      group.externalClock.pendingGateSync = true;
      group.noteOffAt = 0;
    } else if (nextStepContinuesSource &&
               group.noteOffAt < (playbackStartedAt + stepDuration)) {
      // Keep the source note alive until the next tied step can extend it,
      // otherwise short gates would end before Tie had a chance to continue it.
      group.noteOffAt = playbackStartedAt + stepDuration;
    }
  }
  for (byte noteIndex = 0; noteIndex < SEQUENCER_MAX_NOTES_PER_STEP; noteIndex++) {
    group.pitchSteps[noteIndex] = SEQUENCER_NO_PITCH;
  }

  for (byte noteIndex = 0; noteIndex < noteCount && noteIndex < SEQUENCER_MAX_NOTES_PER_STEP; noteIndex++) {
    int16_t pitchSteps = sequencerStepPitchSteps[stepIndex][noteIndex];
    if (pitchSteps == SEQUENCER_NO_PITCH) {
      continue;
    }
    sendSequencerManagedNoteOn(pitchSteps, true, stepVelocity);
    group.pitchSteps[group.noteCount++] = pitchSteps;
  }
}

void previewSequencerStep(byte stepIndex) {
  if (stepIndex >= SEQUENCER_STEP_COUNT || sequencerStepTie[stepIndex]) {
    return;
  }
  startSequencerPlaybackGroup(stepIndex, sequencerStepDurationMicros(), false, false);
}

void clearSelectedSequencerStep() {
  if (sequencerSelectedStep < 0) {
    return;
  }
  clearSequencerNoteBuffer(sequencerEditPitchSteps, sequencerEditNoteCount);
  saveEditBufferToStep(static_cast<byte>(sequencerSelectedStep));
  sequencerStepTie[sequencerSelectedStep] = false;
  setSequencerDirtyState(true);
  sequencerOverlayMode = SequencerOverlayMode::StepCleared;
  sequencerOverlayDirty = true;
  sequencerConfirmHeld = false;
  sequencerConfirmPressedAt = 0;
}

void resetSequencerState() {
  byte preservedTapPreview = sequencerTapPreview;
  byte preservedClockSource = sequencerClockSource;
  byte preservedSendClock = sequencerSendClock;
  byte preservedSendTransport = sequencerSendTransport;
  stopSequencerAuditionNotes();
  stopSequencerPlaybackNote();

  for (byte step = 0; step < SEQUENCER_STEP_COUNT; step++) {
    clearSequencerNoteBuffer(sequencerStepPitchSteps[step], sequencerStepNoteCount[step]);
    sequencerStepGatePercent[step] = 100;
    sequencerStepVelocity[step] = SEQUENCER_DEFAULT_VELOCITY;
    sequencerStepProbability[step] = SEQUENCER_DEFAULT_PROBABILITY;
    sequencerStepTie[step] = false;
  }
  for (byte heldIndex = 0; heldIndex < SEQUENCER_MAX_MANAGED_HELD_NOTES; heldIndex++) {
    sequencerManagedHeldNotes[heldIndex] = SequencerManagedHeldNote{};
  }
  for (byte groupIndex = 0; groupIndex < SEQUENCER_MAX_ACTIVE_PLAYBACK_GROUPS; groupIndex++) {
    sequencerPlaybackGroups[groupIndex] = SequencerPlaybackGroup{};
  }
  clearSequencerNoteBuffer(sequencerEditPitchSteps, sequencerEditNoteCount);
  clearSequencerNoteBuffer(sequencerUndoPitchSteps, sequencerUndoNoteCount);
  sequencerUndoGatePercent = 100;
  sequencerUndoVelocity = SEQUENCER_DEFAULT_VELOCITY;
  sequencerUndoProbability = SEQUENCER_DEFAULT_PROBABILITY;
  sequencerUndoTie = false;
  sequencerSelectedStep = -1;
  sequencerCopySourceStep = -1;
  sequencerPlayingStep = -1;
  sequencerStepPlayCount = SEQUENCER_STEP_COUNT;
  sequencerTapPreview = preservedTapPreview;
  sequencerPlayType = SEQUENCER_PLAY_TYPE_MIDI;
  sequencerClockSource = preservedClockSource;
  sequencerSendClock = preservedSendClock;
  sequencerSendTransport = preservedSendTransport;
  sequencerDirection = SEQUENCER_DIRECTION_FORWARD;
  sequencerPingPongDelta = 1;
  sequencerTempo = 120;
  sequencerConfirmHeld = false;
  sequencerConfirmPressedAt = 0;
  sequencerNextStepAt = 0;
  sequencerCurrentStepStartedAt = 0;
  sequencerOverviewStartStep = 0;
  sequencerLengthPercentDisplay = 100;
  sequencerExactLengthOriginal = 100;
  sequencerExactLengthBuffer[0] = '\0';
  sequencerExactLengthLength = 0;
  sequencerVelocityDisplay = SEQUENCER_DEFAULT_VELOCITY;
  sequencerExactVelocityOriginal = SEQUENCER_DEFAULT_VELOCITY;
  sequencerProbabilityDisplay = SEQUENCER_DEFAULT_PROBABILITY;
  sequencerExactProbabilityOriginal = SEQUENCER_DEFAULT_PROBABILITY;
  resetSequencerClockSyncState();
  hideSequencerOverlay();
}

void parseSequencerStepNotes(byte stepIndex, const String& value, bool valuesArePitchSteps) {
  clearSequencerNoteBuffer(sequencerStepPitchSteps[stepIndex], sequencerStepNoteCount[stepIndex]);
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
      if (valuesArePitchSteps) {
        if (noteValue >= -32768 && noteValue <= 32767) {
          sequencerStepPitchSteps[stepIndex][sequencerStepNoteCount[stepIndex]++] = static_cast<int16_t>(noteValue);
        }
      } else if (noteValue >= 0 && noteValue < 128) {
        sequencerStepPitchSteps[stepIndex][sequencerStepNoteCount[stepIndex]++] =
          static_cast<int16_t>(noteValue - 60 - getSequencerCurrentTranspose());
      }
    }
    if (commaIndex < 0) {
      break;
    }
    start = commaIndex + 1;
  }
  sortSequencerNoteBuffer(sequencerStepPitchSteps[stepIndex], sequencerStepNoteCount[stepIndex]);
}

bool loadSequencerFromFlash() {
  resetSequencerState();
  if (!fileSystemExists) {
    setSequencerDirtyState(false);
    return false;
  }

  File f = LittleFS.open(SEQUENCER_LEGACY_STORAGE_PATH, "r");
  if (!f) {
    setSequencerDirtyState(false);
    return false;
  }

  bool sawFormat = false;
  uint16_t fileVersion = 1;
  bool noteFormatIsPitchSteps = false;
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
    } else if (key == "version") {
      int parsedVersion = value.toInt();
      if (parsedVersion >= 1) {
        fileVersion = static_cast<uint16_t>(parsedVersion);
      }
    } else if (key == "noteFormat") {
      noteFormatIsPitchSteps = (value == "stepsFromC");
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
        parseSequencerStepNotes(
          static_cast<byte>(stepNumber - 1),
          value,
          noteFormatIsPitchSteps || fileVersion >= 2);
      }
    } else if (key.startsWith("gate")) {
      int stepNumber = key.substring(4).toInt();
      int gateValue = value.toInt();
      if (stepNumber >= 1 && stepNumber <= SEQUENCER_STEP_COUNT && gateValue >= 0 && gateValue <= 1000) {
        sequencerStepGatePercent[stepNumber - 1] = static_cast<uint16_t>(gateValue);
      }
    } else if (key.startsWith("vel")) {
      int stepNumber = key.substring(3).toInt();
      int velocityValue = value.toInt();
      if (stepNumber >= 1 && stepNumber <= SEQUENCER_STEP_COUNT && velocityValue >= 0 && velocityValue <= 127) {
        sequencerStepVelocity[stepNumber - 1] = static_cast<byte>(velocityValue);
      }
    } else if (key.startsWith("prob")) {
      int stepNumber = key.substring(4).toInt();
      int probabilityValue = value.toInt();
      if (stepNumber >= 1 && stepNumber <= SEQUENCER_STEP_COUNT && probabilityValue >= 0 && probabilityValue <= 100) {
        sequencerStepProbability[stepNumber - 1] = static_cast<byte>(probabilityValue);
      }
    } else if (key.startsWith("tie")) {
      int stepNumber = key.substring(3).toInt();
      int tieValue = value.toInt();
      if (stepNumber >= 1 && stepNumber <= SEQUENCER_STEP_COUNT) {
        sequencerStepTie[stepNumber - 1] = (tieValue != 0);
      }
    }
  }

  f.close();
  resetSequencerClockSyncState();
  setSequencerDirtyState(false);
  return sawFormat;
}

bool loadSequencerFromPath(const char* path) {
  resetSequencerState();
  if (!fileSystemExists || path == nullptr || path[0] == '\0') {
    setSequencerDirtyState(false);
    return false;
  }

  File f = LittleFS.open(path, "r");
  if (!f) {
    setSequencerDirtyState(false);
    return false;
  }

  bool sawFormat = false;
  uint16_t fileVersion = 1;
  bool noteFormatIsPitchSteps = false;
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
    } else if (key == "version") {
      int parsedVersion = value.toInt();
      if (parsedVersion >= 1) {
        fileVersion = static_cast<uint16_t>(parsedVersion);
      }
    } else if (key == "noteFormat") {
      noteFormatIsPitchSteps = (value == "stepsFromC");
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
        parseSequencerStepNotes(
          static_cast<byte>(stepNumber - 1),
          value,
          noteFormatIsPitchSteps || fileVersion >= 2);
      }
    } else if (key.startsWith("gate")) {
      int stepNumber = key.substring(4).toInt();
      int gateValue = value.toInt();
      if (stepNumber >= 1 && stepNumber <= SEQUENCER_STEP_COUNT && gateValue >= 0 && gateValue <= 1000) {
        sequencerStepGatePercent[stepNumber - 1] = static_cast<uint16_t>(gateValue);
      }
    } else if (key.startsWith("vel")) {
      int stepNumber = key.substring(3).toInt();
      int velocityValue = value.toInt();
      if (stepNumber >= 1 && stepNumber <= SEQUENCER_STEP_COUNT && velocityValue >= 0 && velocityValue <= 127) {
        sequencerStepVelocity[stepNumber - 1] = static_cast<byte>(velocityValue);
      }
    } else if (key.startsWith("prob")) {
      int stepNumber = key.substring(4).toInt();
      int probabilityValue = value.toInt();
      if (stepNumber >= 1 && stepNumber <= SEQUENCER_STEP_COUNT && probabilityValue >= 0 && probabilityValue <= 100) {
        sequencerStepProbability[stepNumber - 1] = static_cast<byte>(probabilityValue);
      }
    } else if (key.startsWith("tie")) {
      int stepNumber = key.substring(3).toInt();
      int tieValue = value.toInt();
      if (stepNumber >= 1 && stepNumber <= SEQUENCER_STEP_COUNT) {
        sequencerStepTie[stepNumber - 1] = (tieValue != 0);
      }
    }
  }

  f.close();
  resetSequencerClockSyncState();
  setSequencerDirtyState(false);
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
  f.println("version=3");
  f.println("noteFormat=stepsFromC");
  f.print("tempo=");
  f.println(sequencerTempo);
  f.print("steps=");
  f.println(sequencerStepPlayCount);
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
      f.print(sequencerStepPitchSteps[step][noteIndex]);
    }
    f.println();
    f.print("gate");
    f.print(step + 1);
    f.print('=');
    f.println(sequencerStepGatePercent[step]);
    f.print("vel");
    f.print(step + 1);
    f.print('=');
    f.println(sequencerStepVelocity[step]);
    f.print("prob");
    f.print(step + 1);
    f.print('=');
    f.println(sequencerStepProbability[step]);
    f.print("tie");
    f.print(step + 1);
    f.print('=');
    f.println(sequencerStepTie[step] ? 1 : 0);
  }

  f.close();
  if (LittleFS.exists(path)) {
    LittleFS.remove(path);
  }
  if (!LittleFS.rename(tempPath, path)) {
    LittleFS.remove(tempPath);
    return false;
  }
  setSequencerDirtyState(false);
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
    setSequencerDirtyState(false);
    return false;
  }

  if (loadRememberedSequencerCurrentPath() && sequencerCurrentSequencePath[0] != '\0') {
    if (loadSequencerFromPath(sequencerCurrentSequencePath)) {
      return true;
    }
    clearSequencerCurrentPath();
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

bool guardSequencerStorageForUsbBackup(const char* actionLineTwo) {
  if (!isUsbBackupActive()) {
    return true;
  }
  showSequencerStatusMessage("USB Backup", actionLineTwo);
  return false;
}

void refreshSequencerUsbBackupMenu(bool redrawMenu) {
  if (isUsbBackupActive()) {
    char statusLineOne[SEQUENCER_BROWSER_TITLE_LENGTH];
    char statusLineTwo[SEQUENCER_BROWSER_TITLE_LENGTH];
    getUsbBackupStatusLines(statusLineOne, sizeof(statusLineOne), statusLineTwo, sizeof(statusLineTwo));
    snprintf(sequencerUsbBackupStatusLineOne, sizeof(sequencerUsbBackupStatusLineOne), "%s", statusLineOne);
    snprintf(sequencerUsbBackupStatusLineTwo, sizeof(sequencerUsbBackupStatusLineTwo), "%s", statusLineTwo);
    menuItemSequencerUsbBackupStart.hide();
    menuItemSequencerUsbBackupStop.show();
  } else {
    copySequencerString(sequencerUsbBackupStatusLineOne, sizeof(sequencerUsbBackupStatusLineOne), "USB Backup Off");
    copySequencerString(sequencerUsbBackupStatusLineTwo, sizeof(sequencerUsbBackupStatusLineTwo), "Host tool idle");
    menuItemSequencerUsbBackupStart.show();
    menuItemSequencerUsbBackupStop.hide();
  }

  menuItemSequencerUsbBackupStatusOne.setTitle(sequencerUsbBackupStatusLineOne);
  menuItemSequencerUsbBackupStatusTwo.setTitle(sequencerUsbBackupStatusLineTwo);

  if (redrawMenu) {
    menu.drawMenu();
  }
}

void usbBackupStatusMenuCallback() {
}

void startUsbBackupMenuCallback() {
  if (enterUsbBackupMode()) {
    setSequencerTransportState(SEQUENCER_TRANSPORT_STOP);
    stopSequencerAuditionNotes();
    showSequencerStatusMessage("USB Backup", "Run host tool");
  } else {
    showSequencerStatusMessage("USB Backup", "FS unavailable");
  }
  refreshSequencerUsbBackupMenu(true);
}

void stopUsbBackupMenuCallback() {
  if (!isUsbBackupActive()) {
    exitUsbBackupMode();
    showSequencerStatusMessage("USB Backup", "Session closed");
    refreshSequencerUsbBackupMenu(true);
    return;
  }

  menu.setMenuPageCurrent(menuPageSequencerUsbBackupStopConfirm);
  menu.drawMenu();
  sequencerLastMenuPage = &menuPageSequencerUsbBackupStopConfirm;
}

void usbBackupExitPromptMenuCallback() {
}

void usbBackupStopPromptMenuCallback() {
}

void confirmUsbBackupExitMenuCallback() {
  exitUsbBackupMode();
  menu.setMenuPageCurrent(menuPageSequencerFiles);
  refreshSequencerUsbBackupMenu(false);
  menu.drawMenu();
  showSequencerStatusMessage("USB Backup", "Session closed");
  sequencerLastMenuPage = &menuPageSequencerFiles;
}

void cancelUsbBackupExitMenuCallback() {
  menu.setMenuPageCurrent(menuPageSequencerUsbBackup);
  refreshSequencerUsbBackupMenu(false);
  menu.drawMenu();
  showSequencerStatusMessage("USB Backup", "Session active");
  sequencerLastMenuPage = &menuPageSequencerUsbBackup;
}

void confirmUsbBackupStopMenuCallback() {
  exitUsbBackupMode();
  menu.setMenuPageCurrent(menuPageSequencerUsbBackup);
  refreshSequencerUsbBackupMenu(false);
  menu.drawMenu();
  showSequencerStatusMessage("USB Backup", "Session closed");
  sequencerLastMenuPage = &menuPageSequencerUsbBackup;
}

void cancelUsbBackupStopMenuCallback() {
  menu.setMenuPageCurrent(menuPageSequencerUsbBackup);
  refreshSequencerUsbBackupMenu(false);
  menu.drawMenu();
  showSequencerStatusMessage("USB Backup", "Session active");
  sequencerLastMenuPage = &menuPageSequencerUsbBackup;
}

void guardUsbBackupMenuExit() {
  GEMPage* currentMenuPage = menu.getCurrentMenuPage();
  if (sequencerLastMenuPage == &menuPageSequencerUsbBackup &&
      currentMenuPage != &menuPageSequencerUsbBackup &&
      currentMenuPage != &menuPageSequencerUsbBackupExitConfirm &&
      currentMenuPage != &menuPageSequencerUsbBackupStopConfirm) {
    if (isUsbBackupActive()) {
      menu.setMenuPageCurrent(menuPageSequencerUsbBackupExitConfirm);
      menu.drawMenu();
      sequencerLastMenuPage = &menuPageSequencerUsbBackupExitConfirm;
      return;
    }
  }
  sequencerLastMenuPage = currentMenuPage;
}

void saveSequencerMenuCallback() {
  if (!guardSequencerStorageForUsbBackup("Stop session first")) {
    return;
  }
  if (sequencerCurrentSequencePath[0] == '\0') {
    openSequencerSaveNewBrowser();
    return;
  }
  if (saveSequencerToCurrentPath()) {
    showSequencerPathStatusMessage("Saved", sequencerCurrentSequencePath);
  } else {
    showSequencerStatusMessage("Error Saving", "Flash write failed");
  }
}

void newSequencerMenuCallback() {
  if (!guardSequencerStorageForUsbBackup("Stop session first")) {
    return;
  }
  resetSequencerState();
  clearSequencerCurrentPath();
  setSequencerDirtyState(false);
  showSequencerStatusMessage("New", "Blank sequence");
}

void revertSequencerMenuCallback() {
  if (!guardSequencerStorageForUsbBackup("Stop session first")) {
    return;
  }
  if (sequencerCurrentSequencePath[0] != '\0') {
    if (loadSequencerFromPath(sequencerCurrentSequencePath)) {
      showSequencerPathStatusMessage("Prev save loaded:", sequencerCurrentSequencePath);
    } else {
      // Keep the failing file name visible so Revert explains which save could not be restored.
      showSequencerPathStatusMessage("Error loading:", sequencerCurrentSequencePath);
    }
  } else if (LittleFS.exists(SEQUENCER_LEGACY_STORAGE_PATH) && loadSequencerFromFlash()) {
    showSequencerStatusMessage("Reverted", "Legacy sequence");
  } else {
    resetSequencerState();
    setSequencerDirtyState(false);
    showSequencerStatusMessage("Reverted", "Blank sequence");
  }
}

void openSequencerBrowser(SequencerBrowserMode browserMode) {
  if (!guardSequencerStorageForUsbBackup("Stop session first")) {
    return;
  }
  sequencerDeleteTargetPath[0] = '\0';
  sequencerDeleteTargetIsFolder = false;
  clearSequencerTargetActionState();
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

void openSequencerRenameOrDeleteBrowser() {
  openSequencerBrowser(SequencerBrowserMode::RenameOrDelete);
}

void openSequencerCreateFolderBrowser() {
  openSequencerBrowser(SequencerBrowserMode::CreateFolder);
}

void sequencerBrowserUpCallback() {
  extractSequencerParentPath(sequencerBrowserPath, sequencerBrowserPath, sizeof(sequencerBrowserPath));
  sequencerBrowserOffset = 0;
  refreshSequencerBrowserMenu();
}

int getVisibleBrowserEntryMenuIndex(bool firstVisible);

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

void setSequencerBrowserSelectionToBoundaryEntry(bool firstVisible) {
  int boundaryIndex = getVisibleBrowserEntryMenuIndex(firstVisible);
  if (boundaryIndex < 0) {
    return;
  }
  menuPageSequencerBrowser.setCurrentMenuItemIndex(static_cast<byte>(boundaryIndex));
  menu.drawMenu();
}

void sequencerBrowserMoreAboveCallback() {
  sequencerBrowserPrevPageCallback();
  setSequencerBrowserSelectionToBoundaryEntry(false);
}

void sequencerBrowserMoreBelowCallback() {
  sequencerBrowserNextPageCallback();
  setSequencerBrowserSelectionToBoundaryEntry(true);
}

void sequencerBrowserIndicatorCallback() {
}

void clearSequencerTargetActionState() {
  sequencerTargetActionPath[0] = '\0';
  sequencerTargetActionIsFolder = false;
}

void refreshSequencerTargetActionMenu() {
  char displayName[SEQUENCER_BROWSER_TITLE_LENGTH];
  extractSequencerDisplayName(sequencerTargetActionPath, displayName, sizeof(displayName));
  if (displayName[0] == '\0') {
    copySequencerString(displayName, sizeof(displayName), sequencerTargetActionIsFolder ? "Folder" : "File");
  }

  copySequencerString(
    sequencerTargetActionPromptLineOne,
    sizeof(sequencerTargetActionPromptLineOne),
    sequencerTargetActionIsFolder ? "Folder:" : "File:");
  copySequencerString(sequencerTargetActionPromptLineTwo, sizeof(sequencerTargetActionPromptLineTwo), displayName);
  copySequencerString(sequencerTargetActionPromptLineThree, sizeof(sequencerTargetActionPromptLineThree), "");
  copySequencerString(sequencerTargetActionPromptLineFour, sizeof(sequencerTargetActionPromptLineFour), "Choose action.");

  menuItemSequencerTargetActionPromptOne.setTitle(sequencerTargetActionPromptLineOne);
  menuItemSequencerTargetActionPromptTwo.setTitle(sequencerTargetActionPromptLineTwo);
  menuItemSequencerTargetActionPromptThree.setTitle(sequencerTargetActionPromptLineThree);
  menuItemSequencerTargetActionPromptFour.setTitle(sequencerTargetActionPromptLineFour);
  menuPageSequencerTargetAction.setTitle(sequencerTargetActionIsFolder ? "Folder Action?" : "File Action?");
}

void openSequencerTargetActionMenu(const char* targetPath, bool isFolder) {
  copySequencerString(sequencerTargetActionPath, sizeof(sequencerTargetActionPath), targetPath);
  sequencerTargetActionIsFolder = isFolder;
  refreshSequencerTargetActionMenu();
  menu.setMenuPageCurrent(menuPageSequencerTargetAction);
  menuPageSequencerTargetAction.setCurrentMenuItemIndex(4);
  menu.drawMenu();
}

void sequencerTargetActionPromptMenuCallback() {
}

// The combined browser deliberately reuses the shared rename/delete flows so
// File Management only needs one board-side entry point for those actions.
void sequencerTargetActionRenameCallback() {
  if (sequencerTargetActionPath[0] == '\0') {
    sequencerTargetActionCancelCallback();
    return;
  }

  char targetPath[SEQUENCER_MAX_PATH_LENGTH];
  copySequencerString(targetPath, sizeof(targetPath), sequencerTargetActionPath);
  bool targetIsFolder = sequencerTargetActionIsFolder;
  clearSequencerTargetActionState();
  startSequencerRename(targetIsFolder ? SequencerNamingTarget::RenameFolder : SequencerNamingTarget::RenameSequence, targetPath);
}

void sequencerTargetActionDeleteCallback() {
  if (sequencerTargetActionPath[0] == '\0') {
    sequencerTargetActionCancelCallback();
    return;
  }

  char targetPath[SEQUENCER_MAX_PATH_LENGTH];
  copySequencerString(targetPath, sizeof(targetPath), sequencerTargetActionPath);
  bool targetIsFolder = sequencerTargetActionIsFolder;
  clearSequencerTargetActionState();
  openSequencerDeleteConfirm(targetPath, targetIsFolder);
}

void sequencerTargetActionCancelCallback() {
  clearSequencerTargetActionState();
  menu.setMenuPageCurrent(menuPageSequencerBrowser);
  menu.drawMenu();
}

void refreshSequencerDeleteConfirmMenu() {
  char displayName[SEQUENCER_BROWSER_TITLE_LENGTH];
  extractSequencerDisplayName(sequencerDeleteTargetPath, displayName, sizeof(displayName));
  if (displayName[0] == '\0') {
    copySequencerString(displayName, sizeof(displayName), sequencerDeleteTargetIsFolder ? "Folder" : "File");
  }

  copySequencerString(
    sequencerDeletePromptLineOne,
    sizeof(sequencerDeletePromptLineOne),
    sequencerDeleteTargetIsFolder ? "Delete folder:" : "Delete file:");
  copySequencerString(
    sequencerDeletePromptLineTwo,
    sizeof(sequencerDeletePromptLineTwo),
    displayName);
  copySequencerString(
    sequencerDeletePromptLineThree,
    sizeof(sequencerDeletePromptLineThree),
    "This cannot be");
  copySequencerString(
    sequencerDeletePromptLineFour,
    sizeof(sequencerDeletePromptLineFour),
    "undone.");

  menuItemSequencerDeletePromptOne.setTitle(sequencerDeletePromptLineOne);
  menuItemSequencerDeletePromptTwo.setTitle(sequencerDeletePromptLineTwo);
  menuItemSequencerDeletePromptThree.setTitle(sequencerDeletePromptLineThree);
  menuItemSequencerDeletePromptFour.setTitle(sequencerDeletePromptLineFour);
  menuPageSequencerDeleteConfirm.setTitle(sequencerDeleteTargetIsFolder ? "Delete Folder?" : "Delete File?");
}

void openSequencerDeleteConfirm(const char* targetPath, bool isFolder) {
  copySequencerString(sequencerDeleteTargetPath, sizeof(sequencerDeleteTargetPath), targetPath);
  sequencerDeleteTargetIsFolder = isFolder;
  refreshSequencerDeleteConfirmMenu();
  menu.setMenuPageCurrent(menuPageSequencerDeleteConfirm);
  menuPageSequencerDeleteConfirm.setCurrentMenuItemIndex(4);
  menu.drawMenu();
}

void sequencerDeletePromptMenuCallback() {
}

void confirmSequencerDeleteMenuCallback() {
  if (sequencerDeleteTargetPath[0] == '\0') {
    cancelSequencerDeleteMenuCallback();
    return;
  }

  char deletedPath[SEQUENCER_MAX_PATH_LENGTH];
  copySequencerString(deletedPath, sizeof(deletedPath), sequencerDeleteTargetPath);
  bool deletedFolder = sequencerDeleteTargetIsFolder;
  sequencerDeleteTargetPath[0] = '\0';
  sequencerDeleteTargetIsFolder = false;

  if (deletedFolder) {
    char parentPath[SEQUENCER_MAX_PATH_LENGTH];
    extractSequencerParentPath(deletedPath, parentPath, sizeof(parentPath));
    clearSequencerCurrentPathIfDeleted(deletedPath, true);
    if (deleteSequencerFolderRecursive(deletedPath)) {
      copySequencerString(sequencerBrowserPath, sizeof(sequencerBrowserPath), parentPath);
      sequencerBrowserOffset = 0;
      refreshSequencerBrowserMenu();
      showSequencerPathStatusMessage("Deleted", deletedPath);
      return;
    }
    menu.setMenuPageCurrent(menuPageSequencerBrowser);
    menu.drawMenu();
    showSequencerStatusMessage("Error Delete", "Folder failed");
    return;
  }

  clearSequencerCurrentPathIfDeleted(deletedPath, false);
  if (LittleFS.remove(deletedPath)) {
    refreshSequencerBrowserMenu(false);
    showSequencerPathStatusMessage("Deleted", deletedPath);
  } else {
    menu.setMenuPageCurrent(menuPageSequencerBrowser);
    menu.drawMenu();
    showSequencerStatusMessage("Error Delete", "File failed");
  }
}

void cancelSequencerDeleteMenuCallback() {
  sequencerDeleteTargetPath[0] = '\0';
  sequencerDeleteTargetIsFolder = false;
  menu.setMenuPageCurrent(menuPageSequencerBrowser);
  menu.drawMenu();
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
  } else if (sequencerBrowserMode == SequencerBrowserMode::RenameOrDelete) {
    openSequencerTargetActionMenu(entry.path, false);
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

void sequencerBrowserThisFolderCallback() {
  if (sequencerBrowserMode != SequencerBrowserMode::RenameOrDelete || sequencerPathIsRoot(sequencerBrowserPath)) {
    return;
  }
  openSequencerTargetActionMenu(sequencerBrowserPath, true);
}

bool commitSequencerNaming() {
  if (!isSequencerNamingActive()) {
    return false;
  }
  if (sequencerNamingLength == 0) {
    // Show the validation error before returning to the naming overlay.
    showSequencerStatusMessageAndReturn("Name Empty", "Enter a name", SequencerOverlayMode::Naming);
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
      showSequencerStatusMessageAndReturn("Name Exists", "Pick another", SequencerOverlayMode::Naming);
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
      showSequencerStatusMessageAndReturn("Error Rename", "File failed", SequencerOverlayMode::Naming);
      return true;
    } else if (saveSequencerToPath(targetPath)) {
      setSequencerCurrentPath(targetPath);
      setSequencerDirtyState(false);
      sequencerNamingTarget = SequencerNamingTarget::None;
      menu.setMenuPageCurrent(menuPageSequencer);
      menu.drawMenu();
      showSequencerPathStatusMessage("Saved New", targetPath);
      return true;
    }

    showSequencerStatusMessageAndReturn("Error Saving", "Save New failed", SequencerOverlayMode::Naming);
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
    showSequencerStatusMessageAndReturn("Name Exists", "Pick another", SequencerOverlayMode::Naming);
    return true;
  }
  if (sequencerNamingTarget == SequencerNamingTarget::RenameFolder) {
    if (!LittleFS.rename(sequencerRenameSourcePath, targetPath)) {
      showSequencerStatusMessageAndReturn("Error Rename", "Folder failed", SequencerOverlayMode::Naming);
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
    showSequencerStatusMessageAndReturn("Error Folder", "Create failed", SequencerOverlayMode::Naming);
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
  sequencerOverlayReturnMode = SequencerOverlayMode::Hidden;
  sequencerOverlayUntil = runTime + SEQUENCER_NOTE_CONFIRM_MICROS;
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = true;
}

void showSequencerStatusMessageAndReturn(const char* lineOne, const char* lineTwo, SequencerOverlayMode returnMode) {
  showSequencerStatusMessage(lineOne, lineTwo);
  sequencerOverlayReturnMode = returnMode;
}

void showSequencerPersistentStatusMessage(const char* lineOne, const char* lineTwo) {
  snprintf(sequencerStatusLineOne, sizeof(sequencerStatusLineOne), "%s", lineOne);
  snprintf(sequencerStatusLineTwo, sizeof(sequencerStatusLineTwo), "%s", lineTwo);
  sequencerOverlayMode = SequencerOverlayMode::StatusMessage;
  sequencerOverlayReturnMode = SequencerOverlayMode::Hidden;
  sequencerOverlayUntil = static_cast<uint64_t>(-1);
  sequencerOverlayVisible = false;
  sequencerOverlayDirty = true;
}

void setSequencerTransportState(byte newState, bool redrawMenu, bool sendMidi) {
  byte normalizedState = (newState == SEQUENCER_TRANSPORT_PLAY) ? SEQUENCER_TRANSPORT_PLAY : SEQUENCER_TRANSPORT_STOP;
  sequencerTransportState = normalizedState;
  resetSequencerClockSyncState();
  if (sequencerTransportState == SEQUENCER_TRANSPORT_PLAY) {
    sequencerPlayingStep = -1;
    sequencerPingPongDelta = 1;
    sequencerNextStepAt = runTime;
    sequencerCurrentStepStartedAt = runTime;
    if (sequencerShouldSendMidiTransport() && sendMidi) {
      sendSequencerMidiTransportStart();
    }
    if (sequencerShouldSendMidiClock()) {
      sequencerNextMidiClockAt = runTime;
    }
  } else {
    stopSequencerPlaybackNote();
    sequencerPlayingStep = -1;
    sequencerNextStepAt = 0;
    sequencerCurrentStepStartedAt = 0;
    if (sequencerShouldSendMidiTransport() && sendMidi) {
      sendSequencerMidiTransportStop();
    }
  }
  if (redrawMenu) {
    menu.drawMenu();
  }
}

void sequencerTempoMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  setSequencerDirtyState(true);
}

void sequencerStepPlayCountMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  if (sequencerStepPlayCount < 1) {
    sequencerStepPlayCount = 1;
  } else if (sequencerStepPlayCount > SEQUENCER_STEP_COUNT) {
    sequencerStepPlayCount = SEQUENCER_STEP_COUNT;
  }
  setSequencerDirtyState(true);
}

void sequencerTapPreviewMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  persistSequencerGeneralSettingsToProfile();
}

void sequencerPlayTypeMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  setSequencerDirtyState(true);
}

void sequencerClockSourceMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  resetSequencerClockSyncState();
  if (sequencerTransportState == SEQUENCER_TRANSPORT_PLAY && !sequencerUsesExternalClock()) {
    sequencerNextStepAt = runTime;
    sequencerCurrentStepStartedAt = runTime;
    if (sequencerShouldSendMidiClock()) {
      sequencerNextMidiClockAt = runTime;
    }
  }
  persistSequencerGeneralSettingsToProfile();
}

void sequencerSendClockMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  persistSequencerGeneralSettingsToProfile();
}

void sequencerSendTransportMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  persistSequencerGeneralSettingsToProfile();
}

// Hide Seq Lights options that are not relevant for the current mode/state.
void refreshSequencerLightsMenu(bool redrawMenu) {
  menuItemSequencerStepHue.hide(sequencerStepColorMode != SEQUENCER_STEP_COLOR_REGULAR);
  if (redrawMenu && menu.getCurrentMenuPage() == &menuPageSequencerLights) {
    menu.drawMenu();
  }
}

void openSequencerLightsMenu() {
  refreshSequencerLightsMenu(false);
  menu.setMenuPageCurrent(menuPageSequencerLights);
  menu.drawMenu();
}

void sequencerStepAccentEveryMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  if (sequencerStepAccentEvery == 1 || sequencerStepAccentEvery > 8) {
    sequencerStepAccentEvery = SEQUENCER_STEP_ACCENT_OFF;
  }
  persistSequencerGeneralSettingsToProfile();
  refreshSequencerLightsMenu(true);
}

void sequencerStepColorModeMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  if (sequencerStepColorMode != SEQUENCER_STEP_COLOR_REGULAR) {
    sequencerStepColorMode = SEQUENCER_STEP_COLOR_NOTE;
  }
  persistSequencerGeneralSettingsToProfile();
  refreshSequencerLightsMenu(true);
}

void sequencerStepHueMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  if (sequencerStepHue > SEQUENCER_STEP_HUE_PINK) {
    sequencerStepHue = SEQUENCER_STEP_HUE_INDIGO;
  }
  persistSequencerGeneralSettingsToProfile();
}

// Preview Step Hue live while scrolling so the Sequencer LEDs update before selection is confirmed.
void previewSequencerStepHue(GEMPreviewCallbackData previewData) {
  sequencerStepHue = (previewData.previewValByte <= SEQUENCER_STEP_HUE_PINK) ? previewData.previewValByte : SEQUENCER_STEP_HUE_INDIGO;
}

void sequencerDirectionMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  sequencerPingPongDelta = 1;
  setSequencerDirtyState(true);
}

const GEMSpinnerBoundariesByte spinnerBoundariesSequencerStepPlayCount = { 1, 1, SEQUENCER_STEP_COUNT };
const GEMSpinnerBoundariesByte spinnerBoundariesSequencerTempo = { 1, 1, 255 };

GEMSpinner spinnerSequencerStepPlayCount(spinnerBoundariesSequencerStepPlayCount, GEM_LOOP);
GEMSpinner spinnerSequencerTempo(spinnerBoundariesSequencerTempo, GEM_LOOP);

SelectOptionByte optionByteSequencerTapPreview[] = { { "Off", SEQUENCER_TAP_PREVIEW_OFF }, { "On", SEQUENCER_TAP_PREVIEW_ON } };
GEMSelect selectSequencerTapPreview(sizeof(optionByteSequencerTapPreview) / sizeof(SelectOptionByte), optionByteSequencerTapPreview);
SelectOptionByte optionByteSequencerPlayType[] = { { "MIDI", SEQUENCER_PLAY_TYPE_MIDI }, { "OB Synth", SEQUENCER_PLAY_TYPE_OB_SYNTH } };
GEMSelect selectSequencerPlayType(sizeof(optionByteSequencerPlayType) / sizeof(SelectOptionByte), optionByteSequencerPlayType);
SelectOptionByte optionByteSequencerClockSource[] = {
  { "Internal", SEQUENCER_CLOCK_SOURCE_INTERNAL },
  { "External MIDI", SEQUENCER_CLOCK_SOURCE_EXTERNAL_MIDI }
};
GEMSelect selectSequencerClockSource(sizeof(optionByteSequencerClockSource) / sizeof(SelectOptionByte), optionByteSequencerClockSource);
SelectOptionByte optionByteSequencerSendClock[] = {
  { "Off", SEQUENCER_SEND_CLOCK_OFF },
  { "On", SEQUENCER_SEND_CLOCK_ON }
};
GEMSelect selectSequencerSendClock(sizeof(optionByteSequencerSendClock) / sizeof(SelectOptionByte), optionByteSequencerSendClock);
SelectOptionByte optionByteSequencerSendTransport[] = {
  { "Off", SEQUENCER_SEND_TRANSPORT_OFF },
  { "On", SEQUENCER_SEND_TRANSPORT_ON }
};
GEMSelect selectSequencerSendTransport(sizeof(optionByteSequencerSendTransport) / sizeof(SelectOptionByte), optionByteSequencerSendTransport);
SelectOptionByte optionByteSequencerStepAccentEvery[] = {
  { "Off", SEQUENCER_STEP_ACCENT_OFF },
  { " 2", 2 },
  { " 3", 3 },
  { " 4", 4 },
  { " 5", 5 },
  { " 6", 6 },
  { " 7", 7 },
  { " 8", 8 }
};
GEMSelect selectSequencerStepAccentEvery(sizeof(optionByteSequencerStepAccentEvery) / sizeof(SelectOptionByte), optionByteSequencerStepAccentEvery);
SelectOptionByte optionByteSequencerStepColor[] = {
  { "Note", SEQUENCER_STEP_COLOR_NOTE },
  { "Regular", SEQUENCER_STEP_COLOR_REGULAR }
};
GEMSelect selectSequencerStepColor(sizeof(optionByteSequencerStepColor) / sizeof(SelectOptionByte), optionByteSequencerStepColor);
SelectOptionByte optionByteSequencerStepHue[] = {
  { "Red", SEQUENCER_STEP_HUE_RED },
  { "Orange", SEQUENCER_STEP_HUE_ORANGE },
  { "Yellow", SEQUENCER_STEP_HUE_YELLOW },
  { "Lime", SEQUENCER_STEP_HUE_LIME },
  { "Green", SEQUENCER_STEP_HUE_GREEN },
  { "Teal", SEQUENCER_STEP_HUE_TEAL },
  { "Cyan", SEQUENCER_STEP_HUE_CYAN },
  { "Lt Blue", SEQUENCER_STEP_HUE_LIGHT_BLUE },
  { "Blue", SEQUENCER_STEP_HUE_BLUE },
  { "Indigo", SEQUENCER_STEP_HUE_INDIGO },
  { "Purple", SEQUENCER_STEP_HUE_PURPLE },
  { "Magenta", SEQUENCER_STEP_HUE_MAGENTA },
  { "Pink", SEQUENCER_STEP_HUE_PINK }
};
GEMSelect selectSequencerStepHue(sizeof(optionByteSequencerStepHue) / sizeof(SelectOptionByte), optionByteSequencerStepHue);

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
GEMItem menuGotoSequencerPlayback("Playback Settings", menuPageSequencerPlayback);
GEMItem menuGotoSequencerLights("Seq Lights", openSequencerLightsMenu);
GEMItem menuGotoSequencerMidiSync("MIDI Sync", menuPageSequencerMidiSync);
GEMItem menuGotoSynthFromSequencer("Synth Options", menuPageSynthSequencer);
GEMItem menuGotoColorsFromSequencer("Color Options", menuPageColorsSequencer);
GEMItem menuGotoSequencerFiles("File Management", menuPageSequencerFiles);
GEMItem menuGotoSequencerUsbBackup("USB Backup", menuPageSequencerUsbBackup);
GEMItem menuItemSequencerNew("New", newSequencerMenuCallback);
GEMItem menuItemSequencerSave("Save", saveSequencerMenuCallback);
GEMItem menuItemSequencerSaveNew("Save New", openSequencerSaveNewBrowser);
GEMItem menuItemSequencerLoad("Load", openSequencerLoadBrowser);
GEMItem menuItemSequencerCreateFolder("Create Folder", openSequencerCreateFolderBrowser);
GEMItem menuItemSequencerRenameOrDelete("Rename or Delete", openSequencerRenameOrDeleteBrowser);
GEMItem menuItemSequencerRevert("Revert", revertSequencerMenuCallback);
GEMItem menuItemSequencerStepPlayCount("Steps", sequencerStepPlayCount, spinnerSequencerStepPlayCount, sequencerStepPlayCountMenuCallback);
GEMItem menuItemSequencerTapPreview("Tap Preview", sequencerTapPreview, selectSequencerTapPreview, sequencerTapPreviewMenuCallback);
GEMItem menuItemSequencerPlayType("Play Type", sequencerPlayType, selectSequencerPlayType, sequencerPlayTypeMenuCallback);
GEMItem menuItemSequencerClockSource("Clock Source", sequencerClockSource, selectSequencerClockSource, sequencerClockSourceMenuCallback);
GEMItem menuItemSequencerSendClock("Send Clock", sequencerSendClock, selectSequencerSendClock, sequencerSendClockMenuCallback);
GEMItem menuItemSequencerSendTransport("Send Transport", sequencerSendTransport, selectSequencerSendTransport, sequencerSendTransportMenuCallback);
GEMItem menuItemSequencerStepAccentEvery("Accent Every  ", sequencerStepAccentEvery, selectSequencerStepAccentEvery, sequencerStepAccentEveryMenuCallback);
GEMItem menuItemSequencerStepColor("Step Color", sequencerStepColorMode, selectSequencerStepColor, sequencerStepColorModeMenuCallback);
GEMItem menuItemSequencerStepHue("Step Hue", sequencerStepHue, selectSequencerStepHue, sequencerStepHueMenuCallback);
GEMItem menuItemSequencerDirection("Direction", sequencerDirection, selectSequencerDirection, sequencerDirectionMenuCallback);
GEMItem menuItemSequencerTempo("Tempo", sequencerTempo, spinnerSequencerTempo, sequencerTempoMenuCallback);
GEMItem menuItemSequencerFirmwareUpdate("Update Firmware", rebootToBootloader);
GEMItem menuItemSequencerBrowserSaveHere("Save Here", sequencerBrowserSaveHereCallback);
GEMItem menuItemSequencerBrowserNewFolder("New Folder", sequencerBrowserNewFolderCallback);
GEMItem menuItemSequencerBrowserThisFolder("This Folder...", sequencerBrowserThisFolderCallback);
GEMItem menuItemSequencerBrowserUp("..", sequencerBrowserUpCallback);
GEMItem menuItemSequencerBrowserMoreAbove("^ more ^", sequencerBrowserMoreAboveCallback);
GEMItem menuItemSequencerBrowserEntry0("", sequencerBrowserEntryCallback, 0);
GEMItem menuItemSequencerBrowserEntry1("", sequencerBrowserEntryCallback, 1);
GEMItem menuItemSequencerBrowserEntry2("", sequencerBrowserEntryCallback, 2);
GEMItem menuItemSequencerBrowserEntry3("", sequencerBrowserEntryCallback, 3);
GEMItem menuItemSequencerBrowserEntry4("", sequencerBrowserEntryCallback, 4);
GEMItem menuItemSequencerBrowserEntry5("", sequencerBrowserEntryCallback, 5);
GEMItem menuItemSequencerBrowserEntry6("", sequencerBrowserEntryCallback, 6);
GEMItem menuItemSequencerBrowserEntry7("", sequencerBrowserEntryCallback, 7);
GEMItem menuItemSequencerBrowserMoreBelow("v more v", sequencerBrowserMoreBelowCallback);
GEMItem menuItemSequencerBrowserPrev("Prev", sequencerBrowserPrevPageCallback);
GEMItem menuItemSequencerBrowserNext("Next", sequencerBrowserNextPageCallback);
GEMItem menuItemSequencerUsbBackupStatusOne(sequencerUsbBackupStatusLineOne, usbBackupStatusMenuCallback);
GEMItem menuItemSequencerUsbBackupStatusTwo(sequencerUsbBackupStatusLineTwo, usbBackupStatusMenuCallback);
GEMItem menuItemSequencerUsbBackupStart("Start Session", startUsbBackupMenuCallback);
GEMItem menuItemSequencerUsbBackupStop("Stop Session", stopUsbBackupMenuCallback);
GEMItem menuItemSequencerUsbBackupExitPromptOne("Leaving this page", usbBackupExitPromptMenuCallback);
GEMItem menuItemSequencerUsbBackupExitPromptTwo("will close", usbBackupExitPromptMenuCallback);
GEMItem menuItemSequencerUsbBackupExitPromptThree("USB Backup.", usbBackupExitPromptMenuCallback);
GEMItem menuItemSequencerUsbBackupExitPromptFour("Continue?", usbBackupExitPromptMenuCallback);
GEMItem menuItemSequencerUsbBackupExitYes("Yes, Leave", confirmUsbBackupExitMenuCallback);
GEMItem menuItemSequencerUsbBackupExitNo("No, Stay", cancelUsbBackupExitMenuCallback);
GEMItem menuItemSequencerUsbBackupStopPromptOne("Stopping this", usbBackupStopPromptMenuCallback);
GEMItem menuItemSequencerUsbBackupStopPromptTwo("session ends", usbBackupStopPromptMenuCallback);
GEMItem menuItemSequencerUsbBackupStopPromptThree("any transfer.", usbBackupStopPromptMenuCallback);
GEMItem menuItemSequencerUsbBackupStopPromptFour("Continue?", usbBackupStopPromptMenuCallback);
GEMItem menuItemSequencerUsbBackupStopYes("Yes, Stop", confirmUsbBackupStopMenuCallback);
GEMItem menuItemSequencerUsbBackupStopNo("No, Stay", cancelUsbBackupStopMenuCallback);
GEMItem menuItemSequencerDeletePromptOne("Delete file:", sequencerDeletePromptMenuCallback);
GEMItem menuItemSequencerDeletePromptTwo("This cannot be", sequencerDeletePromptMenuCallback);
GEMItem menuItemSequencerDeletePromptThree("undone.", sequencerDeletePromptMenuCallback);
GEMItem menuItemSequencerDeletePromptFour("", sequencerDeletePromptMenuCallback);
GEMItem menuItemSequencerDeleteYes("Yes, Delete", confirmSequencerDeleteMenuCallback);
GEMItem menuItemSequencerDeleteCancel("Cancel", cancelSequencerDeleteMenuCallback);
GEMItem menuItemSequencerTargetActionPromptOne("File:", sequencerTargetActionPromptMenuCallback);
GEMItem menuItemSequencerTargetActionPromptTwo("", sequencerTargetActionPromptMenuCallback);
GEMItem menuItemSequencerTargetActionPromptThree("", sequencerTargetActionPromptMenuCallback);
GEMItem menuItemSequencerTargetActionPromptFour("Choose action.", sequencerTargetActionPromptMenuCallback);
GEMItem menuItemSequencerTargetActionRename("Rename", sequencerTargetActionRenameCallback);
GEMItem menuItemSequencerTargetActionDelete("Delete", sequencerTargetActionDeleteCallback);
GEMItem menuItemSequencerTargetActionCancel("Cancel", sequencerTargetActionCancelCallback);
GEMItem* sequencerBrowserEntryItems[SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT] = {
  &menuItemSequencerBrowserEntry0,
  &menuItemSequencerBrowserEntry1,
  &menuItemSequencerBrowserEntry2,
  &menuItemSequencerBrowserEntry3,
  &menuItemSequencerBrowserEntry4,
  &menuItemSequencerBrowserEntry5,
  &menuItemSequencerBrowserEntry6,
  &menuItemSequencerBrowserEntry7
};

int getVisibleBrowserEntryMenuIndex(bool firstVisible) {
  int matchIndex = -1;
  byte visibleItemCount = menuPageSequencerBrowser.getItemsCount();
  for (byte itemIndex = 0; itemIndex < visibleItemCount; itemIndex++) {
    GEMItem* item = menuPageSequencerBrowser.getMenuItem(itemIndex);
    if (item == nullptr) {
      continue;
    }
    for (byte entryIndex = 0; entryIndex < SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT; entryIndex++) {
      if (item == sequencerBrowserEntryItems[entryIndex]) {
        if (firstVisible) {
          return itemIndex;
        }
        matchIndex = itemIndex;
      }
    }
  }
  return matchIndex;
}

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
  } else if (sequencerBrowserMode == SequencerBrowserMode::RenameOrDelete) {
    snprintf(sequencerBrowserPageTitle, sizeof(sequencerBrowserPageTitle), "RenDel:%s", folderName);
  } else {
    snprintf(sequencerBrowserPageTitle, sizeof(sequencerBrowserPageTitle), "Load:%s", folderName);
  }

  menuPageSequencerBrowser.setTitle(sequencerBrowserPageTitle);

  bool showSaveHere = (sequencerBrowserMode == SequencerBrowserMode::SaveNew);
  bool showCreateHere = (sequencerBrowserMode == SequencerBrowserMode::CreateFolder);
  bool showThisFolder = (sequencerBrowserMode == SequencerBrowserMode::RenameOrDelete) &&
                        !sequencerPathIsRoot(sequencerBrowserPath);
  bool showMoreAbove = (sequencerBrowserOffset > 0);
  bool showMoreBelow = (sequencerBrowserOffset + SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT < sequencerBrowserEntryCount);
  menuItemSequencerBrowserSaveHere.setTitle(showCreateHere ? "Create Here" : "Save Here");
  menuItemSequencerBrowserSaveHere.hide(!(showSaveHere || showCreateHere));
  menuItemSequencerBrowserNewFolder.hide(!showSaveHere);
  menuItemSequencerBrowserThisFolder.hide(!showThisFolder);
  menuItemSequencerBrowserUp.hide(sequencerPathIsRoot(sequencerBrowserPath));
  menuItemSequencerBrowserMoreAbove.hide(!showMoreAbove);
  menuItemSequencerBrowserMoreBelow.hide(!showMoreBelow);

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

  menuItemSequencerBrowserPrev.hide();
  menuItemSequencerBrowserNext.hide();

  if (resetSelection) {
    menuPageSequencerBrowser.setCurrentMenuItemIndex(0);
  }
  menu.setMenuPageCurrent(menuPageSequencerBrowser);
  menu.drawMenu();
}

}  // namespace

bool isSequencerTransportPlaybackLedActive(byte buttonIndex) {
  return isSequencerTransportPlaybackLedActiveInternal(buttonIndex);
}

bool didSequencerTransportPlaybackLedJustStart(byte buttonIndex) {
  return didSequencerTransportPlaybackLedJustStartInternal(buttonIndex);
}

uint64_t getSequencerTransportPlaybackLedTimePressed(byte buttonIndex) {
  return getSequencerTransportPlaybackLedTimePressedInternal(buttonIndex);
}

SequencerPersistentSettings getSequencerPersistentSettings() {
  SequencerPersistentSettings values;
  values.tapPreview = sequencerTapPreview;
  values.clockSource = sequencerClockSource;
  values.sendClock = sequencerSendClock;
  values.sendTransport = sequencerSendTransport;
  values.stepAccentEvery = sequencerStepAccentEvery;
  values.stepColorMode = sequencerStepColorMode;
  values.stepHue = sequencerStepHue;
  return values;
}

// Apply profile-backed sequencer preferences without touching per-sequence musical data.
void applySequencerPersistentSettings(const SequencerPersistentSettings& values) {
  sequencerTapPreview = (values.tapPreview == SEQUENCER_TAP_PREVIEW_OFF) ? SEQUENCER_TAP_PREVIEW_OFF : SEQUENCER_TAP_PREVIEW_ON;
  sequencerClockSource =
    (values.clockSource == SEQUENCER_CLOCK_SOURCE_EXTERNAL_MIDI) ? SEQUENCER_CLOCK_SOURCE_EXTERNAL_MIDI : SEQUENCER_CLOCK_SOURCE_INTERNAL;
  sequencerSendClock = (values.sendClock == SEQUENCER_SEND_CLOCK_ON) ? SEQUENCER_SEND_CLOCK_ON : SEQUENCER_SEND_CLOCK_OFF;
  sequencerSendTransport =
    (values.sendTransport == SEQUENCER_SEND_TRANSPORT_ON) ? SEQUENCER_SEND_TRANSPORT_ON : SEQUENCER_SEND_TRANSPORT_OFF;
  if (values.stepAccentEvery >= 2 && values.stepAccentEvery <= 8) {
    sequencerStepAccentEvery = values.stepAccentEvery;
  } else {
    sequencerStepAccentEvery = SEQUENCER_STEP_ACCENT_OFF;
  }
  sequencerStepColorMode = (values.stepColorMode == SEQUENCER_STEP_COLOR_REGULAR) ? SEQUENCER_STEP_COLOR_REGULAR : SEQUENCER_STEP_COLOR_NOTE;
  sequencerStepHue = (values.stepHue <= SEQUENCER_STEP_HUE_PINK) ? values.stepHue : SEQUENCER_STEP_HUE_INDIGO;

  resetSequencerClockSyncState();
  refreshSequencerLightsMenu(false);
  if (sequencerTransportState == SEQUENCER_TRANSPORT_PLAY && !sequencerUsesExternalClock()) {
    sequencerNextStepAt = runTime;
    sequencerCurrentStepStartedAt = runTime;
    if (sequencerShouldSendMidiClock()) {
      sequencerNextMidiClockAt = runTime;
    }
  }
}

bool isSequencerShortcutValueEditActive() {
  // Command-button encoder shortcuts should use "top increases / lower
  // decreases" anywhere a rotary turn edits a value instead of navigating rows.
  if (sequencerOverlayMode == SequencerOverlayMode::PerformanceMonitor ||
      sequencerOverlayMode == SequencerOverlayMode::CopyTargetSelect ||
      sequencerOverlayMode == SequencerOverlayMode::FunctionPicker ||
      isSequencerNamingActive() ||
      sequencerOverlayMode == SequencerOverlayMode::ExactLengthEdit) {
    return false;
  }
  if (menu.getCurrentMenuPage() == &menuPageSequencerBrowser) {
    return false;
  }
  if (sequencerOverlayMode == SequencerOverlayMode::ExactVelocityEdit ||
      sequencerOverlayMode == SequencerOverlayMode::ExactProbabilityEdit) {
    return true;
  }
  return sequencerSelectedStep >= 0;
}

bool handleSequencerRotaryTurn(int8_t direction) {
  if (sequencerOverlayMode == SequencerOverlayMode::PerformanceMonitor) {
    (void)direction;
    return true;
  }
  if (sequencerOverlayMode == SequencerOverlayMode::CopyTargetSelect) {
    (void)direction;
    return true;
  }
  if (sequencerOverlayMode == SequencerOverlayMode::FunctionPicker) {
    (void)direction;
    return true;
  }
  if (isSequencerNamingActive()) {
    (void)direction;
    return true;
  }
  if (menu.getCurrentMenuPage() == &menuPageSequencerBrowser && direction != 0) {
    int currentIndex = menuPageSequencerBrowser.getCurrentMenuItemIndex();
    int firstEntryIndex = getVisibleBrowserEntryMenuIndex(true);
    int lastEntryIndex = getVisibleBrowserEntryMenuIndex(false);
    if (direction > 0 &&
        lastEntryIndex >= 0 &&
        currentIndex >= lastEntryIndex &&
        sequencerBrowserOffset + SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT < sequencerBrowserEntryCount) {
      sequencerBrowserOffset += SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT;
      refreshSequencerBrowserMenu(false);
      int nextFirstEntryIndex = getVisibleBrowserEntryMenuIndex(true);
      if (nextFirstEntryIndex >= 0) {
        menuPageSequencerBrowser.setCurrentMenuItemIndex(static_cast<byte>(nextFirstEntryIndex));
        menu.drawMenu();
      }
      return true;
    }
    if (direction < 0 &&
        firstEntryIndex >= 0 &&
        currentIndex <= firstEntryIndex &&
        sequencerBrowserOffset > 0) {
      if (sequencerBrowserOffset >= SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT) {
        sequencerBrowserOffset -= SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT;
      } else {
        sequencerBrowserOffset = 0;
      }
      refreshSequencerBrowserMenu(false);
      int previousLastEntryIndex = getVisibleBrowserEntryMenuIndex(false);
      if (previousLastEntryIndex >= 0) {
        menuPageSequencerBrowser.setCurrentMenuItemIndex(static_cast<byte>(previousLastEntryIndex));
        menu.drawMenu();
      }
      return true;
    }
  }
  return handleSequencerRotaryTurnInternal(direction);
}

bool shouldShowSequencerPlayedNotesOverlay() {
  if (sequencerSelectedStep >= 0) {
    return false;
  }

  for (byte i = 0; i < SEQUENCER_MAX_MANAGED_HELD_NOTES; i++) {
    if (sequencerManagedHeldNotes[i].active && sequencerManagedHeldNotes[i].auditionCount > 0) {
      return true;
    }
  }
  return false;
}

byte rebuildSequencerDisplayedNotes(int16_t* notes, byte maxCount) {
  if (notes == nullptr || maxCount == 0) {
    return 0;
  }

  for (byte i = 0; i < maxCount; i++) {
    notes[i] = INT16_MIN;
  }

  if (sequencerSelectedStep >= 0) {
    return 0;
  }

  byte out = 0;
  for (byte i = 0; i < SEQUENCER_MAX_MANAGED_HELD_NOTES && out < maxCount; i++) {
    if (!sequencerManagedHeldNotes[i].active || sequencerManagedHeldNotes[i].auditionCount == 0) {
      continue;
    }
    notes[out++] = static_cast<int16_t>(sequencerManagedHeldNotes[i].pitchSteps + getSequencerCurrentTranspose());
  }

  if (out > 1) {
    for (byte i = 0; i + 1 < out; i++) {
      for (byte j = static_cast<byte>(i + 1); j < out; j++) {
        if (notes[j] < notes[i]) {
          int16_t temp = notes[i];
          notes[i] = notes[j];
          notes[j] = temp;
        }
      }
    }
  }
  return out;
}

bool handleSequencerEncoderClick() {
  if (sequencerOverlayMode == SequencerOverlayMode::PerformanceMonitor) {
    return true;
  }
  if (sequencerOverlayMode == SequencerOverlayMode::CopyTargetSelect) {
    exitSequencerCopyTargetSelect(true);
    return true;
  }
  if (sequencerOverlayMode == SequencerOverlayMode::ExactProbabilityEdit) {
    exitSequencerExactProbabilityEdit(true);
    return true;
  }
  if (sequencerOverlayMode == SequencerOverlayMode::ExactVelocityEdit) {
    exitSequencerExactVelocityEdit(true);
    return true;
  }
  if (sequencerOverlayMode == SequencerOverlayMode::FunctionPicker) {
    return true;
  }
  if (isSequencerNamingActive()) {
    return commitSequencerNaming();
  }
  if (sequencerOverlayMode == SequencerOverlayMode::ExactLengthEdit) {
    exitSequencerExactLengthEdit(true);
    return true;
  }
  if (sequencerSelectedStep >= 0) {
    sequencerSelectedStep = -1;
    hideSequencerOverlay();
    return true;
  }
  return false;
}

GEMPage menuPageSequencer("Sequencer");
GEMPage menuPageSequencerFiles("File Management", menuPageSequencer);
GEMPage menuPageSequencerPlayback("Playback Settings", menuPageSequencer);
GEMPage menuPageSequencerLights("Seq Lights", menuPageSequencer);
GEMPage menuPageSequencerMidiSync("MIDI Sync", menuPageSequencerPlayback);
GEMPage menuPageSequencerBrowser("Load", menuPageSequencer);
GEMPage menuPageSequencerUsbBackup("USB Backup", menuPageSequencerFiles);
GEMPage menuPageSequencerUsbBackupExitConfirm("Leave Backup?", menuPageSequencerUsbBackup);
GEMPage menuPageSequencerUsbBackupStopConfirm("Stop Session?", menuPageSequencerUsbBackup);
GEMPage menuPageSequencerDeleteConfirm("Delete?", menuPageSequencerBrowser);
GEMPage menuPageSequencerTargetAction("File Action?", menuPageSequencerBrowser);

void handleSequencerButtonEvent(byte buttonIndex, bool pressed) {
  if (isUsbBackupActive()) {
    return;
  }

  if (buttonIndex == SEQUENCER_TRANSPORT_BUTTON_INDEX) {
    if (pressed) {
      screenTime = 0;
      if (screenSaverOn) {
        screenSaverOn = false;
        u8g2.setContrast(SEQUENCER_OVERLAY_CONTRAST);
      }
      sequencerTransportHeld = true;
      sequencerTransportPressedAt = runTime;
    } else {
      bool showedPerformanceMonitor = (sequencerOverlayMode == SequencerOverlayMode::PerformanceMonitor);
      sequencerTransportHeld = false;
      sequencerTransportPressedAt = 0;
      if (showedPerformanceMonitor) {
        hideSequencerPerformanceMonitor();
      } else {
        if (sequencerSelectedStep >= 0) {
          sequencerSelectedStep = -1;
          hideSequencerOverlay();
        }
        setSequencerTransportState(
          (sequencerTransportState == SEQUENCER_TRANSPORT_PLAY) ? SEQUENCER_TRANSPORT_STOP : SEQUENCER_TRANSPORT_PLAY,
          false);
      }
    }
    return;
  }

  if (sequencerOverlayMode == SequencerOverlayMode::PerformanceMonitor) {
    return;
  }

  // Tools-range/status messages should be modal so repeated presses do not leak
  // into lower-keyboard note actions before we return to the picker.
  if (sequencerOverlayMode == SequencerOverlayMode::StatusMessage &&
      sequencerOverlayReturnMode == SequencerOverlayMode::FunctionPicker) {
    if (pressed) {
      sequencerOverlayUntil = runTime + SEQUENCER_NOTE_CONFIRM_MICROS;
    }
    return;
  }

  if (sequencerOverlayMode == SequencerOverlayMode::FunctionPicker) {
    if (!pressed) {
      return;
    }

    if (buttonIndex == SEQUENCER_FUNCTION_BUTTON_INDEX) {
      exitSequencerFunctionPicker();
      return;
    }

    const SequencerToolKey* toolKey = getSequencerToolKey(buttonIndex);
    if (toolKey != nullptr) {
      handleSequencerToolAction(toolKey->action);
      return;
    }

    int8_t stepIndex = buttonIndexToSequencerStep(buttonIndex);
    if (stepIndex < 0) {
      return;
    }

    // Keep step pads live while the picker is open so tools can be reused across nearby steps.
    if (sequencerTapPreview == SEQUENCER_TAP_PREVIEW_ON) {
      previewSequencerStep(static_cast<byte>(stepIndex));
    }
    if (sequencerSelectedStep != stepIndex) {
      selectSequencerStepForEditing(static_cast<byte>(stepIndex), SequencerOverlayMode::FunctionPicker);
    }
    return;
  }

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

  if (sequencerOverlayMode == SequencerOverlayMode::ExactVelocityEdit) {
    if (!pressed) {
      return;
    }
    return;
  }

  if (sequencerOverlayMode == SequencerOverlayMode::ExactProbabilityEdit) {
    if (!pressed) {
      return;
    }
    return;
  }

  if (sequencerOverlayMode == SequencerOverlayMode::ExactLengthEdit) {
    if (!pressed) {
      return;
    }

    const SequencerNamingKey* exactKey = getSequencerExactLengthKey(buttonIndex);
    if (exactKey == nullptr) {
      return;
    }

    if (exactKey->action == SequencerNamingAction::InsertChar) {
      insertSequencerExactLengthChar(exactKey->character);
    } else if (exactKey->action == SequencerNamingAction::Backspace) {
      backspaceSequencerExactLengthChar();
    } else if (exactKey->action == SequencerNamingAction::Cancel) {
      exitSequencerExactLengthEdit(false);
      return;
    }

    return;
  }

  if (sequencerOverlayMode == SequencerOverlayMode::CopyTargetSelect) {
    if (!pressed) {
      return;
    }

    if (buttonIndex == SEQUENCER_FUNCTION_BUTTON_INDEX) {
      exitSequencerCopyTargetSelect(true);
      return;
    }

    int8_t destinationStep = buttonIndexToSequencerStep(buttonIndex);
    if (destinationStep < 0 || sequencerCopySourceStep < 0) {
      return;
    }

    byte sourceStep = static_cast<byte>(sequencerCopySourceStep);
    byte targetStep = static_cast<byte>(destinationStep);
    snapshotUndoBufferFromStep(targetStep);
    copySequencerStepData(sourceStep, targetStep);
    setSequencerDirtyState(true);
    exitSequencerCopyTargetSelect(true);
    selectSequencerStepForEditing(targetStep, SequencerOverlayMode::FunctionPicker);
    return;
  }

  if (!pressed) {
    if (buttonIndex == SEQUENCER_OVERVIEW_BUTTON_INDEX) {
      return;
    }
    if (buttonIndex == SEQUENCER_FUNCTION_BUTTON_INDEX) {
      return;
    }
    if (buttonIndex == SEQUENCER_CONFIRM_BUTTON_INDEX) {
      if (sequencerSelectedStep >= 0 && sequencerConfirmHeld) {
        restoreSelectedSequencerStepFromUndo();
        sequencerOverlayMode = SequencerOverlayMode::AwaitingNote;
        sequencerOverlayDirty = true;
      }
      sequencerConfirmHeld = false;
      sequencerConfirmPressedAt = 0;
      return;
    }

    int16_t releasedPitchSteps = SEQUENCER_NO_PITCH;
    if (getButtonPitchStepsForSequencer(buttonIndex, releasedPitchSteps)) {
      sendSequencerManagedNoteOff(releasedPitchSteps, false);
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

  if (buttonIndex == SEQUENCER_FUNCTION_BUTTON_INDEX) {
    if (sequencerSelectedStep < 0) {
      showSequencerPersistentStatusMessage("Select step", "Then open tools");
      return;
    }
    enterSequencerFunctionPicker();
    return;
  }

  if (sequencerOverlayMode == SequencerOverlayMode::Overview) {
    hideSequencerOverlay();
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
    selectSequencerStepForEditing(static_cast<byte>(stepIndex), SequencerOverlayMode::AwaitingNote);
    return;
  }

  int16_t pitchSteps = SEQUENCER_NO_PITCH;
  if (getButtonPitchStepsForSequencer(buttonIndex, pitchSteps)) {
    sendSequencerManagedNoteOn(pitchSteps, false, sequencerSelectedStepVelocity());
    if (sequencerSelectedStep >= 0) {
      toggleEditBufferNote(pitchSteps);
      saveEditBufferToStep(static_cast<byte>(sequencerSelectedStep));
      setSequencerDirtyState(true);
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
  menuPageSequencer.addMenuItem(menuGotoSequencerPlayback);
  menuPageSequencer.addMenuItem(menuGotoSynthFromSequencer);
  menuPageSequencer.addMenuItem(menuGotoSequencerFiles);
  menuPageSequencer.addMenuItem(menuItemSequencerNew);
  menuPageSequencer.addMenuItem(menuItemSequencerSave);
  menuPageSequencer.addMenuItem(menuItemSequencerSaveNew);
  menuPageSequencer.addMenuItem(menuItemSequencerLoad);
  menuPageSequencer.addMenuItem(menuItemSequencerRevert);
  menuPageSequencer.addMenuItem(menuGotoSequencerLights);
  menuPageSequencer.addMenuItem(menuGotoColorsFromSequencer);
  menuPageSequencer.addMenuItem(menuItemSequencerTapPreview);
  menuPageSequencer.addMenuItem(menuItemSequencerFirmwareUpdate);

  menuPageSequencerPlayback.addMenuItem(menuItemSequencerStepPlayCount);
  menuPageSequencerPlayback.addMenuItem(menuItemSequencerDirection);
  menuPageSequencerPlayback.addMenuItem(menuItemSequencerTempo);
  menuPageSequencerPlayback.addMenuItem(menuItemSequencerPlayType);
  menuPageSequencerPlayback.addMenuItem(menuGotoSequencerMidiSync);

  // Keep MIDI sync in its own submenu so these general settings are clearly separate from per-sequence playback data.
  menuPageSequencerMidiSync.addMenuItem(menuItemSequencerClockSource);
  menuPageSequencerMidiSync.addMenuItem(menuItemSequencerSendClock);
  menuPageSequencerMidiSync.addMenuItem(menuItemSequencerSendTransport);

  menuPageSequencerLights.addMenuItem(menuItemSequencerStepAccentEvery);
  menuPageSequencerLights.addMenuItem(menuItemSequencerStepColor);
  menuPageSequencerLights.addMenuItem(menuItemSequencerStepHue);
  menuItemSequencerStepHue.setPreviewCallback(previewSequencerStepHue);
  refreshSequencerLightsMenu(false);

  menuPageSequencerFiles.addMenuItem(menuItemSequencerRenameOrDelete);
  menuPageSequencerFiles.addMenuItem(menuItemSequencerCreateFolder);
  menuPageSequencerFiles.addMenuItem(menuGotoSequencerUsbBackup);

  menuPageSequencerBrowser.addMenuItem(menuItemSequencerBrowserSaveHere);
  menuPageSequencerBrowser.addMenuItem(menuItemSequencerBrowserNewFolder);
  menuPageSequencerBrowser.addMenuItem(menuItemSequencerBrowserThisFolder);
  menuPageSequencerBrowser.addMenuItem(menuItemSequencerBrowserUp);
  menuPageSequencerBrowser.addMenuItem(menuItemSequencerBrowserMoreAbove);
  for (byte i = 0; i < SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT; i++) {
    menuPageSequencerBrowser.addMenuItem(*sequencerBrowserEntryItems[i]);
  }
  menuPageSequencerBrowser.addMenuItem(menuItemSequencerBrowserMoreBelow);
  menuPageSequencerBrowser.addMenuItem(menuItemSequencerBrowserPrev);
  menuPageSequencerBrowser.addMenuItem(menuItemSequencerBrowserNext);

  menuItemSequencerBrowserSaveHere.hide();
  menuItemSequencerBrowserNewFolder.hide();
  menuItemSequencerBrowserThisFolder.hide();
  menuItemSequencerBrowserUp.hide();
  menuItemSequencerBrowserMoreAbove.hide();
  menuItemSequencerBrowserMoreBelow.hide();
  menuItemSequencerBrowserPrev.hide();
  menuItemSequencerBrowserNext.hide();
  for (byte i = 0; i < SEQUENCER_BROWSER_VISIBLE_ENTRY_COUNT; i++) {
    sequencerBrowserEntryItems[i]->hide();
  }

  menuPageSequencerUsbBackup.addMenuItem(menuItemSequencerUsbBackupStatusOne);
  menuPageSequencerUsbBackup.addMenuItem(menuItemSequencerUsbBackupStatusTwo);
  menuPageSequencerUsbBackup.addMenuItem(menuItemSequencerUsbBackupStart);
  menuPageSequencerUsbBackup.addMenuItem(menuItemSequencerUsbBackupStop);
  menuPageSequencerUsbBackupExitConfirm.addMenuItem(menuItemSequencerUsbBackupExitPromptOne);
  menuPageSequencerUsbBackupExitConfirm.addMenuItem(menuItemSequencerUsbBackupExitPromptTwo);
  menuPageSequencerUsbBackupExitConfirm.addMenuItem(menuItemSequencerUsbBackupExitPromptThree);
  menuPageSequencerUsbBackupExitConfirm.addMenuItem(menuItemSequencerUsbBackupExitPromptFour);
  menuPageSequencerUsbBackupExitConfirm.addMenuItem(menuItemSequencerUsbBackupExitYes);
  menuPageSequencerUsbBackupExitConfirm.addMenuItem(menuItemSequencerUsbBackupExitNo);
  menuPageSequencerUsbBackupStopConfirm.addMenuItem(menuItemSequencerUsbBackupStopPromptOne);
  menuPageSequencerUsbBackupStopConfirm.addMenuItem(menuItemSequencerUsbBackupStopPromptTwo);
  menuPageSequencerUsbBackupStopConfirm.addMenuItem(menuItemSequencerUsbBackupStopPromptThree);
  menuPageSequencerUsbBackupStopConfirm.addMenuItem(menuItemSequencerUsbBackupStopPromptFour);
  menuPageSequencerUsbBackupStopConfirm.addMenuItem(menuItemSequencerUsbBackupStopYes);
  menuPageSequencerUsbBackupStopConfirm.addMenuItem(menuItemSequencerUsbBackupStopNo);
  menuPageSequencerDeleteConfirm.addMenuItem(menuItemSequencerDeletePromptOne);
  menuPageSequencerDeleteConfirm.addMenuItem(menuItemSequencerDeletePromptTwo);
  menuPageSequencerDeleteConfirm.addMenuItem(menuItemSequencerDeletePromptThree);
  menuPageSequencerDeleteConfirm.addMenuItem(menuItemSequencerDeletePromptFour);
  menuPageSequencerDeleteConfirm.addMenuItem(menuItemSequencerDeleteYes);
  menuPageSequencerDeleteConfirm.addMenuItem(menuItemSequencerDeleteCancel);
  menuPageSequencerTargetAction.addMenuItem(menuItemSequencerTargetActionPromptOne);
  menuPageSequencerTargetAction.addMenuItem(menuItemSequencerTargetActionPromptTwo);
  menuPageSequencerTargetAction.addMenuItem(menuItemSequencerTargetActionPromptThree);
  menuPageSequencerTargetAction.addMenuItem(menuItemSequencerTargetActionPromptFour);
  menuPageSequencerTargetAction.addMenuItem(menuItemSequencerTargetActionRename);
  menuPageSequencerTargetAction.addMenuItem(menuItemSequencerTargetActionDelete);
  menuPageSequencerTargetAction.addMenuItem(menuItemSequencerTargetActionCancel);
  refreshSequencerUsbBackupMenu(false);
}

void drawSequencerOverlay() {
  guardUsbBackupMenuExit();

  if (consumeUsbBackupUiRefreshRequested()) {
    refreshSequencerUsbBackupMenu(menu.getCurrentMenuPage() == &menuPageSequencerUsbBackup);
  }

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
    u8g2.drawStr(4, 78, "Y Z SPC - 1 2 3 4 5");
    u8g2.drawStr(4, 96, "<- 6 7 8 9 0 CANCEL");
    u8g2.sendBuffer();
    return;
  }

  if (sequencerOverlayMode == SequencerOverlayMode::PerformanceMonitor) {
    refreshSequencerPerformanceStats(false);
    sequencerOverlayVisible = true;
    sequencerOverlayDirty = false;

    char cpuLine[20];
    char memoryLine[24];
    char storageLine[24];
    char midiQueueLine[20];
    char midiStateLine[24];
    char usedLabel[10];
    char totalLabel[10];

    snprintf(cpuLine, sizeof(cpuLine), "AudioEng %u%%", static_cast<unsigned>(sequencerPerformanceCpuPercent));

    formatSequencerUsageLabel(sequencerPerformanceHeapUsedBytes, usedLabel, sizeof(usedLabel));
    formatSequencerUsageLabel(sequencerPerformanceHeapTotalBytes, totalLabel, sizeof(totalLabel));
    snprintf(memoryLine, sizeof(memoryLine), "Mem %s/%s", usedLabel, totalLabel);

    if (sequencerPerformanceStorageValid) {
      formatSequencerUsageLabel(sequencerPerformanceStorageUsedBytes, usedLabel, sizeof(usedLabel));
      formatSequencerUsageLabel(sequencerPerformanceStorageTotalBytes, totalLabel, sizeof(totalLabel));
      snprintf(storageLine, sizeof(storageLine), "FS  %s/%s", usedLabel, totalLabel);
    } else {
      snprintf(storageLine, sizeof(storageLine), "FS  unavailable");
    }

    snprintf(midiQueueLine, sizeof(midiQueueLine), "MIDI Q %u",
             static_cast<unsigned>(midiMonitorQueueDepth));
    snprintf(midiStateLine, sizeof(midiStateLine), "Drop %u Late %u",
             static_cast<unsigned>(midiMonitorDroppedCount),
             static_cast<unsigned>(midiMonitorLateCount));

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(8, 16, cpuLine);
    u8g2.drawStr(8, 36, memoryLine);
    u8g2.drawStr(8, 56, storageLine);
    u8g2.drawStr(8, 76, midiQueueLine);
    u8g2.drawStr(8, 96, midiStateLine);
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
      u8g2.clearBuffer();
      u8g2.sendBuffer();
    }
    return;
  }

  screenTime = 0;
  if (screenSaverOn) {
    screenSaverOn = false;
    u8g2.setContrast(SEQUENCER_OVERLAY_CONTRAST);
  }

  if (sequencerOverlayMode == SequencerOverlayMode::ExactLengthEdit) {
    sequencerOverlayVisible = true;
    sequencerOverlayDirty = false;

    char headerLabel[24];
    char currentLabel[16];
    char newLabel[16];
    char currentValueLabel[5];
    char newValueLabel[6];
    snprintf(headerLabel, sizeof(headerLabel), "Exact Length #%02d", sequencerSelectedStep + 1);
    snprintf(currentLabel, sizeof(currentLabel), "Current:");
    snprintf(currentValueLabel, sizeof(currentValueLabel), "%u", static_cast<unsigned>(sequencerExactLengthOriginal));
    if (sequencerExactLengthLength > 0) {
      snprintf(newValueLabel, sizeof(newValueLabel), "%s", sequencerExactLengthBuffer);
    } else {
      snprintf(newValueLabel, sizeof(newValueLabel), "");
    }
    snprintf(newLabel, sizeof(newLabel), "New:");
    bool showCursor = ((runTime / 400000ULL) % 2ULL) == 0ULL;
    size_t newLength = strlen(newValueLabel);
    if (showCursor && newLength < sizeof(newValueLabel) - 1) {
      newValueLabel[newLength] = '_';
      newValueLabel[newLength + 1] = '\0';
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(2, 12, headerLabel);
    u8g2.drawStr(8, 24, currentLabel);
    u8g2.drawStr(8, 38, newLabel);

    u8g2.setFont(u8g2_font_7x14_tf);
    u8g2.drawStr(68, 25, currentValueLabel);
    u8g2.drawStr(68, 39, newValueLabel);

    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(8, 54, "0 1 2 3 4");
    u8g2.drawStr(8, 68, "5 6 7 8 9");

    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(8, 82, "<- CANCEL");
    u8g2.drawStr(8, 98, "Press encoder");
    u8g2.drawStr(8, 110, "to save");
    u8g2.sendBuffer();
    return;
  }

  if (sequencerOverlayMode == SequencerOverlayMode::ExactProbabilityEdit) {
    sequencerOverlayVisible = true;
    sequencerOverlayDirty = false;

    char headerLabel[20];
    char infoLine[32];
    char valueLabel[8];
    char noteLineOne[24];
    char noteLineTwo[24];
    snprintf(headerLabel, sizeof(headerLabel), "Prob #%02d", sequencerSelectedStep + 1);
    snprintf(infoLine, sizeof(infoLine), "L %u%% V %u P %u%%",
             static_cast<unsigned>(sequencerStepGatePercent[sequencerSelectedStep]),
             static_cast<unsigned>(sequencerStepVelocity[sequencerSelectedStep]),
             static_cast<unsigned>(sequencerProbabilityDisplay));
    snprintf(valueLabel, sizeof(valueLabel), "%u%%", static_cast<unsigned>(sequencerProbabilityDisplay));
    fillOverlayNoteLines(noteLineOne, sizeof(noteLineOne), noteLineTwo, sizeof(noteLineTwo));

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(18, 14, headerLabel);
    u8g2.drawStr(2, 30, infoLine);
    u8g2.drawStr(12, 48, noteLineOne);
    if (noteLineTwo[0] != '\0') {
      u8g2.drawStr(12, 60, noteLineTwo);
    }
    u8g2.drawStr(40, 82, valueLabel);

    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(8, 98, "Turn +/-5%");
    u8g2.drawStr(8, 116, "Press encoder to save");
    u8g2.sendBuffer();
    return;
  }

  if (sequencerOverlayMode == SequencerOverlayMode::ExactVelocityEdit) {
    sequencerOverlayVisible = true;
    sequencerOverlayDirty = false;

    char headerLabel[20];
    char infoLine[32];
    char valueLabel[8];
    char noteLineOne[24];
    char noteLineTwo[24];
    snprintf(headerLabel, sizeof(headerLabel), "Vel #%02d", sequencerSelectedStep + 1);
    snprintf(infoLine, sizeof(infoLine), "L %u%% V %u P %u%%",
             static_cast<unsigned>(sequencerStepGatePercent[sequencerSelectedStep]),
             static_cast<unsigned>(sequencerVelocityDisplay),
             static_cast<unsigned>(sequencerStepProbability[sequencerSelectedStep]));
    snprintf(valueLabel, sizeof(valueLabel), "%u", static_cast<unsigned>(sequencerVelocityDisplay));
    fillOverlayNoteLines(noteLineOne, sizeof(noteLineOne), noteLineTwo, sizeof(noteLineTwo));

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(24, 14, headerLabel);
    u8g2.drawStr(2, 30, infoLine);
    u8g2.drawStr(12, 48, noteLineOne);
    if (noteLineTwo[0] != '\0') {
      u8g2.drawStr(12, 60, noteLineTwo);
    }
    u8g2.drawStr(44, 82, valueLabel);

    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(8, 98, "Turn +/-5");
    u8g2.drawStr(8, 116, "Press encoder to save");
    u8g2.sendBuffer();
    return;
  }

  if (sequencerOverlayMode == SequencerOverlayMode::FunctionPicker && sequencerSelectedStep >= 0) {
    sequencerOverlayVisible = true;
    sequencerOverlayDirty = false;

    char headerLabel[20];
    char infoLine[32];
    char noteLineOne[24];
    char noteLineTwo[24];
    snprintf(headerLabel, sizeof(headerLabel), "Tools #%02d", sequencerSelectedStep + 1);
    snprintf(infoLine, sizeof(infoLine), "L %u%% V %u P %u%%",
             static_cast<unsigned>(sequencerStepGatePercent[sequencerSelectedStep]),
             static_cast<unsigned>(sequencerStepVelocity[sequencerSelectedStep]),
             static_cast<unsigned>(sequencerStepProbability[sequencerSelectedStep]));
    fillOverlayNoteLines(noteLineOne, sizeof(noteLineOne), noteLineTwo, sizeof(noteLineTwo));

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(20, 14, headerLabel);
    u8g2.drawStr(2, 30, infoLine);
    u8g2.drawStr(12, 48, noteLineOne);
    if (noteLineTwo[0] != '\0') {
      u8g2.drawStr(12, 60, noteLineTwo);
    }
    u8g2.drawStr(8, 80, "Len Vel Oct+ Oct-");
    u8g2.drawStr(8, 92, "Prob Tie Copy");
    u8g2.drawStr(8, 104, "Cancel/Finished");
    u8g2.sendBuffer();
    return;
  }

  if (sequencerOverlayMode == SequencerOverlayMode::CopyTargetSelect &&
      sequencerSelectedStep >= 0 &&
      sequencerCopySourceStep >= 0) {
    sequencerOverlayVisible = true;
    sequencerOverlayDirty = false;

    char headerLabel[20];
    char infoLine[32];
    char noteLineOne[24];
    char noteLineTwo[24];
    snprintf(headerLabel, sizeof(headerLabel), "Copy #%02d", sequencerCopySourceStep + 1);
    snprintf(infoLine, sizeof(infoLine), "L %u%% V %u P %u%%",
             static_cast<unsigned>(sequencerStepGatePercent[sequencerCopySourceStep]),
             static_cast<unsigned>(sequencerStepVelocity[sequencerCopySourceStep]),
             static_cast<unsigned>(sequencerStepProbability[sequencerCopySourceStep]));
    fillOverlayNoteLines(noteLineOne, sizeof(noteLineOne), noteLineTwo, sizeof(noteLineTwo));

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.drawStr(20, 14, headerLabel);
    u8g2.drawStr(2, 30, infoLine);
    u8g2.drawStr(12, 48, noteLineOne);
    if (noteLineTwo[0] != '\0') {
      u8g2.drawStr(12, 60, noteLineTwo);
    }
    u8g2.drawStr(8, 88, "Tap target step");
    u8g2.drawStr(8, 100, "Tools/enc = exit");
    u8g2.sendBuffer();
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
    } else if (sequencerOverlayMode == SequencerOverlayMode::StatusMessage &&
               sequencerOverlayReturnMode != SequencerOverlayMode::Hidden) {
      sequencerOverlayMode = sequencerOverlayReturnMode;
      sequencerOverlayReturnMode = SequencerOverlayMode::Hidden;
      sequencerOverlayVisible = false;
      sequencerOverlayDirty = true;
      return;
    }
    sequencerOverlayReturnMode = SequencerOverlayMode::Hidden;
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
    sequencerOverlayVisible = true;
    sequencerOverlayDirty = false;

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_tf);

    int y = SEQUENCER_OVERVIEW_FIRST_LINE_Y;
    char lineOne[24];
    char lineTwo[24];
    for (byte stepIndex = sequencerOverviewStartStep; stepIndex < SEQUENCER_STEP_COUNT; stepIndex++) {
      fillOverviewStepLines(stepIndex, lineOne, sizeof(lineOne), lineTwo, sizeof(lineTwo));
      bool usesSecondLine = (lineTwo[0] != '\0');
      int lastBaseline = usesSecondLine ? (y + SEQUENCER_OVERVIEW_SECOND_LINE_OFFSET) : y;
      if (lastBaseline > SEQUENCER_OVERVIEW_MAX_BASELINE_Y) {
        break;
      }

      u8g2.drawStr(4, y, lineOne);
      if (usesSecondLine) {
        u8g2.drawStr(4, y + SEQUENCER_OVERVIEW_SECOND_LINE_OFFSET, lineTwo);
      }
      y += usesSecondLine ? SEQUENCER_OVERVIEW_DOUBLE_STEP_ADVANCE : SEQUENCER_OVERVIEW_SINGLE_STEP_ADVANCE;
    }

    u8g2.sendBuffer();
    return;
  }

  char headerLabel[20];
  char stepLabel[16];
  char noteLineOne[24];
  char noteLineTwo[24];
  char hintLineOne[32];
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
    snprintf(hintLineOne, sizeof(hintLineOne), "L %u%% V %u P %u%%",
             static_cast<unsigned>(sequencerStepGatePercent[sequencerSelectedStep]),
             static_cast<unsigned>(sequencerStepVelocity[sequencerSelectedStep]),
             static_cast<unsigned>(sequencerStepProbability[sequencerSelectedStep]));
    hintLineTwo[0] = '\0';
  } else if (sequencerOverlayMode == SequencerOverlayMode::LengthEdit) {
    snprintf(headerLabel, sizeof(headerLabel), "Step Length");
    snprintf(hintLineOne, sizeof(hintLineOne), "Length %u%%", static_cast<unsigned>(sequencerLengthPercentDisplay));
    fillOverlayNoteLines(noteLineOne, sizeof(noteLineOne), noteLineTwo, sizeof(noteLineTwo));
  } else if (sequencerOverlayMode == SequencerOverlayMode::StepCleared) {
    snprintf(headerLabel, sizeof(headerLabel), "Erased #%02d", sequencerSelectedStep + 1);
    snprintf(stepLabel, sizeof(stepLabel), "Press blue key");
    snprintf(hintLineOne, sizeof(hintLineOne), "to undo.");
    snprintf(hintLineTwo, sizeof(hintLineTwo), "Press blinking");
    snprintf(noteLineOne, sizeof(noteLineOne), "key to exit.");
    noteLineTwo[0] = '\0';
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
  int headerY = 18;
  int stepLabelY = 36;
  int stepLabelX = 36;
  int hintLineOneY = (stepLabel[0] != '\0') ? 54 : 40;
  int hintLineTwoY = (stepLabel[0] != '\0') ? 68 : 54;
  int hintLineTwoX = 8;
  int noteLineOneX = 12;
  int noteLineTwoX = 12;
  int noteLineOneY = (stepLabel[0] != '\0') ? 96 : 88;
  int noteLineTwoY = (stepLabel[0] != '\0') ? 112 : 104;

  if (sequencerOverlayMode == SequencerOverlayMode::AwaitingNote) {
    headerY = 14;
    hintLineOneY = 30;
    noteLineOneY = 48;
    noteLineTwoY = 60;
  } else if (sequencerOverlayMode == SequencerOverlayMode::StepCleared) {
    headerY = 18;
    stepLabelY = 38;
    stepLabelX = 2;
    hintLineOneY = 52;
    hintLineTwoY = 74;
    hintLineTwoX = 2;
    noteLineOneX = 2;
    noteLineOneY = 88;
  } else if (sequencerOverlayMode == SequencerOverlayMode::StatusMessage) {
    headerY = 18;
    hintLineOneY = 42;
  }

  u8g2.drawStr(20, headerY, headerLabel);
  if (stepLabel[0] != '\0') {
    u8g2.drawStr(stepLabelX, stepLabelY, stepLabel);
  }
  if (hintLineOne[0] != '\0') {
    u8g2.drawStr(2, hintLineOneY, hintLineOne);
  }
  if (hintLineTwo[0] != '\0') {
    u8g2.drawStr(hintLineTwoX, hintLineTwoY, hintLineTwo);
  }
  if (noteLineOne[0] != '\0') {
    u8g2.drawStr(noteLineOneX, noteLineOneY, noteLineOne);
  }
  if (noteLineTwo[0] != '\0') {
    u8g2.drawStr(noteLineTwoX, noteLineTwoY, noteLineTwo);
  }
  u8g2.sendBuffer();
}

void applySequencerLedOverrides() {
  bool toolsPromptActive = (sequencerOverlayMode == SequencerOverlayMode::StatusMessage &&
                            strcmp(sequencerStatusLineOne, "Select step") == 0 &&
                            strcmp(sequencerStatusLineTwo, "Then open tools") == 0);

  if (isUsbBackupActive()) {
    uint16_t ledCount = strip.numPixels();
    for (uint16_t buttonIndex = 0; buttonIndex < ledCount; buttonIndex++) {
      strip.setPixelColor(buttonIndex, 0);
    }
    return;
  }

  if (isSequencerNamingActive()) {
    // Keep naming helper keys readable but not overpowering while typing.
    uint32_t activeColor = getSequencerMediumBlueActionLedColor();
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

  if (sequencerOverlayMode == SequencerOverlayMode::ExactLengthEdit) {
    uint32_t activeColor = getSequencerMediumBlueActionLedColor();
    uint16_t ledCount = strip.numPixels();
    for (uint16_t buttonIndex = 0; buttonIndex < ledCount; buttonIndex++) {
      strip.setPixelColor(buttonIndex, 0);
    }
    // Keep the step rows visible while the exact-length keypad moves to the lower board area.
    applySequencerStepLedState();
    for (const SequencerNamingKey& key : sequencerExactLengthKeys) {
      if (key.buttonIndex < ledCount) {
        strip.setPixelColor(key.buttonIndex, activeColor);
      }
    }
    return;
  }

  if (sequencerOverlayMode == SequencerOverlayMode::ExactVelocityEdit) {
    uint16_t ledCount = strip.numPixels();
    for (uint16_t buttonIndex = 0; buttonIndex < ledCount; buttonIndex++) {
      strip.setPixelColor(buttonIndex, 0);
    }
    return;
  }

  if (sequencerOverlayMode == SequencerOverlayMode::ExactProbabilityEdit) {
    uint16_t ledCount = strip.numPixels();
    for (uint16_t buttonIndex = 0; buttonIndex < ledCount; buttonIndex++) {
      strip.setPixelColor(buttonIndex, 0);
    }
    return;
  }

  if (sequencerOverlayMode == SequencerOverlayMode::FunctionPicker) {
    uint16_t ledCount = strip.numPixels();
    for (uint16_t buttonIndex = 0; buttonIndex < ledCount; buttonIndex++) {
      strip.setPixelColor(buttonIndex, 0);
    }
  }

  strip.setPixelColor(
    SEQUENCER_TRANSPORT_BUTTON_INDEX,
    getSequencerTransportLedColor(sequencerTransportState == SEQUENCER_TRANSPORT_PLAY));
  strip.setPixelColor(
    SEQUENCER_OVERVIEW_BUTTON_INDEX,
    getSequencerUtilityLedColor(sequencerOverlayMode == SequencerOverlayMode::Overview));
  strip.setPixelColor(SEQUENCER_CONFIRM_BUTTON_INDEX, getSequencerConfirmLedColor());
  strip.setPixelColor(
    SEQUENCER_FUNCTION_BUTTON_INDEX,
    getSequencerUtilityLedColor(sequencerOverlayMode == SequencerOverlayMode::FunctionPicker ||
                                toolsPromptActive));

  applySequencerStepLedState();
  // Transport playback highlights the lower keyboard pitches for the full
  // held duration, including ties that extend an earlier source note.
  applySequencerTransportPlaybackNoteLedState();

  if (sequencerOverlayMode == SequencerOverlayMode::FunctionPicker) {
    uint32_t activeColor = getSequencerMediumBlueActionLedColor();
    uint16_t ledCount = strip.numPixels();
    for (const SequencerToolKey& key : sequencerToolKeys) {
      if (key.buttonIndex < ledCount) {
        strip.setPixelColor(key.buttonIndex, activeColor);
      }
    }
  }
}

void updateSequencerTransport() {
  sequencerTransportPlaybackLedEpoch++;
  if (sequencerTransportPlaybackLedEpoch == 0) {
    sequencerTransportPlaybackLedEpoch = 1;
  }

  if (sequencerConfirmHeld && sequencerSelectedStep >= 0) {
    uint64_t heldMicros = runTime - sequencerConfirmPressedAt;
    if (heldMicros >= SEQUENCER_CLEAR_HOLD_MICROS) {
      clearSelectedSequencerStep();
    }
  }

  if (sequencerTransportHeld && sequencerTransportPressedAt != 0) {
    uint64_t heldMicros = runTime - sequencerTransportPressedAt;
    if (heldMicros >= SEQUENCER_PERFORMANCE_HOLD_MICROS) {
      showSequencerPerformanceMonitor();
    }
  } else if (sequencerOverlayMode == SequencerOverlayMode::PerformanceMonitor) {
    hideSequencerPerformanceMonitor();
  }

  serviceSequencerInternalMidiClock();

  if (sequencerTransportState == SEQUENCER_TRANSPORT_PLAY &&
      !sequencerUsesExternalClock() &&
      runTime >= sequencerNextStepAt) {
    uint64_t stepDuration = sequencerStepDurationMicros();
    sequencerCurrentStepStartedAt = sequencerNextStepAt;
    sequencerNextStepAt += stepDuration;
    advanceSequencerPlaybackStep(stepDuration);
  }

  serviceSequencerPlaybackGroups();

  if (sequencerTransportState != SEQUENCER_TRANSPORT_PLAY) {
    return;
  }

  if (sequencerUsesExternalClock()) {
    return;
  }
}

void handleSequencerExternalMidiClock() {
  if (!sequencerUsesExternalClock()) {
    return;
  }

  recordSequencerExternalClockPulse();

  if (sequencerTransportState != SEQUENCER_TRANSPORT_PLAY) {
    return;
  }

  uint64_t stepDuration = 0;
  if (!advanceSequencerExternalClockState(stepDuration)) {
    return;
  }

  sequencerCurrentStepStartedAt = runTime;
  advanceSequencerPlaybackStep(stepDuration);

  resolvePendingExternalTieBoundaries(sequencerActiveStepCount());

  serviceSequencerPlaybackGroups();
}

void handleSequencerExternalMidiStart() {
  if (!sequencerUsesExternalClock()) {
    return;
  }

  setSequencerTransportState(SEQUENCER_TRANSPORT_PLAY, false, false);
  sequencerCurrentStepStartedAt = runTime;
  advanceSequencerPlaybackStep(sequencerCurrentStepDurationMicros());
  serviceSequencerPlaybackGroups();
}

void handleSequencerExternalMidiStop() {
  if (!sequencerUsesExternalClock()) {
    return;
  }

  setSequencerTransportState(SEQUENCER_TRANSPORT_STOP, false, false);
}

void handleSequencerExternalMidiContinue() {
  if (!sequencerUsesExternalClock()) {
    return;
  }

  setSequencerTransportState(SEQUENCER_TRANSPORT_PLAY, false, false);
  sequencerCurrentStepStartedAt = runTime;
  advanceSequencerPlaybackStep(sequencerCurrentStepDurationMicros());
  serviceSequencerPlaybackGroups();
}
