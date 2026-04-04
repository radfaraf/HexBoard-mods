---
name: hexboard-persistence-guard
description: Enforce whole-board HexBoard persistence decisions for settings, profiles, Auto-Save, save/load behavior, startup restore, defaults, migrations, and any question about where a value should be stored. Use when adding or changing menu options, state fields, file-format entries, reset helpers, or restore behavior so the value is classified as profile, saved document/sequence, or runtime-only before implementation.
---

# HexBoard Persistence Guard

Use this skill whenever a HexBoard change touches settings or persistent state.

Treat these rules as strong defaults unless the user explicitly overrides them.

## Core Rule

Before implementation, classify each new or changed value as exactly one of:

- `profile`
  Board preference saved in the profile system and restored on power-up or profile load.
- `saved document/sequence`
  Data stored in the relevant saved file and restored when that file is loaded.
- `runtime-only`
  Temporary state that must not be written to profiles or saved files.

If the value does not fit cleanly into one of those buckets, stop and call out the exception instead of guessing.

## Required Decision Flow

1. Classify the value as `profile`, `saved document/sequence`, or `runtime-only`.
2. Decide which paths must overwrite or preserve it:
   - profile load
   - startup restore
   - document/sequence load
   - `New`
   - `Revert`
   - local reset helpers
3. Update only the persistence system that matches the chosen bucket.
4. Remove the value from any old persistence path if the bucket changed.
5. Update docs so the written behavior matches the code.

## Strong Rules

- `profile` values must be wired into the board settings/profile system and Auto-Save path.
- `saved document/sequence` values must be wired into the correct file format plus matching load/save logic.
- `runtime-only` state must not leak into profiles or saved files.
- Reset helpers must preserve `profile` values unless the intended behavior explicitly says otherwise.
- `New`, `Revert`, startup restore, and file load must follow the chosen bucket consistently.
- If a change moves a value from one bucket to another, remove the old storage path and update the docs in the same pass.

## Use The References

- Read [references/persistence-categories.md](references/persistence-categories.md) for known examples and anti-patterns.
- Read [references/touchpoints.md](references/touchpoints.md) before editing persistence-related code.
- For Sequencer persistence changes, keep `src/HexBoard.ino` for profile/shared persistence and `src/SequencerMode.cpp` for sequence-file and Sequencer runtime behavior unless a bridge is truly required.

## Output Expectations

When using this skill during a task:

- state the chosen bucket clearly
- mention what should overwrite or preserve the value
- name the persistence touchpoints that must change
- name the docs that must change
- warn about mismatches such as `saved in file but reset as runtime` or `profile-backed but overwritten by sequence load`
