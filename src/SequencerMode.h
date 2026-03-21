#pragma once

#include <Arduino.h>
#include <GEM_u8g2.h>

bool getButtonMidiNoteForSequencer(byte buttonIndex, byte& midiNote);
void enterKeyboardMode();

extern GEMPage menuPageSequencer;

void handleSequencerButtonEvent(byte buttonIndex, bool pressed);
void setupSequencerMenu();
void drawSequencerOverlay();
