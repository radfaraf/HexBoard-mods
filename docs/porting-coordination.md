# HexBoard Upstream Port Coordination

Last updated: 2026-06-25 12:26PM EDT

This document is the planning ledger for selectively porting useful work from
the old `hexboard-sequencer` branch into the current upstream HexBoard
`development` firmware. It is not an implementation plan for the full sequencer
yet. Robert will choose each slice manually, and implementation workers should
receive one focused task at a time.

## Current Status

- Source/archive repo: `/Users/robertw/Documents/Arduino/HexBoard-sequencer`.
- Source/archive branch: `hexboard-sequencer`.
- Preserved source commit: `e33dae7fd0421c5e82dcb9902e583e4d1c104757`
  (`delegated control md update`), matching `origin/hexboard-sequencer`.
- Coordination branch: `codex/porting-coordinator`.
- Upstream implementation workspace:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream`.
- The upstream implementation workspace should be used by other workers for
  approved porting tasks. This planning thread may inspect it for context and
  status, but should not edit it unless Robert explicitly asks.

## Working Rules

- Do not bulk merge, rebase, or blindly cherry-pick the old branch into
  upstream.
- Do not port the sequencer feature until Robert explicitly selects that phase.
- Treat the current upstream `development` architecture as the target shape:
  root `HexBoard.ino` is thin, and implementation lives under `src/firmware/`.
- Keep this branch focused on planning and coordination docs. Code changes
  belong in separate implementation work handled by other workers.
- When Robert accepts, rejects, defers, or completes a candidate, update this
  ledger in the same coordination branch.

## Standard Porting Flow

Use this flow for each small upstream PR. The sequencer feature port is an
exception; see the aggregate sequencer flow below.

1. Robert chooses one candidate.
2. This planning thread writes a worker-ready task brief with source commits,
   target behavior, target seams, docs to update, exact verification, firmware
   compile requirements, and the local path/link to the compiled firmware
   artifact Robert should test.
   Worker briefs must be delivered as exactly one fenced `text` block and the
   entire assistant reply should contain only that block. Put all task context,
   commands, verification, caveats, handoff notes, and warnings inside the
   block so Robert can copy/paste the whole reply directly into another worker
   chat. Do not put summaries, apologies, commit message suggestions, status
   notes, or any other prose before or after the block. If ledger/status notes
   are needed too, say they are separate from the worker brief and ask Robert
   before mixing them into the same reply. Every worker brief should begin with
   this Plan Mode instruction before the task details:

   ```text
   Please use Plan Mode first. Review this task brief against the current
   upstream workspace and old sequencer reference, then produce a
   decision-complete implementation plan.
   ```
   Every worker brief should also include this behavior fidelity boundary,
   either verbatim or with slice-specific additions when Robert has approved
   different behavior:

   ```text
   Behavior fidelity boundary:
   Use the old sequencer code, manuals, layouts, and requirements as the
   reference for user-visible behavior in this slice. Preserve the existing
   control flow, step layout, menu meaning, LED/key colors, defaults, timing
   behavior, and workflow unless this brief explicitly says to change them.
   Modernize the implementation to fit upstream development architecture:
   keep sequencer-owned code under src/firmware/sequencer/ as much as
   practical, use narrow hooks in non-sequencer files, split code by
   responsibility, and avoid copying fragile old storage or memory patterns
   without review.
   ```
3. The worker creates a branch in
   `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` named
   `codex/<short-feature-name>`. Default branch policy: each independent
   feature/fix starts from fresh upstream `development`, not from another
   unmerged PR branch. Only stack on another unmerged PR when the new task truly
   depends on that PR's code; if stacking is required, the worker brief must
   explicitly name the dependency PR/branch. Normal independent branch setup:

   ```bash
   cd /Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream
   git fetch upstream
   git switch development
   git pull --ff-only upstream development
   git switch -c codex/<short-feature-name>
   ```

4. The worker implements one focused slice, updates relevant upstream docs, runs
   `git diff --check`, compiles the firmware using the appropriate HexBoard
   build method, provides Robert a local path/link to the compiled firmware
   artifact for hardware testing, and commits locally. If the build fails, the
   worker should report the failure and not claim the task is ready for hardware
   testing.
5. This planning thread reviews the implementation before the ledger changes.
6. If accepted, push the branch to Robert's fork:
   `git push -u origin <branch>`.
7. Open a PR with this compare URL pattern:
   `https://github.com/shapingthesilence/HexBoard/compare/development...radfaraf:HexBoard-mods:<branch>?expand=1`.
8. Update this ledger with the branch, verification, PR URL, and current status.

## Sequencer Aggregate PR Flow

Use this flow while porting the sequencer feature.

1. Continue or stack sequencer implementation slices on
   `codex/sequencer-feature-flag-shell` unless a slice truly needs isolation.
2. Keep each local slice focused, reviewed, and committed separately so the
   aggregate PR remains understandable.
3. Run disabled and enabled builds for every meaningful sequencer milestone, and
   hardware-test milestones that change board behavior.
