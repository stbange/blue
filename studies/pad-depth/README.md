# Tideline

Play the same D minor ninth chord through two Blue patches. Five notes enter over 420 ms, hold until 0:52, then release together. There are no further note-ons, knob gestures or sequenced changes. Both previews use the Blue C renderer, identical input velocities and one constant gain adjustment per file. The decoded MP3s measure −23.01 LUFS for Glass Tide and −23.00 LUFS for Tideline.

| Listen | What to listen for |
| --- | --- |
| [Glass Tide](glass-tide.mp3) | The factory patch opens during the first 12 seconds and then holds its last envelope value. |
| [Tideline](tideline.mp3) | Follow the reed near the center. Compare the upper register around 0:06, 0:22 and 0:43, then listen through the release at 0:52. |

Import **[Tideline.zip](Tideline.zip)** with Blue's browser pack importer. On desktop, import the ZIP with the distributed pack tool, then launch Blue with the new content root.

```sh
blue-pack import Tideline.zip TidelineLibrary
Blue --content-root TidelineLibrary --preset Tideline
```

**Play it.** Hold D2, A2, F3, C4 and E4. Bring up **Glass presence** to hear the slightly detuned FM layer beating against the reed. Move **Grain activity** to thicken the particulate sound, and **Tide distance** to pull it back into the stereo returns. **Reed brightness** changes the spectrum of the centered layer. Pressure brings the glass forward.

**How the sound is made.** The two additive frames emphasize different groups of partials. A near-integer FM ratio introduces slow beating. The granular lane scans an original four-second source containing synthetic friction, pitched rubs and five small ringing contacts. Opposing level and pan routes give the glass and grains different places in the mix. Three slow modulation periods are approximately 24.4, 37.0 and 12.0 seconds. All synthesis, spatial movement and tails in the previews come from the current Blue engine.

The loose [preset](pack/presets/tideline.json), [Lua mapping](pack/scripts/tideline.lua), [source audio](pack/samples/friction.wav) and [input score](held-chord.score) are editable. To reproduce the comparison from the repository root:

```sh
build/bin/blue-render content/packs/blue.factory "Glass Tide" baseline.wav studies/pad-depth/held-chord.score
build/bin/blue-render studies/pad-depth/pack Tideline study.wav studies/pad-depth/held-chord.score
```

The automated check imports the distributable ZIP, compares its native render byte-for-byte with the loose pack, measures native/Wasm level and spectrum, and checks that upper-register movement continues during the held chord. Run it with Python, NumPy, Node and the two Blue builds.

```sh
build/bin/blue_lua_test_runner studies/pad-depth/check.lua studies/pad-depth/pack/scripts/tideline.lua
python studies/pad-depth/check.py build/bin build-web/bin
```
