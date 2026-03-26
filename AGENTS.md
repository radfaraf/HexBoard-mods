# HexBoard Sequencer Project Instructions

These are standing instructions for work on the Sequencer for Codex to follow.

## Core Project Rules

- Always maintain Arduino IDE compatibility.
- Keep Keyboard mode and Sequencer mode separated as much as practical.
- Prefer keeping Keyboard-related logic in `src/HexBoard.ino`.
- Prefer keeping Sequencer-related logic in `src/SequencerMode.cpp` and `src/SequencerMode.h`.
- Only leave bridge/helper code in `src/HexBoard.ino` when it is truly needed for shared board access or integration.

## Sequencer Manual Maintenance

Whenever Sequencer mode behavior, controls, menus, file management, naming, playback, or editing workflow changes, update the sequencer manuals to match the current behavior. Or any other Sequencer related features.

Preferred workflow for larger Sequencer changes:

- do the code changes first
- then tell the user the code work is done and ready for testing as part of the same reply
- use this exact line for that handoff:
  `CODING READY FOR TESTING.`
- then suggest a commit message for the current uncommitted work
- then immediately update the documentation files in the same overall pass when Sequencer docs, layouts, or requirements need it
- when beginning that documentation pass, use this exact line:
  `UPDATING DOCS.`
- if a Sequencer change affects requirements, include the relevant `docs/requirements` updates as part of the documentation pass
- do not regenerate the manual PDFs unless the user explicitly asks for PDF updates
- this lets the user start compiling and testing while commit-message and documentation work continues
- only wait for separate confirmation before docs if the user explicitly asks for that slower workflow in a specific case

Manual files:

- `docs/sequencer_quick_manual.txt`
- `docs/sequencer_quick_manual.pdf`
- `docs/sequencer_manual.txt`
- `docs/sequencer_manual.pdf`

Manual roles:

- `sequencer_quick_manual` is the short manual.
- `sequencer_manual` is the more detailed manual.

When the manuals are updated:

- keep the quick manual brief but complete
- keep the detailed manual clearer and more thorough without becoming bloated
- avoid leaving outdated behavior or superseded workflows in either manual
- describe the current behavior directly without comparing it to removed older behavior unless the user specifically asks for history or migration notes
- only regenerate the PDF versions after updating the text versions when the user explicitly asks for PDF updates

## Layout Reference Maintenance

Keep the layout reference files in `docs/layouts` current whenever a sequencer
screen, menu, browser flow, naming screen, or other structured UI layout
changes in a meaningful way. 

Current layout reference folder:

- `docs/layouts/`

When needed:

- update the existing layout reference text files
- create new layout reference files for newly added complex screens or menus
- remove or revise outdated layout descriptions so they match current behavior
- describe layouts in terms of the current UI only, not past versions, unless the user specifically asks for comparison notes

These layout files are documentation only and are not part of the firmware.

## Requirements Maintenance

Keep the requirements files in `docs/requirements` current whenever Sequencer
behavior changes in a meaningful way.

Current requirements folder:

- `docs/requirements/`

When needed:

- update the existing requirement entries so they match the current intended behavior
- add new requirement entries for meaningful new Sequencer features or workflows
- avoid reusing old requirement IDs for unrelated meanings
- prefer updating an existing requirement ID when the same feature changes details
- treat the requirements docs as an intended-behavior reference alongside the latest explicit user instructions
- if requirements and code disagree, treat that as something to clarify or align rather than ignoring it

## Maintenance Preference

- If a behavior has changed over time, make sure the current code and the manuals reflect only the latest intended behavior.

## Commit Message Preference

- When any changes are made and there are uncommitted changes available, always suggest a commit message for the current uncommitted work.
- Base commit message suggestions on what is currently uncommitted now, not on older work that may already have been committed.
- If earlier changes are still uncommitted, suggest one combined commit message that covers all currently uncommitted changes together.
- Commit message suggestions must always cover all currently uncommitted changes, not just the most recent change from the current turn.
- This applies to all affected files, including `AGENTS.md`, manuals, layout docs, requirements, scripts, and code.
- Only stop including earlier work in the suggested message after it has actually been committed.
- After the user commits those changes, future commit message suggestions should cover only the remaining new uncommitted changes.
- Do not keep mentioning already committed work in later commit message suggestions.
- Keep commit message suggestions simple, clear, and easy to use.
- Format commit suggestions like this so they are easy to spot:
  `COMMIT MESSAGE SUGGEST:`
  `message here`

## Personal Notes File

- `docs/TODO.txt` is the user's personal project notes and todo file.
- Do not modify, reorganize, or overwrite `docs/TODO.txt` unless the user explicitly asks for help with it.
- Use `docs/TODO.txt` as one source to reference when suggesting future improvements or next features.
- Do not treat `docs/TODO.txt` as the only source of truth for suggestions; also consider the current codebase, recent changes, and the user's current goals.
