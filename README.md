# Blue

Blue is a playable hybrid synthesizer with a C17 audio engine and editable Lua instruments. Three lanes per note combine wavetable, subtractive, FM, ring-modulated, sampled and granular sources. A patch can use two inserts per lane, curved breakpoint envelopes, macros, delay, granular echo and stereo reverb. The desktop application, Web Audio instrument and WebXR performance surface use the same engine.

## Play

Unpack the distribution, keep the `content` directory beside the program and launch `Blue` or `Blue.exe`. Select a patch with the arrow keys. Play the chromatic octave with **A W S E D F T G Y H U J K** or connect a MIDI controller before starting.

| Desktop control | Action |
| --- | --- |
| Left / Right | Select a preset |
| F2 / F3 | Cycle macro pairs / audio outputs |
| F4 / F6 | Open the patch JSON / Lua mapping in the source editor |
| Ctrl+S or Cmd+S in the editor | Validate and save the edited loose file |
| Esc | Close the source editor, or quit from the keyboard |
| Z | Reload the pack from disk |
| R | Start recording, then save the stereo WAV on the next press |
| [ / ] | Decrease or increase the second displayed macro by one eighth |
| F8 | Open the graphical sound editor. Select a field with Up/Down, adjust it with Left/Right and save with Ctrl+S or Cmd+S |
| F7 | Export the active preset, its eight macro values, Lua mapping and sample dependencies to a new ZIP |
| F5 / F9 | Save or recall performance controls |

Use `--preset "Calibration"` to choose the startup sound and `--user-dir DIR` to choose an existing writable settings directory. Recordings default to `Blue-recording.wav` in the launch directory. Use `--record-to PATH` to choose another destination. Use `--content-root DIR` to open a different tree containing `index.json` and its packs.

## Build from source

Use a C17 compiler, CMake 3.24 or later and Python 3 for the unit suite. The first configure downloads hash-pinned Lua, cJSON, SDL, PortMidi and miniz sources. On Linux install the ALSA and X11 development headers. Use Ninja or a single-configuration Makefiles generator for the paths below.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The native programs are in `build/bin`. This command renders a 20-second performance without opening an audio device.

```sh
build/bin/blue-render content/packs/blue.factory "Glass Tide" tide.wav "content/performances/Glass Tide.score"
```

For Web and WebXR, activate Emscripten 4.0.20 and build the static site.

```sh
emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
python -m http.server 8000 --directory build-web/bin
```

Open `http://localhost:8000`, click **Start audio**, then select a patch or play the keyboard. The editor can save a revision in browser storage and import or export a ZIP containing the patch, its Lua mapping and sound dependencies. To move a browser ZIP back to desktop, run `blue-pack import sound.zip MyLibrary`, then `Blue --content-root MyLibrary`. Desktop F7 writes `Blue-pack.zip` in the launch directory. Use `--export-to PATH` to choose a new ZIP destination. The bundled `blue-pack export PACK_DIR PRESET NEW.zip` command also exports a preset without opening an audio device.

On Meta Quest 3, serve the same folder over HTTPS and open it in Meta Quest Browser. Start audio, then choose **Enter WebXR**. A trigger plays a note. Grip moves the nearest layer and changes the Motion macro. Turning your head changes the stereo position of each source.

## Extend an instrument

A content root lists packs in `index.json`. Each pack has a `pack.json` mapping stable preset and asset IDs to relative file paths. Factory samples are original, deterministic PCM textures. The desktop font atlas is based on DejaVu Sans Mono. [Font credits](content/graphics/FONT-LICENSE.txt)

Each lane accepts boolean `mute` and `solo` fields. Granular `scan` advances the source position in source seconds per real second, and `freeze: true` holds it. Root tuning controls sample and grain pitch. A preset declares its lanes, optional sample, insert order, harmonic frames, envelope, LFOs, sends and seed. Up to 128 envelope points are supported. A v0 preset with a `volume` field loads with that value as its gain. The factory mapping shows the Lua API in a working instrument. [Factory presets](content/packs/blue.factory/presets/) · [Editable mapping](content/packs/blue.factory/scripts/performance.lua)

```lua
blue.lane(2, "fm", 0.25)
blue.control(1, "Motion")
blue.mod("pressure", 2, "pitch", 7)
blue.route(1, "density", 2)
blue.at(0.5, 1, 0.75)
```

`blue.mod(source, lane, destination, depth)` adds one of up to 32 routes. Sources include `macro1`–`macro8`, `lfo1`–`lfo3`, the envelope, velocity, pressure and wheel. Lane destinations include level, pan, pitch in semitones, cutoff, morph, density, delay send and reverb space. Use the IDs `send` and `space` for the last two. Routes default to audio rate. A fifth argument of `"control"` samples the source every 16 frames and linearly interpolates between updates. Macro inputs are smoothed in both modes.

`blue.at` schedules a macro value in seconds after patch activation. Lua runs during preparation. A failed revision reports its source and leaves the current audio graph playable. A new sample-rate algorithm can register a versioned C node descriptor before starting audio. Configure native and web builds with `-DBLUE_EXTENSION_SOURCE=/absolute/path/nodes.c`. Define `blue_register_extensions(void)` there and call `blue_register_node` once for each static descriptor. The build links the extension into the authoring and rendering programs. [C node interface](include/blue_node.h)

## Verification

The unit suite exercises the engine, pack compiler, PCM import/export, factory Lua and Web C bindings. The following commands reproduce the unit coverage report with GCC and the cross-target audio comparisons with Python, NumPy and Node.js.

```sh
cmake -S . -B coverage -G Ninja -DBLUE_COVERAGE=ON
cmake --build coverage
python tests/coverage.py coverage
python tests/sonic.py build/bin/blue-render
python tests/parity.py build/bin/blue-render build-web/bin
```

The desktop E2E suite uses OS input and a virtual sound device. Browser and emulated XR suites use Playwright, an actual AudioWorklet and recorded stereo output. GitHub Actions builds and exercises the distributable programs. The active OpenSpec change tracks remaining release qualifications and the 95% first-party unit coverage gate. [Candidate measurements and open release gates](openspec/changes/add-blue-instrument/verification.md) · [CI workflow](.github/workflows/build.yml) · [Milestones](openspec/changes/add-blue-instrument/tasks.md) · [Requirements](openspec/changes/add-blue-instrument/specs/)
