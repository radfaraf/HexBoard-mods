#include "SequencerMode.h"

#include <Adafruit_NeoPixel.h>
#include <cstdio>
#include <cstring>

extern void rebootToBootloader();
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
constexpr byte SEQUENCER_STEP_COUNT = 16;
constexpr byte SEQUENCER_TRANSPORT_BUTTON_INDEX = 9;
constexpr byte SEQUENCER_CONFIRM_BUTTON_INDEX = 19;
constexpr byte SEQUENCER_MAX_NOTES_PER_STEP = 4;
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

byte sequencerStepMidiNotes[SEQUENCER_STEP_COUNT][SEQUENCER_MAX_NOTES_PER_STEP] = {};
byte sequencerStepNoteCount[SEQUENCER_STEP_COUNT] = {};
byte sequencerStepGatePercent[SEQUENCER_STEP_COUNT] = {
  100, 100, 100, 100, 100, 100, 100, 100,
  100, 100, 100, 100, 100, 100, 100, 100
};

enum class SequencerOverlayMode : uint8_t {
  Hidden = 0,
  AwaitingNote = 1,
  NoteAssigned = 2,
  StepCleared = 3
};

int8_t sequencerSelectedStep = -1;
SequencerOverlayMode sequencerOverlayMode = SequencerOverlayMode::Hidden;
uint64_t sequencerOverlayUntil = 0;
bool sequencerOverlayVisible = false;
bool sequencerOverlayDirty = false;
byte sequencerEditMidiNotes[SEQUENCER_MAX_NOTES_PER_STEP] = {
  SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE
};
byte sequencerEditNoteCount = 0;
byte sequencerUndoMidiNotes[SEQUENCER_MAX_NOTES_PER_STEP] = {
  SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE
};
byte sequencerUndoNoteCount = 0;
byte sequencerPreviewHeldNoteCounts[128] = {};
int8_t sequencerPlayingStep = -1;
bool sequencerPlaybackNoteActive = false;
byte sequencerPlaybackMidiNotes[SEQUENCER_MAX_NOTES_PER_STEP] = {
  SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE
};
byte sequencerPlaybackNoteCount = 0;
uint64_t sequencerNextStepAt = 0;
uint64_t sequencerCurrentStepStartedAt = 0;
uint64_t sequencerPlaybackNoteOffAt = 0;
uint64_t sequencerConfirmPressedAt = 0;
bool sequencerConfirmHeld = false;
byte sequencerStepPlayCount = 16;
byte sequencerDirection = SEQUENCER_DIRECTION_FORWARD;
int8_t sequencerPingPongDelta = 1;
byte sequencerTempo = 120;
byte sequencerTransportState = 0;

const char* sequencerChromaticNames[12] = {
  "C", "C#", "D", "Eb", "E", "F",
  "F#", "G", "G#", "A", "Bb", "B"
};

int8_t buttonIndexToSequencerStep(byte buttonIndex) {
  if (buttonIndex >= 1 && buttonIndex <= 8) {
    return static_cast<int8_t>(buttonIndex - 1);
  }
  if (buttonIndex >= 10 && buttonIndex < 18) {
    return static_cast<int8_t>(8 + (buttonIndex - 10));
  }
  return -1;
}

int8_t sequencerStepToButtonIndex(byte stepIndex) {
  if (stepIndex < 8) {
    return static_cast<int8_t>(stepIndex + 1);
  }
  if (stepIndex < SEQUENCER_STEP_COUNT) {
    return static_cast<int8_t>(10 + (stepIndex - 8));
  }
  return -1;
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
    char* target = (i < 2) ? lineOne : lineTwo;
    size_t targetSize = (i < 2) ? lineOneSize : lineTwoSize;
    if (target[0] != '\0') {
      strncat(target, " ", targetSize - strlen(target) - 1);
    }
    strncat(target, noteLabel, targetSize - strlen(target) - 1);
  }
}

uint64_t sequencerStepDurationMicros() {
  byte tempo = (sequencerTempo == 0) ? 1 : sequencerTempo;
  return 60000000ULL / static_cast<uint64_t>(tempo) / 4ULL;
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
  if (!sequencerPlaybackNoteActive) {
    return;
  }
  for (byte i = 0; i < sequencerPlaybackNoteCount; i++) {
    if (sequencerPlaybackMidiNotes[i] < 128) {
      sendBoardPreviewMidiNote(sequencerPlaybackMidiNotes[i], false);
    }
    sequencerPlaybackMidiNotes[i] = SEQUENCER_NO_NOTE;
  }
  sequencerPlaybackNoteCount = 0;
  sequencerPlaybackNoteActive = false;
}

void clearSelectedSequencerStep() {
  if (sequencerSelectedStep < 0) {
    return;
  }
  clearSequencerNoteBuffer(sequencerEditMidiNotes, sequencerEditNoteCount);
  saveEditBufferToStep(static_cast<byte>(sequencerSelectedStep));
  sequencerOverlayMode = SequencerOverlayMode::StepCleared;
  sequencerOverlayDirty = true;
  sequencerConfirmHeld = false;
  sequencerConfirmPressedAt = 0;
}

