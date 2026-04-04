---
name: hexboard-key-menu-builder
description: Plan and implement HexBoard key-driven menus, overlays, and button-based pickers for Keyboard mode, Sequencer mode, or shared board UI. Use when adding, moving, or redesigning menus that are selected with HexBoard keys, command buttons, OLED prompts, or LED-guided overlays, especially when Codex should ask the right menu-design questions, map the keys cleanly, keep the correct areas live or masked, and update the matching HexBoard docs in the same pass.
---

# HexBoard Key Menu Builder

Use this skill for HexBoard menu work where keys are the primary selection method.

Treat documentation updates as mandatory, not optional.

## Workflow

1. Classify the menu as `Keyboard`, `Sequencer`, or `shared`.
2. Keep logic in the mode-owned file when possible:
   - `src/HexBoard.ino` for Keyboard-owned behavior
   - `src/SequencerMode.cpp` and `src/SequencerMode.h` for Sequencer-owned behavior
   - only leave bridge code in `src/HexBoard.ino` when shared hardware access truly requires it
3. Define the target interaction before editing code:
   - what opens the menu
   - which keys choose items
   - which keys cancel, confirm, or page
   - which pads stay visible
   - which pads stay functional
   - which LEDs must be masked off
   - what the OLED must show
   - what happens after selection
4. If the request touches saved settings, profiles, startup restore, or sequence-file data, use [$hexboard-persistence-guard](../hexboard-persistence-guard/SKILL.md) before implementation.
5. Implement the menu and its input, LED, and OLED behavior together so the interaction works as one system.
6. Update the matching manuals, layouts, and requirements in the same pass.

## Ask Or Assume

When the request is underspecified, ask only the smallest missing set of questions needed to avoid a bad menu design.

Prefer asking or confirming these items:

- menu owner: Keyboard, Sequencer, or shared
- entry point: what opens the menu
- key map: which physical keys should trigger each action
- live area: which pads should still show state or remain usable while the menu is open
- OLED copy: title, summary line, hints, and any visible tool labels
- LED rules: which keys light up, which stay visible, and which must go dark
- exit flow: what cancel, confirm, re-entry, and completion should do
- persistence: runtime-only, profile, or saved document/sequence when relevant

If nearby code already establishes a strong pattern, follow it and state the assumption after the work.

## Required Touchpoints

For any substantial key-menu change, review all of these:

- input handling for the relevant mode
- overlay state transitions
- OLED rendering
- LED masking and highlight behavior
- selection retention or reselection behavior
- any save/load or persistence path affected by the menu
- mode-owned manuals
- mode-owned layout references
- mode-owned requirements

Read [references/menu-checklist.md](references/menu-checklist.md) before implementation when the change is more than a tiny label tweak.

## Output Expectations

When using this skill during a task:

- state the menu owner
- state the target interaction in plain language
- name the key map
- name what stays visible, live, or masked
- name any persistence decision or explicitly say `runtime-only`
- update the matching docs in the same pass
- mention any assumption you made from nearby patterns
