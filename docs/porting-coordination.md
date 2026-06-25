# HexBoard Upstream Port Coordination

Last updated: 2026-06-24 9:17PM EDT

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
   Worker briefs should be formatted as one plain copy/paste `text` block with
   no extra surrounding prose, so Robert can paste them directly into another
   worker chat. Every worker brief should begin with this Plan Mode instruction
   before the task details:

   ```text
   Please use Plan Mode first. Review this task brief against the current
   upstream workspace and old sequencer reference, then produce a
   decision-complete implementation plan. Do not edit files until Robert
   approves the plan.
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
- Sequencer manuals, layouts, and requirements docs.
- Sequencer USB backup and backup GUI tools.
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
     lighting refinements, backup tools, and sequencer-specific played-note
     overlay behavior as separate reviewable slices.
6. Sequencer documentation pass.
   - Update sequencer user docs, requirements, and layouts once enough behavior
     is present to document accurately.

## Selected Next

- Sequencer Playback Controls Foundation. This should build on
  `codex/sequencer-feature-flag-shell` and remain part of the aggregate
  sequencer PR rather than opening a standalone upstream PR.
- Selected scope: add volatile/in-memory playback controls for Tempo, Steps,
  and Direction. Port all old direction modes: Forward, Backward, Ping-Pong,
  Random, Brownian, and Drunk.
- Explicitly out of scope: persistence/file backing, storage/browser flows,
  quick length editing, onboard synth sequencer playback, audition/tap preview,
  probability, ties, detailed tools, external MIDI sync, MIDI clock/transport
  send, and settings schema changes.

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
- Next: no follow-up implementation slice has been selected yet. The likely
  next planning candidate is sequencer edit/playback controls phase 2.

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

- Personal `docs/TODO.txt` ignore rule: do not port. This file is Robert's
  personal notes file and should be left alone by workers unless Robert
  explicitly asks for help with it.
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
