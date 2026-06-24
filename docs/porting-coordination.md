# HexBoard Upstream Port Coordination

Last updated: 2026-06-24 3:22PM EDT

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

## Strong Candidates

These are the most plausible non-sequencer, non-doc changes from the old branch
to consider first.

- Bigger menu/header text.
  - Source commit: `98bfc91`.
  - Behavior: adjusts GEM menu sizing/font so menu headers are easier to read.
  - Status: candidate, not selected yet. Needs fit check against upstream's
    newer menu layout.
- Personal `docs/TODO.txt` ignore rule.
  - Source commit: `3afbfd6`.
  - Behavior: keeps Robert's personal `docs/TODO.txt` out of version control.
  - Status: candidate, likely fork/local workflow only unless upstream wants
    personal notes ignored.
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

- Current tuning/layout/scale visibility in menus.
  - Source commit: `7ad7e46`.
  - Upstream status: likely covered by dynamic main-menu labels such as
    `Tuning:...`, `Layout:...`, and `Scale:...`, plus newer user-geometry menu
    handling.
  - Only reconsider if Robert specifically wants row-level `* current item`
    markers inside the list rows.
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

- None yet. Robert has not selected the first implementation slice.

## Completed

- Preserved the `hexboard-sequencer` source/archive branch locally and on
  GitHub at `e33dae7fd0421c5e82dcb9902e583e4d1c104757`.
- Created the upstream implementation workspace:
  `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream`.
- Created this coordination branch and ledger.
- Physical menu shortcut buttons.
  - Source commits: `7579e06`, `2f32fc4`, `a54fcf5`.
  - Implementation branch:
    `/Users/robertw/Documents/Arduino/HexBoard-port-sequencer-upstream` on
    `codex/physical-menu-shortcut-buttons`.
  - Changed files reported/reviewed:
    `src/firmware/hardware/GridScanRotary.cpp`,
    `docs/user-manual.md`, and `docs/developer-guide.md`.
  - Behavior completed: while an OLED menu or virtual list browser is active,
    hold the bottom command button as a modifier and press the top/middle
    command buttons to navigate. Normal navigation uses top=up and middle=down;
    GEM value editing uses top=increase and middle=decrease. Shortcut button
    state is masked during wheel updates so menu navigation does not also move
    velocity, modulation, or pitch bend.
  - Verification reported by implementation worker: `git diff --check` passed;
    direct checkout `make` failed because the folder name does not match
    `HexBoard.ino`; temp-folder build from
    `/private/tmp/hexboard-build-physical-menu-shortcut-20260624/HexBoard`
    passed with `648544` bytes program storage and `191764` bytes globals.
    Robert manually tested the shortcut and confirmed it works.
  - Review status: reviewed in this planning thread with no blocking findings.
    Leave the implementation branch in place until the implementation is
    committed and either merged/cherry-picked into the intended upstream-port
    branch or otherwise archived.

## Rejected Or Already Upstream

- MIDI-IN animation press-lighting fix: already upstream.
- Note display overlay behavior: likely already upstream; verify only if a
  hardware test shows a gap.
- Settings migration safety for growing settings arrays: already upstream in a
  stronger migration framework.

## Open Questions

- Which candidate should be selected first for an implementation worker?
- Should `docs/TODO.txt` ignore behavior remain local/fork-only, or is it worth
  proposing upstream?
- Does current upstream still need any Arduino IDE sync helper, or did the new
  root-sketch layout make that obsolete?
- If current menu labels already show the selected tuning/layout/scale, does
  Robert still want row-level `* current item` markers?
