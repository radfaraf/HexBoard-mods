#pragma once

#include <Arduino.h>
#include <GEM_u8g2.h>

extern int sequencerConfirmHue;
extern byte sequencerConfirmSaturation;
extern byte sequencerConfirmValue;

struct SequencerTunedNoteHandle {
  bool active = false;
  bool useSynth = false;
  bool releaseMidiChannel = false;
  int16_t pitchSteps = 0;
  byte midiNote = 0;
  byte midiChannel = 0;
  int16_t synthSlot = -1;
};

struct SequencerPersistentSettings {
  byte tapPreview = 0;
  byte clockSource = 0;
  byte sendClock = 0;
  byte sendTransport = 0;
  byte stepAccentEvery = 0;
  byte stepColorMode = 1;
  byte stepHue = 0;
};

bool getButtonPitchStepsForSequencer(byte buttonIndex, int16_t& pitchSteps);
bool isBoardButtonPressed(byte buttonIndex);
bool getBoardLedColorForPitchSteps(int16_t pitchSteps, bool highlighted, uint32_t& colorOut);
bool getBoardSelectedLedColorForPitchSteps(int16_t pitchSteps, uint32_t& colorOut);
bool getBoardSelectedAccentedLedColorForPitchSteps(int16_t pitchSteps, uint32_t& colorOut);
bool getBoardAccentedLedColorForPitchSteps(int16_t pitchSteps, uint32_t& colorOut);
bool getBoardBaseLedColorForPitchSteps(int16_t pitchSteps, float& hueOut, byte& satOut);
byte applyBoardRestLedLevel(byte value);
uint32_t buildBoardLedColor(float hue, byte sat, byte val);
uint32_t buildBoardLinearLedColor(float hue, byte sat, byte val);
uint32_t gammaBoardLedColor(uint32_t color);
float getBoardNamedHue(byte hueIndex);
byte getSequencerTuningCycleLength();
int getSequencerCurrentTranspose();
void formatBoardPitchStepsForSequencer(int16_t pitchSteps, char* out, size_t outSize);
uint32_t getSequencerTransportLedColor(bool running);
uint32_t getSequencerConfirmLedColor();
uint32_t getSequencerUtilityLedColor(bool highlighted);
uint32_t getSequencerUnsetStepLedColor(bool highlighted);
void sendSequencerMidiClockPulse();
void sendSequencerMidiTransportStart();
void sendSequencerMidiTransportStop();
SequencerPersistentSettings getSequencerPersistentSettings();
void applySequencerPersistentSettings(const SequencerPersistentSettings& values);
void persistSequencerGeneralSettingsToProfile();
bool startSequencerTunedNote(int16_t pitchSteps, bool useSynth, byte velocity, SequencerTunedNoteHandle& handle);
void stopSequencerTunedNote(SequencerTunedNoteHandle& handle);
void enterKeyboardMode();

extern GEMPage menuPageSequencer;

void handleSequencerButtonEvent(byte buttonIndex, bool pressed);
void setupSequencerMenu();
void drawSequencerOverlay();
void applySequencerLedOverrides();
bool handleSequencerRotaryTurn(int8_t direction);
bool handleSequencerEncoderClick();
void updateSequencerTransport();
void handleSequencerExternalMidiClock();
void handleSequencerExternalMidiStart();
void handleSequencerExternalMidiStop();
void handleSequencerExternalMidiContinue();
bool shouldShowSequencerPlayedNotesOverlay();
byte rebuildSequencerDisplayedNotes(int16_t* notes, byte maxCount);
