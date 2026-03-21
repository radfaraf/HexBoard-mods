#include "SequencerMode.h"

#include <cstdio>

extern GEM_u8g2 menu;
extern U8G2_SH1107_SEEED_128X128_F_HW_I2C u8g2;
extern bool screenSaverOn;
extern uint64_t screenTime;

namespace {
constexpr byte SEQUENCER_STEP_COUNT = 16;
constexpr byte SEQUENCER_OVERLAY_CONTRAST = 63;
constexpr byte SEQUENCER_DEFAULT_MIDI_NOTE = 60;  // C4

bool sequencerStepHeld[SEQUENCER_STEP_COUNT] = { false };
byte sequencerStepMidiNote[SEQUENCER_STEP_COUNT] = {
  SEQUENCER_DEFAULT_MIDI_NOTE, SEQUENCER_DEFAULT_MIDI_NOTE, SEQUENCER_DEFAULT_MIDI_NOTE, SEQUENCER_DEFAULT_MIDI_NOTE,
  SEQUENCER_DEFAULT_MIDI_NOTE, SEQUENCER_DEFAULT_MIDI_NOTE, SEQUENCER_DEFAULT_MIDI_NOTE, SEQUENCER_DEFAULT_MIDI_NOTE,
  SEQUENCER_DEFAULT_MIDI_NOTE, SEQUENCER_DEFAULT_MIDI_NOTE, SEQUENCER_DEFAULT_MIDI_NOTE, SEQUENCER_DEFAULT_MIDI_NOTE,
  SEQUENCER_DEFAULT_MIDI_NOTE, SEQUENCER_DEFAULT_MIDI_NOTE, SEQUENCER_DEFAULT_MIDI_NOTE, SEQUENCER_DEFAULT_MIDI_NOTE
};
int8_t sequencerSelectedStep = -1;
bool sequencerOverlayVisible = false;
bool sequencerOverlayDirty = false;

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

byte firstHeldSequencerStep() {
  for (byte step = 0; step < SEQUENCER_STEP_COUNT; step++) {
    if (sequencerStepHeld[step]) {
      return step;
    }
  }
  return SEQUENCER_STEP_COUNT;
}

void formatSequencerStepNote(char* out, size_t outSize, byte midiNote) {
  const char* label = sequencerChromaticNames[midiNote % 12];
  int octave = (midiNote / 12) - 1;
  snprintf(out, outSize, "%s%d", label, octave);
}

void sequencerPlaceholderMenuCallback(GEMCallbackData callbackData) {
  (void)callbackData;
}

const GEMSpinnerBoundariesByte spinnerBoundariesSequencerTempo = { 1, 255, 1 };
GEMSpinner spinnerSequencerTempo(spinnerBoundariesSequencerTempo, GEM_LOOP);

byte sequencerTempo = 120;
byte sequencerTransportState = 0;
SelectOptionByte optionByteSequencerTransport[] = { { "Stop", 0 }, { "Play", 1 } };
GEMSelect selectSequencerTransport(sizeof(optionByteSequencerTransport) / sizeof(SelectOptionByte), optionByteSequencerTransport);

GEMItem menuItemEnterKeyboard("Keyboard", enterKeyboardMode);
GEMItem menuItemSequencerPlayStop("Play/Stop", sequencerTransportState, selectSequencerTransport, sequencerPlaceholderMenuCallback);
GEMItem menuItemSequencerTempo("Tempo", sequencerTempo, spinnerSequencerTempo, sequencerPlaceholderMenuCallback);

}  // namespace

GEMPage menuPageSequencer("Sequencer");

void handleSequencerButtonEvent(byte buttonIndex, bool pressed) {
  int8_t stepIndex = buttonIndexToSequencerStep(buttonIndex);
  if (stepIndex < 0) {
    return;
  }

  sequencerStepHeld[stepIndex] = pressed;
  screenTime = 0;
  if (screenSaverOn) {
    screenSaverOn = false;
    u8g2.setContrast(SEQUENCER_OVERLAY_CONTRAST);
  }

  if (pressed) {
    sequencerSelectedStep = stepIndex;
  } else if (sequencerSelectedStep == stepIndex) {
    byte replacementStep = firstHeldSequencerStep();
    sequencerSelectedStep = (replacementStep < SEQUENCER_STEP_COUNT) ? static_cast<int8_t>(replacementStep) : -1;
  }

  sequencerOverlayDirty = true;
}

void setupSequencerMenu() {
  menuPageSequencer.addMenuItem(menuItemEnterKeyboard);
  menuPageSequencer.addMenuItem(menuItemSequencerPlayStop);
  menuPageSequencer.addMenuItem(menuItemSequencerTempo);
}

void drawSequencerOverlay() {
  if (sequencerSelectedStep < 0) {
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

  if (!sequencerOverlayDirty && sequencerOverlayVisible) {
    return;
  }

  char stepLabel[16];
  char noteLabel[12];
  snprintf(stepLabel, sizeof(stepLabel), "Step %02d", sequencerSelectedStep + 1);
  formatSequencerStepNote(noteLabel, sizeof(noteLabel), sequencerStepMidiNote[sequencerSelectedStep]);

  sequencerOverlayVisible = true;
  sequencerOverlayDirty = false;

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawStr(18, 18, "Sequencer Note");
  u8g2.drawStr(36, 36, stepLabel);
  u8g2.setFont(u8g2_font_logisoso24_tf);
  u8g2.drawStr(28, 92, noteLabel);
  u8g2.sendBuffer();
}
