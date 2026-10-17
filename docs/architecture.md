# Architecture

This document explains how Juicy Loops Studio is put together: the layers, the threads, how an
edit reaches the audio, and where to make common changes. The source files have header
comments with the details. This page covers how the pieces fit together.

## Goals that shape the design

- **FL Studio-like workflow.** Channel Rack, patterns, Playlist, Piano Roll, Mixer and the same
  shortcuts. The data model follows FL's (patterns hold notes per channel, clips place patterns).
- **Real-time safe audio.** The audio thread never waits for a lock, allocates or frees memory,
  or does I/O. Everything it needs is prepared on the UI thread and handed over without locks.
- **Testable core.** The engine, model and theme code are plain C++ with no JUCE, so they're
  unit-tested without a GUI or audio device.
- **Undo everywhere.** Every user edit is undoable, by construction.

## Layers

```
            ┌──────────────────────────────────────────────────────────┐
            │ src/app   JUCE application: main window, MainComponent   │
            │           (owns Document + Engine + EffectPool), menus,  │
            │           shortcuts, audio device callback               │
            └───────────────┬───────────────────────┬──────────────────┘
                            │                       │
            ┌───────────────▼──────────┐   ┌────────▼─────────────────┐
            │ src/ui    JUCE views:    │   │ src/io  file I/O, src/   │
            │ ChannelRack, Playlist,   │   │ plugins  VST3/LV2 hosting│
            │ PianoRoll, Mixer,        │   │ (JUCE)                   │
            │ EffectPanel, ThemeDialog,│   └──────────────────────────┘
            │ ThemeManager/LookAndFeel │
            └──┬────────────────────┬──┘
               │                    │
   ┌───────────▼───────────┐  ┌─────▼────────────────────┐
   │ src/model  (no JUCE)  │  │ src/theme  (no JUCE)     │
   │ Project, Document     │  │ Theme data, JSON theme   │
   │ (undo), toEngineState │  │ files, built-in themes   │
   └───────────┬───────────┘  └──────────────────────────┘
               │
   ┌───────────▼─────────────────────────────────────────────┐
   │ src/engine  (no JUCE): Engine, Transport, StepSequencer, │
   │ Sampler, Metronome, effects, EffectPool, DrumSynth,      │
   │ SpscQueue, EngineState snapshot types                    │
   └──────────────────────────────────────────────────────────┘
```

Dependencies only point downwards. Third-party code comes in through CMake FetchContent (JUCE,
Catch2, nlohmann/json). Fixes we need in it live in `patches/` and are applied automatically
at configure time ([patches/README.md](../patches/README.md)). `jls_engine`, `jls_model` and `jls_theme` are static
libraries without JUCE. The `JuicyLoopsStudio` app target compiles `src/app`, `src/ui` and
`src/io` together with JUCE. `jls_tests` links only the JUCE-free libraries.

## Threads

| Thread | Runs | May |
|---|---|---|
| **UI (message) thread** | All JUCE components, `Document` edits, `toEngineState()`, `EffectPool`, `Engine::setState()` / `collectGarbage()` | Allocate, block, do file I/O |
| **Audio thread** | `Engine::process()` (from the audio device callback) | Only read its current snapshot, write its own preallocated state and atomics |

They share exactly three things:
1. **Snapshots** (`EngineState`), passed through lock-free queues (below).
2. **Transport and metronome controls**: atomics (`play()`, `setTempo()`, ...).
3. **Meters**: atomic peak values (`Engine::takePeak()`).

## How an edit reaches the audio

```
 mouse/keyboard
      │
      ▼
 ui component ──► Document::apply(edit, gestureId) ──► Project (new value; old one on undo stack)
                                      │
                                      ▼ onChange
                       MainComponent::documentChanged()
                         ├─ views refresh() from the Project
                         └─ toEngineState(project, &effectPool) ──► Engine::setState(snapshot)
                                                                         │ (lock-free queue)
 audio thread:  Engine::process() ── adoptNewestState() ◄────────────────┘
                     │                       │
                     │                       └─ old snapshot ──► retired queue ──► deleted on the
                     ▼                                                     UI thread by collectGarbage()
                 sound
```

- A **snapshot** is a complete, immutable description of what to play: channels with
  samples and gains, patterns as note lists, clips, mixer tracks with effect parameters.
  It's rebuilt from scratch on every edit. That's cheap at this scale, and it means the
  audio thread never sees a half-applied change.
- **Samples** are `shared_ptr<const SampleBuffer>`, shared by the model, undo history and
  snapshots, so copying is cheap. Freeing a sample on the audio thread could stall it, so:
  1. The audio thread never drops the last reference to anything. Old snapshots are sent
     back and deleted on the UI thread.
  2. When the audio thread switches snapshots, the sampler first stops voices whose
     channel or sample is gone, so no voice keeps reading memory that's about to be freed.