void setSequencerTransportState(byte newState) {
  byte normalizedState = (newState == SEQUENCER_TRANSPORT_PLAY) ? SEQUENCER_TRANSPORT_PLAY : SEQUENCER_TRANSPORT_STOP;
  sequencerTransportState = normalizedState;
  if (sequencerTransportState == SEQUENCER_TRANSPORT_PLAY) {
    sequencerPlayingStep = -1;
    sequencerPingPongDelta = 1;
    sequencerNextStepAt = runTime;
    sequencerCurrentStepStartedAt = runTime;
    sequencerPlaybackNoteOffAt = 0;
  } else {
    stopSequencerPlaybackNote();
    sequencerPlayingStep = -1;
    sequencerNextStepAt = 0;
    sequencerCurrentStepStartedAt = 0;
    sequencerPlaybackNoteOffAt = 0;
  }
  menu.drawMenu();
}

void sequencerTransportMenuCallback(GEMCallbackData callbackData) {
  setSequencerTransportState(callbackData.valByte);
}

void sequencerTempoMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
}

void sequencerStepPlayCountMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  if (sequencerStepPlayCount < 1) {
    sequencerStepPlayCount = 1;
  } else if (sequencerStepPlayCount > SEQUENCER_STEP_COUNT) {
    sequencerStepPlayCount = SEQUENCER_STEP_COUNT;
  }
}

void sequencerDirectionMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
  sequencerPingPongDelta = 1;
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
GEMItem menuItemSequencerPlayStop("Play/Stop", sequencerTransportState, selectSequencerTransport, sequencerTransportMenuCallback);
GEMItem menuItemSequencerStepPlayCount("Steps", sequencerStepPlayCount, spinnerSequencerStepPlayCount, sequencerStepPlayCountMenuCallback);
GEMItem menuItemSequencerDirection("Direction", sequencerDirection, selectSequencerDirection, sequencerDirectionMenuCallback);
GEMItem menuItemSequencerTempo("Tempo", sequencerTempo, spinnerSequencerTempo, sequencerTempoMenuCallback);
GEMItem menuItemSequencerBtnHue("Btn Hue", sequencerConfirmHue, spinnerSequencerHue, sequencerConfirmHueMenuCallback);
GEMItem menuItemSequencerBtnSat("Btn Sat", sequencerConfirmSaturation, spinnerSequencerSat, sequencerConfirmSatMenuCallback);
GEMItem menuItemSequencerBtnVal("Btn Val", sequencerConfirmValue, spinnerSequencerVal, sequencerConfirmValMenuCallback);
GEMItem menuItemSequencerFirmwareUpdate("Update Firmware", rebootToBootloader);

}  // namespace

GEMPage menuPageSequencer("Sequencer");

