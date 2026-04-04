# Persistence Categories

Use this file to map HexBoard values into the correct persistence bucket.

## Profile

Use `profile` for board preferences that should survive power cycles and profile loads.

Common examples in this repo:

- Keyboard tuning, layout, scale, key, transpose, mirror, and rotate settings
- board settings stored in `settings.dat` through `SettingKey`
- Auto-Save and other board-level preferences
- display and LED preference settings such as `DisplayNotes`, brightness, and color settings
- Sequencer general preferences that are not sequence-file data:
  - `Tap Preview`
  - `Clock Source`
  - `Send Clock`
  - `Send Transport`

## Saved Document / Sequence

Use `saved document/sequence` for data that belongs to one saved work item and should change when a different file is loaded.

Common Sequencer examples:

- current sequence note data
- per-step gate/length
- per-step velocity
- per-step probability
- sequence tempo
- active step count
- play type
- direction
- remembered sequence file path metadata used to reopen the current sequence

## Runtime-Only

Use `runtime-only` for temporary state that should not be restored from profiles or files.

Common examples:

- overlay visibility flags
- held-button timing and pressed-at timestamps
- current audition or playback bookkeeping
- temporary editor buffers
- one-shot status messages
- transient browser UI state
- active selections and temporary screen modes

## Anti-Patterns

Do not:

- save global preferences into sequence files
- forget to add a new profile-backed setting to the board settings table, default value, sync path, and dirty/save path
- reset profile-backed settings inside local reset helpers
- document a value as profile-backed while the code still loads it from a file
- leave old storage behavior in place after moving a setting to a different bucket

## Bucket Change Rule

When moving a value between buckets:

- remove the old persistence path
- add the new persistence path
- check defaults and migration behavior
- update docs in the same pass
