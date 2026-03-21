#include "SequencerMode.h"

#include <Adafruit_NeoPixel.h>
#include <LittleFS.h>
#include <cstdio>
#include <cstring>

extern void rebootToBootloader();
extern bool fileSystemExists;
extern GEM_u8g2 menu;
extern U8G2_SH1107_SEEED_128X128_F_HW_I2C u8g2;
extern bool screenSaverOn;
extern uint64_t screenTime;
extern uint64_t runTime;
extern Adafruit_NeoPixel strip;

int sequencerConfirmHue = 250;
byte sequencerConfirmSaturation = 255;
byte sequencerConfirmValue = 211;
bool sequencerConfirmPreviewActive = false;
int sequencerConfirmPreviewHue = sequencerConfirmHue;
byte sequencerConfirmPreviewSaturation = sequencerConfirmSaturation;
byte sequencerConfirmPreviewValue = sequencerConfirmValue;

namespace {
constexpr byte SEQUENCER_STEP_COUNT = 32;
constexpr byte SEQUENCER_TRANSPORT_BUTTON_INDEX = 9;
constexpr byte SEQUENCER_OVERVIEW_BUTTON_INDEX = 18;
constexpr byte SEQUENCER_CONFIRM_BUTTON_INDEX = 19;
constexpr byte SEQUENCER_MAX_NOTES_PER_STEP = 4;
constexpr byte SEQUENCER_OVERVIEW_STEPS_PER_PAGE = 6;
constexpr byte SEQUENCER_OVERLAY_CONTRAST = 63;
constexpr byte SEQUENCER_NO_NOTE = 255;
constexpr uint64_t SEQUENCER_NOTE_CONFIRM_MICROS = 2000000ULL;
constexpr uint64_t SEQUENCER_CLEAR_HOLD_MICROS = 1000000ULL;
constexpr uint64_t SEQUENCER_SELECTED_ON_MICROS = 1000000ULL;
constexpr uint64_t SEQUENCER_SELECTED_OFF_MICROS = 200000ULL;
constexpr byte SEQUENCER_TRANSPORT_STOP = 0;
constexpr byte SEQUENCER_TRANSPORT_PLAY = 1;
constexpr byte SEQUENCER_DIRECTION_FORWARD = 0;
constexpr byte SEQUENCER_DIRECTION_BACKWARD = 1;
constexpr byte SEQUENCER_DIRECTION_PING_PONG = 2;
constexpr byte SEQUENCER_DIRECTION_RANDOM = 3;
constexpr byte SEQUENCER_DIRECTION_BROWNIAN = 4;
constexpr byte SEQUENCER_DIRECTION_DRUNK = 5;
constexpr const char* SEQUENCER_STORAGE_PATH = "/sequence.hbseq";
constexpr byte SEQUENCER_GATE_CHOICE_COUNT = 12;
constexpr byte SEQUENCER_MAX_ACTIVE_PLAYBACK_GROUPS = 16;

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
  Overview = 6
};

struct SequencerPlaybackGroup {
  bool active = false;
  byte midiNotes[SEQUENCER_MAX_NOTES_PER_STEP] = {
    SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE
  };
  byte noteCount = 0;
  uint64_t noteOffAt = 0;
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
byte sequencerDirection = SEQUENCER_DIRECTION_FORWARD;
int8_t sequencerPingPongDelta = 1;
byte sequencerTempo = 120;
byte sequencerTransportState = 0;
bool sequencerDirty = false;
bool sequencerStorageInitialized = false;
uint16_t sequencerLengthPercentDisplay = 100;
byte sequencerOverviewPage = 0;

void showSequencerStatusMessage(const char* lineOne, const char* lineTwo);

const char* sequencerChromaticNames[12] = {
  "C", "C#", "D", "Eb", "E", "F",
  "F#", "G", "G#", "A", "Bb", "B"
};
const uint16_t sequencerGateChoices[SEQUENCER_GATE_CHOICE_COUNT] = {
  0, 25, 50, 75, 100, 150, 200, 250, 300, 350, 400, 1000
};

byte sequencerGateChoiceIndex(uint16_t gatePercent);

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
    sendBoardPreviewMidiNote(midiNote, true);
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
    sendBoardPreviewMidiNote(midiNote, false);
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