void handleSequencerButtonEvent(byte buttonIndex, bool pressed) {
  if (!pressed) {
    if (buttonIndex == SEQUENCER_CONFIRM_BUTTON_INDEX) {
      if (sequencerSelectedStep >= 0 && sequencerConfirmHeld) {
        clearSequencerNoteBuffer(sequencerEditMidiNotes, sequencerEditNoteCount);
        for (byte i = 0; i < sequencerUndoNoteCount && i < SEQUENCER_MAX_NOTES_PER_STEP; i++) {
          sequencerEditMidiNotes[i] = sequencerUndoMidiNotes[i];
        }
        sequencerEditNoteCount = sequencerUndoNoteCount;
        saveEditBufferToStep(static_cast<byte>(sequencerSelectedStep));
        sequencerOverlayMode = SequencerOverlayMode::AwaitingNote;
        sequencerOverlayDirty = true;
      }
      sequencerConfirmHeld = false;
      sequencerConfirmPressedAt = 0;
      return;
    }

    byte releasedMidiNote = 0;
    if (getButtonMidiNoteForSequencer(buttonIndex, releasedMidiNote) && releasedMidiNote < 128) {
      if (sequencerPreviewHeldNoteCounts[releasedMidiNote] > 0) {
        sequencerPreviewHeldNoteCounts[releasedMidiNote]--;
        if (sequencerPreviewHeldNoteCounts[releasedMidiNote] == 0) {
          sendBoardPreviewMidiNote(releasedMidiNote, false);
        }
      }
    }
    return;
  }

  screenTime = 0;
  if (screenSaverOn) {
    screenSaverOn = false;
    u8g2.setContrast(SEQUENCER_OVERLAY_CONTRAST);
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
      sequencerOverlayMode = SequencerOverlayMode::Hidden;
      sequencerOverlayUntil = 0;
      sequencerOverlayDirty = false;
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

  if (sequencerSelectedStep < 0) {
    return;
  }

  byte midiNote = 0;
  if (getButtonMidiNoteForSequencer(buttonIndex, midiNote)) {
    toggleEditBufferNote(midiNote);
    saveEditBufferToStep(static_cast<byte>(sequencerSelectedStep));
    sequencerOverlayMode = SequencerOverlayMode::AwaitingNote;
    if (midiNote < 128) {
      if (sequencerPreviewHeldNoteCounts[midiNote] == 0) {
        sendBoardPreviewMidiNote(midiNote, true);
      }
      if (sequencerPreviewHeldNoteCounts[midiNote] < 255) {
        sequencerPreviewHeldNoteCounts[midiNote]++;
      }
    }
    sequencerOverlayDirty = true;
  }
}

void setupSequencerMenu() {
  menuItemSequencerBtnHue.setPreviewCallback(previewSequencerConfirmHue);
  menuItemSequencerBtnSat.setPreviewCallback(previewSequencerConfirmSat);
  menuItemSequencerBtnVal.setPreviewCallback(previewSequencerConfirmVal);

  menuPageSequencer.addMenuItem(menuItemEnterKeyboard);
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
  if (sequencerOverlayMode == SequencerOverlayMode::Hidden || sequencerSelectedStep < 0) {
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

  if (sequencerOverlayMode == SequencerOverlayMode::NoteAssigned && runTime >= sequencerOverlayUntil) {
    sequencerSelectedStep = -1;
    sequencerOverlayMode = SequencerOverlayMode::Hidden;
    sequencerOverlayVisible = false;
    sequencerOverlayDirty = false;
    menu.drawMenu();
    return;
  }

  if (!sequencerOverlayDirty && sequencerOverlayVisible) {
    return;
  }

  char headerLabel[20];
  char stepLabel[16];
  char noteLineOne[24];
  char noteLineTwo[24];
  char hintLineOne[24];
  char hintLineTwo[24];
  snprintf(stepLabel, sizeof(stepLabel), "Step %02d", sequencerSelectedStep + 1);
  fillOverlayNoteLines(noteLineOne, sizeof(noteLineOne), noteLineTwo, sizeof(noteLineTwo));
  hintLineOne[0] = '\0';
  hintLineTwo[0] = '\0';

  if (sequencerOverlayMode == SequencerOverlayMode::AwaitingNote) {
    snprintf(headerLabel, sizeof(headerLabel), "Edit Chord");
    snprintf(hintLineOne, sizeof(hintLineOne), "Bottom 10 rows");
    snprintf(hintLineTwo, sizeof(hintLineTwo), "Blue key undoes");
  } else if (sequencerOverlayMode == SequencerOverlayMode::StepCleared) {
    snprintf(headerLabel, sizeof(headerLabel), "Note(s) Erased");
    snprintf(hintLineOne, sizeof(hintLineOne), "Press blue key");
    snprintf(hintLineTwo, sizeof(hintLineTwo), "to undo");
  } else {
    snprintf(headerLabel, sizeof(headerLabel), "Chord Saved");
  }

  sequencerOverlayVisible = true;
  sequencerOverlayDirty = false;

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawStr(20, 18, headerLabel);
  u8g2.drawStr(36, 36, stepLabel);
  if (hintLineOne[0] != '\0') {
    u8g2.drawStr(8, 54, hintLineOne);
  }
  if (hintLineTwo[0] != '\0') {
    u8g2.drawStr(4, 68, hintLineTwo);
  }
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawStr(12, 96, noteLineOne);
  if (noteLineTwo[0] != '\0') {
    u8g2.drawStr(12, 112, noteLineTwo);
  }
  u8g2.sendBuffer();
}

void applySequencerLedOverrides() {
  strip.setPixelColor(
    SEQUENCER_TRANSPORT_BUTTON_INDEX,
    getSequencerTransportLedColor(sequencerTransportState == SEQUENCER_TRANSPORT_PLAY));
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

  if (sequencerTransportState != SEQUENCER_TRANSPORT_PLAY) {
    return;
  }

  if (sequencerPlaybackNoteActive && runTime >= sequencerPlaybackNoteOffAt) {
    stopSequencerPlaybackNote();
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

  byte gatePercent = sequencerStepGatePercent[sequencerPlayingStep];
  sequencerPlaybackNoteCount = 0;
  for (byte i = 0; i < noteCount && i < SEQUENCER_MAX_NOTES_PER_STEP; i++) {
    byte midiNote = sequencerStepMidiNotes[sequencerPlayingStep][i];
    if (midiNote >= 128) {
      continue;
    }
    sendBoardPreviewMidiNote(midiNote, true);
    sequencerPlaybackMidiNotes[sequencerPlaybackNoteCount++] = midiNote;
  }
  if (sequencerPlaybackNoteCount == 0) {
    return;
  }
  sequencerPlaybackNoteActive = true;
  sequencerPlaybackNoteOffAt = sequencerCurrentStepStartedAt + ((stepDuration * gatePercent) / 100ULL);
}
