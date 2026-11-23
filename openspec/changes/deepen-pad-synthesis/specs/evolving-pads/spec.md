## ADDED Requirements

### Requirement: Independent polyphonic contours

Blue SHALL support independently timed amplitude and timbre contours for each lane, including a loop during a hold and a release starting continuously from the current value.

#### Scenario: Release a chord during a slow bloom
- **WHEN** the player releases a chord midway through one lane's 18-second bloom while another lane sustains
- **THEN** each voice enters the release defined for each lane from its instantaneous value, and the voice retires when all audible lanes complete their releases

### Requirement: Playable granular sources and tails

Blue SHALL support distinct per-lane stereo source clips, controllable grain position and pitch dispersion, and a damped stereo granular feedback return.

#### Scenario: Change the texture while retaining the chord
- **WHEN** a player holds a chord and sweeps a macro assigned to grain position
- **THEN** the read position traverses the assigned clip while the underlying pitched lane retains its tuning and the other lane retains its assigned clip

#### Scenario: Hear the pitched return
- **WHEN** an excited return disperses grains across the stereo field and the player releases the keys
- **THEN** the return retains an audible pitched tail, remains finite under the supported feedback settings, and decays after input stops

### Requirement: Spectral movement under player control

Blue SHALL allow a lane to interpolate through at least three authored spectral frames while sustaining the played note.

#### Scenario: Move from a hollow reed through a vocal resonance into glass
- **WHEN** the player sweeps a frame-position control from its minimum to maximum on a sustained note
- **THEN** the partial amplitudes reach the authored intermediate frame and endpoint frames in sequence, with continuous interpolation between them and stable fundamental tuning

### Requirement: Comparable listening evidence

Each proposed flagship pad SHALL have a reproducible held-chord score and a loudness-matched listening comparison with a reference patch.

#### Scenario: Audition internal motion
- **WHEN** both patches receive the same chord with fixed velocities and no subsequent controller events
- **THEN** the listening package supplies full stereo renders including the release, matched within 0.5 LU, and identifies the input score and patch revisions
