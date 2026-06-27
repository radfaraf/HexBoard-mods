# HexBoard Completed Port Ledger

Last updated: 2026-06-27 2:06PM EDT

This file stores detailed completion records for porting work from the old
`hexboard-sequencer` branch into upstream HexBoard `development`.

Use [porting-coordination.md](porting-coordination.md) as the active planning
and decision ledger. That file keeps the compact status table, current next
selection, candidate work, deferred work, and open questions.

When a slice completes, update both:

- the compact status table in `docs/porting-coordination.md`
- the detailed completion entry in this file

## Completed Port Details

### Sequencer feature flag and shell

- Sequencer roadmap slice: 1. Compile-time feature flag plus sequencer shell.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/sequencer-feature-flag-shell`.
- Aggregate PR:
  `https://github.com/shapingthesilence/HexBoard/pull/16`
  (`shapingthesilence/HexBoard#16`), opened after final HEAD `119a7bb`.
- Implementation commit: `80cd76b` (`Add optional sequencer feature shell`).
- Changed files reported/reviewed: `Makefile`, `README.md`,
  `docs/developer-guide.md`, `docs/user-manual.md`,
  `src/firmware/config/FeatureFlags.h`,
  `src/firmware/menu/MenuAndDisplay.cpp`,
  `src/firmware/sequencer/SequencerMode.h`, and
  `src/firmware/sequencer/SequencerMode.cpp`.
- Behavior completed: `HEXBOARD_ENABLE_SEQUENCER` defaults to `0`; disabled
  builds install no Sequencer menu item and add no sequencer runtime behavior.
  Enabled builds install only a top-level `Sequencer` placeholder page. No old
  sequencer editing, playback, storage, MIDI sync, LED override, settings, or
  alternate board-mode behavior was bulk-ported.
- Verification: `git diff --check upstream/development..HEAD` passed. Default
  temp-folder build with `HEXBOARD_ENABLE_SEQUENCER=0` passed with `648544`
  bytes program storage and `191348` bytes globals; firmware artifact:
  `/private/tmp/hexboard-sequencer-flag-disabled/HexBoard/build/HexBoard.ino.uf2`.
  Enabled temp-folder build with `HEXBOARD_ENABLE_SEQUENCER=1` passed with
  `648816` bytes program storage and `191644` bytes globals; firmware artifact:
  `/private/tmp/hexboard-sequencer-flag-enabled/HexBoard/build/HexBoard.ino.uf2`.
  Robert manually tested and verified both firmware versions.
- Review notes: planning-thread review found no blocking issues. The enabled
  shell uses sequencer-owned static GEM objects and a no-op disabled build path,
  while the existing root sketch stays thin. The only integration point is
  `setupSequencerMenu()` from `MenuAndDisplay.cpp`.
- Next: the following Step Note Entry Verification And Basic Transport Playback
  slice has since been completed and recorded below.

### Step note entry and basic transport playback

- Sequencer roadmap slices: 2. Core sequencer editing, plus the basic MIDI
  portion of 3. Sequencer playback.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/sequencer-feature-flag-shell`.
- Implementation commits after the shell: `3f38289` (`Add sequencer mode
  foundation`), `111eafc` (`Separate sequencer build artifacts`), `0f88510`
  (`Match sequencer utility LED colors`), `ed7e02c` (`Enable sequencer
  transport playback in staged sketch build`), and `827abb1` (`update
  sequencer docs`).
- Changed files reported/reviewed: `Makefile`, `README.md`,
  `docs/code-analysis.md`, `docs/developer-guide.md`,
  `docs/sequencer-manual.md`, `docs/user-manual.md`,
  `src/firmware/app/Runtime.cpp`,
  `src/firmware/hardware/GridScanRotary.cpp`,
  `src/firmware/hardware/LedRender.cpp`,
  `src/firmware/sequencer/SequencerInput.cpp`,
  `src/firmware/sequencer/SequencerInput.h`,
  `src/firmware/sequencer/SequencerLeds.cpp`,
  `src/firmware/sequencer/SequencerLeds.h`,
  `src/firmware/sequencer/SequencerMidi.cpp`,
  `src/firmware/sequencer/SequencerMidi.h`,
  `src/firmware/sequencer/SequencerMode.cpp`,
  `src/firmware/sequencer/SequencerMode.h`,
  `src/firmware/sequencer/SequencerState.cpp`,
  `src/firmware/sequencer/SequencerState.h`,
  `src/firmware/sequencer/SequencerTransport.cpp`,
  `src/firmware/sequencer/SequencerTransport.h`, and
  `src/firmware/synth/SynthVoiceAllocation.cpp`.
- Behavior completed: enabled sequencer builds now support 32-step
  selection/deselection, tuning-relative selected-step note toggling, up to 6
  notes per step, selected-step clear on button `19`, transport toggle on
  button `9`, fixed 120 BPM internal forward playback across all 32 steps,
  MIDI output through the current tuning/transpose/routing/MPE helpers,
  sequencer LED overrides, and panic/exit note release.
- Behavior intentionally not included: persistence, save/load/browser flows,
  external sync, MIDI clock/transport send, onboard synth sequencer playback,
  audition/preview sound, direction modes, probability, ties, detailed tools,
  and settings schema changes.
- Verification: `git diff --check 80cd76b..codex/sequencer-feature-flag-shell`
  passed. Robert already built and hardware-tested the disabled and enabled
  variants.
- Review notes: planning-thread static review found no blocking issues. The
  implementation keeps sequencer-owned state, input, LED, MIDI, and transport
  code under `src/firmware/sequencer/`. Integration remains narrow: runtime
  service call, grid event handoff while Sequencer mode is active, LED render
  override while active, and panic-stop release. The default disabled build
  remains intended to have no Sequencer menu or runtime behavior.
- Next: the following Sequencer Playback Controls Foundation slice has since
  been completed and recorded below.

### Sequencer playback controls foundation

- Sequencer roadmap slice: focused continuation of 3. Sequencer playback.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/sequencer-feature-flag-shell`.
- Implementation commits after the basic playback slice: `a87a1fc` (`Add
  volatile sequencer playback settings`) and `19bc37d` (`update manual with
  respect to sequencer wording`).
- Changed files reported/reviewed: `README.md`, `docs/code-analysis.md`,
  `docs/developer-guide.md`, `docs/sequencer-manual.md`,
  `docs/user-manual.md`, `src/firmware/sequencer/SequencerLeds.cpp`,
  `src/firmware/sequencer/SequencerMode.cpp`,
  `src/firmware/sequencer/SequencerPlaybackMenu.cpp`,
  `src/firmware/sequencer/SequencerPlaybackMenu.h`,
  `src/firmware/sequencer/SequencerPlaybackSettings.cpp`,
  `src/firmware/sequencer/SequencerPlaybackSettings.h`,
  `src/firmware/sequencer/SequencerTransport.cpp`, and
  `src/firmware/sequencer/SequencerTransport.h`.
- Behavior completed: enabled sequencer builds now provide a Playback Settings
  page with volatile `Steps`, `Direction`, and `Tempo` controls. `Steps`
  limits transport playback to an active range of 1 through 32 steps and keeps
  inactive step LEDs off. `Tempo` defaults to 120 BPM, supports 1 through 255
  BPM, and controls the internal 16th-note step duration. `Direction` supports
  Forward, Backward, Ping-Pong, Random, Brownian, and Drunk.
- Behavior intentionally not included: persistence or file backing for playback
  controls, sequence save/load/browser flows, settings schema changes, quick
  length editing, onboard synth sequencer playback, audition/tap preview,
  probability, ties, detailed tools, external MIDI sync, and MIDI
  clock/transport send.
