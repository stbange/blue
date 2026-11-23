# Audio rendering

## Purpose

Provide a portable C audio engine whose timing, voice lifecycle and sound remain predictable in real-time hosts and offline exports.

## ADDED Requirements

### Requirement: Sample-clock rendering

The C engine SHALL consume timestamped note, expression, parameter and transport events at sample-frame precision. It SHALL render the same timeline across host block sizes at 44.1, 48 and 96 kHz and record late events.

#### Scenario: An event falls inside an uneven block

- **WHEN** a dry constant-signal test voice opens at frame 137 and closes at frame 279 using immediate envelopes
- **THEN** its nonzero frames are exactly 137 through 278 when rendered in blocks of 1, 63, 128, 511 or a mixture of these sizes

#### Scenario: Late input reaches the engine

- **WHEN** an event for frame 99 first becomes available before processing frame 120
- **THEN** the engine applies it at frame 120 and advances the late-event counter

### Requirement: Bounded real-time work

Once prepared, the audio callback SHALL render within fixed memory and queue budgets. It SHALL perform zero heap allocations, blocking waits, file or network operations, Lua calls or logging. Event overload SHALL preserve a path to release all notes.

#### Scenario: Control traffic floods the queue

- **WHEN** macro changes fill the normal event queue while notes are held and all keys are subsequently released
- **THEN** redundant controls can be coalesced, every note reaches release, overload is observable and callback instrumentation records zero forbidden operations

### Requirement: Voice and generation lifetime

The engine SHALL expose a bounded polyphony setting, deterministic voice stealing and sustain-pedal behavior. It SHALL activate a prepared structural edit at a block boundary, preserve held notes on their original generation and bound the number of live generations.

#### Scenario: A patch changes during a held chord

- **WHEN** a chord is held across activation of a revised patch and a new note is played
- **THEN** the chord uses its original voices, the new note uses the revision, and releasing the keys and pedal retires both sets of voices after their respective tails

#### Scenario: Reach a configured voice limit

- **WHEN** a fixed event sequence exceeds the configured polyphony twice with the same initial state
- **THEN** both runs steal the same voice and subsequent note-offs leave zero active voices after release

### Requirement: Reproducible reference renders

The offline and real-time hosts SHALL use the common C renderer. A preset, asset revision, rate, event sequence and seed SHALL reproduce a calibrated sound within the published cross-target tolerances.

#### Scenario: Compare native and Wasm outputs

- **WHEN** native and Wasm builds render the same dry calibration tone and seeded three-lane texture at 48 kHz
- **THEN** scheduled event frames match, the tone's peak sample difference is at most 0.0001 full scale, and the texture matches within 0.5 dB RMS and 1 dB per occupied band
- **AND** the comparison uses the same analysis windows and counts bands above −60 dBFS in the reference render
