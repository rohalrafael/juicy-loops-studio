# AGENTS.md: guide for AI agents and contributors

Juicy Loops Studio is a free, open-source (AGPLv3), cross-platform DAW whose workflow is modeled
on FL Studio, so FL users can switch without relearning: Channel Rack with step sequencer, Piano
Roll, pattern-based Playlist, Mixer, Browser, FL's default shortcuts. It's written in **C++20 with
JUCE**, and **Linux is a first-class target** (ALSA, JACK, PipeWire; no Wine), alongside Windows
and macOS. Read this file first, then `docs/`.

## 1. Orientation

| Read | Why |
|---|---|
| `docs/architecture.md` | Layers, threads, how an edit reaches the audio thread, the engine, "How to..." recipes |
| `docs/development.md` | Build, test, run, the sandbox, testing the UI, CI |
| `docs/sandbox.md` | The Docker development sandbox (all tools and test plugins, no local installs) |
| `docs/user-guide.md` | What users can do: every window, mouse action and shortcut |
| `docs/project-format.md`, `docs/themes.md` | The `.jlp` project format; the theme format |
| `docs/roadmap.md` | Milestones, the backlog of agreed features, known gaps |
| `docs/decisions.md` | Why things are the way they are (read before changing a behaviour) |

## 2. Source map

```text
src/engine/   real-time core, plain C++ (no JUCE): Engine, Transport, sequencer, Sampler,
              effects, instruments, previews, time-stretching (Signalsmith)
src/model/    project data, plain C++: Project, Document (undo), .jlp files, export rendering
src/theme/    theme data and JSON theme files, plain C++
src/ui/       JUCE components: Channel Rack, Playlist, Piano Roll, Mixer, Browser, Sampler, dialogs
src/app/      the app shell: MainComponent, commands (shortcuts), live input, stretch cache
src/io/       JUCE file I/O: sample loading, WAV export
src/plugins/  VST3 / LV2 hosting
tests/        Catch2 tests (engine, model, theme only: never JUCE)
```

`jls_engine`, `jls_model` and `jls_theme` must stay free of JUCE so they build and test without a
GUI. Includes are rooted at `src/` (`#include "engine/transport.h"`); namespaces follow the
folders (`jls::engine`, `jls::model`, ...).

## 3. Golden rules

1. **Real-time safety.** The audio thread never blocks, locks, allocates, frees, logs or does
   file I/O. The UI never calls into it directly: it edits the model, and the engine receives an
   immutable snapshot through a lock-free queue (`Engine::setState`); anything else crosses
   threads through lock-free queues or atomics. Anything slow (decoding, time-stretching,
   plugin loading) happens on another thread first.
2. **Clean-room, and not FL's trade dress.** We reproduce FL Studio's *workflow and interaction
   patterns*, never Image-Line's code, assets, icons, skins, sample content or plugin names.
   "FL" / "FL Studio" never appears in the product name, logo or UI (mentioning FL descriptively
   in docs is fine). A future `.flp` import may only be built from public or clean-room
   documentation. Proprietary algorithms aren't an option either (e.g. élastique).
3. **Everything is undoable and saved.** User edits go through `model::Document::apply()`
   (continuous gestures share one gesture id); view-only state uses `setViewState()`. New
   project data is saved and loaded in `model/project_file.cpp`, with defaults so older files
   still open, and documented in `docs/project-format.md`.
4. **Every action is a command.** Anything a user can trigger gets a command in
   `src/app/commands` (stable id, name, category, default keys: FL's where FL has one), handled
   in `MainComponent::perform()`. Ids are saved in users' shortcut files: never renumber or
   reuse one. Controls that run a command show its current keys in their tooltip.
5. **Themes are colours; the look is fixed.** Colours come from the active theme
   (`ui::currentTheme()`), read in `paint()` since themes switch at runtime. Shapes, sizes and
   the font are the same for every theme: `ui::metrics` and IBM Plex Sans, never theme data. A
   new colour goes into `theme::Colours`, its parse/serialize table, both built-in themes and
   `docs/themes.md`.
6. **Tests are the gate.** Engine and model changes come with Catch2 tests; timing is tested to
   the sample. UI changes are checked visually on Xvfb (see `docs/development.md`); sound by
   exporting and analysing, or listening.
7. **Documentation is part of every change.** Every header and test file starts with a comment on
   its role (a `.cpp` too when it has more to say, like an algorithm overview); classes and
   non-obvious functions are documented where declared. Update
   `docs/architecture.md`, `docs/user-guide.md` and the format docs in the same commit, and add a
   `docs/decisions.md` entry for decisions worth explaining.
8. **Dependencies:** fetched by CMake (`FetchContent`) at a pinned version, licence compatible
   with AGPLv3. Bundled assets (fonts) keep their licence file next to them.

## 4. Style

- C++20, formatted with `.clang-format` (Allman braces, 4 spaces, 100 columns).
- The engine builds with `-Wall -Wextra -Wpedantic -Werror` (`/W4 /WX` on MSVC); keep the app
  free of warnings too.
- RAII and value types; no raw `new` / `delete` (JUCE's owned-pointer APIs excepted).
- `PascalCase` types, `camelCase` functions and variables, `snake_case` files.
- Comments explain *why*; plain, short sentences.

## 5. Before you finish

```sh
cmake --build build && ctest --test-dir build --output-on-failure
clang-format -i <files you touched>
```

- UI change: try it on Xvfb and look at the screenshots.
- Docs and `docs/decisions.md` updated; `docs/roadmap.md` if a milestone or backlog item moved.
- Commits: a short summary line, then what changed and why. Keep the tree building at every
  commit.

## 6. Picking work

Check `docs/roadmap.md`: the backlog lists agreed features, and "Known gaps" lists smaller
shortcomings (keep it current: remove what you fix, add what you leave out). Discuss anything
bigger in an issue first.