## Model and undo

`model::Project` is a plain value:

| Part | Holds |
|---|---|
| `channels` | Name, sample, volume, pan, mute, mixer track |
| `patterns` | Notes per channel id: start, length, pitch, velocity, in ticks |
| `playlist.clips` | Which pattern sits on which track, from which step, for how long; a clip longer than its pattern repeats it |
| `mixer.tracks` | Master + 16 inserts: fader, pan, mute, solo, effect slots |
| view state | `currentPattern`, `selectedChannel`, `selectedMixerTrack`, `songMode` |

A Channel Rack **step** is just a note at C5 (MIDI 60), one step long, on a step boundary
(`Note::isStep()`). A channel whose notes are all steps shows editable steps; otherwise it
shows a note preview and is edited in the Piano Roll, as in FL.

`model::Document` owns the current `Project` plus undo/redo stacks of whole-project copies:
- **`apply(edit, gestureId)`** pushes the old value and applies the edit. Edits sharing a
  nonzero gesture id (one knob drag, one paint stroke) merge into a single undo step. Get
  an id from `newGestureId()` on mouse-down.
- **View state** changes (selection, PAT/SONG) go through `setViewState()`, which doesn't
  create an undo step. Undo/redo keep the current view state.

## Engine

### Time

| Unit | Size | Used for |
|---|---|---|
| beat | quarter note | tempo, transport position |
| step | 1/4 beat (16th) | Channel Rack, Playlist positions |
| tick | 1/96 beat (1/24 step) | note positions and lengths (`ticksPerBeat`) |
| sample | 1 / sample rate | rendering |

**Transport.** The position is computed from a whole-number sample count since the last
tempo or sample-rate change, so it doesn't drift; `advance()` returns the block's beat span
`[startBeat, endBeat)`. Each block then goes through three stages:
- **Sequencer** (`collectTriggers`): finds every note start in that span, with pattern
  looping, clip repeats and song looping, and converts each to an exact sample offset in the
  block.
- **Sampler:** renders the voices in segments between those offsets, so timing doesn't
  depend on buffer size.
- **Metronome:** uses the same beat span.

### Audio callback

`Engine::process()` splits the device block into chunks of at most the prepared size, then
for each chunk:

1. Clear the per-track stereo **buses** (Master + inserts), preallocated in `prepare()`.
2. Render sampler voices into the bus of their channel's mixer track.
3. For each insert: run its effect chain in place → apply fader and pan (mute/solo already
   folded into the gains) → record the peak → add into the Master bus.
4. Master: effects → fader/pan → peak → add to the device output.
5. Metronome clicks go straight to the output, bypassing the mixer.

### Effects

- **Description:** each built-in effect is an `EffectType` (id, name, parameter list, factory)
  with an `EffectProcessor` doing the DSP. Parameters carry their range, unit, scale
  (linear, log or stepped) and names for stepped values, so the UI builds controls generically.
- **State:** effects have state that must survive edits (reverb tails, delay lines). That
  state lives in `EffectProcessor` objects owned by the UI-side `EffectPool`, keyed by the
  effect slot's id, and every snapshot shares the same processor through `shared_ptr`.
- **Rules:** `prepare()` (may allocate) runs on the UI thread when the pool creates a
  processor, or in `Engine::prepare()` before audio starts. `process()` never allocates. The
  pool is reset when the audio device format changes.

### Plugins

Hosted plugins (VST3 and LV2, through JUCE) are effects like any other.
- **Instances:** a plugin slot has `type: "plugin"` plus the plugin's id, name and saved state.
  The `EffectPool`'s plugin factory, set by the app to `plugins::PluginHost::createEffect`, turns
  that into a `plugins::PluginEffect`, which wraps the JUCE plugin instance as an
  `EffectProcessor`. It preallocates its buffers; what the plugin itself does on the audio
  thread is up to the plugin.
- **State:** a plugin's settings live inside it. Before saving or exporting,
  `MainComponent::syncPluginStates()` copies each live plugin's state into the project (not
  as an undo step).
- **Failures:** creating a plugin that isn't installed or fails to load gives no processor. The
  slot is skipped (audio passes through), and the failure is remembered so it isn't retried on
  every edit.
- **Scanning:** `PluginHost` scans with JUCE's `PluginListComponent` (Options › Plugins), keeps
  the list in `plugins.xml` in the settings folder, and uses a "dead man's pedal" file so a
  plugin that crashes the scanner is skipped next time.
- **Not yet:** plugins run in the app's process, so a crashing plugin takes the app down.
  Plugin instruments are phase 2: they need note-offs from the sequencer.

## UI

`MainComponent` arranges:
- the menu bar (native on macOS) and the transport bar
- the main area, showing either the **Playlist** (F5) or the **Piano Roll** (F7)
- the **Mixer** docked below (F9)
- floating panels inside the window: the **Channel Rack** (F6) and the **effect panel**,
  both dragged by their title bars

