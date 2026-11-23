# WebXR performance

## Purpose

Let a player perform an already loaded Blue patch using tracked controllers and hear the spatial response while retaining the same audio engine and patch state.

## ADDED Requirements

### Requirement: Immersive session lifecycle

On a supported secure origin, Blue SHALL let the player enter immersive VR with an explicit action and return to the flat editor with the same patch. Unsupported or denied sessions SHALL keep the flat instrument playable.

#### Scenario: Enter and leave a session

- **WHEN** a player edits a macro on the flat instrument, enters immersive VR, moves the macro again and ends the session
- **THEN** the same patch stays active and the flat editor shows the final value

#### Scenario: Immersive mode is unavailable

- **WHEN** the runtime is unavailable or denies the requested session
- **THEN** the player can still play and edit in the flat view and can see the reason immersive mode is unavailable

### Requirement: Tracked performance controls

The XR host SHALL let a controller trigger and release notes, change a mapped macro and position a selected source. It SHALL release notes owned by a disconnected input source or an ended session.

#### Scenario: A controller disappears during a note

- **WHEN** the player holds the trigger and the controller is disconnected before a trigger-up event
- **THEN** the note enters release and the remaining inputs continue to operate the instrument

### Requirement: Audible spatial placement

In spatial mode Blue SHALL use listener-relative source position to control stereo pan and distance gain. With spatial mode disabled, a saved patch SHALL retain its ordinary stereo mix.

#### Scenario: Move and listen to an isolated source

- **WHEN** a player moves an isolated sustained source from listener-left to listener-right at constant distance, with effects off
- **THEN** measured RMS is higher in the left channel at the first position and in the right channel at the second
- **AND** rotating the listener by 180 degrees reverses the lateral response
