# Hybrid synthesis

## Purpose

Let musicians combine pitched, recorded and granular sources in a single expressive instrument and hear how each layer contributes.

## ADDED Requirements

### Requirement: Independent sound layers

Each voice SHALL provide three independently playable and mutable lanes. Each lane SHALL support a selectable source, two ordered insert slots, level, pan and effect sends. Sources SHALL include basic subtractive waves, morphing wavetables, two-operator FM, ring modulation, pitched samples and granular samples.

#### Scenario: Isolate the FM component of a chord

- **WHEN** a musician loads a patch with a wavetable on A, an FM pair on B and grains on C, plays a chord and solos B
- **THEN** only the FM lane feeds the dry mix and new effect input
- **AND** clearing solo restores the three lanes with their saved settings

#### Scenario: Choose a ring modulator

- **WHEN** a dry lane multiplies sine waves at 440 Hz and 110 Hz with unity depth
- **THEN** the rendered steady-state spectrum contains peaks at 330 Hz and 550 Hz, with the 440 Hz carrier at least 40 dB below the stronger peak

### Requirement: Editable waves and recordings

The instrument SHALL support editing two harmonic frames, morphing between them, and playing a PCM WAV at a saved root note with optional loop points. Granular playback SHALL expose position, size, density, pitch and seeded scatter independently.

#### Scenario: Morph without transposing

- **WHEN** the player holds A4 and sweeps between harmonic frames with matching fundamentals and different upper partials
- **THEN** the measured fundamental stays within one cent of 440 Hz and the relative upper-partial levels change toward the edited frame

#### Scenario: Freeze a recording

- **WHEN** the performer freezes the read position in a sample and doubles grain density while scatter and pitch remain fixed
- **THEN** the source position holds and the number of grain starts per second doubles within one grain over the one-second measurement window, with pool capacity available

### Requirement: Articulation and modulation

The instrument SHALL provide curved breakpoint envelopes with up to 128 points, sustain, tempo sync, loops and release, at least three LFOs, eight assignable macros, velocity, pressure, pitch bend and modulation wheel input. Routes SHALL persist their source, destination and depth.

#### Scenario: Release a tempo-locked loop

- **WHEN** a player runs a quarter-note step envelope at 120 BPM, holds a note for three beats and releases during the loop
- **THEN** successive steps are 0.5 seconds apart and the envelope enters its defined release segment from the current value

#### Scenario: Recall a macro with two destinations

- **WHEN** the musician maps Motion to filter cutoff and grain density with different depths, saves and reopens the patch
- **THEN** the same knob changes both destinations over the saved ranges

### Requirement: Tone shaping and shared tails

The instrument SHALL offer low-pass, band-pass and high-pass filtering, waveshaping, a tuned comb or resonator, tempo delay with an optional pitch-shifted granular echo, and stereo reverb. Delay and reverb SHALL receive contributions from sounding voices and complete their tails after note release.

#### Scenario: Hear the decay after a short note

- **WHEN** a short excitation feeds a tuned resonator and a quarter-note delay at 120 BPM, with reverb disabled
- **THEN** the resonant tail contains the tuned frequency and the first delay return begins 0.5 seconds after the input transient

#### Scenario: Pitch the delayed grains

- **WHEN** a 440 Hz burst feeds the isolated granular echo at an octave up, with fixed grain timing and no feedback
- **THEN** the first wet return contains a spectral peak within one semitone of 880 Hz
