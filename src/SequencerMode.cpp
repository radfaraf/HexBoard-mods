#include "SequencerMode.h"

#include <Adafruit_NeoPixel.h>
#include <cstdio>

extern GEMItem menuItemUSBBootloader;
extern GEM_u8g2 menu;
extern U8G2_SH1107_SEEED_128X128_F_HW_I2C u8g2;
extern bool screenSaverOn;
extern uint64_t screenTime;
extern uint64_t runTime;
extern Adafruit_NeoPixel strip;

namespace {
constexpr byte SEQUENCER_STEP_COUNT = 16;
constexpr byte SEQUENCER_TRANSPORT_BUTTON_INDEX = 9;
constexpr byte SEQUENCER_OVERLAY_CONTRAST = 63;
constexpr byte SEQUENCER_NO_NOTE = 255;
constexpr uint64_t SEQUENCER_NOTE_CONFIRM_MICROS = 2000000ULL;
constexpr byte SEQUENCER_TRANSPORT_STOP = 0;
constexpr byte SEQUENCER_TRANSPORT_PLAY = 1;

byte sequencerStepMidiNote[SEQUENCER_STEP_COUNT] = {
  SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE,
  SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE,
  SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE,
  SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE, SEQUENCER_NO_NOTE
};
byte sequencerStepGatePercent[SEQUENCER_STEP_COUNT] = {
  100, 100, 100, 100, 100, 100, 100, 100,
  100, 100, 100, 100, 100, 100, 100, 100
};

enum class SequencerOverlayMode : uint8_t {
  Hidden = 0,
  AwaitingNote = 1,
  NoteAssigned = 2
};

int8_t sequencerSelectedStep = -1;
SequencerOverlayMode sequencerOverlayMode = SequencerOverlayMode::Hidden;
uint64_t sequencerOverlayUntil = 0;
bool sequencerOverlayVisible = false;
bool sequencerOverlayDirty = false;
int16_t sequencerPreviewButtonIndex = -1;
byte sequencerPreviewMidiNote = SEQUENCER_NO_NOTE;
int8_t sequencerPlayingStep = -1;
bool sequencerPlaybackNoteActive = false;
byte sequencerPlaybackMidiNote = SEQUENCER_NO_NOTE;
uint64_t sequencerNextStepAt = 0;
uint64_t sequencerCurrentStepStartedAt = 0;
uint64_t sequencerPlaybackNoteOffAt = 0;
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

uint64_t sequencerStepDurationMicros() {
  byte tempo = (sequencerTempo == 0) ? 1 : sequencerTempo;
  return 60000000ULL / static_cast<uint64_t>(tempo) / 4ULL;
}

void stopSequencerPlaybackNote() {
  if (!sequencerPlaybackNoteActive) {
    return;
  }
  sendBoardPreviewMidiNote(sequencerPlaybackMidiNote, false);
  sequencerPlaybackNoteActive = false;
}

void setSequencerTransportState(byte newState) {
  byte normalizedState = (newState == SEQUENCER_TRANSPORT_PLAY) ? SEQUENCER_TRANSPORT_PLAY : SEQUENCER_TRANSPORT_STOP;
  sequencerTransportState = normalizedState;
  if (sequencerTransportState == SEQUENCER_TRANSPORT_PLAY) {
    sequencerPlayingStep = -1;
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

const GEMSpinnerBoundariesByte spinnerBoundariesSequencerTempo = { 1, 255, 1 };
GEMSpinner spinnerSequencerTempo(spinnerBoundariesSequencerTempo, GEM_LOOP);

SelectOptionByte optionByteSequencerTransport[] = { { "Stop", 0 }, { "Play", 1 } };
GEMSelect selectSequencerTransport(sizeof(optionByteSequencerTransport) / sizeof(SelectOptionByte), optionByteSequencerTransport);

GEMItem menuItemEnterKeyboard("Keyboard", enterKeyboardMode);
GEMItem menuItemSequencerPlayStop("Play/Stop", sequencerTransportState, selectSequencerTransport, sequencerTransportMenuCallback);
GEMItem menuItemSequencerTempo("Tempo", sequencerTempo, spinnerSequencerTempo, sequencerTempoMenuCallback);

}  // namespace

GEMPage menuPageSequencer("Sequencer");

void handleSequencerButtonEvent(byte buttonIndex, bool pressed) {
  if (!pressed && sequencerPreviewButtonIndex == buttonIndex) {
    sendBoardPreviewMidiNote(sequencerPreviewMidiNote, false);
    sequencerPreviewButtonIndex = -1;
    return;
  }

  if (!pressed) {
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

  int8_t stepIndex = buttonIndexToSequencerStep(buttonIndex);
  if (stepIndex >= 0) {
    if (sequencerPreviewButtonIndex >= 0) {
      sendBoardPreviewMidiNote(sequencerPreviewMidiNote, false);
      sequencerPreviewButtonIndex = -1;
    }
    sequencerSelectedStep = stepIndex;
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
    if (sequencerPreviewButtonIndex >= 0) {
      sendBoardPreviewMidiNote(sequencerPreviewMidiNote, false);
    }
    sequencerStepMidiNote[sequencerSelectedStep] = midiNote;
    sequencerPreviewButtonIndex = buttonIndex;
    sequencerPreviewMidiNote = midiNote;
    sendBoardPreviewMidiNote(midiNote, true);
    sequencerOverlayMode = SequencerOverlayMode::NoteAssigned;
    sequencerOverlayUntil = runTime + SEQUENCER_NOTE_CONFIRM_MICROS;
    sequencerOverlayDirty = true;
  }
}

void setupSequencerMenu() {
  menuPageSequencer.addMenuItem(menuItemEnterKeyboard);
  menuPageSequencer.addMenuItem(menuItemSequencerPlayStop);
  menuPageSequencer.addMenuItem(menuItemSequencerTempo);
  menuPageSequencer.addMenuItem(menuItemUSBBootloader);
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
  char noteLabel[12];
  snprintf(stepLabel, sizeof(stepLabel), "Step %02d", sequencerSelectedStep + 1);
  formatSequencerStepNote(noteLabel, sizeof(noteLabel), sequencerStepMidiNote[sequencerSelectedStep]);

  if (sequencerOverlayMode == SequencerOverlayMode::AwaitingNote) {
    snprintf(headerLabel, sizeof(headerLabel), "Pick Note");
  } else {
    snprintf(headerLabel, sizeof(headerLabel), "Note Assigned");
  }

  sequencerOverlayVisible = true;
  sequencerOverlayDirty = false;

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawStr(20, 18, headerLabel);
  u8g2.drawStr(36, 36, stepLabel);
  if (sequencerOverlayMode == SequencerOverlayMode::AwaitingNote) {
    u8g2.drawStr(10, 54, "Bottom 10 rows");
    u8g2.drawStr(20, 68, "choose pitch");
  }
  u8g2.setFont(u8g2_font_logisoso24_tf);
  u8g2.drawStr(28, 92, noteLabel);
  u8g2.sendBuffer();
}

void applySequencerLedOverrides() {
  strip.setPixelColor(
    SEQUENCER_TRANSPORT_BUTTON_INDEX,
    getSequencerTransportLedColor(sequencerTransportState == SEQUENCER_TRANSPORT_PLAY));

  for (byte step = 0; step < SEQUENCER_STEP_COUNT; step++) {
    int8_t buttonIndex = sequencerStepToButtonIndex(step);
    if (buttonIndex < 0) {
      continue;
    }

    uint32_t colorCode = 0;
    bool highlighted = ((sequencerSelectedStep == step) && (sequencerOverlayMode != SequencerOverlayMode::Hidden))
      || (sequencerPlayingStep == step);
    if (sequencerStepMidiNote[step] >= 128) {
      strip.setPixelColor(buttonIndex, getSequencerUnsetStepLedColor());
    } else if (getBoardLedColorForMidiNote(sequencerStepMidiNote[step], highlighted, colorCode)) {
      strip.setPixelColor(buttonIndex, colorCode);
    }
  }
}

void updateSequencerTransport() {
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
  sequencerPlayingStep = (sequencerPlayingStep + 1) % SEQUENCER_STEP_COUNT;

  if (sequencerStepMidiNote[sequencerPlayingStep] >= 128) {
    return;
  }

  byte gatePercent = sequencerStepGatePercent[sequencerPlayingStep];
  sendBoardPreviewMidiNote(sequencerStepMidiNote[sequencerPlayingStep], true);
  sequencerPlaybackMidiNote = sequencerStepMidiNote[sequencerPlayingStep];
  sequencerPlaybackNoteActive = true;
  sequencerPlaybackNoteOffAt = sequencerCurrentStepStartedAt + ((stepDuration * gatePercent) / 100ULL);
}
