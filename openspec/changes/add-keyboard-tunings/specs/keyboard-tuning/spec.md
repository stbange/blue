## ADDED Requirements

### Requirement: Thirty-six consecutive scale degrees

Desktop and web SHALL map the 26 alphabet keys and 10 number keys to 36 ascending scale degrees with pitch labels. A dropdown SHALL offer C major, E natural minor, D Hijaz, C Rast, pentatonic and chromatic factory tunings.

#### Scenario: Select C major and play both ends of the keyboard

- **WHEN** the player selects C major and plays Z and 0
- **THEN** Blue plays C2 and C7 respectively, and every key between them uses a note in C major

### Requirement: Editable microtonal tuning

Desktop and web SHALL support a saved Custom tuning with a whole MIDI root and 4–24 increasing cents offsets starting at 0 and repeating every 1200 cents. All 36 keys SHALL fit the supported MIDI pitch range.

#### Scenario: Change a tuning while a key is held

- **WHEN** the player holds a note, saves a custom scale and releases the key
- **THEN** the held voice releases at its starting pitch, and the next press plays the custom interval

#### Scenario: Build a quarter-tone scale

- **WHEN** the player enters MIDI root 36 and an interval of 350 cents as the third degree of the scale
- **THEN** the third key plays 350 cents above C2

#### Scenario: Enter a scale outside the supported range

- **WHEN** the last computed key falls above MIDI 127
- **THEN** the builder reports the problem and preserves the active tuning
