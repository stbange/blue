# Release verification

## Purpose

Give maintainers reproducible evidence of code coverage, audible end-to-end operation, real-time capacity and the musical behavior of each release.

## ADDED Requirements

### Requirement: Unit-test coverage gate

Unit tests alone SHALL cover at least 95% of first-party executable lines in total and in each of the C engine/control, per-host C adapter, factory Lua and handwritten Web/XR binding groups. Reports SHALL count untouched shipping files, identify exclusions and fail the gate for missing expected data.

#### Scenario: A weak adapter would be hidden in the total

- **WHEN** unit coverage reaches 97% overall but one host group covers 949 of 1,000 executable lines
- **THEN** the check fails and identifies that group even if end-to-end tests execute the other 51 lines

#### Scenario: A shipping script is never loaded by unit tests

- **WHEN** a new executable Lua script is in the factory pack but its unit suite never loads it
- **THEN** its executable lines count in the denominator with zero covered lines and its file appears in the report

### Requirement: Automated end-to-end evidence

Automated tests SHALL exercise packaged Windows, Linux and macOS applications, the browser distribution and an emulated WebXR session through their normal user entry points. Evidence SHALL include audio captured from the live instance, saved state and failure diagnostics.

#### Scenario: A control animates while the synth is silent

- **WHEN** an automated player loads the calibration patch, presses a note through the UI and observes a successful UI update but silence in the captured master
- **THEN** the corresponding platform's end-to-end job fails and retains the trace and audio capture for diagnosis

#### Scenario: Validate a release candidate

- **WHEN** all native, browser and emulated XR journeys complete on a release candidate
- **THEN** CI records per-target results, including XR entry, gesture, audible spatial change, exit and fallback
- **AND** the release record also identifies the physical headset, browser, controller and outcome of the on-device acceptance run

### Requirement: Real-time performance evidence

A release candidate SHALL publish results on named reference desktop and headset profiles. With the specified benchmark patch at 48 kHz and the chosen voice counts, the p99 DSP time SHALL stay at or below 70% of the audio block period and a ten-minute output capture SHALL contain zero observed dropouts.

#### Scenario: Validate the 128-frame profile

- **WHEN** the optimized build plays the three-lane benchmark for ten minutes at 48 kHz and 128 frames with 16 desktop voices or eight headset voices on the respective reference profiles
- **THEN** the unrounded p99 DSP time is at most 70% of the block period (about 1.87 ms), and the duration, counters, capture continuity, device details and voice settings are recorded

### Requirement: Musical reference set

Blue SHALL ship four original reference patches covering an evolving pad, an FM strike, a frozen granular texture and a tempo pulse. Each SHALL have a repeatable input performance, an audio metric and a recorded listening assessment.

#### Scenario: Qualify the factory sounds

- **WHEN** the four patches are assessed with the versioned performance traces and three listeners score movement, clarity and control response on a five-point scale
- **THEN** each patch passes its documented audio metric and reaches a median of at least four for each listening attribute
