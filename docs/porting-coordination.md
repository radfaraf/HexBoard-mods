# HexBoard Upstream Port Coordination

Last updated: 2026-06-25 7:59PM EDT

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

Storage browser outcome from the completed persistence/file-management slice:

- The old browser used a 128-entry table and built each folder view around that
  limit. The upstream v1 browser does not copy that design.
- The completed v1 browser scans only the current folder under `/Sequences`,
  orders folders before `.hbseq` files, uses `VirtualListMenu` callbacks, and
  caches current-folder counts plus an 8-row visible window instead of keeping a
  tree-wide path list in memory.
- This is intended to support hundreds of saved sequences when users organize
  them into folders. If a later real-world need proves that very large single
  folders are common, revisit optional indexing or more efficient ordered scans
  as a separate performance slice.

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
   - Completed as v1 and recorded below: sequence save/load/new/revert,
     foldered browser, create folder, rename/delete, current-path restore,
     title/dirty state, and profile-backed `Tap Preview`.
   - Sequencer storage remains separate from normal Keyboard settings except
     for profile-backed sequencer preferences.
5. Advanced sequencer features and bug fixes.
   - Remaining deferred candidates include USB Backup and desktop backup tools
     as separate reviewable slices. Sequencer step light color refinements, Step
     Tools/playback semantics, monophonic/Play Type routing, selected-step blink,
     persistence/file management v1, external MIDI clock receive, MIDI sync send,
     the performance monitor overlay, the sequencer overview screen,
     played-note overlay integration, and the follow-up sequencer display-state
     fixes are complete and recorded below.
6. Sequencer documentation pass.
   - Update sequencer user docs, requirements, and layouts once enough behavior
     is present to document accurately.

## Selected Next

- None currently selected. Sequencer monophonic note entry and Play Type
  routing, the selected-step blink fix, sequencer persistence/file management
  v1, external MIDI clock receive, MIDI sync send, the performance monitor
  overlay, the sequencer overview screen, played-note overlay integration, and
  the follow-up sequencer display-state fixes have been completed and recorded
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
| Sequencer persistence and file management v1 | Complete, compiled, Robert-tested, and planning-thread checked against final HEAD | Held for aggregate PR | `codex/sequencer-feature-flag-shell` |
| Sequencer external MIDI clock receive | Complete, compiled, Robert-tested, and planning-thread checked against `e63518d` | Held for aggregate PR | `codex/sequencer-feature-flag-shell` |
| Sequencer MIDI sync send | Complete, compiled, Robert-tested, and planning-thread checked against `275a638` | Held for aggregate PR | `codex/sequencer-feature-flag-shell` |
| Sequencer performance monitor overlay | Complete, compiled, Robert-tested, and planning-thread checked against `874ac2f` | Held for aggregate PR | `codex/sequencer-feature-flag-shell` |
| Sequencer overview screen | Complete, compiled, Robert-tested, and planning-thread checked against `22eec18` | Held for aggregate PR | `codex/sequencer-feature-flag-shell` |
| Sequencer played-note overlay integration | Complete on branch; planning-thread checked against `2bcb4c3`; build/manual verification not yet recorded | Held for aggregate PR | `codex/sequencer-feature-flag-shell` |
| Sequencer close-edit blank display fix | Complete on branch; planning-thread checked against `6d7c191`; build/manual verification not yet recorded | Held for aggregate PR | `codex/sequencer-feature-flag-shell` |
| Sequencer audition overlay blank-state preservation | Complete on branch; planning-thread checked against `94b758e`; build/manual verification not yet recorded | Held for aggregate PR | `codex/sequencer-feature-flag-shell` |

## Completed Port Details

Detailed completion records now live in [porting-completed.md](porting-completed.md).

- The status table above remains the quick summary of completed, submitted,
  and held work.
- Use the completed ledger for per-slice commit hashes, verification notes,
  firmware artifacts, manual checklist records, and planning-thread review
  notes.
- When a slice completes, update both this status table and
  `docs/porting-completed.md`.

## Rejected Or Already Upstream

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
  names, and delegated-control suppression. The aggregate sequencer branch now
  adds no-selection lower-grid audition notes to the shared played-note overlay.
- Settings migration safety for growing settings arrays: already upstream in a
  stronger migration framework.

## Open Questions

- None currently.
