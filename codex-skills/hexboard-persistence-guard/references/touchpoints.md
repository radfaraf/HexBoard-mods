# Persistence Touchpoints

Use this file to find the usual implementation surfaces for persistence changes.

## Board Profile Persistence

Typical touchpoints in `src/HexBoard.ino`:

- `SettingKey`
- `factoryDefaults`
- settings versioning and migration inside `load_settings()`
- `save_settings()`
- `syncSettingsToRuntime()`
- `markSettingsDirty()`
- profile save/load helpers
- `PersistentCallbackInfo`
- `universalSaveCallback()`

Sequencer profile-backed bridges:

- `SequencerPersistentSettings`
- `getSequencerPersistentSettings()`
- `applySequencerPersistentSettings()`
- `persistSequencerGeneralSettingsToProfile()`

Review these when a value is `profile`:

- default value
- Auto-Save behavior
- profile load behavior
- power-up restore behavior
- whether local reset helpers accidentally overwrite it

## Sequencer Saved-File Persistence

Typical touchpoints in `src/SequencerMode.cpp`:

- `saveSequencerToPath()`
- `saveSequencerToCurrentPath()`
- `saveSequencerAsNewInDirectory()`
- `loadSequencerFromPath()`
- `loadSequencerAtStartup()`
- `loadRememberedSequencerCurrentPath()`
- `saveRememberedSequencerCurrentPath()`
- `resetSequencerState()`
- `saveSequencerMenuCallback()`
- `revertSequencerMenuCallback()`
- `newSequencerMenuCallback()`

Review these when a value is `saved document/sequence`:

- file format read/write
- default when creating a blank/new sequence
- load behavior
- revert behavior
- current-file-path behavior
- dirty-state behavior if applicable

## Runtime-Only State

Typical touchpoints:

- overlay state enums and flags
- temporary edit buffers
- transport timing values
- current held-note or active-note bookkeeping
- ephemeral status messages and monitor values

Review these when a value is `runtime-only`:

- confirm it is not written to profile settings
- confirm it is not written to saved files
- confirm reset helpers clear it as needed

## Startup / Reset Review

Whenever persistence changes, review:

- profile load on boot
- startup file restore
- local reset helpers
- `New`
- `Revert`
- file load
- profile load

These paths often reveal mismatches between the intended bucket and the actual behavior.

## Docs To Update

For Sequencer-related persistence changes, usually review:

- `docs/sequencer_quick_manual.txt`
- `docs/sequencer_manual.txt`
- `docs/requirements/storage_and_files_requirements.txt`
- `docs/requirements/menus_and_settings_requirements.txt`
- `docs/requirements/playback_and_timing_requirements.txt`
- relevant files in `docs/layouts/`

For broader board-profile changes, also review any repo instructions or project docs that describe profile behavior.

## Migration Reminder

If adding or moving persistent values:

- check default value behavior
- check settings migration/version handling
- remove the old persistence path if the bucket changed
- make sure the docs describe only the new behavior
