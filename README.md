# Blue

Blue is a proposed C and Lua synthesizer for evolving pads, metallic tones and granular textures. One C engine serves Windows, Linux, macOS, Web and WebXR. Musicians can edit and share the external sounds, graphics, presets and Lua scripts that define an instrument.

| Design review | Contents |
| --- | --- |
| [Instrument concept](openspec/changes/add-blue-instrument/proposal.md) | How Blue plays, the Absynth reference and the scope of v1 |
| [Technical design](openspec/changes/add-blue-instrument/design.md) | Signal path, real-time C/Lua boundary, editable packs, platform adapters and test strategy |
| [Implementation plan](openspec/changes/add-blue-instrument/tasks.md) | Seven milestones, dependencies, effort estimates and acceptance checks |
| [Behavior specifications](openspec/changes/add-blue-instrument/specs/) | Testable requirements and acceptance scenarios for the six proposed capabilities |

The project is in the planning phase. To validate the proposal from the repository root with Node.js 22.20 or newer, run the pinned OpenSpec CLI.

```sh
npx --yes --package=@fission-ai/openspec@2.1.1 openspec validate add-blue-instrument --strict --no-interactive
```
