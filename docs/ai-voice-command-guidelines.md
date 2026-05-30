AI Instruction: Voice Command Mapping for New Features
===============================================

Goal
----
Automatic helper instruction for adding Portuguese voice commands whenever a new device function is created.

When a developer or AI adds a new feature (device/web/serial), also generate and register voice command phrases so users can invoke it by voice (PT-BR).

Required outputs (for each new feature)
--------------------------------------
- 1–3 natural Portuguese command phrases (PT-BR) including 1 preferred phrase prefixed with "luci". Use short, explicit verbs.
  - Examples: "luci abrir camera", "luci gravar audio", "luci desconectar do wifi"
- Suggested command IDs (unique): use the 200+ range for ad-hoc quick commands (avoid collisions with `Runtime::kCmdBase`).
  - Example: assign consecutive IDs starting at 200 for the phrases created for the feature.
- Patch suggestions for code:
  - Add `board_.speech().queueCommand(<id>, <phrase>)` call in `LuciApp::ensureSpeechReady()` (or centralized `SpeechRegistry`).
  - Call `board_.speech().applyQueuedCommands()` after batching new phrases.
  - Add detection handling in the `Mode::VoiceWait` block to call the new feature entry function when the command ID is detected.
- Documentation updates:
  - Add a short "Voice" subsection in `docs/features/<feature>-instructions.md` with the phrases and IDs.

Constraints and best-practices
------------------------------
- Always provide phrases in lower-case and avoid diacritics when possible (ASR is more robust with ASCII), but include common accents as synonyms if convenient.
- Prefer explicit prefix "luci" to reduce accidental matches.
- Use multiple IDs if you want to register synonyms that map to the same action; handle all those IDs in the detection switch.
- Keep IDs unique and add them to the feature docs so future changes don't collide.

Example (for a fictional "Torch" feature)
-----------------------------------------
- Phrases to generate:
  - "luci ligar lanterna" (id 210)
  - "luci acender lanterna" (id 211)
- Code patches to suggest:
  - In `LuciApp::ensureSpeechReady()` add:
    - `board_.speech().queueCommand(210, "luci ligar lanterna");`
    - `board_.speech().queueCommand(211, "luci acender lanterna");`
  - In the `Mode::VoiceWait` detection add:
    - `if (board_.speech().detectCommand(210) || board_.speech().detectCommand(211)) { enterTorchOn(); return; }`
  - Update `docs/features/torch-instructions.md` with the phrases and ids.

Notes for AI implementers
------------------------
- When producing code snippets, always prefer minimal diffs: add queueCommand calls alongside existing speech registrations and add detection handling near `Mode::VoiceWait` existing checks.
- If there is a centralized `SpeechRegistry` in `unihiker-pro`, prefer updating it instead of ad-hoc changes to `ensureSpeechReady()`.