- Verification: worker reported `rtk git diff --check`,
  `rtk make sequencer-disabled`, and `rtk make sequencer-enabled` passed.
  Enabled firmware artifact:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream/build/sequencer-enabled/HexBoard.ino.uf2`.
  Robert compiled, tested, and confirmed the build. This planning-thread update
  did not rerun firmware builds.
- Review notes: planning-thread read-only check found the implementation branch
  clean at `19bc37d` and confirmed the reported files, commits, volatile
  playback settings/menu modules, transport direction logic, inactive-step LED
  handling, and documentation updates. No settings schema, persistence,
  preset-sync, or web changes were present.
- Next: the following Sequencer MIDI Audition And Edit Overlay slice has since
  been completed and recorded below.

### Sequencer MIDI audition and edit overlay

- Sequencer roadmap slices: focused continuation of 2. Core sequencer editing
  and 3. Sequencer playback.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/sequencer-feature-flag-shell`.
- Implementation commit after the playback-controls slice: `6ef6ec8` (`Add
  sequencer MIDI audition and edit overlay`).
- Changed files reported/reviewed: `README.md`, `docs/code-analysis.md`,
  `docs/developer-guide.md`, `docs/sequencer-manual.md`,
  `src/firmware/app/Runtime.cpp`,
  `src/firmware/sequencer/SequencerInput.cpp`,
  `src/firmware/sequencer/SequencerManagedNotes.cpp`,
  `src/firmware/sequencer/SequencerManagedNotes.h`,
  `src/firmware/sequencer/SequencerMode.cpp`,
  `src/firmware/sequencer/SequencerMode.h`,
  `src/firmware/sequencer/SequencerOverlay.cpp`,
  `src/firmware/sequencer/SequencerOverlay.h`,
  `src/firmware/sequencer/SequencerPlaybackMenu.cpp`,
  `src/firmware/sequencer/SequencerPlaybackSettings.cpp`,
  `src/firmware/sequencer/SequencerPlaybackSettings.h`, and
  `src/firmware/sequencer/SequencerTransport.cpp`.
- Behavior completed: enabled sequencer builds now use sequencer-owned managed
  MIDI notes for audition, tap preview, and playback release ownership.
  Lower-grid playable note keys audition MIDI while Sequencer mode is active;
  with a selected step, the same press auditions and toggles that pitch in the
  selected step, and release reliably releases the audition note. The volatile
  `Tap Preview` playback setting defaults to `On`; selecting a programmed step
  previews its stored MIDI note or chord unless the setting is off. A compact
  selected-step `Edit #NN` overlay shows length, velocity, probability, and
  wrapped note labels for the selected step.
- Behavior intentionally not included: persistence or file backing for `Tap
  Preview`, sequence save/load/browser flows, settings schema changes, onboard
  synth or OB Synth sequencer playback/preview, probability playback behavior,
  ties, exact legacy edit screens, external MIDI sync, MIDI clock/transport
  send, backup tools, and PR submission.
- Verification: worker reported `rtk git diff --check`,
  `rtk make sequencer-disabled`, and `rtk make sequencer-enabled` passed.
  Enabled firmware artifact:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream/build/sequencer-enabled/HexBoard.ino.uf2`.
  Robert reported the build was compiled and tested. The implementation worker
  did not run physical hardware checks, so the manual device checklist from the
  brief still remains. This planning-thread update did not rerun firmware
  builds.
- Review notes: planning-thread read-only check found the upstream
  implementation worktree clean at `6ef6ec8` and confirmed the reported commit
  stat, changed files, managed-note module, Tap Preview menu setting, lower-grid
  audition/release path, preview-step path, selected-step overlay module, and
  documentation updates. No PR was opened.
- Next: the following Sequencer Step Light Colors slice has since been
  completed and recorded below.

### Sequencer step light colors

- Sequencer roadmap slice: focused continuation of 5. Advanced sequencer
  features and bug fixes for sequencer LED/light behavior.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/sequencer-feature-flag-shell`.
- Implementation commit after the MIDI audition/edit-overlay slice: `7cd37e2`
  (`Port sequencer step light colors`).
- Changed files reported/reviewed: `docs/code-analysis.md`,
  `docs/developer-guide.md`, `docs/sequencer-manual.md`,
  `src/firmware/hardware/LedRender.cpp`,
  `src/firmware/hardware/LedRender.h`,
  `src/firmware/menu/MenuAndDisplay.cpp`,
  `src/firmware/sequencer/SequencerLeds.cpp`,
  `src/firmware/sequencer/SequencerLightMenu.cpp`,
  `src/firmware/sequencer/SequencerLightMenu.h`,
  `src/firmware/sequencer/SequencerLightSettings.cpp`,
  `src/firmware/sequencer/SequencerLightSettings.h`,
  `src/firmware/sequencer/SequencerMode.cpp`,
  `src/firmware/storage/PersistentDataModels.h`, and
  `src/firmware/storage/Settings.cpp`.
- Behavior completed: enabled sequencer builds now include a `Seq Lights` page
  with profile-backed `Accent Every`, `Step Color`, and `Step Hue` controls.
  The new profile keys are `SequencerStepAccentEvery`,
  `SequencerStepColorMode`, and `SequencerStepHue`, with
  `CURRENT_SETTINGS_VERSION` intentionally left at `20` for this aggregate
  branch compatibility tradeoff. Programmed-step LEDs use the new
  Regular/Note step color behavior, accent brightness, and the medium/high/
  highest programmed-step brightness ladder. `Step Color = Note` uses the
  stored step's lowest note and the board palette hue/saturation, with a narrow
  LED base-color cache plus linear/gamma helpers to avoid double dimming.
- Behavior intentionally not included: sequence-file persistence for light
  preferences, full sequencer save/load/browser persistence, a settings version
  bump or forced settings migration, remaining advanced sequencer tools, and PR
  submission.
- Verification: worker reported `rtk git diff --check`,
  `rtk make sequencer-disabled`, and `rtk make sequencer-enabled` passed. The
  disabled build reported `648768` bytes program storage and `192732` bytes
  globals/RAM. The enabled build reported `656504` bytes program storage and
  `195484` bytes globals/RAM. Enabled firmware artifact:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream/build/sequencer-enabled/HexBoard.ino.uf2`.
  Robert compiled and tested the build. No temp-copy fallback was needed. This
  planning-thread update did not rerun firmware builds.
- Review notes: planning-thread read-only check found the upstream
  implementation worktree clean at `7cd37e2` and confirmed the reported commit,
  changed files, `14 files changed, 471 insertions(+), 33 deletions(-)` stat,
  profile-backed light settings, `Seq Lights` menu, unchanged settings version,
  LED base-color and linear/gamma helper path, upstream docs updates, and that
  no PR was opened.
- Next: the following Sequencer Step Tools And Playback Semantics slice has
  since been completed and recorded below.

### Sequencer Step Tools and playback semantics

- Sequencer roadmap slices: focused continuation of 2. Core sequencer editing,
  3. Sequencer playback, and 5. Advanced sequencer features and bug fixes.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/sequencer-feature-flag-shell`.
- Implementation commits after the step-light-colors slice: `6e2d0ba` (`Add
  sequencer step copy and transpose helpers`), `26ed045` (`Handle tied notes in
  sequencer playback`), and `ab284e5` (`Add sequencer step tools palette`).
- Changed files reported/reviewed: `README.md`, `docs/code-analysis.md`,
  `docs/developer-guide.md`, `docs/sequencer-manual.md`,
  `src/firmware/hardware/GridScanRotary.cpp`,
  `src/firmware/sequencer/SequencerInput.cpp`,
  `src/firmware/sequencer/SequencerLeds.cpp`,
  `src/firmware/sequencer/SequencerManagedNotes.cpp`,
  `src/firmware/sequencer/SequencerMode.cpp`,
  `src/firmware/sequencer/SequencerMode.h`,
  `src/firmware/sequencer/SequencerOverlay.cpp`,
  `src/firmware/sequencer/SequencerState.cpp`,
  `src/firmware/sequencer/SequencerState.h`,
  `src/firmware/sequencer/SequencerTools.cpp`,
  `src/firmware/sequencer/SequencerTools.h`,
  `src/firmware/sequencer/SequencerTransport.cpp`, and
  `src/firmware/sequencer/SequencerTransport.h`.