4. Do not open an upstream sequencer PR for placeholder-only scaffolding. Open
   one aggregate upstream PR after the enabled sequencer build has coherent core
   behavior, expected to include at least mode entry, step editing, step LEDs,
   and playback.
5. If upstream maintainers ask for smaller PRs, update this ledger before
   changing the submission strategy.

## PR Template

Use this as the starting body for future upstream PRs.

```md
## Summary

[One short paragraph describing the behavior and why it is useful.]

## Verification

- `git diff --check` passed
- Build result:
- Firmware artifact for testing:
- Manual hardware checks:
```

## Strong Candidates

- None currently selected. Most remaining old-branch work is either already
  upstream, intentionally skipped, or part of the deferred sequencer phase.

## Maybe Already Covered

These should be reviewed before selecting, because current upstream appears to
already include equivalent or stronger behavior.

- MIDI-IN animation press-lighting fix.
  - Source commit: `21428d3`.
  - Upstream status: already present. Upstream avoids normal user key
    press-lighting while MIDI-IN animation mode owns the LED path.
- Settings migration safety when settings count grows.
  - Source commit: hidden inside `2f32fc4`.
  - Upstream status: already covered in stronger form with versioned
    per-profile widths, CRC checks, legacy remaps, and migration saves.

## Deferred

- Sequencer feature port. See the sequencer roadmap below.
- Full sequencer manuals, layouts, and requirements docs.
- Sequencer USB backup and backup GUI tools.
- Sequencer performance monitor overlay.
- TB-303 pattern decoder skill.
- Repo-owned Codex skill updates and old AGENTS/process-only changes.
- Old compile-helper tweaks unless a new concrete need appears.

## Sequencer Port Roadmap

The sequencer should be ported as an optional compile-time feature, not as an
always-on change to upstream firmware. The goal is for users and maintainers to
be able to build either normal HexBoard firmware without the sequencer or a
sequencer-enabled firmware.

Working assumptions until upstream gives different guidance:

- Disabled build should be the low-risk default for early upstream PRs.
- Enabled build should be selected by a clear build flag such as
  `HEXBOARD_ENABLE_SEQUENCER=1` or an equivalent upstream-preferred name.
- Arduino IDE compatibility still matters, so the worker should consider both a
  build flag path and a small config-header path if needed.
- Both disabled and enabled builds must compile during sequencer PRs.
- When disabled, there should be no sequencer menu item, no active sequencer
  runtime behavior, and no changes to normal Keyboard-mode flow.

## Sequencer Port Architecture Notes

These notes are guardrails for later sequencer implementation slices. They are
not final runtime architecture decisions for every subsystem.

- Treat the old `hexboard-sequencer` branch as a behavior/reference source, not
  as a file-structure template to recreate.
- Preserve old user-visible sequencer behavior unless the selected worker brief
  explicitly says Robert approved a behavior change. Do not casually redesign
  menus, key colors, step layout, defaults, timing behavior, or workflows while
  reshaping the implementation for upstream.
- Keep sequencer-owned code under `src/firmware/sequencer/` as much as
  practical. Non-sequencer firmware modules should use narrow sequencer API
  hooks instead of depending on sequencer internals.
- Split sequencer code by responsibility so no single file grows into another
  oversized mode implementation. Expected seams include the mode shell,
  state/model, input/editing, LEDs, menus/overlay, playback/timing, and
  storage/file-management.
- For storage/file-management, review old heap-using paths before porting code
  that relies on `String` or `std::vector<String>`.
- For step data, consider a compact `SequencerStep` struct instead of the old
  parallel arrays, but treat that as a design review item rather than a locked
  decision.
- Do not add sequencer settings to upstream profile storage until a sequencer
  slice truly needs them. Handle settings schema and migration changes as
  deliberate compatibility work.
- During the playback slice, review old fixed playback-state buffers before
  copying their sizes. Preserve reliable note-off, tie, and preview behavior,
  but measure enabled-build RAM and choose the smallest safe static limits.
- Record program storage and globals for meaningful disabled and enabled
  sequencer milestones.

Storage browser ideas to revisit when the persistence/file-management slice
begins:

- The old browser used a 128-entry table and built each folder view around that
  limit. Do not copy that design as a settled plan without review.
- Possible directions include visible-window browsing, bounded page buffers,
  folder-first organization, repeated scans, or an optional on-disk index if a
  later need proves it useful.
- Robert and the planning thread should review these browser ideas together
  before giving a worker the storage/file-management task brief.

Local implementation slices for the aggregate sequencer PR:

1. Compile-time feature flag plus sequencer shell.
   - Add the feature flag/config mechanism.
   - Add minimal `SequencerMode` module stubs or shell integration.
   - Add no-op behavior when the feature is disabled.
   - Add a visible sequencer entry only when enabled.
   - Prove both disabled and enabled firmware builds compile.
