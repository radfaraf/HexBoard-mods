# TB-303 Pattern Notation Guide

## Common Meanings

Classic TB-303 programming separates `pitch` entry from `time mode` entry.

Typical `time mode` symbols:

- `filled circle` = note
- `open circle` = rest
- `horizontal line` = tie

Typical web-chart rows:

- `NOTE` = pitch name for the step's pitch entry order
- `UP` = octave up
- `DOWN` = octave down
- `ACCENT` = accented step
- `SLIDE` = slide into the next note on a real 303-style synth
- `TIME` = rhythmic step type, usually note/rest/tie

## Translation Rules For HexBoard

Translate the source into HexBoard in this order:

1. Build the 16-step or 32-step grid.
2. Use the pitch row plus octave flags to determine the sounding pitch on each `note` step.
3. Convert `rest` time steps into empty HexBoard steps.
4. Convert `tie` time steps into HexBoard `Tie`.
5. Map accents to higher velocity.
6. For actionable `TD-3` / `TD-3-MO` conversion, map `slide` to `150%` length on the source note step so it overlaps into the next note.
7. State that this overlap method assumes the target device is configured with `Slide mode` enabled and `Multi Trigger Off`, because that is what lets the overlapping note length trigger the slide properly.

## Confidence Guidance

Be explicit about uncertainty when:

- a screenshot only shows part of the legend
- a website may use its own circle or line styling
- octave markers are implied but not named
- the displayed note row does not fully reveal whether a time step is a sounded note, rest, or tie

## Common Caveats

- `Tie` and `slide` are not the same thing on a TB-303.
- For this skill's default actionable `TD-3` / `TD-3-MO` output, `slide` maps to overlapping length (`150%`), not to HexBoard `Tie`.
- A 303 chart may show a pitch label above a step that is not a sounded new note if the paired `time mode` step is a tie or rest.
- Web diagrams sometimes simplify or stylize the original 303 entry process, so describe the confidence level when decoding them.