- Behavior completed: enabled sequencer builds now include the full in-memory
  Step Tools editing slice: Tools picker on button `29`, no-selected-step
  tools status, Len/Vel/Oct+/Oct-/Prob/Tie/Copy/Cancel mapping, step switching
  while tools and exact Vel/Prob are open, quick encoder length editing,
  encoder-click deselect, exact length entry, exact velocity/probability
  encoder editing, octave transpose with displayed octave `0` through `9`
  bounds, per-step Tie toggle, full-step Copy, blue-action undo, and hold-clear
  of selected-step notes and Tie while preserving length/velocity/probability.
- Playback behavior completed: per-step velocity affects transport playback,
  tap preview, and selected-step live audition; probability gates whole steps
  during transport only; length `0%`, short gates, `100%` boundary release, and
  overlength overlaps are handled with bounded playback groups; Tie continues
  the nearest earlier eligible active source from the same pattern pass without
  loop-boundary wrap; tied steps stay silent for tap preview; stop/panic/exit
  and step-data changes release sequencer-managed notes.
- Behavior intentionally not included: sequence persistence, save/load/new/
  rename/delete file flows, file browser/storage UI, USB backup tools and GUI,
  MIDI sync, MIDI clock/transport send, onboard synth or OB Synth playback,
  Play Type settings, Monophonic mode, full old sequencer manuals/layouts/
  requirements port, and PR submission.
- Verification: worker reported `rtk git diff --check`,
  `rtk make sequencer-disabled`, and `rtk make sequencer-enabled` passed. The
  disabled build reported `648920` bytes program storage and `192732` bytes
  globals/RAM. The enabled build reported `667808` bytes program storage and
  `195952` bytes globals/RAM. Enabled firmware artifact:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream/build/sequencer-enabled/HexBoard.ino.uf2`.
  The manual hardware checklist has not been run yet.
- Review notes: implementation left the upstream worktree clean at `ab284e5`
  and kept sequencer-owned tools, state mutation, overlays, LEDs, and playback
  semantics under `src/firmware/sequencer/` with only a narrow encoder hook in
  `GridScanRotary.cpp`. Disabled builds remain intended to have no Sequencer
  menu item or sequencer runtime behavior.
- Next: the following Sequencer Monophonic And Play Type Routing slice has
  since been completed and recorded below.

### Sequencer monophonic and Play Type routing

- Sequencer roadmap slices: focused continuation of 3. Sequencer playback and
  5. Advanced sequencer features and bug fixes.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/sequencer-feature-flag-shell`.
- Implementation commit after the Step Tools/playback-semantics slice:
  `ee1b82f9a7621fbb3405786d305ec28cc2e6a534` (`Port sequencer monophonic and
  play type routing`).
- Changed files verified from the implementation commit: `README.md`,
  `docs/code-analysis.md`, `docs/developer-guide.md`,
  `docs/sequencer-manual.md`, `src/firmware/menu/MenuAndDisplay.cpp`,
  `src/firmware/sequencer/SequencerInput.cpp`,
  `src/firmware/sequencer/SequencerManagedNotes.cpp`,
  `src/firmware/sequencer/SequencerOutput.cpp`,
  `src/firmware/sequencer/SequencerOutput.h`,
  `src/firmware/sequencer/SequencerPlaybackMenu.cpp`,
  `src/firmware/sequencer/SequencerPlaybackSettings.cpp`,
  `src/firmware/sequencer/SequencerPlaybackSettings.h`,
  `src/firmware/sequencer/SequencerState.cpp`,
  `src/firmware/sequencer/SequencerState.h`,
  `src/firmware/storage/PersistentDataModels.h`,
  `src/firmware/storage/Settings.cpp`, `src/firmware/synth/SynthAudio.cpp`,
  `src/firmware/synth/SynthAudio.h`,
  `src/firmware/synth/SynthAudioInternal.h`, and
  `src/firmware/synth/SynthVoiceAllocation.cpp`.
- Behavior completed: Playback Settings now include a volatile `Play Type`
  selector with `MIDI` and `OB Synth`, defaulting to `MIDI`. The same page now
  includes profile-backed `Monophonic` selected-step entry with `Off`/`On`,
  defaulting to `Off`. With `Monophonic` Off, selected-step note entry keeps
  normal toggle behavior. With `Monophonic` On, pressing the same pitch clears
  it, and pressing a different pitch replaces the selected step with one note.
- Output behavior completed: sequencer-managed audition notes, tap preview, and
  transport playback now route through `SequencerOutput` handles so each active
  note releases through the same route it started on. `MIDI` output continues
  through the sequencer MIDI path. `OB Synth` output uses shared synth tuning
  and audio settings through synth preview-note handles backed by hidden matrix
  slots `141..159`, leaving hardware flag slot `140` untouched.
- Behavior intentionally not included: sequence save/load, file browser and
  file-management flows, per-sequence `Play Type` persistence, MIDI sync,
  MIDI clock/transport send, backup tools, and PR submission. `Play Type`
  remains volatile until the sequence persistence slice stores and restores it
  per sequence.
- Documentation/web notes: maintained upstream docs were updated in the
  implementation commit. No `web/` change was made because there is no current
  sequencer UI/protocol surface there for these settings.
- Verification: worker reported `rtk git diff --check` was clean, and
  `rtk git diff --check --cached` was clean before commit. Worker also reported
  `rtk make sequencer-disabled` passed with `649080` bytes program storage and
  `192844` bytes globals/RAM, and `rtk make sequencer-enabled` passed with
  `669000` bytes program storage and `196468` bytes globals/RAM. Enabled
  firmware artifact:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream/build/sequencer-enabled/HexBoard.ino.uf2`.
  Robert reported the build was compiled and tested. This planning-thread
  update did not rerun any build commands.
- Review notes: planning-thread read-only check found the implementation
  worktree clean on `codex/sequencer-feature-flag-shell` at `ee1b82f`, verified
  the commit stat as `20 files changed, 492 insertions(+), 91 deletions(-)`,
  confirmed the reported changed files and artifact path, and spot-checked the
  `Play Type`, `Monophonic`, profile setting, sequencer output handle, and
  hidden synth preview-slot paths. Hardware behavior still needs the focused
  manual OB Synth checklist, especially release, stop, and panic behavior
  across route changes.
- Next: the following Sequencer Selected-Step Blink Fix slice has since been
  completed and recorded below.

### Sequencer selected-step blink fix

- Sequencer roadmap slice: focused continuation of 5. Advanced sequencer
  features and bug fixes for sequencer LED/light behavior.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/sequencer-feature-flag-shell`.
- Implementation commit after the Monophonic/Play Type routing slice:
  `06923ea` (`Blink selected sequencer steps before LED rendering`).
- Changed files verified from the implementation commit: `docs/code-analysis.md`,
  `docs/developer-guide.md`, `docs/sequencer-manual.md`, and
  `src/firmware/sequencer/SequencerLeds.cpp`.
- Behavior completed: the upstream sequencer LED renderer now restores the
  legacy selected-step blink behavior. Selected steps use a `600000ULL` microsecond
  on phase and a `200000ULL` microsecond off phase, gating the selected step
  fully off before the normal empty/programmed step rendering path. The existing
  brightness ladder remains unchanged, so programmed selected steps are still
  brighter during the on phase.
