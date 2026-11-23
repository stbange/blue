## ADDED Requirements

### Requirement: HTTPS access to the browser instrument

Blue SHALL publish its web distribution to GitHub Pages over HTTPS, including its WebAssembly modules, loose content packs and browser entry point.

#### Scenario: Play the instrument at the project URL

- **WHEN** a player opens the Pages project URL over HTTPS and starts audio
- **THEN** the browser loads the WebAssembly modules and external preset files under the project path, and the selected sound is playable

### Requirement: Publish verified main builds

A successful web build from the current `main` commit SHALL publish after the browser, unit coverage and audio parity checks pass.

#### Scenario: A pull request completes its tests

- **WHEN** the browser workflow completes for a pull request
- **THEN** its build and test artifacts remain available in Actions and the public site stays at the last published main commit
