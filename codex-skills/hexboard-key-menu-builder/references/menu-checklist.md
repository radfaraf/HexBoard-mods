## Menu Checklist

Use this checklist for HexBoard key-driven menu or overlay work.

### 1. Interaction Shape

- Define what opens the menu.
- Define whether it is a full menu page, temporary overlay, picker, or modal tool screen.
- Define what closes it.
- Define whether it returns to the prior overlay, the main mode, or the current selection state.

### 2. Physical Mapping

- List the exact button indices or physical rows used by the menu.
- Keep step pads, note pads, and command buttons conceptually separate.
- If moving a menu lower or higher on the board, verify what newly exposed area should do while the menu is open.

### 3. Visible And Live Areas

- Decide which keys remain visible.
- Decide which keys remain interactive.
- Mask unrelated LEDs so stale Keyboard or Sequencer colors do not leak through.
- Keep only the intended mode-owned LEDs active during the menu.

### 4. OLED Contract

- Define the title line.
- Define the compact state line if needed.
- Define the visible choices or helper labels.
- Keep wording short enough for the OLED width.

### 5. State And Persistence

- Decide whether the menu edits runtime-only state, profile settings, or saved document/sequence data.
- If persistence is involved, use `hexboard-persistence-guard`.
- Verify `New`, `Revert`, load, startup restore, and reset helpers if the menu changes saved behavior.

### 6. Documentation

- Update the mode manual.
- Update the quick manual.
- Update or add a layout reference for the screen if the physical or OLED layout changed.
- Update requirements when the intended behavior changed, not just the implementation.

### 7. Hardware-Facing Result

Capture the result as a short statement such as:

- "Open tools from the third-row last button, show tool choices on lower rows, keep the top four step rows visible and tappable, and turn all unrelated lower note LEDs off."

Write this before coding when the request is ambiguous.