- Behavior intentionally not included: any sequence persistence, unrelated
  Sequencer light-setting changes, PR submission, or additional playback/routing
  behavior.
- Verification: worker reported `git diff --check` passed and
  `make sequencer-builds` passed. The disabled build reported `649080` bytes
  program storage and `192844` bytes globals/RAM. The enabled build reported
  `669056` bytes program storage and `196468` bytes globals/RAM. Enabled firmware
  artifact:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream/build/sequencer-enabled/HexBoard.ino.uf2`.
  Robert reported the change was done and tested. This planning-thread update
  did not rerun any build or compile commands.
- Review notes: planning-thread spot-check confirmed the committed upstream
  change adds the blink constants/helper and gates selected step LEDs off
  during the off phase before normal rendering, with matching upstream
  documentation updates. The final branch now has the sequencer
  persistence/file-management commits on top.
- Next: the following Sequencer Persistence And File Management v1 slice has
  since been completed and recorded below.

### Sequencer persistence and file management v1

- Sequencer roadmap slice: 4. Sequencer persistence and file management, plus
  the profile-backed `Tap Preview` cleanup from the persistence bucket split.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/sequencer-feature-flag-shell`.
- Implementation commits after the selected-step blink fix: `00dd06c` (`Port
  sequencer save and file management`), `623de05` (`Fix sequencer file menu
  entry tracking`), and `5ee86dd` (`Cache sequencer file browser rows and
  counts`).
- Changed files verified from final HEAD after the blink fix: `README.md`,
  `docs/code-analysis.md`, `docs/developer-guide.md`,
  `docs/sequencer-manual.md`, `src/firmware/app/Runtime.cpp`,
  `src/firmware/sequencer/SequencerFileMenu.cpp`,
  `src/firmware/sequencer/SequencerFileMenu.h`,
  `src/firmware/sequencer/SequencerInput.cpp`,
  `src/firmware/sequencer/SequencerLeds.cpp`,
  `src/firmware/sequencer/SequencerMode.cpp`,
  `src/firmware/sequencer/SequencerMode.h`,
  `src/firmware/sequencer/SequencerPlaybackMenu.cpp`,
  `src/firmware/sequencer/SequencerPlaybackSettings.cpp`,
  `src/firmware/sequencer/SequencerPlaybackSettings.h`,
  `src/firmware/sequencer/SequencerStorage.cpp`,
  `src/firmware/sequencer/SequencerStorage.h`,
  `src/firmware/sequencer/SequencerTools.cpp`,
  `src/firmware/storage/PersistentDataModels.h`, and
  `src/firmware/storage/Settings.cpp`.
- Behavior completed: enabled sequencer builds now include a `File Management`
  page with `New`, `Save`, `Save New`, `Load`, `Revert`, `Create Folder`, and
  `Rename/Delete`. Sequence files live under `/Sequences`, use the `.hbseq`
  extension, remember the current path in `/Sequences/.current`, and reload the
  remembered sequence after settings/profile restore during startup.
- Sequence-file data completed: saved files use text records with
  `format=HBSEQ`, `version=3`, and `noteFormat=stepsFromC`. They store all 32
  step records, tuning-relative pitch-step values, note counts, per-step
  length/velocity/probability/Tie, `Tempo`, active `Steps`, `Direction`, and
  `Play Type`. Unknown keys are ignored and invalid values are clamped while
  parsing.
- Runtime/file workflow completed: `New`, `Load`, and `Revert` stop transport,
  release sequencer-managed notes, reset sequencer input/tools/overlay state as
  needed, and clear dirty state after a successful reset or file operation.
  The Sequencer title is `Sequencer` with no current file, `Seq-NAME` for a
  clean current file, and `Seq-*NAME` when sequence-owned data is dirty.
- Browser/naming behavior completed: the file browser scans only the current
  folder, lists folders before sequence files, supports parent navigation, and
  uses `VirtualListMenu` callbacks with cached current-folder counts plus an
  8-row visible-window cache instead of keeping a full tree/list of paths in
  memory. The naming screen accepts letters, numbers, spaces, and hyphen up to
  20 visible characters, rejects duplicates in the target folder, and appends
  `.hbseq` internally for sequence files. Rename/delete supports files and
  folders; folder delete is recursive, and rename/delete repairs or clears the
  remembered current path when it affects the current sequence.
- Persistence bucket split completed: `Tap Preview` is now profile-backed, with
  `CURRENT_SETTINGS_VERSION` bumped to `21` and version `20` settings migrated
  by filling the new `SequencerTapPreview` byte from factory defaults.
  Sequence files do not store `Tap Preview`, `Monophonic`, `Seq Lights`,
  selected step, undo/tool/naming/browser state, overlay messages, active note
  handles, transport timing, or dirty state.
- Behavior intentionally not included: external MIDI clock, MIDI
  start/stop/clock send, USB Backup, desktop backup scripts or launchers, the
  performance monitor overlay, full old sequencer manuals/layouts/requirements
  port, PR submission, or any broad merge from the old branch.
- Verification: Robert reported the final build was compiled and tested after
  the file-browser follow-up fixes. This planning-thread update did not rerun
  any build or compile commands at Robert's request.
- Review notes: planning-thread read-only check found the upstream
  implementation worktree clean on `codex/sequencer-feature-flag-shell` at
  `5ee86dd`, confirmed the final commit stack and changed files after
  `06923ea`, and spot-checked the storage/menu modules, fixed-buffer sequence
  serialization/parsing, atomic temp-file save path, startup restore,
  current-folder browser row/count cache, profile-backed `Tap Preview`
  settings migration, and maintained upstream docs. This ledger entry is based
  on final HEAD rather than the worker's earlier completion summary.
- Next: the following Sequencer External MIDI Clock Receive slice has since
  been completed and recorded below.

### Sequencer external MIDI clock receive

- Sequencer roadmap slices: focused continuation of 3. Sequencer playback and
  5. Advanced sequencer features and bug fixes for MIDI sync receive.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/sequencer-feature-flag-shell`.
- Implementation commit after the persistence/file-management slice:
  `e63518d084d7b45ed5c7ccd2d040dbea1704e85b` (`Port external MIDI clock
  receive`).
- Changed files verified from the implementation commit: `docs/code-analysis.md`,
  `docs/developer-guide.md`, `docs/sequencer-manual.md`,
  `src/firmware/midi/MidiInput.cpp`,
  `src/firmware/sequencer/SequencerMode.cpp`,
  `src/firmware/sequencer/SequencerMode.h`,
  `src/firmware/sequencer/SequencerPlaybackMenu.cpp`,
  `src/firmware/sequencer/SequencerPlaybackSettings.cpp`,
  `src/firmware/sequencer/SequencerPlaybackSettings.h`,
  `src/firmware/sequencer/SequencerTransport.cpp`,
  `src/firmware/sequencer/SequencerTransport.h`,
  `src/firmware/storage/PersistentDataModels.h`, and
  `src/firmware/storage/Settings.cpp`.
- Behavior completed: Playback Settings now includes profile-backed
  `Clock Source` with `Internal` and `External MIDI`, defaulting to
  `Internal`. The profile setting is stored as appended `SequencerClockSource`;
  `CURRENT_SETTINGS_VERSION` was intentionally not bumped for this in-flight
  aggregate branch update.
- External receive behavior completed: when `Clock Source` is `Internal`,
  button `9` and local tempo continue to control transport as before. When
  `Clock Source` is `External MIDI`, incoming MIDI Clock `0xF8` advances one
  sequencer step every six pulses, Start `0xFA` starts from the beginning, Stop
  `0xFC` stops transport and releases sequencer playback notes, and Continue
  `0xFB` follows the old sequencer's start-like behavior rather than resuming
  from a MIDI song position.
- MIDI parser behavior completed: realtime MIDI bytes are handled narrowly
  before normal channel/SysEx parsing continues. Running status, SysEx,
  preset-sync, delegated control, and MIDI note LED behavior remain isolated;
  delegated mode ignores the sequencer realtime transport hooks.
- Transport timing behavior completed: external clock playback preserves the
  existing gate/tie playback model with measured step-boundary timing after
  clock pulses establish a boundary. USB host clock jitter may still be audible
  depending on host and routing.
- Behavior intentionally not included: MIDI clock send, MIDI transport send,
  USB Backup, desktop backup scripts or launchers, the performance monitor
  overlay, and PR submission.
- Verification: Robert reported `rtk git diff --check` passed,
  `rtk make sequencer-disabled` passed with `650192` bytes program storage and
  `192860` bytes globals/RAM, and `rtk make sequencer-enabled` passed with
  `686512` bytes program storage and `203004` bytes globals/RAM. The enabled
  build emitted the usual low-memory warning. Enabled firmware artifact:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream/build/sequencer-enabled/HexBoard.ino.uf2`.
  Robert reported the build was compiled and tested. This planning-thread
  update did not rerun any build or compile commands.
