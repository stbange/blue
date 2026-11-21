# Blue instrument concept

## Why

Blue is for musicians who want to design a sound that changes throughout a held note, then perform that sound with a keyboard, a mouse or a pair of XR controllers. Absynth is the musical reference. Its three oscillator channels, hybrid synthesis, editable waves, envelopes and effects let a single patch move from a pitched instrument to a texture. [Native Instruments Absynth guide](https://docs.native-instruments.com/online-guides/absynth-6-user-guide/en/welcome-to-absynth-6)

## What Changes

Imagine holding a chord in **Glass Tide**. One lane starts as a mellow wavetable, a second adds a metallic FM attack, and a third scatters grains from an original recording of a glass rim. As the attack fades, a twelve-second envelope opens the filter. Turning **Motion** brings the grains forward. Turning **Space** feeds the tail into a stereo reverb. In a headset, the player can grab the granular lane and move it across the listening field. The same preset opens on a laptop.

- **Play.** Three parallel sound sources per voice, two inserts per lane, eight assignable macros and a shared stereo effects section. Start with a playable preset, solo a layer, then open its wave or envelope editor.
- **Design.** Combine subtractive and morphing wavetable synthesis, two-operator FM, ring modulation, sample playback and granular synthesis. Long, looping breakpoint envelopes give motion its own shape and tempo.
- **Extend.** Put an original sample, a preset and a plain-text Lua script in a content pack. The script can build a patch from C modules, define a panel and map a gesture. Reload the pack and play the result on each host.
- **Compare by ear.** Four original listening patches cover an evolving pad, a struck FM instrument, a granular freeze and a tempo-locked pulse. The plan pairs musical listening tests with measurements of pitch, timing, spectrum and tail behavior.
- **Ship in stages.** Prove the signal path with a single voice on desktop and Web first. The proposed v1 delivers standalone Windows, Linux and macOS instruments, a browser instrument built with Emscripten, and an immersive WebXR performance mode. A DAW plug-in is a subsequent release decision.

## Capabilities

### New Capabilities

- `hybrid-synthesis` defines the playable voice, modulation and effects. [Requirements](specs/hybrid-synthesis/spec.md)
- `audio-rendering` defines timing, real-time behavior and render consistency. [Requirements](specs/audio-rendering/spec.md)
- `patch-packs` defines loose assets, patch compatibility and editable Lua extensions. [Requirements](specs/patch-packs/spec.md)
- `instrument-hosts` defines native and browser playback, editing and recall. [Requirements](specs/instrument-hosts/spec.md)
- `xr-performance` defines immersive controls, spatial response and session recovery. [Requirements](specs/xr-performance/spec.md)
- `release-verification` defines the unit coverage gate, end-to-end evidence and musical acceptance. [Requirements](specs/release-verification/spec.md)

### Modified Capabilities

This is the repository's first instrument proposal.

## Impact

The first release will establish a portable C17 engine and preset format, a versioned Lua authoring API, and adapters for native audio, Web Audio and WebXR. Factory sounds, graphics and scripts will ship as editable files beside the program. The main investment is getting the same patch to sound and behave consistently across the five targets. Stereo, portable packs and XR gesture mapping are proposed v1 priorities. [Technical design](design.md) · [Implementation milestones and estimates](tasks.md)

This change is open for design review. The checked-in requirements describe planned behavior. Track implementation in the numbered checklist and archive the change after the release evidence is available. [OpenSpec workflow](https://github.com/Fission-AI/OpenSpec/blob/main/docs/getting-started.md)
