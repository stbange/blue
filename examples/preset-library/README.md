# Play the expanded factory library

Blue has 24 new instruments in four banks. In the browser or on Quest, choose a bank from the pack menu, then select a preset. On desktop, the arrow keys step through all 30 catalog entries. To start on a particular sound, run `Blue --preset "Kelp Lanterns"`.

| Bank | Preview | Import into an existing Blue installation |
| --- | --- | --- |
| Formants | [Listen](formants.mp3) | [Download Formants](formants.zip) |
| Littoral | [Listen](littoral.mp3) | [Download Littoral](littoral.zip) |
| Nocturnes | [Listen](nocturnes.mp3) | [Download Nocturnes](nocturnes.zip) |
| Signals | [Listen](signals.mp3) | [Download Signals](signals.zip) |

Each preview plays all six presets in the order below. Every ten seconds, the same D minor seventh chord starts again on a different instrument. A higher A enters partway through the held chord. The keys lift at 6.25 seconds. The excerpts use the native renderer at default macro values, with a short fade at the end of each cut. On this score, the 24 source renders measure between −22.5 and −22.4 LUFS.

For existing web installations, use **Import pack** on each ZIP. On desktop, `blue-pack import littoral.zip LittoralLibrary` creates a new content root. Run `Blue --content-root LittoralLibrary --preset "Kelp Lanterns"` to play it. A fresh build includes all four banks and the earlier **Tideline** sound study. Select its own pack to play it. [Tideline listening comparison](https://github.com/stbange/blue/tree/4bf208063d2f4d0d1b210c594532c2ae2f731607/studies/pad-depth)

## Formants

Use **Vowel** for a slow spectral change, **Bellows** to bring forward the second register, **Ensemble width** to reposition the outer parts, and **Hall** for room and delay. Pressure brings out the third layer.

| Time | Instrument | Try playing |
| --- | --- | --- |
| 0:00 | Alder Choir | Hold an open ninth while a narrow vowel turns into a wider choir. |
| 0:10 | Felt Harmonium | Play legato triads. The sampled bellows and organ register breathe in a short cycle. |
| 0:20 | Amber Viola | Play a slow two-note line and lean into the bow. A gently detuned saw adds vibrato. |
| 0:30 | Hollow Consort | Space the notes a fifth apart and wait for the odd harmonics to surface. |
| 0:40 | Porcelain Alto | Play high and lightly. The bright FM onset gives way to short grains of air. |
| 0:50 | Candle Organ | Play close, quiet voicings. The sine register holds the center under the upper drawbars. |

## Littoral

**Grain activity** changes the density and level of the grain layer. **Horizon** sends it further into the room. **Undertow** brings up the pitched foundation, and **Reflections** feeds the delay. Slate, eelgrass, stones and buoy are original synthesized PCM textures, built from filtered noise, damped contacts and beating tones. The bellows recording in Formants is also synthesized.

| Time | Instrument | Try playing |
| --- | --- | --- |
| 0:00 | Rain on Slate | Hold one low note. Two grain streams pick out different droplets on either side. |
| 0:10 | Kelp Lanterns | Spread a low fifth beneath a high third and increase Reflections for an octave echo. |
| 0:20 | Pebble Choir | Play sparse chords and listen for individual stone contacts inside the grain cloud. |
| 0:30 | Saltgrass | Sustain a midrange cluster and raise Grain activity for a denser rustle. |
| 0:40 | Harbor Lights | Let a low chord settle. A brass tone anchors the buoy while the echo adds fifths. |
| 0:50 | Tidal Ink | Hold the bass through a full swell. Long grains scan backward under a low ring tone. |

## Nocturnes

**Bloom** brings the upper color forward. **Orbit** places the outer layers in stereo. **Unrest** detunes them in opposite directions. **Afterglow** sends them into the room and delay. Hold the chord steady while turning one control at a time.

| Time | Instrument | Try playing |
| --- | --- | --- |
| 0:00 | Gravity Well | Play low octaves. A clean fundamental sits under an inharmonic edge and a descending echo. |
| 0:10 | Velvet Eclipse | Hold a minor ninth for half a minute. The dark saw and hollow spectrum trade prominence. |
| 0:20 | Copper Aurora | Start with a fifth and let the brass swell before adding the third. |
| 0:30 | Slow Meteor | Strike a high note and leave it down. The metallic onset thins into sine and spectral ice. |
| 0:40 | Polar Veil | Play a high open voicing, then lift your hands to hear the octave echoes. |
| 0:50 | Night Ferry | Hold a low dyad across the repeated swell. The ring-modulated register sits to the left. |

## Signals

**Edge** adds upper harmonics or brings forward the second layer. **Repeat** feeds the delay, **Accent** balances the upper and lower layers, and **Spread** opens the stereo image. Four patches have looping envelopes in beats. The other two follow a struck decay.

| Time | Instrument | Try playing |
| --- | --- | --- |
| 0:00 | Lantern Morse | Hold one key for a two-beat, short–short–long signal at the default 88 BPM. |
| 0:10 | Prism Mallets | Play broken chords. The FM strike fades quickly and leaves an octave echo. |
| 0:20 | Switchyard | Hold an octave to hear two unequal accents in every beat at 116 BPM. |
| 0:30 | Quartz Steps | Sustain three notes for a repeating three-half-beat figure at 98 BPM. |
| 0:40 | Reed Telegraph | Alternate low and high notes. Each attack restarts a three-quarter-beat double pulse at 76 BPM. |
| 0:50 | Last Transmission | Leave space between notes for the falling fifth echoes to answer the metallic attack. |

## Edit and verify

Each bank has six JSON patches, an editable Lua performance mapping, and any required WAV files under `content/packs/blue.<bank>/`. To rebuild a downloadable bank after editing, run the following from the repository root.

```sh
python tools/bank_zip.py content/packs/blue.littoral examples/preset-library/littoral.zip
python tests/preset_library.py build/bin build-web/bin --save-renders renders
```

The test uses NumPy and Node. It imports each ZIP, compares the files against both installed trees, and plays the [audition score](audition.score) through native and Wasm. It checks stereo output, headroom, spectral agreement and accidental duplicate sounds. Hollow Consort, Tidal Ink and Polar Veil also undergo a 36-second held-chord check for continuing spectral movement and a decaying release.