- Review notes: planning-thread read-only check found the upstream
  implementation worktree clean on `codex/sequencer-feature-flag-shell` at
  `e63518d`, confirmed the commit stat as `13 files changed, 417 insertions(+),
  42 deletions(-)`, verified the changed-file list above, and spot-checked the
  appended profile setting/default, Playback Settings menu item, sequencer mode
  external MIDI hooks, MIDI input realtime dispatch, external clock transport
  state, Start/Stop/Continue handlers, gate/tie boundary handling, and maintained
  upstream docs.
- Next: the following Sequencer MIDI Sync Send slice has since been completed
  and recorded below.

### Sequencer MIDI sync send

- Sequencer roadmap slices: focused continuation of 3. Sequencer playback and
  5. Advanced sequencer features and bug fixes for MIDI sync send.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/sequencer-feature-flag-shell`.
- Implementation commit after the external MIDI clock receive slice:
  `275a63839fa4d9a3938a90c257bca3d64ff66dd2` (`Port sequencer MIDI sync
  send`).
- Changed files verified from the implementation commit: `docs/code-analysis.md`,
  `docs/developer-guide.md`, `docs/sequencer-manual.md`,
  `src/firmware/midi/MidiRouting.cpp`, `src/firmware/midi/MidiRouting.h`,
  `src/firmware/midi/MidiTransport.h`,
  `src/firmware/sequencer/SequencerMidi.cpp`,
  `src/firmware/sequencer/SequencerMidi.h`,
  `src/firmware/sequencer/SequencerPlaybackMenu.cpp`,
  `src/firmware/sequencer/SequencerPlaybackSettings.cpp`,
  `src/firmware/sequencer/SequencerPlaybackSettings.h`,
  `src/firmware/sequencer/SequencerTransport.cpp`,
  `src/firmware/sequencer/SequencerTransport.h`,
  `src/firmware/storage/PersistentDataModels.h`, and
  `src/firmware/storage/Settings.cpp`.
- Behavior completed: `Playback Settings > MIDI Sync` now contains
  `Clock Source`, `Send Clock`, and `Send Transport`. `Send Clock` and
  `Send Transport` are profile-backed settings, default to `Off`, and are
  stored as appended `SequencerSendClock` and `SequencerSendTransport` profile
  keys. Sequence `.hbseq` files do not store these MIDI sync send preferences.
- MIDI send behavior completed: when `Clock Source` is `Internal` and
  `Send Clock` is `On`, the running sequencer transport emits six MIDI Clock
  pulses per sequencer step through the configured MIDI outputs. When
  `Clock Source` is `Internal` and `Send Transport` is `On`, the local
  transport toggle sends MIDI Start on play and MIDI Stop on stop.
- Realtime isolation completed: external MIDI clock receive remains
  receive-only. When `Clock Source` is `External MIDI`, inbound MIDI Clock,
  Start, Stop, and Continue can drive the sequencer as recorded in the previous
  slice, but the sequencer does not echo outbound MIDI Clock, Start, or Stop.
  Delegated mode continues to ignore incoming realtime sequencer hooks.
- Settings migration completed: `CURRENT_SETTINGS_VERSION` is now `22`.
  Version `20` settings migrate using the pre-`SequencerClockSource` width,
  version `21` settings migrate using the pre-`SequencerSendClock` width, and
  the new send preferences are filled from factory defaults during migration.
- Behavior intentionally not included: MIDI Continue send, sequence-file
  persistence for MIDI sync send preferences, USB Backup, desktop backup
  scripts or launchers, the performance monitor overlay, and PR submission.
- Verification: Robert reported `rtk git --no-optional-locks diff --check`
  passed, `rtk make sequencer-disabled` passed with `650352` bytes program
  storage and `192880` bytes globals/RAM, and `rtk make sequencer-enabled`
  passed with `687736` bytes program storage and `203384` bytes globals/RAM.
  The enabled build emitted the existing low-memory warning. Enabled firmware
  artifact:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream/build/sequencer-enabled/HexBoard.ino.uf2`.
  Robert reported the build was compiled and tested. This planning-thread
  update did not rerun any build or compile commands.
- Review notes: planning-thread read-only check found the upstream
  implementation worktree clean on `codex/sequencer-feature-flag-shell` at
  `275a638`, confirmed the commit stat as `15 files changed, 313
  insertions(+), 34 deletions(-)`, verified the changed-file list above, and
  spot-checked the profile-backed send defaults, `Playback Settings > MIDI
  Sync` menu entries, internal MIDI Clock pulse scheduler, local Start/Stop
  send hooks, external-clock send suppression, realtime output helper, v20/v21
  migration path, and maintained upstream docs. The local tracking ref
  `origin/codex/sequencer-feature-flag-shell` also pointed at `275a638` during
  this check, so this ledger records the verified clean local branch state.
- Next: the following Sequencer Performance Monitor Overlay slice has since
  been completed and recorded below.

### Sequencer performance monitor overlay

- Sequencer roadmap slice: focused continuation of 5. Advanced sequencer
  features and bug fixes for the old hold-to-view diagnostic overlay.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/sequencer-feature-flag-shell`.
- Implementation commit after the MIDI sync send slice:
  `874ac2f062a5ac2766d09f0e519968b44bd2332d` (`Port sequencer performance
  monitor overlay`).
- Changed files verified from the implementation commit:
  `docs/code-analysis.md`, `docs/developer-guide.md`,
  `docs/sequencer-manual.md`, `src/firmware/midi/MidiInput.cpp`,
  `src/firmware/midi/MidiInput.h`,
  `src/firmware/sequencer/SequencerInput.cpp`,
  `src/firmware/sequencer/SequencerMode.cpp`,
  `src/firmware/sequencer/SequencerOverlay.cpp`,
  `src/firmware/sequencer/SequencerPerformanceMonitor.cpp`, and
  `src/firmware/sequencer/SequencerPerformanceMonitor.h`.
- Behavior completed: enabled sequencer builds now restore the old temporary
  Performance Monitor overlay. Holding the Sequencer Play/Stop key for about
  two seconds shows the monitor while held; releasing the key closes it and
  restores the previous sequencer view or edit state. Short Play/Stop still
  toggles transport, and with a selected step it deselects the step and toggles
  transport. Holding for the monitor does not create an accidental transport
  toggle.
- Diagnostic display completed: the overlay shows `AudioEng`, `Mem`, `FS`,
  `MIDI Q`, `Drop`, and `Late`. `AudioEng` uses the existing audio ISR
  profiling data, `Mem` shows heap used versus total heap, `FS` shows LittleFS
  used versus total or `FS  unavailable`, `MIDI Q` shows pending MIDI input,
  and `Drop`/`Late` are receive-stress backlog episode counters rather than
  exact hardware packet-loss counters.
- Behavior intentionally not included: USB Backup, desktop backup scripts or
  launchers, sequencer-specific played-note overlay behavior, full old
  sequencer manuals/layouts/requirements port, or PR submission.
- Verification: Robert reported `rtk git diff --check` passed,
  `rtk git diff --cached --check` passed, `rtk make sequencer-disabled` passed
  with `650536` bytes program storage and `192920` bytes globals/RAM, and
  `rtk make sequencer-enabled` passed with `689272` bytes program storage and
  `203508` bytes globals/RAM. The enabled build emitted the usual low-memory
  warning. Enabled firmware artifact:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream/build/sequencer-enabled/HexBoard.ino.uf2`.
  Robert reported the build was compiled and hardware-tested. This
  planning-thread update did not rerun any build or compile commands.
