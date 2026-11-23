## Why

Players should be able to open Blue in a browser without building or serving the WebAssembly distribution themselves. An HTTPS deployment also gives Meta Quest Browser a secure origin for immersive mode.

## What Changes

Publish the verified Emscripten distribution from `main` to GitHub Pages. Keep preset manifests, Lua mappings and samples in the published content tree.

## Capabilities

### New Capabilities

- `web-publishing` covers HTTPS access and automatic publication from `main`.

## Impact

The existing GitHub Actions workflow packages the browser build for Pages and deploys it after the browser, unit coverage and audio parity checks pass. GitHub Pages uses GitHub Actions as the publishing source.
