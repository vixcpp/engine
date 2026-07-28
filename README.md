# Vix Engine

Vix Engine is the reusable execution foundation behind Vix build-like
workflows. It owns data models and parsing logic that are not specific to the
command-line interface.

The CLI remains responsible for command parsing, terminal output, progress
rendering, colors, Cloud reporting, and command dispatch. The dependency
direction is one-way:

```text
modules/cli
  -> modules/engine
```

`modules/engine` must not depend on `modules/cli`.

## Current Scope

This first extraction phase provides:

- the `vix::engine` CMake target;
- the `<vix/engine.hpp>` umbrella header;
- minimal public execution boundary types;
- build node and build task models;
- `compile_commands.json` parsing;
- GCC/Clang dependency-file parsing.

High-level orchestration, CMake/Ninja execution, object and artifact caches,
scheduling, terminal rendering, and Cloud integration still live in
`modules/cli` and will move in later phases when their boundaries are clean.

## Standalone Build

From this directory:

```bash
cmake -S . -B build-ninja -G Ninja -DVIX_ENGINE_BUILD_TESTS=ON
cmake --build build-ninja
ctest --test-dir build-ninja --output-on-failure
```

When built inside the main Vix repository, the umbrella build provides
`vix::json` before adding `vix::engine`.
