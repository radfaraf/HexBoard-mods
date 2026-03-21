#include "SequencerMode.h"

namespace {

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

void setupSequencerMenu() {
  menuPageSequencer.addMenuItem(menuItemEnterKeyboard);
  menuPageSequencer.addMenuItem(menuItemSequencerPlayStop);
  menuPageSequencer.addMenuItem(menuItemSequencerTempo);
}
