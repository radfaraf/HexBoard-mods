#pragma once

#include <Arduino.h>
#include <GEM_u8g2.h>

extern int sequencerConfirmHue;
extern byte sequencerConfirmSaturation;
extern byte sequencerConfirmValue;

bool getButtonMidiNoteForSequencer(byte buttonIndex, byte& midiNote);
bool isBoardButtonPressed(byte buttonIndex);
bool getBoardLedColorForMidiNote(byte midiNote, bool highlighted, uint32_t& colorOut);
uint32_t getSequencerTransportLedColor(bool running);
uint32_t getSequencerConfirmLedColor();
uint32_t getSequencerUnsetStepLedColor(bool highlighted);
void sendBoardPreviewMidiNote(byte midiNote, bool noteOn);
void sendBoardPreviewSynthNote(byte midiNote, bool noteOn);
void enterKeyboardMode();

extern GEMPage menuPageSequencer;

void handleSequencerButtonEvent(byte buttonIndex, bool pressed);
void setupSequencerMenu();
void drawSequencerOverlay();
void applySequencerLedOverrides();
bool handleSequencerRotaryTurn(int8_t direction);
void updateSequencerTransport();
