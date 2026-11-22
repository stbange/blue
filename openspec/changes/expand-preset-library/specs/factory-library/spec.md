## ADDED Requirements

### Requirement: Selectable factory banks

Blue SHALL install four additional banks with six individually named presets each. Each bank SHALL include an editable Lua mapping and all referenced assets as loose files.

#### Scenario: Choose a bank after installation
- **WHEN** the player opens Blue with the distributed content directory
- **THEN** all new presets are available through the existing catalog, alongside the original five entries

### Requirement: Playable and distinguishable sounds

Each new preset SHALL produce stereo audio from ordinary note events at its default macro values and expose named performance controls. Each bank SHALL contain distinct synthesis or articulation designs.

#### Scenario: Render the installed bank
- **WHEN** the native and Wasm renderers play the same held chord and release using a preset from their respective installed trees
- **THEN** both produce finite audio with headroom at the fixture velocities and their levels and occupied spectral bands meet the existing parity tolerances