Views are plain JUCE components that read the `Document` in `refresh()` and change it only
through `Document::apply()` / `setViewState()`.

**Keyboard focus.** Shortcuts are handled in `MainComponent::keyPressed`, and most widgets
don't take focus. Focus is steered back after menus (`menuBarActivated`), on clicks (a mouse
listener on all children) and by a timer while the window is active.

**Theming.** Colours, radii, border widths, glow and fonts all come from the active theme
(`ui::currentTheme()`). Components read them in `paint()`, so switching themes repaints live.
`ThemeManager`:
- builds the `LookAndFeel` from the active theme
- loads the bundled fonts
- remembers the choice in the settings file
- manages user theme files

Only layout sizes are fixed (`ui::metrics`). The theme file format is in
[themes.md](themes.md).

**Projects and export.**
- **Files:** `model::saveProject()` / `loadProject()` convert between `Project` and JSON
  ([project-format.md](project-format.md)). The app does the file I/O and decodes sample files
  (`io::SampleLoader`); the model resolves sample paths (next to the project first) and re-creates
  built-in sounds by name. Channels remember where their sample came from (`builtinSample` /
  `samplePath`) for this.
- **Unsaved changes:** `Document` tracks them with revision numbers, so undoing back to the
  saved state counts as clean.
- **Export:** `model::renderProject()` plays the project through a private `Engine` with its own
  `EffectPool`, block by block, stops at the end of the song, renders the tail, and
  `io::writeWav()` writes the result.

**Settings** live in `$XDG_CONFIG_HOME/JuicyLoopsStudio/` on Linux (usually `~/.config`), in
`~/Library/Application Support/JuicyLoopsStudio/` on macOS, and in `%APPDATA%\JuicyLoopsStudio\`
on Windows. User themes go in the `themes` folder next to the settings file. Projects are
saved wherever the user chooses (`.jls`).

## Testing

- **Unit tests** (`tests/`, Catch2, run with `ctest`) cover the JUCE-free code:
  - timing to the sample, including a long-run drift test
  - pattern and song sequencing, sampler pitch and velocity
  - snapshot hand-off and freeing, effect behaviour and stability, mixer routing
  - model operations, undo
  - theme parsing
- **UI tests:** `tools/ui-test.sh` runs the app on an invisible Xvfb display and drives it
  with xdotool, checking screenshots. It doesn't touch the desktop, so it also works under
  Wayland.
- **CI** (`.github/workflows/ci.yml`) builds and tests on Linux for every push. The
  Windows/macOS matrix is manual only, to save CI minutes.

## How to…

**Add a built-in effect**
1. Write an `EffectProcessor` subclass in `src/engine/effects.cpp`: allocate only in
   `prepare()`, process stereo in place.
2. Add an `EffectType` entry to `effectTypes()` with a new, permanent `id`, a display name and
   its `ParamInfo` list. The UI picks it up automatically.
3. Add tests to `tests/mixer_tests.cpp`. The "silent on silence / finite at extremes" test
   covers it automatically. Also add one test for its actual behaviour.

**Add a themeable property**
1. Add it to `theme::Theme` (`src/theme/theme.h`) with Juicy's value as the default.
2. Add it to the parse/serialize tables in `src/theme/theme.cpp`, and to the known-keys list.
3. Add it to all four built-in theme JSONs (`builtin_themes.cpp`). The tests check that each
   loads without warnings and that Juicy equals the defaults.
4. Document it in `docs/themes.md`, then read it via `ui::currentTheme()` in `paint()`.

**Add a project setting or object**
1. Add it to `model::Project` (or a struct inside it) as a plain value.
2. If the audio thread needs it, add it to the snapshot in `engine_state.h` and fill it in
   `toEngineState()`.
3. Change it only via `Document::apply()`, or `setViewState()` if it's view state. If it is
   view state, also carry it over in `keepViewState()` (`document.cpp`).
4. Save and load it in `src/model/project_file.cpp` (load with a default, so older files still
   open), extend the round-trip test in `tests/project_file_tests.cpp`, and document it in
   `docs/project-format.md`.

**Add a view or panel**
1. Create a component in `src/ui`, constructed with the `Document`, with a `refresh()` method.
2. Own it in `MainComponent`, call `refresh()` from `documentChanged()`, and lay it out in
   `resized()`.
3. Give it a shortcut in `keyPressed()` and an entry in the View menu.
4. Keep its widgets from taking keyboard focus (`setWantsKeyboardFocus(false)`,
   `setMouseClickGrabsKeyboardFocus(false)`).

**Add a keyboard shortcut**
1. Handle it in `MainComponent::keyPressed()`, using FL's key where FL has one.
2. List it in [user-guide.md](user-guide.md).
