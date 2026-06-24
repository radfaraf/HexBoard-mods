# HexBoard Upstream Port Coordination

Last updated: 2026-06-24 4:38PM EDT

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

Use this flow for each small upstream PR.

1. Robert chooses one candidate.
2. This planning thread writes a worker-ready task brief with source commits,
   target behavior, target seams, docs to update, exact verification, firmware
   compile requirements, and the local path/link to the compiled firmware
   artifact Robert should test.
   Worker briefs should be formatted as one plain copy/paste `text` block with
   no extra surrounding prose, so Robert can paste them directly into another
   worker chat.
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

These are the most plausible non-sequencer, non-doc changes from the old branch
to consider first.

- Arduino IDE sync helper improvements.
  - Source commits include `1cc98fe` plus older helper-script commits.
  - Behavior: improves helper workflow for syncing the project to an Arduino
    IDE-friendly layout.
  - Status: candidate only if the current upstream layout still has a concrete
    Arduino IDE pain point. Do not port blindly because upstream has changed
    structure.

## Maybe Already Covered

These should be reviewed before selecting, because current upstream appears to
already include equivalent or stronger behavior.

- Note display overlay behavior.
  - Source commits: `05ed91a`, `f88b66f`, `6ae908b`.
  - Upstream status: likely covered by the modular `PlayedNotesOverlay` system,
    including sleep/wake behavior, hold-after-release behavior, menu badge, and
    overlay dismissal.
  - Only reconsider after hardware testing finds missing behavior.
- MIDI-IN animation press-lighting fix.
  - Source commit: `21428d3`.
  - Upstream status: already present. Upstream avoids normal user key
    press-lighting while MIDI-IN animation mode owns the LED path.
- Settings migration safety when settings count grows.
  - Source commit: hidden inside `2f32fc4`.
  - Upstream status: already covered in stronger form with versioned
    per-profile widths, CRC checks, legacy remaps, and migration saves.

## Deferred

- Sequencer feature port.
- Sequencer manuals, layouts, and requirements docs.
- Sequencer USB backup and backup GUI tools.
- TB-303 pattern decoder skill.
- Repo-owned Codex skill updates and old AGENTS/process-only changes.
- Old compile-helper tweaks unless a new concrete need appears.

## Selected Next

- None selected.

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
| Current tuning/layout/scale row markers | Complete, reviewed, tested | Not submitted | `codex/current-item-menu-markers` |

## Completed Port Details

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
- Next: push/open a PR when Robert is ready, then update this ledger with the PR
  URL and upstream status.

## Rejected Or Already Upstream

- Personal `docs/TODO.txt` ignore rule: do not port. This file is Robert's
  personal notes file and should be left alone by workers unless Robert
  explicitly asks for help with it.
- Bigger menu/header text: do not port for now. Avoid changing upstream menu
  sizing unless later device testing shows the current menu needs readability
  tweaks.
- MIDI-IN animation press-lighting fix: already upstream.
- Note display overlay behavior: likely already upstream; verify only if a
  hardware test shows a gap.
- Settings migration safety for growing settings arrays: already upstream in a
  stronger migration framework.

## Open Questions

- Does current upstream still need any Arduino IDE sync helper, or did the new
  root-sketch layout make that obsolete?