  File f = LittleFS.open(SEQUENCER_STORAGE_PATH, "r");
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

bool saveSequencerToFlash() {
  if (!fileSystemExists) {
    return false;
  }

  File f = LittleFS.open(SEQUENCER_STORAGE_PATH, "w");
  if (!f) {
    return false;
  }

  f.println("format=HBSEQ");
  f.println("version=1");
  f.print("tempo=");
  f.println(sequencerTempo);
  f.print("steps=");
  f.println(sequencerStepPlayCount);
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
  sequencerDirty = false;
  return true;
}

void saveSequencerMenuCallback() {
  if (saveSequencerToFlash()) {
    showSequencerStatusMessage("Sequence Saved", "Flash write OK");
  } else {
    showSequencerStatusMessage("Error Saving", "Flash write failed");
  }
}

void revertSequencerMenuCallback() {
  if (loadSequencerFromFlash()) {
    showSequencerStatusMessage("Reverted", "Loaded saved file");
  } else {
    showSequencerStatusMessage("Error Reverting", "Load failed");
  }
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

void sequencerDirectionMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  sequencerPingPongDelta = 1;
  sequencerDirty = true;
}

void sequencerConfirmHueMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  sequencerConfirmPreviewActive = false;
  sequencerConfirmPreviewHue = sequencerConfirmHue;
}

void sequencerConfirmSatMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  sequencerConfirmPreviewActive = false;
  sequencerConfirmPreviewSaturation = sequencerConfirmSaturation;
}

void sequencerConfirmValMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  sequencerConfirmPreviewActive = false;
  sequencerConfirmPreviewValue = sequencerConfirmValue;
}

void previewSequencerConfirmHue(GEMPreviewCallbackData previewData) {
  if (previewData.previewSelectNum < 0) {
    sequencerConfirmPreviewActive = false;
    return;
  }
  sequencerConfirmPreviewActive = true;
  sequencerConfirmPreviewSaturation = sequencerConfirmSaturation;
  sequencerConfirmPreviewValue = sequencerConfirmValue;
  sequencerConfirmPreviewHue = constrain(previewData.previewValInt, 0, 360);
}

void previewSequencerConfirmSat(GEMPreviewCallbackData previewData) {
  if (previewData.previewSelectNum < 0) {
    sequencerConfirmPreviewActive = false;
    return;
  }
  sequencerConfirmPreviewActive = true;
  sequencerConfirmPreviewHue = sequencerConfirmHue;
  sequencerConfirmPreviewValue = sequencerConfirmValue;
  sequencerConfirmPreviewSaturation = previewData.previewValByte;
}

void previewSequencerConfirmVal(GEMPreviewCallbackData previewData) {
  if (previewData.previewSelectNum < 0) {
    sequencerConfirmPreviewActive = false;
    return;
  }
  sequencerConfirmPreviewActive = true;
  sequencerConfirmPreviewHue = sequencerConfirmHue;
  sequencerConfirmPreviewSaturation = sequencerConfirmSaturation;
  sequencerConfirmPreviewValue = previewData.previewValByte;
}

const GEMSpinnerBoundariesInt spinnerBoundariesSequencerHue = { 1, 0, 360 };
const GEMSpinnerBoundariesByte spinnerBoundariesSequencerStepPlayCount = { 1, 1, SEQUENCER_STEP_COUNT };
const GEMSpinnerBoundariesByte spinnerBoundariesSequencerTempo = { 1, 1, 255 };
const GEMSpinnerBoundariesByte spinnerBoundariesSequencerSat = { 1, 0, 255 };
const GEMSpinnerBoundariesByte spinnerBoundariesSequencerVal = { 1, 0, 255 };

