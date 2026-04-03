HEXBOARD DOCUMENTATION

Purpose

This folder now separates Keyboard and Sequencer documentation so the two
mode-specific doc sets do not get mixed together.

Top-level structure

- `docs/keyboard/`
  Keyboard manuals, requirements, and layout references.
- `docs/sequencer/`
  Sequencer manuals, requirements, and layout references.
- `docs/TODO.txt`
  Personal project notes. Do not rewrite unless explicitly asked.
- other top-level text files
  Project-specific notes that are not part of the formal mode documentation.

Confidence note

- The Sequencer documentation was built alongside the feature work and is
  generally the higher-confidence user reference.
- The Keyboard documentation is a first structured pass reconstructed from the
  current firmware code and comments in `src/HexBoard.ino`.
- Keyboard manuals and requirements should stay readable, but may include short
  confidence notes where behavior is inferred rather than hardware-verified.

Shared behavior policy

- Keep Keyboard and Sequencer documentation in their own folders.
- Do not create a large shared manual set.
- When a setting is visible from Keyboard mode and also affects Sequencer mode,
  document it in the Keyboard docs with a short `Shared setting:` note.
- When a setting is visible from Sequencer mode and also affects Keyboard mode,
  document it in the Sequencer docs with a short `Shared setting:` note.

Update guidance

- Update the relevant mode folder whenever that mode changes in a meaningful
  way.
- Keep layout references in the matching mode folder.
- Keep requirements written as current intended behavior, not implementation
  history.
