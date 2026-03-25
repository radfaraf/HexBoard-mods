# HexBoard Sequencer Project Instructions

These are standing instructions for work on the Sequencer for Codex to follow.

## Core Project Rules

- Always maintain Arduino IDE compatibility.
- Keep Keyboard mode and Sequencer mode separated as much as practical.
- Prefer keeping Keyboard-related logic in `src/HexBoard.ino`.
- Prefer keeping Sequencer-related logic in `src/SequencerMode.cpp` and `src/SequencerMode.h`.
- Only leave bridge/helper code in `src/HexBoard.ino` when it is truly needed for shared board access or integration.

## Sequencer Manual Maintenance

Whenever Sequencer mode behavior, controls, menus, file management, naming, playback, or editing workflow changes, update the sequencer manuals to match the current behavior. Or any other Sequencer related features.

Preferred workflow for larger Sequencer changes:

- do the code changes first
- then tell the user the code work is done and ready for testing
- wait for the user to say to proceed with documentation updates when practical
- if the user forgets to confirm after testing time has passed, remind them that
  the manuals and layout docs still need updating
- once confirmed, update the documentation files and regenerate the PDFs

Manual files:

- `docs/sequencer_quick_manual.txt`
- `docs/sequencer_quick_manual.pdf`
- `docs/sequencer_manual.txt`
- `docs/sequencer_manual.pdf`

Manual roles:

- `sequencer_quick_manual` is the short manual.
- `sequencer_manual` is the more detailed manual.

When the manuals are updated:

- keep the quick manual brief but complete
- keep the detailed manual clearer and more thorough without becoming bloated
- avoid leaving outdated behavior or superseded workflows in either manual
- regenerate the PDF versions after updating the text versions

## Layout Reference Maintenance

Keep the layout reference files in `docs/layouts` current whenever a sequencer
screen, menu, browser flow, naming screen, or other structured UI layout
changes in a meaningful way. 

Current layout reference folder:

- `docs/layouts/`

When needed:

- update the existing layout reference text files
- create new layout reference files for newly added complex screens or menus
- remove or revise outdated layout descriptions so they match current behavior

These layout files are documentation only and are not part of the firmware.

## Maintenance Preference

- If a behavior has changed over time, make sure the current code and the manuals reflect only the latest intended behavior.