2. Core sequencer editing.
   - Port the basic step grid, step selection, note entry, deselect behavior,
     and step LEDs behind the feature flag.
   - Keep storage, MIDI sync, backup, and advanced tools out of this slice.
3. Sequencer playback.
   - Port play/stop, timing, note output, and onboard synth/MIDI playback paths.
   - Keep advanced probability, ties, and external sync for later unless Robert
     explicitly chooses to include them.
4. Sequencer persistence and file management.
   - Port save/load/new/rename/delete behavior after core editing and playback
     are stable.
   - Keep sequencer storage separate from normal Keyboard settings where
     practical.
5. Advanced sequencer features and bug fixes.
   - Port velocity/probability editing, ties, MIDI sync, monophonic entry,
     remaining lighting refinements, backup tools, the performance monitor
     overlay, and sequencer-specific played-note overlay behavior as separate
     reviewable slices. Sequencer step light color refinements, Step Tools/
     playback semantics, and monophonic/Play Type routing are complete and
     recorded below.
6. Sequencer documentation pass.
   - Update sequencer user docs, requirements, and layouts once enough behavior
     is present to document accurately.

## Selected Next

- None currently selected. Sequencer monophonic note entry and Play Type
  routing plus the selected-step blink fix have been completed and recorded
  below.

## Completed Infrastructure

- Preserved the `hexboard-sequencer` source/archive branch locally and on
  GitHub at `e33dae7fd0421c5e82dcb9902e583e4d1c104757`.
- Created the upstream implementation workspace:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream`.
- Created this coordination branch and ledger.

## Porting Status

| Item | Local Status | Upstream Status | Branch / PR |
| --- | --- | --- | --- |
| Physical menu shortcut buttons | Complete, reviewed, tested | Submitted | `codex/physical-menu-shortcut-buttons`, `shapingthesilence/HexBoard#14` |
| Current tuning/layout/scale row markers | Complete, reviewed, tested | Submitted | `codex/current-item-menu-markers`, `shapingthesilence/HexBoard#15` |
| Sequencer feature flag and shell | Complete, reviewed, tested | Held for aggregate PR | `codex/sequencer-feature-flag-shell` |
| Step note entry and basic transport playback | Complete, reviewed, tested | Held for aggregate PR | `codex/sequencer-feature-flag-shell` |
| Sequencer playback controls foundation | Complete, reviewed, tested | Held for aggregate PR | `codex/sequencer-feature-flag-shell` |
| Sequencer MIDI audition and edit overlay | Complete, reviewed, build-tested; manual device checklist pending | Held for aggregate PR | `codex/sequencer-feature-flag-shell` |
| Sequencer step light colors | Complete, reviewed, tested | Held for aggregate PR | `codex/sequencer-feature-flag-shell` |
| Sequencer Step Tools and playback semantics | Complete, build-tested; manual device checklist pending | Held for aggregate PR | `codex/sequencer-feature-flag-shell` |
| Sequencer monophonic and Play Type routing | Complete, build-tested and locally tested; OB Synth edge-case checklist pending | Held for aggregate PR | `codex/sequencer-feature-flag-shell` |
| Sequencer selected-step blink fix | Complete, build-tested and Robert-tested | Held for aggregate PR | `codex/sequencer-feature-flag-shell` |

## Completed Port Details

### Sequencer feature flag and shell

- Sequencer roadmap slice: 1. Compile-time feature flag plus sequencer shell.
- Implementation branch:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
  `codex/sequencer-feature-flag-shell`.
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
- Implementation state: uncommitted follow-up changes after the Monophonic/Play
  Type routing slice.
- Changed files reported/spot-checked: `docs/code-analysis.md`,
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
- Review notes: planning-thread spot-check confirmed the current upstream diff
  adds the blink constants/helper and gates selected step LEDs off during the
  off phase before normal rendering, with matching upstream documentation
  updates.
- Next: no follow-up implementation slice has been selected yet.

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

## Rejected Or Already Upstream

- Personal `docs/TODO.txt` ignore rule: do not port or use as a porting
  reminder source. This file is Robert's personal notes file and should be left
  alone by workers unless Robert explicitly asks for help with it.
- Bigger menu/header text: do not port for now. Avoid changing upstream menu
  sizing unless later device testing shows the current menu needs readability
  tweaks.
- Arduino IDE sync helper improvements: do not port. The current command-line
  and temp-folder build workflow is fast enough, so adding helper-script
  changes would not solve a current upstream pain point.
- MIDI-IN animation press-lighting fix: already upstream.
- Note display overlay behavior: already upstream and improved. Upstream's
  modular `PlayedNotesOverlay` covers the old hold-after-release, release-grace,
  sleep/wake, and dismissal behavior, and adds stronger behavior such as a menu
  badge, sorted notes, label/number modes, user-geometry labels, 12-EDO chord
  names, and delegated-control suppression. Sequencer-specific note display
  behavior remains deferred with the sequencer phase.
- Settings migration safety for growing settings arrays: already upstream in a
  stronger migration framework.

## Open Questions

- None currently.
