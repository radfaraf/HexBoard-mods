---
name: tb303-pattern-decoder
description: Decode Roland TB-303, RE-303, TD-3, TD-3-MO, and web-style acid pattern diagrams or screenshots, especially when a user asks what circles, tie lines, note and time rows, accent or slide marks, octave up/down flags, or shorthand symbols mean, or wants a 303 pattern converted into a HexBoard-friendly step entry list.
---

# TB-303 Pattern Decoder

Use this skill when a user shares a 303-style pattern image, acid pattern screenshot, chart, or shorthand text and wants to know what the symbols mean or how to enter it on HexBoard.

## Workflow

1. Identify the notation style:
   - classic 303 split `pitch` and `time mode`
   - web chart with `NOTE`, `UP`, `DOWN`, `ACCENT`, `SLIDE`, `TIME`
   - shorthand text that mixes notes with rests, ties, accents, or slides
2. Decode the source symbols before translating them.
3. Separate `certain` meanings from `best-effort` guesses. If a symbol convention is screenshot-specific or site-specific, say so.
4. When converting to HexBoard, prefer what HexBoard actually supports:
   - notes per step
   - empty steps
   - Tie
   - length percent
   - velocity
   - probability
5. For actionable `TD-3` or `TD-3-MO` entry guidance, map `slide` to `150%` length on the source note step rather than `Tie`.
6. Always say that this overlap-based slide mapping is specific to `TD-3` / `TD-3-MO` playback with the device configured for `Slide mode` plus `Multi Trigger Off`.
7. If the source pattern depends on other 303 behavior that HexBoard does not match exactly, give the closest usable HexBoard entry and name the compromise.

## HexBoard Mapping

- `note` time step: put a note on that step
- `rest` time step: leave the step empty
- `tie` time step: use HexBoard `Tie`
- `accent`: raise velocity on that step
- `slide`: explain that it is distinct from Tie and, by default, map it to `150%` length on the sliding note's source step for `TD-3` / `TD-3-MO` playback with `Slide mode` enabled and `Multi Trigger Off`
- octave flags such as `UP` or `DOWN`: shift the displayed pitch by one octave or tuning cycle as appropriate

## Output Expectations

When answering with this skill:

- explain the source notation in plain language first
- clearly label any uncertain symbol meanings
- if asked for conversion, provide a step-by-step HexBoard entry list
- when slides appear in a conversion, default to the `150%` overlap method and label it as `TD-3` / `TD-3-MO` specific
- prefer compact tables or numbered steps over long prose

Read [references/notation-guide.md](references/notation-guide.md) when you need the common 303 symbol mappings or translation rules.
