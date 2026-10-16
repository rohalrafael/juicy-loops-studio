# Juicy Loops Studio

A free, open-source, cross-platform DAW (digital audio workstation) whose workflow is
modeled on FL Studio, so FL users can switch over without relearning everything.
Linux is a first-class target, not an afterthought.

> This file is a living document. Update it whenever a decision is made, a convention
> is set, or the build/run steps change. Record *why* alongside *what*.

## Goals

- **Familiar workflow for FL users**: the main ideas carry over (Channel Rack + step
  sequencer, Piano Roll, pattern-based Playlist, Mixer with insert/send tracks,
  Browser, and the default keyboard shortcuts).
- **Cross-platform**: Linux, Windows, macOS from one codebase.
- **Linux-native**: ALSA, JACK and PipeWire support; no Wine.
- **Real-time safe audio**: the audio thread never blocks or allocates.
- **Plugin hosting**: VST3, CLAP, LV2 (and AU on macOS).

## Non-goals / legal boundaries

- Do not copy Image-Line code, assets, icons, skins, sample content or plugin names.
  We reproduce *workflow and interaction patterns*, not trade dress.
- Do not use "FL" or "FL Studio" in the product name, logo or UI. Mention FL only
  descriptively (e.g. "familiar to FL Studio users").
- Any `.flp` import is a later, optional feature, built only from public or
  clean-room documentation of the format.

## Tech stack

| Area            | Choice                                    | Status   |
|-----------------|-------------------------------------------|----------|
| Language        | C++20                                     | Decided  |
| Build system    | CMake (≥ 3.24), Ninja, FetchContent deps  | Decided  |
| Framework       | JUCE 9.0.4 (app/UI/audio I/O only)        | Decided  |
| Tests           | Catch2 v3.16.1 + CTest                    | Decided  |
| Engine          | Custom, or Tracktion Engine (to evaluate) | Open     |
| Plugin formats  | VST3, CLAP, LV2, AU                       | Proposed |
| License         | AGPLv3 (required by JUCE 9's open-source license) | Decided |

Language rationale: C++ has the most mature ecosystem for DAWs (JUCE, plugin SDKs,
existing DAWs to learn from). Rust was considered; its audio libraries are good, but
plugin *hosting* and GUI support are still immature.

## Architecture

```
src/app/      JUCE application shell, windows, (for now) a transport bar   [exists]
src/engine/   Real-time engine: transport, later graph/sequencing/mixing   [exists]
src/ui/       Channel Rack, Piano Roll, Playlist, Mixer, Browser           [planned]
src/model/    Project data: patterns, channels, clips, tracks, automation  [planned]
src/plugins/  Plugin hosting + built-in instruments/effects                [planned]
src/io/       Project save/load, audio import/export, rendering            [planned]
tests/        Catch2 unit tests (link against engine/model, never JUCE GUI)
```

Targets: `jls_engine` (static lib, plain C++, **no JUCE dependency**),
`JuicyLoopsStudio` (GUI app), `jls_tests`. Includes are rooted at `src/`,
e.g. `#include "engine/transport.h"`. Namespaces: `jls`, `jls::engine`, ...

Key rules:
- **UI and engine are separate.** The UI edits the model; the engine reads a
  real-time-safe snapshot of it. No direct UI → audio thread calls.
- **No locks, allocation, file I/O or logging on the audio thread.** Use lock-free
  queues/FIFOs to communicate with it.
- The model is serializable and has undo/redo built in from the start.

## Build & run

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug   # first run fetches JUCE + Catch2
cmake --build build
ctest --test-dir build --output-on-failure
"./build/src/app/JuicyLoopsStudio_artefacts/Debug/Juicy Loops Studio"
```

Options: `-DJLS_BUILD_APP=OFF` (engine + tests only, skips JUCE), `-DJLS_BUILD_TESTS=OFF`.
CI (`.github/workflows/ci.yml`) builds and tests on Ubuntu, Windows and macOS.

## Conventions

- C++20, format with `.clang-format` (Allman braces, 4 spaces, 100 cols).
- Engine code builds with `-Wall -Wextra -Wpedantic -Werror` (`/W4 /WX` on MSVC).
- Prefer RAII and value types; no raw `new`/`delete`.
- Names: `PascalCase` types, `camelCase` functions/variables, `snake_case` files.

## Roadmap

1. [x] Repo skeleton: CMake, JUCE, Catch2, CI on all three OSes
2. [ ] Audio device setup + transport (play/pause/stop, tempo). *Partly done:
       transport class, device settings dialog, Space = play/stop.*
3. [ ] Channel Rack with step sequencer + a built-in sampler
4. [ ] Pattern system + Playlist
5. [ ] Piano Roll
6. [ ] Mixer (inserts, sends, built-in effects)
7. [ ] Plugin hosting
8. [ ] Project save/load, audio export

## Decision log

- 2026-10-16: Language set to **C++** (over Rust) for its plugin-hosting and DAW
  ecosystem.
- 2026-10-16: **JUCE 9 + CMake**, deps via FetchContent. JUCE 9's open-source
  license is AGPLv3 (not GPLv3), so the project is **AGPLv3**.
- 2026-10-16: Engine is a separate JUCE-free library so it can be unit-tested
  without a GUI.
- 2026-10-16: Transport semantics follow FL: the Play button toggles play/pause;
  Space and the Stop button stop *and rewind* to the start.

## Dev environment notes

- The user is happy to install tools on request. Ask instead of working around a
  missing tool.
