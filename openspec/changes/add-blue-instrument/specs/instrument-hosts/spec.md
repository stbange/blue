# Instrument hosts

## Purpose

Make the same instrument playable, editable and portable on Windows, Linux, macOS and the browser, including in the absence of a MIDI device.

## ADDED Requirements

### Requirement: Native standalone playback

Windows, Linux and macOS SHALL each provide a standalone application that can select an audio device, play through the computer keyboard and MIDI, and record its live stereo master to WAV. Device changes SHALL recover to a playable state.

#### Scenario: Play and record on a clean desktop

- **WHEN** the player opens a relocated distribution, chooses an audio device, starts recording, plays A4 on the computer keyboard and releases it
- **THEN** the captured stereo WAV contains the expected attack, a pitched sustain near 440 Hz and the release

#### Scenario: Reopen an audio device

- **WHEN** a player switches output to an available device with a different negotiated rate
- **THEN** the host re-prepares the engine and the next A4 plays at the correct pitch using the new rate

### Requirement: Browser playback

The web host SHALL render through Web Audio using the Emscripten-built C engine on a secure origin. It SHALL start audio from an intentional gesture, offer keyboard or pointer input, and remain playable when MIDI permission or shared memory is unavailable.

#### Scenario: Start with browser permissions restricted

- **WHEN** a user opens the instrument under a subpath without cross-origin isolation or MIDI permission, clicks Start Audio and plays on the on-screen keyboard
- **THEN** the worklet output contains the requested note and release, and the editor stays interactive

### Requirement: Edit and recall a sound

The native and flat web hosts SHALL expose preset browsing, lane selection, wave and envelope editing, routing, macro control, save and recall. They SHALL persist user content in writable storage and support moving user patches and their dependencies between hosts.

#### Scenario: Move an edited patch from desktop to browser

- **WHEN** the user changes a harmonic frame, adds an envelope point, maps a macro, saves a pack on desktop, then imports it in the browser and reopens the app
- **THEN** the saved values and references are recovered, and its performance matches the exported reference within the audio-render tolerances

#### Scenario: Recover a missing factory asset

- **WHEN** an initial pack cannot load because a declared sample is absent and the user restores the file and chooses Reload Pack
- **THEN** the editor identifies the missing pack and sample, then makes the corrected preset playable in the same session
