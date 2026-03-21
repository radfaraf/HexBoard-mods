#pragma once

#include <Arduino.h>
#include <GEM_u8g2.h>

bool getButtonMidiNoteForSequencer(byte buttonIndex, byte& midiNote);
bool getBoardLedColorForMidiNote(byte midiNote, bool highlighted, uint32_t& colorOut);
uint32_t getSequencerTransportLedColor(bool running);
uint32_t getSequencerConfirmLedColor();
uint32_t getSequencerUnsetStepLedColor(bool highlighted);
void sendBoardPreviewMidiNote(byte midiNote, bool noteOn);
void enterKeyboardMode();

extern GEMPage menuPageSequencer;

void handleSequencerButtonEvent(byte buttonIndex, bool pressed);
void setupSequencerMenu();
void drawSequencerOverlay();
void applySequencerLedOverrides();
void updateSequencerTransport();
