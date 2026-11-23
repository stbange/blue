## 1. Current-engine sound study

- [x] 1.1 Author an external pad and editable performance mapping. Export an importable v1 ZIP.
- [x] 1.2 Render a fixed held chord through Glass Tide and Tideline, match listening levels, and verify the archive round-trip and native/Wasm render metrics.

## 2. Independent motion

- [ ] 2.1 Implement per-voice curve evaluators, loop and release handling, Lua bindings and an overlaid curve editor. Test continuous release and old-patch recall.
- [ ] 2.2 Author the dry staggered-bloom example, then measure block time under desktop and headset voice loads.

## 3. Spectral and granular materials

- [ ] 3.1 Implement bounded per-lane stereo resources, independent grain controls, and multi-frame spectral interpolation. Verify reload lifetime and sample position independently of pitch.
- [ ] 3.2 Author and audition the friction-to-glass gesture in native and Wasm renders.

## 4. Spatial feedback and musical qualification

- [ ] 4.1 Implement the stereo granular return and test decay, pitch, stereo spread, feedback bounds, mono compatibility and CPU cost.
- [ ] 4.2 Deliver three contrasting pads and run level-matched listening sessions for internal movement, chord clarity and performance response. Preserve the 95% unit-coverage gate and packaged E2E for all hosts.
