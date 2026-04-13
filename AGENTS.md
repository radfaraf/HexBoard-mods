# HexBoard Sequencer Project Instructions

These are standing instructions for work on the HexBoard firmware and its
mode-specific documentation for Codex to follow.

## Core Project Rules

- Always maintain Arduino IDE compatibility.
- Keep Keyboard mode and Sequencer mode separated as much as practical.
- Prefer keeping Keyboard-related logic in `src/HexBoard.ino`.
- Prefer keeping Sequencer-related logic in `src/SequencerMode.cpp` and `src/SequencerMode.h`.
- Only leave bridge/helper code in `src/HexBoard.ino` when it is truly needed for shared board access or integration.
- When adding shared helpers in `src/SequencerMode.cpp`, either place them after
  the internal functions they call or add explicit forward declarations in the
  existing prototype block near the top of the file.
- Before finishing a `src/SequencerMode.cpp` refactor that adds new helper
  functions, scan for any new calls to file-local functions that are defined
  later in the file and add prototypes for them if needed. ArduinoIDE builds
  can fail on these ordering issues even when the code change itself is small.

## Project Skills

- Repo-owned Codex skill copies live in `codex-skills/`.
- Keep project-specific skills in that folder so they stay versioned with the firmware and docs.
- Treat the repo copies as the project source of truth when reading or updating project skills.
- Current project-owned skills:
  - `codex-skills/hexboard-persistence-guard/`
  - `codex-skills/hexboard-key-menu-builder/`
  - `codex-skills/tb303-pattern-decoder/`
- When a task matches one of those skills, prefer reading the repo copy even if another installed copy exists elsewhere.
- When a project skill is intentionally updated, keep its related references and `agents/openai.yaml` in sync in the same pass.
- Always update the repo copy under `codex-skills/` first so the change is versioned and can be committed and pushed to GitHub.
- Never treat the installed copy under `$CODEX_HOME/skills/` as the source of truth for project skills.
- If an installed copy exists under `$CODEX_HOME/skills/` (for this machine, `/Users/robertw/.codex/skills/`), sync it from the updated repo copy in the same pass.

## Code Commenting Preference

- Add short comments to new or changed code when the purpose is not obvious.
- Prefer comments above functions and important blocks over lots of inline comments.
- Focus comments on intent, reason, assumptions, hardware/UI mapping, or library quirks.
- Do not comment obvious line-by-line behavior when the code already reads clearly.
- Keep comments brief and useful, usually one line and sometimes a few short lines when context matters.
- When changing tricky code, update or remove nearby comments so they stay accurate.
- When a bug fix reveals a non-obvious runtime split such as different render
  paths, empty-vs-programmed state, or mode-specific branches, leave a short
  comment near that branch explaining the split and the bug it can cause.
- Prefer documenting the real decision point in code over only describing the
  symptom in chat, so future work starts from the correct branch faster.

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
- if a Sequencer change affects requirements, include the relevant `docs/sequencer/requirements` updates as part of the documentation pass
- do not regenerate the manual PDFs unless the user explicitly asks for PDF updates
- this lets the user start compiling and testing while commit-message and documentation work continues
- only wait for separate confirmation before docs if the user explicitly asks for that slower workflow in a specific case

Manual files:

- `docs/sequencer/manuals/sequencer_quick_manual.txt`
- `docs/sequencer/manuals/sequencer_quick_manual.pdf`
- `docs/sequencer/manuals/sequencer_manual.txt`
- `docs/sequencer/manuals/sequencer_manual.pdf`

Manual roles:

- `sequencer_quick_manual` is the short manual.
- `sequencer_manual` is the more detailed manual.

When the manuals are updated:

- keep the quick manual brief but complete
- keep the detailed manual clearer and more thorough without becoming bloated
- avoid leaving outdated behavior or superseded workflows in either manual
- describe the current behavior directly without comparing it to removed older behavior unless the user specifically asks for history or migration notes
- only regenerate the PDF versions after updating the text versions when the user explicitly asks for PDF updates

