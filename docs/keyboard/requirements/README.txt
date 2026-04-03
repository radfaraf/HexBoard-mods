HEXBOARD KEYBOARD REQUIREMENTS

Purpose

These files describe the intended current behavior of Keyboard mode in a
requirements-style format.

They are meant to complement:

- `docs/keyboard/manuals/keyboard_quick_manual.txt`
- `docs/keyboard/manuals/keyboard_manual.txt`
- `docs/keyboard/layouts/*.txt`

The manuals describe how Keyboard mode is used.
The layout files describe what important menus look like.
The requirements files describe what the system shall do.

Confidence note

This is a first structured pass based on the current firmware code and comments
in `src/HexBoard.ino`.

- Treat these files as the current best structured Keyboard reference.
- Keep them readable.
- Use short confidence notes when a behavior is inferred rather than directly
  hardware-verified.

How to use these files

- Use them as a current source of truth for intended Keyboard behavior.
- Update them when Keyboard behavior changes in a meaningful way.
- Keep them written in terms of current behavior, not old behavior.
- Prefer updating an existing requirement instead of creating a duplicate.
- Do not reuse old IDs for new meanings.
- When a setting is shared with Sequencer mode, use a short `Shared setting:`
  note rather than moving the requirement into a separate shared folder.

Recommended status values

- Active
- Planned
- Removed
- Deferred

Recommended file roles

- `requirements_template.txt`
  Base requirement block format.
- `keyboard_overview_requirements.txt`
  Overall scope, mode split, and core behavior.
- `playing_and_expression_requirements.txt`
  Note playing, wheels, and synth-expression behavior.
- `tunings_layouts_and_scales_requirements.txt`
  Tuning, layout, key, scale, transpose, and JI-related behavior.
- `menus_profiles_and_persistence_requirements.txt`
  Menu structure, profiles, auto-save, and stored settings behavior.
- `screens_and_feedback_requirements.txt`
  OLED, LED, and feedback behavior.
- `midi_and_delegated_mode_requirements.txt`
  MIDI routing, MPE, program change, and delegated-control behavior.
- `technical_limits_requirements.txt`
  Current limits and fixed boundaries.

ID format

IDs are grouped by area, for example:

- `KB-CORE-001`
- `KB-PLAY-010`
- `KB-TUNE-020`
- `KB-MENU-030`
- `KB-UI-040`
- `KB-MIDI-050`
- `KB-LIMIT-060`

Guidelines

- If a requirement changes but is still the same concept, keep the same ID and
  update its text.
- If a requirement is removed, mark it `Removed` or delete it, but do not reuse
  that ID for a different concept later.
- If a requirement is brand new, give it a new ID.
