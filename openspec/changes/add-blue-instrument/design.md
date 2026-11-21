# Blue technical design

## Context

A musician should be able to email a patch pack to someone with a different operating system and have that person hear the same instrument. That rules out putting synthesis in five host-specific audio graphs. It also makes the patch format, asset lookup and event clock part of the instrument's public contract. The design starts with the smallest complete path from a loose preset to an audible note. [Instrument concept](proposal.md) · [Delivery plan](tasks.md)

## Voice and sound design

Each sounding note owns three lanes. Within a lane, the musician chooses a source, arranges two insert slots, and sets level, pan and send amounts. Lane envelopes and filter histories are per voice. The two effects returns are shared by all voices so that a chord excites one acoustic space and effect tails can outlive note release.

```text
                         one voice per note
                 ┌─ A · source → insert → insert → level/pan ─┐
note + expression ├─ B · source → insert → insert → level/pan ─┼─ dry stereo bus
                 └─ C · source → insert → insert → level/pan ─┘
                        each lane also feeds two sends
all voices → dry stereo bus + delay return + reverb return → master → output
```

| Part | Proposed v1 behavior | What the player hears or edits |
| --- | --- | --- |
| Tonal sources | Band-limited basic waves, interpolated wavetables, a two-frame harmonic editor, two-operator FM and ring modulation | Draw two harmonic spectra and sweep between them, or give a bell a metallic attack without changing its fundamental |
| Recorded sources | Import mono or stereo PCM WAV, set root note and loop points, play it conventionally or as grains with independent position, size, density, pitch and seeded scatter | Sustain a short recording indefinitely, freeze its read position, or move into a new texture while a chord holds |
| Inserts and returns | Multimode state-variable filter, waveshaper and tuned comb/resonator in either insert slot. One send feeds a tempo delay with an optional pitch-shifted granular echo mode. The other feeds a diffused stereo reverb | Darken a layer, excite a pitched resonance, or let fragments of a short note bloom into a pitched tail |
| Motion | Up to 128 breakpoints per envelope with curve, sustain, loop, release and tempo sync. Three LFOs, eight macros, velocity, pressure, pitch bend and mod wheel feed a modulation matrix | Make a 12-second color change that loops, then takes a separate release path. An editor shows each source, destination, depth and unit |
| Performance | Polyphonic note IDs, sustain pedal, deterministic voice stealing, per-lane solo and mute | A player can isolate a moving part, change the sound during a chord, and release every note reliably |