## Keyboard Documentation Maintenance

Whenever Keyboard mode behavior, direct playing controls, tunings, layouts,
scales, keyboard-visible menus, profiles, MIDI, synth, delegated control, or
screen/LED feedback changes in a meaningful way, update the keyboard docs to
match the current behavior.

Keyboard documentation folders:

- `docs/keyboard/manuals/`
- `docs/keyboard/requirements/`
- `docs/keyboard/layouts/`

Keyboard manual files:

- `docs/keyboard/manuals/keyboard_quick_manual.txt`
- `docs/keyboard/manuals/keyboard_manual.txt`

Keyboard documentation guidance:

- keep Keyboard and Sequencer docs separate
- treat the Keyboard docs as the current best structured reference, while
  remembering they began as a reconstruction from code and comments
- keep confidence notes short and readable when a Keyboard behavior is inferred
  rather than hardware-verified
- use `Shared setting:` wording when a keyboard-visible setting also affects
  Sequencer mode
- do not regenerate or create PDF versions for Keyboard docs unless the user
  explicitly asks for them

## Layout Reference Maintenance

Keep the layout reference files in the mode-owned layout folders current
whenever a Keyboard or Sequencer screen, menu, browser flow, naming screen, or
other structured UI layout changes in a meaningful way.

Current layout reference folders:

- `docs/keyboard/layouts/`
- `docs/sequencer/layouts/`

When needed:

- update the existing layout reference text files in the relevant mode folder
- create new layout reference files for newly added complex screens or menus
- remove or revise outdated layout descriptions so they match current behavior
- describe layouts in terms of the current UI only, not past versions, unless the user specifically asks for comparison notes
- keep Keyboard and Sequencer layout references in their own folders

These layout files are documentation only and are not part of the firmware.

## Requirements Maintenance

Keep the requirements files in the matching mode folder current whenever
Keyboard or Sequencer behavior changes in a meaningful way.

Current requirements folders:

- `docs/keyboard/requirements/`
- `docs/sequencer/requirements/`

When needed:

- update the existing requirement entries so they match the current intended behavior
- add new requirement entries for meaningful new Keyboard or Sequencer features or workflows
- avoid reusing old requirement IDs for unrelated meanings
- prefer updating an existing requirement ID when the same feature changes details
- treat the requirements docs as an intended-behavior reference alongside the latest explicit user instructions
- if requirements and code disagree, treat that as something to clarify or align rather than ignoring it
- keep Keyboard and Sequencer requirements in their own folders

## Maintenance Preference

- If a behavior has changed over time, make sure the current code and the manuals reflect only the latest intended behavior.

## Commit Message Preference

- When there are uncommitted changes, always suggest one commit message that covers everything currently uncommitted, not just the latest turn's work.
- If earlier changes are still uncommitted, keep including them in the suggested message until they are actually committed.
- Phrase the message around the main resulting change as a whole. If the uncommitted work is "add feature X" plus follow-up fixes to finish feature X before commit, the message should usually be about adding feature X, not about the latest fix in isolation.
- Once some changes have been committed, stop mentioning them and base future suggestions only on what is still uncommitted.
- This applies across all affected files, including `AGENTS.md`, manuals, layout docs, requirements, scripts, and code.
- Keep commit message suggestions simple, clear, and easy to use.
- Format commit suggestions like this so they are easy to spot:
  `COMMIT MESSAGE SUGGESTION:`
  `message here`

## Personal Notes File

- `docs/TODO.txt` is the user's personal project notes and todo file.
- Do not modify, reorganize, or overwrite `docs/TODO.txt` unless the user explicitly asks for help with it.
- Use `docs/TODO.txt` as one source to reference when suggesting future improvements or next features.
- Do not treat `docs/TODO.txt` as the only source of truth for suggestions; also consider the current codebase, recent changes, and the user's current goals.
