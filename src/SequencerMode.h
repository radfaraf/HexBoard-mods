#pragma once

#include <Arduino.h>
#include <GEM_u8g2.h>

bool getButtonMidiNoteForSequencer(byte buttonIndex, byte& midiNote);
bool getBoardLedColorForMidiNote(byte midiNote, bool highlighted, uint32_t& colorOut);
void sendBoardPreviewMidiNote(byte midiNote, bool noteOn);
void enterKeyboardMode();

extern GEMPage menuPageSequencer;

void handleSequencerButtonEvent(byte buttonIndex, bool pressed);
void setupSequencerMenu();
void drawSequencerOverlay();
void applySequencerLedOverrides();
