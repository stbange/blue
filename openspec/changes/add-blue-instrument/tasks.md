# Blue implementation plan

The first milestone ends with a note a musician can play. The next three make it possible to author Blue's characteristic sounds. Editor, packaging and XR work then turn that engine into the five requested products. Each checkbox includes evidence that the work is complete.

The [candidate verification record](verification.md) distinguishes delivered behavior from open acceptance work. Checkboxes refer to the complete scenario, including its required evidence.

## Milestones at a glance

| Milestone | Depends on | Deliverable | Estimated engineering effort |
| --- | --- | --- | --- |
| 1. First playable instrument | Reviewed proposal | One externally defined patch and Lua macro playing through a native device and a browser worklet | 4–6 person-weeks |
| 2. Portable packs and scripting | 1 | A user edits and reloads a Lua instrument, recovers from a bad edit and moves a preset between hosts | 4–6 |
| 3. Tonal engine and modulation | 1, with 2 for authoring | Polyphonic, three-lane instrument with editable waves, FM, ring modulation and breakpoint motion | 7–10 |
| 4. Samples, grains and effects | 2, 3 | Four musically distinct patches, including a frozen recording and a granular echo | 6–9 |
| 5. Native and web instruments | 1, 2 to start. 3, 4 for full editor acceptance | Installable desktop folders and a persistent browser editor, each with audible packaged E2E | 6–9 |
| 6. WebXR performance | 1 for headset spike, 2 and 5 for integration | Play, place a layer, control a macro and return to the same patch from a headset | 3–6 |
| 7. Release qualification | 3–6 | Reproducible release candidates, measured sound and performance, compatibility evidence | 4–7 |

The proposed v1 totals **34–53 person-weeks of engineering**. This assumes experience with real-time C audio and browser integration, access to three native test environments and one physical headset. Listening-panel time and sourcing original recordings are additional. The estimates include tests with each feature. Re-estimate after the first on-device measurements. Editor work can overlap the tonal and granular engine work.

## 1. First playable instrument

- [x] 1.1 Pin CMake presets, C17 toolchains, Emscripten, a C test harness and OpenSpec. Build a portable engine library, desktop shell, offline render tool and web shell in CI. Verify clean builds on Windows, Linux and macOS and serve the Wasm build under a nested path.
- [x] 1.2 Define the C host contract for preparation, sample-frame events, render buffers and shutdown. Render a dry sine voice to WAV. Verify A4 within one cent at 44.1, 48 and 96 kHz and the exact frame bounds of an immediate gate at several block sizes.
- [x] 1.3 Load a minimal adjacent pack with one preset, a graphic and a Lua mapping for a gain macro. Route keyboard input to the same voice in a native callback and an AudioWorklet. Record a note and a gain change through both paths.
- [ ] 1.4 Bring up a private-memory Wasm worklet and a C/Lua control worker. Exercise setup from a click and run on the nominated headset's flat browser. Measure startup, message jitter and bounded staging of a second sample bank under continuous output.
- [x] 1.5 Add first-party source inventories and unit-only coverage reports for C, Lua and handwritten browser code. Verify an untouched shipping source lowers coverage, a missing report fails, and 94.9% fails without rounding. Run a separate audible E2E smoke on native and web packages.
- [ ] 1.6 Record the hardware, compiler and browser setup, voice settings and measurement procedure for the desktop and headset profiles. Save a ten-minute baseline, timestamped event trace and reference WAV. Use the observed memory and block budgets to size the pools in milestone 2.

**Exit.** Moving the folder and editing its Lua gain mapping changes the recorded sound on native and web. The clean calibration renders meet the cross-target tolerance. [Audio rendering](specs/audio-rendering/spec.md) · [Coverage and E2E](specs/release-verification/spec.md)

## 2. Portable packs and scripting

- [x] 2.1 Define v1 pack and preset documents, lookup precedence, index format, asset types, stable parameter IDs and version compatibility. Document one complete example. Round-trip it, relocate it and test missing assets, duplicate IDs and unsupported versions.
- [x] 2.2 Build the native executable-relative resolver and browser manifest fetcher. Add an explicit native root override and a user revision in browser storage. In E2E replace a sample and a script, reload them and confirm both binary and Wasm digests stay identical.
- [x] 2.3 Embed the pinned Lua interpreter and expose graph composition, named parameters, a macro binding, an editor control and bounded event scheduling. Run one extension pack unchanged on native and Wasm and verify an audible parameter response.
- [x] 2.4 Prepare graphs and samples off the audio callback. Implement two generation slots, note ownership, bounded tail retirement and a queued latest edit. Hold a chord across changes, release it, and verify correct generation selection and reclamation.
- [x] 2.5 Apply script memory/instruction limits and validate route cycles, ranges and references. Test a malformed script, an excessive allocation, a long-running script and a missing sample. Each failure must name the problem and allow a successful retry while playback continues.
- [x] 2.6 Define the versioned C node descriptor and test it with a separately registered fixture node. Verify native and Wasm recognize its type, and that a build without the node diagnoses the dependency. Pin a released-format fixture for later migration tests.

