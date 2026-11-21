# Patch packs and Lua extensions

## Purpose

Allow musicians to copy, edit, share and extend an instrument using ordinary files, and keep saved performances usable as Blue evolves.

## ADDED Requirements

### Requirement: Relocatable external content

Blue SHALL load factory and user graphics, fonts, shaders, sounds, wavetables, presets and human-readable Lua source from files external to all executables, Wasm modules and generated host bundles. Its default content root SHALL be a sibling of the native executable or a relative web deployment path.

#### Scenario: Edit and move a portable installation

- **WHEN** a user edits a Lua mapping and replaces a named sample, moves the distribution, starts it from another working directory and reloads the pack
- **THEN** the changed mapping and sample are used and the executable checksum is identical to its original checksum

#### Scenario: Publish under another web path

- **WHEN** the web distribution and its content tree move together to a nested URL prefix
- **THEN** the browser loads the declared individual assets under that prefix and editing a served script followed by Reload Pack uses the new mapping with the same Wasm files

### Requirement: Versioned patch documents

A saved patch SHALL preserve stable module, parameter and asset IDs, pack dependencies, modulation routes, envelope points, macro state, tuning and seed. The loader SHALL validate versions and references before activating a revision, read supported earlier versions and preserve the original file on migration.

#### Scenario: Reload a compatible earlier revision

- **WHEN** a compatible older-format fixture is opened, migrated and saved to a new file
- **THEN** its parameter IDs keep their meanings, its render meets the fixture tolerances and the original bytes remain available

#### Scenario: Diagnose a broken pack

- **WHEN** a candidate refers to a missing sample, an escaping relative path, a duplicate asset ID or an unsupported format major
- **THEN** activation fails with the pack ID and offending path, ID or version and a previously loaded sound still plays

### Requirement: Scripted instrument extensions

Blue SHALL expose a versioned Lua authoring API for composing existing C nodes, defining parameters and editor controls, and scheduling bounded musical events. A valid extension SHALL run across all supported hosts after a content reload.

#### Scenario: Add an instrument from a folder

- **WHEN** a pack adds a Lua instrument that connects a wavetable to a filter and declares one macro and a panel control
- **THEN** the instrument appears in the browser of presets, the visible control alters the rendered cutoff, and desktop, Web and XR resolve the same parameter ID

### Requirement: Transactional reload

Blue SHALL validate resource limits, Lua API compatibility, script execution and graph references before publishing a pack. It SHALL preserve the previous working revision after a failed edit and accept a subsequent correction in the same session.

#### Scenario: Fix a faulty script while playing

- **WHEN** a candidate script exceeds its instruction or memory budget or has a syntax error during reload
- **THEN** the editor names the script and failure, the current patch keeps playing, and correcting the script allows a later reload to activate the new revision

### Requirement: Extensible DSP node types

The engine SHALL support registration of C node types using stable versioned IDs and declared parameters. A patch that depends on an unavailable node type SHALL identify that dependency before activation.

#### Scenario: Install a pack using a new C node

- **WHEN** a pack referencing a new node type is opened on a build that registers it and on an earlier build that lacks it
- **THEN** the new build renders the node, and the earlier build reports the missing type ID and keeps the active patch playable