- Manual checklist completed: short Play/Stop toggles transport; with a
  selected step, short Play/Stop deselects the step and toggles transport;
  a two-second Play/Stop hold shows the Performance Monitor; release closes the
  monitor and restores the previous sequencer view/edit state; the hold does
  not create an accidental transport toggle; overlay labels and values fit on
  the OLED; `FS` shows used/total or `FS  unavailable`; and `MIDI Q` changes
  under pending input. MIDI `Drop`/`Late` counters were implemented as receive
  stress counters but were not physically stress-tested with external MIDI
  hardware in the worker pass.
- Review notes: planning-thread read-only check found the upstream
  implementation worktree clean on `codex/sequencer-feature-flag-shell` at
  `874ac2f`, with the local branch one commit ahead of
  `origin/codex/sequencer-feature-flag-shell`. The check confirmed the commit
  stat as `10 files changed, 434 insertions(+), 32 deletions(-)`, verified the
  changed-file list above, and confirmed that no PR was opened.
- Next: the following Sequencer Overview Screen slice has since been completed
  and recorded below.

### Sequencer overview screen

- Sequencer roadmap slice: focused continuation of 5. Advanced sequencer
  features and bug fixes for the old multi-step pattern review overlay.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/sequencer-feature-flag-shell`.
- Implementation commit after the performance monitor overlay slice:
  `22eec1818e403543df48fa15daff89a12a19a9d4` (`Port sequencer overview
  screen`).
- Changed files verified from the implementation commit:
  `docs/code-analysis.md`, `docs/developer-guide.md`,
  `docs/sequencer-manual.md`,
  `src/firmware/sequencer/SequencerInput.cpp`,
  `src/firmware/sequencer/SequencerLeds.cpp`,
  `src/firmware/sequencer/SequencerOverlay.cpp`, and
  `src/firmware/sequencer/SequencerOverlay.h`.
- Behavior completed: enabled sequencer builds now restore the old note
  overview screen on the second-row 9th key, physical button `18`. The first
  press opens the overview from step 1; repeated presses advance through packed
  pages and cycle through the 32-step pattern. Normal step or note-entry
  actions hide the overview before continuing normal editing or audition
  behavior. Modal tool screens keep their existing behavior rather than being
  interrupted by overview actions.
- OLED display completed: the overview uses compact no-title rows and packs as
  many steps as fit on the 128x128 OLED. Empty steps show `_`, tied steps show
  `T`, chord notes are shown in ascending order, and large chords wrap onto a
  second indented line for the same step. `12 EDO` uses note labels such as
  `C4`; other tunings use numeric `step.octave` labels.
- LED behavior completed: button `18` is restored as a white sequencer utility
  key, using the idle white level normally and the brighter white level while
  the overview is active.
- Behavior intentionally not included: USB Backup, desktop backup scripts or
  launchers, sequencer-specific played-note overlay behavior, full old
  sequencer manuals/layouts/requirements port, or PR submission.
- Verification: Robert reported `git diff --check` passed,
  `make sequencer-disabled` passed, and `make sequencer-enabled` passed with
  the expected Arduino low-memory warning for enabled sequencer builds.
  Disabled firmware artifact:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream/build/sequencer-disabled/HexBoard.ino.uf2`.
  Enabled firmware artifact:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream/build/sequencer-enabled/HexBoard.ino.uf2`.
  Robert reported the build was compiled and hardware-tested. This
  planning-thread update did not rerun any build or compile commands.
- Manual checklist completed: Robert tested the overview feature on device
  after the compiled build. The implemented scope covers opening the overview
  from button `18`, page advancement, compact row rendering, empty and tied
  step markers, chord wrapping, tuning-aware labels, hiding before normal
  step/note editing, preserving modal tool behavior, and the active/idle white
  utility LED state for button `18`.
- Review notes: planning-thread read-only check found the upstream
  implementation worktree clean on `codex/sequencer-feature-flag-shell` at
  `22eec18`, confirmed the commit stat as `7 files changed, 230
  insertions(+), 10 deletions(-)`, verified the changed-file list above, and
  confirmed that the local tracking ref
  `origin/codex/sequencer-feature-flag-shell` also pointed at `22eec18` during
  this check. No PR was opened.
- Next: the following Sequencer Played-Note Overlay Integration slice has since
  been completed and recorded below.

### Sequencer played-note overlay integration

- Sequencer roadmap slice: focused continuation of 5. Advanced sequencer
  features and bug fixes for sequencer-specific played-note display behavior.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/sequencer-feature-flag-shell`.
- Implementation commit after the overview slice:
  `2bcb4c345012c20546afbceb359ea1aef9ff35a8` (`Show sequencer audition notes
  in played-note overlay`).
- Changed files verified from the implementation commit:
  `docs/code-analysis.md`, `docs/developer-guide.md`,
  `docs/sequencer-manual.md`, `docs/user-manual.md`,
  `src/firmware/app/Runtime.cpp`,
  `src/firmware/menu/PlayedNotesOverlay.cpp`,
  `src/firmware/sequencer/SequencerManagedNotes.cpp`,
  `src/firmware/sequencer/SequencerManagedNotes.h`,
  `src/firmware/sequencer/SequencerMode.cpp`, and
  `src/firmware/sequencer/SequencerMode.h`.
- Behavior completed: no-selection lower-grid sequencer audition notes now feed
  the shared played-note badge and screensaver `Now Playing` overlay when
  `DisplayNotes` is enabled. Selected-step entry, tap preview, and transport
  playback do not feed that display source.
- Display restore behavior completed: when the played-note overlay closes or
  clears while Sequencer mode is active, the firmware redraws the appropriate
  Sequencer screen when needed, including file naming, Performance Monitor,
  Overview, selected-step edit, and tool overlays.
- Behavior intentionally not included: USB Backup, desktop backup scripts or
  launchers, full old sequencer manuals/layouts/requirements port, PR
  submission, or any broader played-note overlay redesign.
- Verification: planning-thread check confirmed commit `2bcb4c3` exists on
  `codex/sequencer-feature-flag-shell` and spot-checked the source/docs diff.
  No compile or manual hardware verification was found in the ledger for this
  specific slice, so this entry does not claim build or device testing.
- Review notes: planning-thread read-only check confirmed the implementation
  commit stat as `10 files changed, 280 insertions(+), 18 deletions(-)`,
  verified the changed-file list above, and spot-checked the source split
  between `PlayedNotesOverlay`, `SequencerManagedNotes`, and `SequencerMode`.
  No PR was opened.
- Next: the following Sequencer Display-State Follow-Up Fixes have since been
  completed and recorded below.

### Sequencer display-state follow-up fixes