**Exit.** A musician can add a new instrument by copying a folder, diagnose a broken edit, correct it and reload during playback. [Patch packs and Lua extensions](specs/patch-packs/spec.md)

## 3. Tonal engine and modulation

- [x] 3.1 Implement bounded voices, note IDs, sustain, pitch bend and deterministic stealing. Unit-test repeated notes, same-frame ordering, pedal release, pool exhaustion and queue saturation. Verify all voices release under event floods.
- [x] 3.2 Build the three-lane graph with two ordered insert slots, lane levels, pan, solo, mute and sends. Sum independent lane fixtures and compare their output with the combined graph within floating-point tolerance.
- [x] 3.3 Add band-limited oscillators, editable harmonic frames, mipmapped wavetables and interpolation. Verify a morph preserves pitch and test alias energy with bin-centered, high-note sweeps against a band-limited reference. Pin accepted thresholds with the fixture.
- [x] 3.4 Implement two-operator FM and ring modulation. Test sideband frequencies and depths with sine inputs. Verify the 440 Hz × 110 Hz ring-mod fixture produces 330 Hz and 550 Hz components.
- [x] 3.5 Add the envelope evaluator, curved segments, tempo sync, sustain, loops and release. Unit-test 1- and 128-point edge cases, invalid point counts, tempo changes and a note-off inside a loop at an intra-block offset.
- [ ] 3.6 Add three LFOs, eight macros and a modulation matrix with stable destination IDs, rate classes and smoothing. Connect the multimode filter, waveshaper and tuned resonator. Measure filter response, finite output under high drive, resonance pitch and recall of one macro feeding two destinations.

**Exit.** Load two independently programmed tonal patches, play a polyphonic phrase and capture distinct pitch, timbre and envelope behavior on native and Wasm. [Hybrid synthesis](specs/hybrid-synthesis/spec.md) · [Audio rendering](specs/audio-rendering/spec.md)

## 4. Samples, grains and effects

- [x] 4.1 Decode mono and stereo PCM WAV away from rendering. Implement root tuning, loop points, resampling and length limits. Verify pitched playback, loops at boundaries, short files, bad headers and oversized inputs.
- [x] 4.2 Build a fixed grain pool with position, size, density, pitch, freeze and per-voice seeded scatter. Verify repeatability with different block partitions, the number of starts at two densities and predictable behavior at the grain cap.
- [x] 4.3 Add tempo delay, a pitch-shifted granular echo mode and stereo diffusion reverb. Bound feedback and tail lifetime. Test tap timing, a pitch-shifted return, stereo decay and finite output through silence and extreme valid settings.
- [ ] 4.4 Stage sample banks incrementally during live playback and bound peak memory for old and new patches. Capture audio during a large reload on desktop and headset. Verify the previous bank handles held notes and the process stays within the milestone 1 budget.
- [x] 4.5 Author Glass Tide, Struck Wire, Paper Cloud and Step Current using original or appropriately licensed input assets with provenance in their pack. Store a 20-second performance per patch and analysis windows for envelope movement, FM decay, freeze/density and beat timing, respectively.
- [x] 4.6 Add seeded native/Wasm parity fixtures, a reference-band definition and automated RMS/spectral/onset comparisons. Confirm that detuning an oscillator, silencing one lane and moving a tempo event past its expected sample each produce a failing report.

**Exit.** The four performances render from loose packs within the prescribed timing and spectral tolerances and are ready for listening review. [Hybrid synthesis](specs/hybrid-synthesis/spec.md) · [Release verification](specs/release-verification/spec.md)

## 5. Native and web instruments