The Absynth 5 manual documents up to 68 breakpoints per envelope. Blue proposes 128 editable points. A save, reload and render of the full envelope makes the capacity claim testable. [Absynth 5 envelope reference](https://docs.native-instruments.com/pdf-guides/Absynth_Manual_English.pdf)

| Original listening patch | Musical brief | Proposed acceptance check |
| --- | --- | --- |
| Glass Tide | Keep the pitch grounded while a 12-second filter envelope and grains bring a bowed-glass pad forward | With the fixed seed and sustained A4, the final two-second window has at least 25% higher spectral centroid than the first two seconds after attack |
| Struck Wire | Put a bright FM strike in front of a ringing body | In the isolated on-pitch fixture, the upper-sideband-to-carrier ratio drops at least 6 dB between the attack and the one-second mark |
| Paper Cloud | Hold a short paper recording in place while its texture thickens | Freeze preserves read position. With fixed scatter and free grain capacity, doubling density doubles the start count within one grain per second |
| Step Current | Keep a rhythmic envelope in time across a held note and its release | At 120 BPM, successive quarter-note steps lie 24,000 samples apart at 48 kHz and key-up starts the release at its scheduled frame |

## Engine, scripts and hosts

| Owner | Responsibilities | Boundary |
| --- | --- | --- |
| C17 engine | Voice allocation, oscillators, grains, filters, envelopes, modulation, effects, metering and offline rendering | Opaque engine instance, a prepared patch, timestamped events and caller-owned planar float output. The same sources compile on every target |
| C control library and embedded PUC-Lua | Resolve packs, validate documents, run scripts, build graphs, prepare samples and map editor gestures | A versioned Lua API produces validated descriptors and commands. Lua owns composition and interaction. C owns sample-rate arithmetic |
| Native C hosts | Device and MIDI I/O, window, input, graphics, file picker and recording | Use miniaudio's callback and SDL3's window/input services. Use WinMM, CoreMIDI and ALSA Sequencer for MIDI input. Convert host input to engine events |
| Web and WebXR | A user gesture starts audio. The browser obtains files, shows the editor and manages XR sessions | Emscripten builds the C engine for an AudioWorklet and the C/Lua control library for a worker. A thin JavaScript layer binds browser APIs. Flat and immersive views share the worker, engine and parameter IDs |

Suggested source layout is `include/blue/` for the C API, `src/engine/` for DSP, `src/control/` for packs and Lua, `hosts/{desktop,web,xr,render}/` for entry points, `content/` for loose packs and `tests/{unit,e2e,fixtures}/` for verification. CMake presets build native and Emscripten targets. Pin compilers, Lua, host libraries and test tools by version. Start with Lua 5.4 for its documented embedding API and test the same interpreter on each host. [Lua reference](https://www.lua.org/manual/5.4/) · [miniaudio](https://miniaud.io/) · [SDL3](https://wiki.libsdl.org/SDL3/FrontPage)

A host creates an engine instance, prepares it for a rate and block size, publishes a prepared patch, renders frames and finally destroys the instance. The render call receives left and right output arrays and an ordered event array, each with an explicit count. Preparation sizes all working memory. A parameter change takes a stable numeric parameter ID and a value in its declared units. The offline test host calls the same render function as a device callback.

### Rendering one block

1. The host passes the sample rate and maximum block size at preparation. C allocates voice, grain, delay and event pools and builds tables. The renderer accepts the actual block length, including odd sizes, and splits longer callbacks into prepared chunks.
2. The control dispatcher timestamps note, transport and parameter events on a 64-bit engine sample clock. A bounded queue delivers them in order. C renders up to each event's offset and applies it there. Events that arrive late take effect at the next available frame and increment a counter.
3. The callback reads immutable tables and plans, updates voice-local state and fills caller-owned buffers. Allocation, locking, Lua, garbage collection, file access, graph validation and logging belong outside the callback. Continuous controls use smoothing. Event floods coalesce redundant macro moves. A reserved release path and an emergency all-notes-off flag protect held notes.
4. A structural edit prepares a new generation and commits it at a block boundary. Existing notes release on their original generation. New notes use the new graph. Keep at most two generations, defer subsequent structural changes and retire old tails after a documented bounded fade. Reclaim storage on the control side. A bad edit leaves the currently playable generation available.
5. Tempo, envelope state and seeded randomness advance on the sample clock. Each stochastic voice/lane has its own deterministic stream. The offline renderer consumes the same events and writes a float WAV, making timing and cross-host differences measurable.

The first source-quality tests cover high-note aliasing, wavetable interpolation and pitch across sample rates. Add band-limited wavetable levels and oversampling for the waveshaper as needed to meet the fixture bounds. Use a bounded grain pool and interpolation for fractional sample reads. Reducing polyphony through an explicit quality setting gives slower devices a predictable workload.

### Browser rendering and XR

Start with an Emscripten build that can run inside an AudioWorklet with private Wasm memory. Use a separate control worker for Lua, sample decoding and plan preparation. Transfer timestamped event batches and offset-based prepared data through a `MessagePort`. Reserve the audio heap before starting. Load the first bank before connecting output. Stage subsequent large transfers incrementally into an inactive, preallocated arena under a per-quantum copy budget, then publish its generation. Benchmark this path on the target headset in the first milestone. The baseline works on a secure origin without shared-memory isolation headers. [Emscripten audio worklets](https://emscripten.org/docs/api_reference/wasm_audio_worklets.html) · [Web Audio and Wasm design patterns](https://developer.chrome.com/blog/audio-worklet-design-pattern)

The flat view exposes the full editor. In immersive mode, each lane appears as a selectable sound source. The trigger plays and releases a note, a grip moves a source, and a second control edits its assigned macro. In spatial mode the C mixer maps listener-relative azimuth to equal-power stereo pan and distance to gain. This gives the player an audible cue when placing sounds. Stereo preset playback uses the saved pan law when spatial mode is off. XR pose events are timestamped and smoothed independently of the display frame rate.

Request `immersive-vr` from an explicit user action on HTTPS or localhost after checking support. A denied request keeps the flat instrument playable. When a controller disappears or a session ends, release notes owned by that input and return control to the flat editor with the same patch. A physical headset check covers tracking, audio continuity and controller ergonomics. [WebXR Device API](https://www.w3.org/TR/webxr/) · [WebXR session lifecycle](https://developer.mozilla.org/en-US/docs/Web/API/WebXR_Device_API/Startup_and_shutdown)

## Content on disk and on the Web

One distribution directory is the installation unit. The desktop build finds `content/` relative to its actual executable path, even when launched with a different working directory. Windows uses `Blue.exe`. Linux and macOS use a `Blue` executable. The initial macOS package is a portable directory with a Mach-O executable so editing nearby content also works there. The web distribution serves the same tree next to its Wasm and host files under the site's deployment subpath.

```text
Blue/
  Blue.exe or Blue                       native distribution
  index.html, audio.wasm, control.wasm    web distribution
  content/
    index.json                           available pack IDs and locations
    packs/
      blue.factory/
        pack.json                        version, API level, entry points, asset map
        presets/glass-tide.json
        scripts/performance.lua
        waves/tide.wav
        samples/glass-rim.wav
        graphics/panel.svg
```

- A preset stores stable module and parameter IDs, tuning, routes, envelopes, macros, random seed and pack dependencies. Asset references use a pack ID and logical asset ID. Paths resolve inside that pack. Copy the directory or deploy under a new URL prefix and the same references work.
- A pack declares a format version and a Lua API major. The loader validates lengths, types, resource budgets, route cycles and references before activation. A delayed feedback line is an explicit DSP module. An unresolved asset names its pack and path. A future format major reports its supported range. Older supported formats migrate in memory and write the new format to a new save.
- The native default is the adjacent `content/` tree. An explicit `--content-root` supports development or managed installations. User edits can live in a chosen writable directory. An explicitly selected user revision takes precedence for that pack. Duplicate IDs inside a revision are errors. Installation updates replace factory revisions and preserve user saves.
- The browser fetches the manifest and individual relative URLs, stores user revisions in IndexedDB, and imports or exports a pack as a ZIP preserving that tree. A reload revalidates edited bytes. Technical build outputs contain executable code. Presets, samples, wavetables, fonts, graphics, shaders and Lua source remain ordinary content files.
- Lua v1 exposes parameter declarations, graph composition, bounded event scheduling, macro mappings and a declarative editor panel. For example, an author can bind a single macro to grain density and filter cutoff and add it to the panel. Use a fresh VM with a memory quota, instruction budget, protected calls and bounded host bindings when validating revisions. A failed reload reports the file and error and keeps the previous instrument playable.
- A new sample-rate algorithm uses a C node descriptor with a stable type ID, version, parameter metadata and prepare/process/reset lifecycle. Compile that node into each target and run its DSP fixtures. A Lua pack can distribute any new combination of existing node types immediately. The pack format and versioned node IDs keep those two extension paths explicit.

## Verification and release evidence

The unit gate is **at least 95% of executable first-party lines**, measured using unit tests alone. Gate the C engine/control libraries, each shipping host's C adapter group, factory Lua, and hand-written Web/XR bindings independently, as well as the total. Enumerate shipping sources before running tests so that an untouched file contributes zero covered lines. Combine native platform results by canonical source path and line, counting each executable line once. Include conditional platform code from its owning toolchain. Exclude third-party dependencies, generated bindings, test harnesses and fixtures. Publish the numerator, denominator, exclusions and branch reports where the tool supports them. A missing expected report fails the gate.

Use CTest and a C unit harness, Clang source coverage with `llvm-profdata`/`llvm-cov`, Lua tests with LuaCov, and JS unit tests with V8 coverage mapped to authored sources. Exercise Wasm-only adapter logic with an instrumented target or equivalent target-specific unit runner, and require an exported report for those lines. Unit tests use analytic signals, synthetic samples, fixed seeds and fake clocks/devices. Run end-to-end suites in separate jobs. [Clang coverage](https://clang.llvm.org/docs/SourceBasedCodeCoverage.html) · [LuaCov](https://lunarmodules.github.io/luacov/)

| Automated run | Action through the shipped entry point | Evidence of success |
| --- | --- | --- |
| Windows, Linux, macOS | Unpack in a path containing spaces, launch from another directory, drive the visible editor with OS input, play a note, change and save a macro, record the master, quit and reopen | Analyze the captured WAV and compare the recalled parameter and changed timbre. Exercise a missing sample and recovery. Use interactive runners or a virtual display and a virtual audio endpoint |
| Web | Serve the distribution under a nested HTTPS path. Playwright clicks Start Audio, plays, edits, records, imports a pack and reloads the page | Measure output downstream of the real worklet, confirm the saved patch and diagnose a missing asset. Run Chromium, Firefox and WebKit on their supported hosted configurations |
| WebXR | Install IWER in the test browser before app startup. Enter an immersive session through the UI, trigger a note, drag a source, move the head pose and exit. Also deny the request and disconnect a controller | Record stereo energy moving to the expected channel, macro change, note release and a playable flat editor after exit. Run a real headset on release candidates |
| Cross-target audio | Feed the public offline renderer and Wasm engine the same patch, assets, score, rate and seed | Check exact scheduled frame offsets. For a clean calibration tone require pitch within one cent and peak sample difference ≤ 0.0001 full scale. For textured patches compare RMS within 0.5 dB and occupied spectral bands within 1 dB across targets |

The browser and native E2E jobs capture audio from the live instance driven by the test. The offline parity suite separately checks deterministic rendering. Keep captured WAVs, input traces, coverage files, console diagnostics and failing screenshots attached to the matching build. [Playwright browsers](https://playwright.dev/docs/browsers) · [IWER](https://github.com/meta-quest/immersive-web-emulation-runtime)

**Performance targets.** In milestone 1, name the desktop and headset models, OS, browser, device settings and benchmark patch in a versioned profile. Use a three-lane patch with one wavetable, one FM pair and one granular source capped at eight simultaneous grains per voice, one active filter per lane, plus the shared returns. Start with 16 voices on a reference desktop and eight on the reference headset. At 48 kHz and 128 frames the block lasts 2.67 ms. The proposed p99 DSP budget is 70% of that period (1.87 ms), measured over ten minutes of sustained load with zero observed output gaps. Record device counters where exposed and check a continuous output capture for dropouts. Repeat at the actual device quantum and report the corresponding deadline. Measure real-time performance on an optimized build with coverage instrumentation disabled.

**Musical acceptance.** For each original patch, store a 20-second MIDI or timestamped performance, target behaviors, and a loudness-matched listening render. Three listeners rate movement, clarity and control response on a five-point scale. Each patch targets a median of at least four for each attribute, plus its objective checks. If a licensed reference instrument is available, program comparable original patches, play the same gestures, match loudness within 0.5 LU, randomize presentation and record preference by attribute. This supplies evidence for any later claim about sound quality. The specified sonic breadth draws on the documented Absynth oscillator and envelope designs. [Absynth oscillator and editor overview](https://docs.native-instruments.com/online-guides/absynth-6-user-guide/en/welcome-to-absynth-6) · [Absynth 5 envelope reference](https://docs.native-instruments.com/pdf-guides/Absynth_Manual_English.pdf)

## Release boundaries and risks

| Decision or risk | How the plan resolves it |
| --- | --- |
| An expensive granular patch or reload misses the audio deadline | Benchmark the headset early, reserve two patch arenas, bound copy/grain work, and run the heaviest factory preset under sustained load before release |
| A Lua extension is slow or invalid | Enforce VM and binding budgets, abort the candidate revision, surface the failing file and keep rendering the last accepted patch |
| A render differs across compilers or browsers | Version the random generator, neutralize denormal sensitivity, fix rate/seed/assets and use signal-specific tolerances with a saved difference report |
| A library or preset becomes hard to move | Install and relocate the actual distribution in CI. Preserve format fixtures, stable IDs and migration tests for every released schema |
| A workstation plug-in or larger surround layout expands v1 | Keep the C host boundary and stable automation IDs suitable for a future CLAP adapter. VST3, Audio Units, discrete surround, MPE and additional XR modes can each receive a scoped OpenSpec change |
| A browser or headset handles immersive sessions differently | Capability-detect at runtime, exercise supported and denied sessions in CI, and record the supported device/browser combinations from physical release tests |

Version the engine API, Lua API and pack schema independently. Release native folders and a static web tree from the same revision with a matching factory pack. Retain prior compatible packs and migrate user saves by copy so a release can be rolled back. Implementation begins with milestone 1 after design review.