GEMSpinner spinnerSequencerHue(spinnerBoundariesSequencerHue, GEM_LOOP);
GEMSpinner spinnerSequencerStepPlayCount(spinnerBoundariesSequencerStepPlayCount, GEM_LOOP);
GEMSpinner spinnerSequencerTempo(spinnerBoundariesSequencerTempo, GEM_LOOP);
GEMSpinner spinnerSequencerSat(spinnerBoundariesSequencerSat, GEM_LOOP);
GEMSpinner spinnerSequencerVal(spinnerBoundariesSequencerVal, GEM_LOOP);

SelectOptionByte optionByteSequencerTransport[] = { { "Stop", 0 }, { "Play", 1 } };
GEMSelect selectSequencerTransport(sizeof(optionByteSequencerTransport) / sizeof(SelectOptionByte), optionByteSequencerTransport);

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
GEMItem menuItemSequencerSave("Save", saveSequencerMenuCallback);
GEMItem menuItemSequencerRevert("Revert", revertSequencerMenuCallback);
GEMItem menuItemSequencerPlayStop("Play/Stop", sequencerTransportState, selectSequencerTransport, sequencerTransportMenuCallback);
GEMItem menuItemSequencerStepPlayCount("Steps", sequencerStepPlayCount, spinnerSequencerStepPlayCount, sequencerStepPlayCountMenuCallback);
GEMItem menuItemSequencerDirection("Direction", sequencerDirection, selectSequencerDirection, sequencerDirectionMenuCallback);
GEMItem menuItemSequencerTempo("Tempo", sequencerTempo, spinnerSequencerTempo, sequencerTempoMenuCallback);
GEMItem menuItemSequencerBtnHue("Btn Hue", sequencerConfirmHue, spinnerSequencerHue, sequencerConfirmHueMenuCallback);
GEMItem menuItemSequencerBtnSat("Btn Sat", sequencerConfirmSaturation, spinnerSequencerSat, sequencerConfirmSatMenuCallback);
GEMItem menuItemSequencerBtnVal("Btn Val", sequencerConfirmValue, spinnerSequencerVal, sequencerConfirmValMenuCallback);
GEMItem menuItemSequencerFirmwareUpdate("Update Firmware", rebootToBootloader);

}  // namespace

bool handleSequencerRotaryTurn(int8_t direction) {
  return handleSequencerRotaryTurnInternal(direction);
}

GEMPage menuPageSequencer("Sequencer");

void handleSequencerButtonEvent(byte buttonIndex, bool pressed) {
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
    loadSequencerFromFlash();
    sequencerStorageInitialized = true;
  }

  menuItemSequencerBtnHue.setPreviewCallback(previewSequencerConfirmHue);
  menuItemSequencerBtnSat.setPreviewCallback(previewSequencerConfirmSat);
  menuItemSequencerBtnVal.setPreviewCallback(previewSequencerConfirmVal);

  menuPageSequencer.addMenuItem(menuItemEnterKeyboard);
  menuPageSequencer.addMenuItem(menuItemSequencerSave);
  menuPageSequencer.addMenuItem(menuItemSequencerRevert);
  menuPageSequencer.addMenuItem(menuItemSequencerPlayStop);
  menuPageSequencer.addMenuItem(menuItemSequencerStepPlayCount);
  menuPageSequencer.addMenuItem(menuItemSequencerDirection);
  menuPageSequencer.addMenuItem(menuItemSequencerTempo);
  menuPageSequencer.addMenuItem(menuItemSequencerBtnHue);
  menuPageSequencer.addMenuItem(menuItemSequencerBtnSat);
  menuPageSequencer.addMenuItem(menuItemSequencerBtnVal);
  menuPageSequencer.addMenuItem(menuItemSequencerFirmwareUpdate);
}

void drawSequencerOverlay() {
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
      int y = 16 + (row * 18);
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