- [ ] 5.1 Build the shared Lua panel model and implement native and web controls for preset browsing, lane solo, source selection, harmonic editing, envelope editing, routes and macros. Replay an editor gesture sequence and compare the resulting preset documents.
- [ ] 5.2 Complete native audio configuration, device loss and rate changes. Map Windows, macOS and Linux MIDI input to the common event stream and support keyboard playing. Verify note, velocity, pressure, bend, mod wheel and sustain with virtual MIDI endpoints and one real controller.
- [x] 5.3 Add writable user saves, pack import/export and live stereo WAV recording with file I/O off the audio thread. Export a desktop patch, import it in the browser, reload the page, export it back and verify parameters, assets and audio.
- [x] 5.4 Package a portable executable with adjacent content on each native OS and a static web folder with loose assets. Verify inventory, build-input and binary-content checks exclude embedded factory assets. Launch distributions from paths with spaces, a different working directory and a nested web URL.
- [x] 5.5 Automate the visible native editor on each OS using OS input and a virtual audio endpoint or live master recording. Play and release a note, edit and save a macro, restart and recall, then restore a missing asset. Assert on the captured WAV and saved state.
- [x] 5.6 Run Playwright on Chromium, Firefox and WebKit against the packaged site. Exercise audio unlock, denied MIDI, editing, recording, pack import, reload and a missing asset. Capture downstream worklet output and retain WAVs, traces and failure screenshots.

**Exit.** CI can unpack and play all three desktop products and the ordinary web instrument. A patch round-trips from desktop to web with its dependencies. [Instrument hosts](specs/instrument-hosts/spec.md) · [Patch packs](specs/patch-packs/spec.md)

## 6. WebXR performance

- [x] 6.1 Detect immersive support, enter from a user gesture and connect the session to the existing engine. Verify supported, absent and denied runtimes all leave a working instrument.
- [ ] 6.2 Build the three-source scene, controller picking, trigger notes, grip placement and macro binding. Feed timestamped commands to the same parameter IDs as the flat editor. Replay controller poses and verify the resulting parameter values and releases.
- [x] 6.3 Implement optional listener-relative panning and distance gain. With one dry sustained lane, verify left/right energy changes with source position and head rotation. Verify spatial bypass recalls the saved stereo pan.
- [x] 6.4 Automate the full XR journey with IWER and Playwright. Capture actual engine output while entering, playing, moving the source and exiting. Disconnect a held controller and assert its note releases. Confirm the flat editor retains the patch and last macro value.
- [ ] 6.5 Run the release journey on the selected physical headset. Record controller reach, tracking and audio continuity through entry, exit and session interruption, plus ten minutes of the agreed performance profile.

**Exit.** A player can locate a sound by ear, move it, change its timbre and return to the flat editor. Keep the emulated and on-device records with the candidate. [WebXR performance](specs/xr-performance/spec.md)

## 7. Release qualification

- [x] 7.1 Run the whole unit-only matrix and gate each coverage group and total at ≥95% executable lines. Publish the source inventory, covered/eligible counts and branch reports. Verify a real uncovered host file appears in its owning report.
- [ ] 7.2 Run all packaged native, browser and XR E2E suites from clean installs. Exercise recall of a prior supported document, a failed pack activation and recovery. Attach audio, input trace, diagnostics and screenshots from the same build.
- [ ] 7.3 Run optimized ten-minute performance trials on the recorded desktop and headset profiles, plus a soak with parameter floods and repeated reloads. Publish p99 DSP time, block period, voice/grain counts, peak memory and capture continuity.
- [ ] 7.4 Loudness-match the four original reference renders and conduct the three-listener review. Record scores by patch and attribute, compare them with the published target, and repeat failed cases after adjustments. If a licensed Absynth reference is available, run the matched blind comparison described in the design.
- [ ] 7.5 Build desktop folders and the static site from the tagged candidate, verify contents and run the install smoke on those exact artifacts. Publish supported OS/browser/device versions, pack/API versions and the measured capability results in the release record.

**Exit.** Every capability has evidence from its defining scenarios, unit coverage meets the threshold, all five end-to-end targets pass and the sound and performance acceptance records are complete. [Release verification](specs/release-verification/spec.md)

## OpenSpec workflow

Keep this change active while implementing its milestones. Map each behavior change to a requirement and scenario, update the relevant checkbox when its evidence is available, and run `openspec validate add-blue-instrument --strict` during review. After delivery, use `openspec archive add-blue-instrument` and validate the resulting canonical specs. Subsequent work such as a DAW plug-in gets its own change, capability deltas and tests. [OpenSpec CLI](https://github.com/Fission-AI/OpenSpec/blob/main/docs/cli.md)