- Sequencer roadmap slice: follow-up bug fixes for ported sequencer display
  behavior. These are not separate old-branch feature ports; they correct
  aggregate-branch behavior that was slightly off after the recent overlay
  integration work.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/sequencer-feature-flag-shell`.
- Implementation commits after the played-note overlay integration slice:
  `6d7c191c28d14ead22a20d8698f7a2a393a3f346` (`Blank sequencer display when
  closing step overlay`) and
  `94b758eb8f1c936e309f5b78f98712193148947b` (`Preserve blank sequencer
  display during audition notes`).
- Changed files verified from the follow-up commits:
  `docs/code-analysis.md`, `docs/sequencer-manual.md`, `docs/user-manual.md`,
  `src/firmware/menu/PlayedNotesOverlay.cpp`,
  `src/firmware/sequencer/SequencerManagedNotes.cpp`,
  `src/firmware/sequencer/SequencerMode.cpp`,
  `src/firmware/sequencer/SequencerOverlay.cpp`, and
  `src/firmware/sequencer/SequencerOverlay.h`.
- Behavior completed: closing selected-step edit focus by pressing the selected
  step again, pressing the encoder, or short-pressing Play/Stop now blanks the
  sequencer display instead of redrawing the Sequencer menu. The next explicit
  step selection, Overview, Tools, or menu action draws its normal screen.
- Played-note overlay restore behavior completed: no-selection audition notes
  still feed the shared played-note display source, but when the sequencer idle
  display is intentionally blank, the full temporary `Now Playing` overlay is
  used and closing it restores the blank sequencer display instead of forcing a
  Sequencer menu redraw.
- Behavior intentionally not included: any new sequencer feature port, USB
  Backup, desktop backup scripts or launchers, full old sequencer manuals/
  layouts/requirements port, PR submission, or a broader played-note overlay
  redesign.
- Verification: planning-thread check confirmed both commits exist on
  `codex/sequencer-feature-flag-shell`, the implementation worktree is clean at
  `94b758e`, and `origin/codex/sequencer-feature-flag-shell` also points at
  `94b758e`. No compile or manual hardware verification was found in the ledger
  for these specific follow-up fixes, so this entry does not claim build or
  device testing.
- Review notes: planning-thread read-only check confirmed commit `6d7c191` as
  `2 files changed, 14 insertions(+), 6 deletions(-)` and commit `94b758e` as
  `8 files changed, 69 insertions(+), 8 deletions(-)`. The source changes add
  explicit blank-idle display tracking in `SequencerOverlay`, preserve that
  state across played-note overlay restore in `SequencerMode`, and keep the
  shared played-note overlay from replacing the intentional blank state with a
  normal menu redraw.
- Next: the following Sequencer USB Backup Workflow And Follow-Up Fixes have
  since been completed and recorded below.

### Sequencer USB backup workflow and follow-up fixes

- Sequencer roadmap slice: focused continuation of 5. Advanced sequencer
  features and bug fixes for USB Backup and desktop backup tools, plus immediate
  follow-up corrections on the aggregate branch.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/sequencer-feature-flag-shell`.
- Implementation commits after the display-state follow-up fixes:
  `15525cd9af952cd2615500fe064a0678f9ba8cb4` (`Add sequencer USB backup
  workflow`), `e742409dff364f53d59bfc6a8f305b0859a4a872` (`Consolidate
  sequencer MIDI realtime handling`),
  `6b061e629079ca46087b6bbbe013230bdb287ac6` (`Fix USB backup PUT payload
  parsing`), and `7414ec81355dbec13864a5dfd09f9b02143be507` (`Remove
  sequencer hold-clear menu hint`).
- Changed files verified from the follow-up commit stack:
  `Launch HexBoard Backup.bat`, `Launch HexBoard Backup.command`, `README.md`,
  `docs/code-analysis.md`, `docs/developer-guide.md`,
  `docs/sequencer-manual.md`, `docs/user-manual.md`,
  `scripts/hexboard_backup.py`, `scripts/hexboard_backup_gui.py`,
  `scripts/hexboard_backup_lib.py`, `src/firmware/hardware/LedRender.cpp`,
  `src/firmware/menu/PlayedNotesOverlay.cpp`,
  `src/firmware/midi/MidiInput.cpp`,
  `src/firmware/sequencer/SequencerFileMenu.cpp`,
  `src/firmware/sequencer/SequencerFileMenu.h`,
  `src/firmware/sequencer/SequencerLeds.cpp`,
  `src/firmware/sequencer/SequencerMode.cpp`,
  `src/firmware/sequencer/SequencerMode.h`,
  `src/firmware/sequencer/SequencerUsbBackup.cpp`,
  `src/firmware/sequencer/SequencerUsbBackup.h`,
  `src/firmware/synth/SynthAudio.cpp`, `src/firmware/synth/SynthAudio.h`,
  `src/firmware/synth/SynthAudioInternal.h`, and
  `src/firmware/synth/SynthVoiceAllocation.cpp`.
- USB Backup behavior completed: enabled sequencer builds now include
  `File Management` -> `USB Backup`, with `Start Session` / `Stop Session`,
  live two-line status, leave/stop confirmations while active, session cleanup
  when leaving Sequencer mode, storage-workflow guards during active sessions,
  blanked sequencer LEDs, and consumed hex-button editing/play actions while
  encoder menu navigation remains available.
- HBK1 protocol behavior completed: `SequencerUsbBackup.*` owns an enabled-only
  USB-serial session rooted at `/Sequences`, with `HELLO`, `PING`, `LIST`,
  `GET`, `PUT`, `MKDIR`, `DELETE`, `RMDIR`, and `RENAME`. Paths stay restricted
  to `/Sequences`, direct file operations are limited to `.hbseq` files, PUT
  restores write through a temporary file before rename, and partial incoming
  restores are cleared on timeout or session exit.
- Host backup tooling completed: the repo now includes
  `scripts/hexboard_backup_gui.py` as the primary desktop workflow,
  `scripts/hexboard_backup_lib.py` as the shared HBK1 client library,
  `scripts/hexboard_backup.py` as support/debug CLI tooling, plus macOS and
  Windows launchers for users who already have Python 3 and `pyserial`.
- Follow-up fixes completed: MIDI realtime handling is now consolidated so
  `MidiInput.cpp` forwards realtime status bytes while Sequencer mode owns
  clock/start/continue/stop policy; USB Backup command parsing now returns from
  command input immediately after a PUT command switches into payload receive
  mode; and the stale `Hold 19 clears` row was removed from the Sequencer top
  page.
- Behavior intentionally not included: TB-303 pattern decoder skill updates,
  full old sequencer manuals/layouts/requirements port, PR submission, or any
  broader backup/protocol redesign outside the completed `/Sequences` HBK1
  workflow.
- Verification: planning-thread check originally confirmed this commit stack
  through `7414ec8`; a later final-state check found the aggregate branch clean
  and pushed through `c0c9616`. Robert reported the final aggregate disabled and
  enabled builds, focused manual hardware checklist, and host backup tool smoke
  check were completed after these fixes. Local artifact check found
  `build/sequencer-disabled/HexBoard.ino.uf2` (`1456128` bytes) and
  `build/sequencer-enabled/HexBoard.ino.uf2` (`1564672` bytes), both modified
  `2026-06-27 15:36:22`, and `git diff --check` passed in the upstream
  implementation checkout.
- Review notes: planning-thread read-only check confirmed commit `15525cd` as
  `16 files changed, 2823 insertions(+), 10 deletions(-)`, commit `e742409` as
  `11 files changed, 48 insertions(+), 46 deletions(-)`, commit `6b061e6` as
  `1 file changed, 3 insertions(+)`, and commit `7414ec8` as
  `1 file changed, 6 deletions(-)`. No PR was opened.
- Next: the following overlay/menu/title/filename follow-up fixes have since
  been completed and recorded below.

### Sequencer overlay, menu, title, filename, and PR-readiness follow-up fixes

- Sequencer roadmap slice: final bug-fix and PR-readiness cleanup pass for
  aggregate-branch sequencer display, overlay, menu action, save suggestion,
  filename/title styling behavior, and generated-artifact cleanup. These are
  follow-up corrections to already-ported sequencer behavior and PR hygiene, not
  a new old-branch feature port.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/sequencer-feature-flag-shell`.
- Aggregate PR:
  `https://github.com/shapingthesilence/HexBoard/pull/16`
  (`shapingthesilence/HexBoard#16`), opened after final HEAD `119a7bb`.
- Implementation commits after the USB backup workflow and follow-up fixes:
  `3097b7589884d2dc4d1c71192d1f60609e24c7f5` (`Release sequencer overlay while
  file menus are active`), `2c31c56479a1ad0830e07067a1d59037c644c9c7`
  (`Clear sequencer overlay for menu display`),
  `9569dc048aa5401e0da1b1a7588afc836c62fe0d` (`Show sequencer file actions as
  status toasts`), `7664ea64961f0cac19a077f6d9dbf04764135647` (`Defer
  sequencer overlay redraws while overlays are active`),
  `fc256d736ff7ebdaaf1ad222b6a452683d506a41` (`Rename blank sequencer state
  and default save suggestion`), `b1d63bc2e2db4a97536fcc50fe3a1c4166b1ace2`
  (`Render sequencer title in menu header strip`),
  `7f08bc118e4c24fc676315c4ae26a89eaedc7975` (`Align sequencer menu title
  strip text`), and `c0c9616178605c6452dedc5eb061fa010510064d` (`Refine
  sequencer filename header styling`).
- Additional PR-readiness and small documentation commits after `c0c9616`:
  `96c3972cf262e5ee0052c7a2ddb4eb872fc44946` (`Remove tracked Python cache
  artifact`), `a709c7b5495d4386d4577fa43568a333f3a6912e` (`add me to
  README.md`), and `119a7bb3c3a9a5aa3c985f9b75b08b8807a3863d` (`reference
  optional sequencer in features.`).
- Changed files verified from the follow-up commit stack:
  `.gitignore`, `README.md`, `docs/sequencer-manual.md`,
  `scripts/__pycache__/hexboard_backup_lib.cpython-314.pyc` (removed by
  cleanup commit `96c3972`),
  `src/firmware/menu/MenuAndDisplay.cpp`,
  `src/firmware/sequencer/SequencerFileMenu.cpp`,
  `src/firmware/sequencer/SequencerMode.cpp`,
  `src/firmware/sequencer/SequencerMode.h`,
  `src/firmware/sequencer/SequencerOverlay.cpp`,
  `src/firmware/sequencer/SequencerOverlay.h`,
  `src/firmware/sequencer/SequencerStorage.cpp`,
  `src/firmware/sequencer/SequencerTools.cpp`, and
  `src/firmware/sequencer/SequencerTools.h`.
- Behavior completed: file menu entry and action feedback now uses transient
  sequencer status/toast behavior instead of fighting the overlay; sequencer
  overlay redraws are deferred while other overlays are active; menu display
  can clear sequencer overlay state when needed; the blank display state and
  default save suggestion names were clarified; and the sequencer menu title
  plus loaded filename header styling now render through the menu header strip
  path with follow-up alignment refinements.
- PR-readiness cleanup completed: the generated Python bytecode file
  `scripts/__pycache__/hexboard_backup_lib.cpython-314.pyc` was removed from
  git, `.gitignore` now ignores Python cache artifacts, and final tracked-file
  checks found no tracked `__pycache__` or `.pyc` files.
- Behavior intentionally not included: full old sequencer manuals/layouts/
  requirements port, any new sequencer feature port, or broader redesign of
  file/menu/overlay behavior outside the completed bug fixes.
- Verification: planning-thread check confirmed the upstream implementation
  checkout is clean at `119a7bb`, `origin/codex/sequencer-feature-flag-shell`
  also points at `119a7bb`, and `git diff --check` passed. Robert reported the
  final aggregate disabled and enabled builds, focused manual hardware
  checklist, and host backup tool smoke check were completed. Local artifact
  check found `build/sequencer-disabled/HexBoard.ino.uf2` (`1456128` bytes),
  `build/sequencer-enabled/HexBoard.ino.uf2` (`1564672` bytes),
  `build/sequencer-disabled/HexBoard.ino.bin` (`728004` bytes), and
  `build/sequencer-enabled/HexBoard.ino.bin` (`782252` bytes), all modified
  `2026-06-27 15:36:22`.
- Next: watch aggregate PR #16 review/CI, update this ledger after meaningful
  PR changes or merge, and leave the full sequencer manuals/layouts/
  requirements documentation pass explicitly deferred until the final docs
  phase.

### Physical menu shortcut buttons

- Source commits: `7579e06`, `2f32fc4`, `a54fcf5`.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/physical-menu-shortcut-buttons`.
- PR: `https://github.com/shapingthesilence/HexBoard/pull/14`, targeting
  upstream `development`.
- Changed files reported/reviewed:
  `src/firmware/hardware/GridScanRotary.cpp`, `docs/user-manual.md`, and
  `docs/developer-guide.md`.
- Behavior completed: while an OLED menu or virtual list browser is active,
  hold the bottom command button as a modifier and press the top/middle command
  buttons to navigate. Normal navigation uses top=up and middle=down; GEM value
  editing uses top=increase and middle=decrease. Shortcut button state is
  masked during wheel updates so menu navigation does not also move velocity,
  modulation, or pitch bend.
- Verification: `git diff --check` passed; direct checkout `make` failed
  because the folder name does not match `HexBoard.ino`; temp-folder build from
  `/private/tmp/hexboard-build-physical-menu-shortcut-20260624/HexBoard` passed
  with `648544` bytes program storage and `191764` bytes globals. Robert
  manually tested the shortcut and confirmed it works.
- Next: watch PR review/CI and update this ledger after the PR is merged,
  closed, or requires follow-up changes.

### Current tuning/layout/scale row markers

- Source commit: `7ad7e46`.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/current-item-menu-markers`.
- Implementation commit: `5d307e3` (`Mark current geometry menu items`).
- PR: `https://github.com/shapingthesilence/HexBoard/pull/15`, targeting
  upstream `development`.
- Changed files reported/reviewed: `src/firmware/menu/VirtualListMenu.h`,
  `src/firmware/menu/VirtualListMenu.cpp`,
  `src/firmware/menu/GeometryMenu.cpp`,
  `src/firmware/hardware/GridState.h`,
  `src/firmware/hardware/GridState.cpp`,
  `src/firmware/storage/PresetSyncGeometry.cpp`,
  `docs/user-manual.md`, and `docs/developer-guide.md`.
- Behavior completed: the Tuning, Layout, and Scales virtual-list browsers mark
  the active selectable item with a leading `*` and initially focus that row
  when it is visible. The generic `VirtualListMenuProvider` support avoids
  marking Back rows, folder links, and label rows.
- Verification: `git diff --check HEAD~1..HEAD` passed; temp-folder build from
  `/private/tmp/hexboard-current-item-menu-markers/HexBoard` passed with
  `649160` bytes program storage and `191380` bytes RAM. Firmware artifact:
  `/private/tmp/hexboard-current-item-menu-markers/HexBoard/build/HexBoard.ino.uf2`.
  Robert manually tested the row markers and confirmed they work.
- Review notes: implementation branch is independent from PR
  `shapingthesilence/HexBoard#14`; `upstream/development..HEAD` contains only
  `5d307e3`. Planning-thread review found no blocking issues.
- Next: watch PR review/CI and update this ledger after the PR is merged,
  closed, or requires follow-up changes.
