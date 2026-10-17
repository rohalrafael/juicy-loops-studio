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
| **MIDI input thread(s)** | `LiveInput::handleIncomingMidiMessage()` | Only call `Engine::sendLive()` and read an atomic |

They share exactly four things:
1. **Snapshots** (`EngineState`), passed through lock-free queues (below).
2. **Transport and metronome controls**: atomics (`play()`, `setTempo()`, ...).
3. **Meters**: atomic peak values (`Engine::takePeak()`).
4. **Live events** (`Engine::sendLive()`): a lock-free queue to the audio thread. Its senders
   (UI and MIDI threads) take a mutex among themselves so the queue keeps a single producer;
   the audio thread never touches that mutex.

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
`[startBeat, endBeat)`. Each block then goes through these stages:
- **Sequencer** (`collectTriggers`): finds every note start in that span, with pattern
  looping, clip repeats and song looping, and converts each to an exact sample offset in the
  block. Each trigger also carries its `endTick`: the note's end, cut at the end of its clip
  or pattern repeat, in the same unwrapped tick timeline as the transport.
- **Sampler:** renders the voices in segments between those offsets, so timing doesn't
  depend on buffer size. Samples play to their end (note length is ignored, as in FL's
  default sampler).
- **Instruments:** triggers on plugin-instrument channels become note events instead (see
  below).
- **Metronome:** uses the same beat span.

### Audio callback

`Engine::process()` splits the device block into chunks of at most the prepared size, then
for each chunk:

1. Clear the per-track stereo **buses** (Master + inserts), preallocated in `prepare()`.
2. Render sampler voices into the bus of their channel's mixer track.
3. Render each instrument channel with its note events for the chunk, into its track's bus.
4. For each insert: run its effect chain in place → apply fader and pan (mute/solo already
   folded into the gains) → record the peak → add into the Master bus.
5. Master: effects → fader/pan → peak → add to the device output.
6. Metronome clicks go straight to the output, bypassing the mixer.

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

### Looping channels, seeking, audition

- **Pattern length:** `Project::lengthInSteps()` is what plays: the notes rounded up to whole
  bars, at least one bar. `Pattern::gridSteps` is only how many steps the Channel Rack shows
  (`gridLengthInSteps()` = the larger of the two); steps past the play length are drawn as the
  pattern's repeat.
- **Looping channels** are a model feature: `Project::loopLengthInSteps()` gives the length a
  channel repeats at (its notes rounded up to 1, 2, 4... bars; 0 when looping is off, the
  channel has no notes, or they fill the pattern), and `toEngineState()` writes the repeats into
  the snapshot. The sequencer just plays notes. `Pattern::fillLoop()` turns repeats into real
  notes when a greyed step is clicked (ChannelRow): it repeats either the channel's own loop or,
  past the play length, the whole pattern, so the click never changes what else is heard.
- **Seeking:** `Transport::seek(beat)` stores a target that `advance()` applies at the start of
  the next block, like the rewind from `stop()` (a later `stop()` wins over an earlier seek and
  vice versa). The engine sees the block start somewhere else and releases held notes, as for
  any jump. The Playlist ruler seeks in song time (and switches to SONG), the Piano Roll ruler
  in pattern time (and switches to PAT).
- **Selection** (Piano Roll notes, Playlist clips) is view state inside the views: a list of
  indices (`PianoRollView::selection`, `PlaylistView::selection`), never in the model or undo.
  It's pruned in `refresh()` when items disappear, and the Piano Roll's is dropped when the
  channel or pattern changes. Moving a selection applies one offset to all, clamped so every
  item stays on the grid. Edit commands reach the views as `ui::EditAction`s through `edit()`,
  sent by `MainComponent::editInMainView()` to whichever view fills the main area.
- **Copy/paste:** `model/clipboard` (tested): a copy keeps its items and a block from the first
  start (rounded down) to the last end (rounded up) in whole beats (notes) or bars (clips);
  each `nextPaste()` lands one block further. The clipboards live in the views (in memory, not
  the system clipboard); pasted items become the selection.
- **Slicer:** `model::sliceNote()` does the cutting (tested); `PianoRollView::sliceNotes()`
  rebuilds the channel's note list in one undoable edit and selects the pieces.
- **Audition:** the Piano Roll calls `audition(pitch)` / `stopAudition()`; MainComponent sends
  them as live events (`Engine::sendLive()`) to the selected channel, so they play instruments
  and samples exactly like the typing keyboard.

### Instruments and note-offs

A channel can be played by a plugin instrument (`Channel::pluginId` set) instead of a sample.
The engine side is plain C++: `engine::InstrumentProcessor` takes a sorted list of
`NoteEvent`s (note-on, note-off, all-notes-off, each at a sample offset) and writes a block of
stereo audio. Instruments run every block, with or without events, so releases ring out.

Stuck notes are the main risk, so the engine (not the sequencer) owns note ends:
- **Held notes:** every note-on is remembered in a fixed array (`Engine::heldNotes`) until its
  `endTick`; the note-off is sent in whichever block reaches that tick. A note that starts
  and ends within one block gets both events at once. Because the end is fixed when the note
  starts, editing or deleting the note, switching patterns or changing tempo can't strand it.
- **Same key again** while it's held: the old note gets a note-off first (MIDI can't stack
  the same key), and a later note-off already queued for it moves up so it doesn't cut the
  new note.
- **Stop, pause or a jump** (the block doesn't continue where the last one ended): every
  held note gets a note-off at offset 0, and every instrument an all-notes-off.
- **Snapshot changes:** held notes follow their channel by id (its index may change). A
  removed channel or a replaced instrument drops its held notes; that instrument isn't
  played again.
- **Full array** (512 held notes): the note gets its note-off immediately rather than being
  forgotten.

**Live input** (`src/app/live_input`, `Engine::sendLive()`): the typing keyboard and MIDI
devices send `LiveEvent`s (note-on/off, controller, pitch bend, release-all) for the selected
channel, which `LiveInput` keeps in an atomic. The audio thread applies them at the start of
each chunk (offset 0, so latency is one block):
- On an instrument channel a note-on is remembered in `Engine::liveNotes` (by channel id, like
  sequenced notes, and remapped/dropped the same way on snapshot changes). A note-off is
  matched **by pitch**, not by channel, so a key still ends on the channel it started on after
  the selection changed. On a sampler channel a note-on just triggers the sample.
- The sustain pedal (CC 64) is lifted on the channel it went down on (`Engine::sustain`).
  `PluginInstrument` also sends sustain-off before every all-notes-off.
- Stopping the transport sends all-notes-off, which ends live notes too; the typing keyboard
  releases its keys when the window loses focus (`focusLost()` plus a check in the timer).
- The typing keyboard polls `KeyPress::isKeyCurrentlyDown()` in `keyStateChanged()` (so key
  auto-repeat doesn't retrigger) and doesn't start notes while Ctrl/Cmd/Alt is held.
- New MIDI devices are enabled when first seen (`juce::MidiDeviceListConnection`); turning one
  off in Audio settings sticks until the next start (device choices aren't saved yet).

Instruments live in the same `EffectPool` as effect processors (keyed by channel id; see
`getInstrument()`), so a synth keeps its voices and settings across edits, and the pool's
format reset and unused-collection cover them too.

### Plugins

Hosted plugins (VST3 and LV2, through JUCE) are effects and instruments like any other.
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
- **Instruments:** `PluginHost::createInstrument` wraps an instrument plugin as a
  `plugins::PluginInstrument` (no audio input, stereo out; mono synths go to both sides).
  Note events become MIDI on channel 1 at their sample offsets, in a `MidiBuffer` reserved in
  `prepare()`. Velocity 0..1 maps to 1..127 (0 would be a note-off in MIDI). The channel's
  volume, pan and mute apply after the plugin, then it feeds its mixer track like a sample.
- **Windows:** `PluginWindow` shows the editor of either kind. It holds the plugin's owner,
  and `MainComponent` closes it when the effect/channel is removed or its plugin replaced (the
  window's `owner()` no longer matches the pool's).
- **Play head:** every chunk, `Engine::updatePlayPosition()` fills a `PlayPosition` (playing,
  tempo, beats per bar, position in beats within the loop, the loop's length, the position in
  seconds and samples) and hands it to each effect and instrument through
  `setPlayPosition()` right before its `process()`. Built-in effects ignore it; the plugin
  wrappers keep it in a `HostPlayHead`, which answers JUCE's `AudioPlayHead::getPosition()`
  (and JUCE passes it on as VST3's ProcessContext / LV2's time position). The position is
  within what's looping (pattern in PAT mode, song in SONG mode) and reported as a loop from 0,
  matching the position display. It's a copy per processor, so the export's private engine
  gives the same plugins their own positions without interfering.
- **Not yet:** plugins run in the app's process, so a crashing plugin takes the app down.

## Browser and previews

- **Panel** (`ui/browser`): a `juce::TreeView` of the folders you added (saved in the settings
  file as `browserFolders`, with `browserOnRight` / `browserVisible`); folder items list their
  children when first opened (folders, then audio files, natural sort, hidden files skipped).
- **Loading** runs on a one-thread `juce::ThreadPool` with its own `io::SampleLoader`; a
  generation counter drops results that a newer click superseded.
- **Playback:** `engine::PreviewPlayer` (JUCE-free, tested) is fed through a lock-free command
  queue and mixed into the output after the Master, like the metronome. It gets a raw
  `SampleBuffer*`; the panel keeps replaced samples in `retired` until `consumed()` shows the
  audio thread has applied the command that dropped them, so nothing is freed while in use.
- **Waveform:** min/max peaks in 1024 buckets, computed once per sample; the playhead comes from
  `PreviewPlayer::position()` on a 30 Hz timer.
- **Dragging:** `MainComponent` is the `juce::DragAndDropContainer`; tree items describe audio
  files as `"file:" + path` (`BrowserPanel::dragPrefix`), and the Channel Rack, a
  `juce::DragAndDropTarget`, treats them like files dropped from the desktop.

## Commands and shortcuts

Every action with a shortcut is a command in `src/app/commands` (id, name, category,
description, default keys). `MainComponent` is the `juce::ApplicationCommandTarget`
(`getAllCommands` / `getCommandInfo` / `perform`) behind a `juce::ApplicationCommandManager`,
which gives:
- **Key handling:** `MainComponent::keyPressed()` lets the typing keyboard take its note keys
  first, then hands the key to the manager's `KeyPressMappingSet`.
- **Menus:** built with `addCommandItem`, so they show the current keys and the tick/disabled
  state from `getCommandInfo()`.
- **Editing:** Options › Keyboard shortcuts is JUCE's `KeyMappingEditorComponent` (themed
  colours). Changes are saved as differences from the defaults to `<settings>/shortcuts.xml`
  and restored at startup, so new default keys still reach existing users. Import/Export (in
  `ui/settings_dialog`) write and read the *full* set (`basedOnDefaults="0"`), so a file means
  the same on any version.
- **Hints:** `MainComponent::updateShortcutHints()` appends each control's current keys to its
  own tooltip (remembered in the component's `baseTooltip` property), and runs again whenever
  the mappings change. Views expose the controls that have commands (`loopSwitch()`,
  `lengthButton()`, `sliceControl()`, `snapControl()`...). Add new ones there.
- **Settings** used by commands (tempo steps) live in the settings file; Options › Settings
  (`ui/settings_dialog`) edits them.

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
- **Export:** `model::renderProject()` plays the project through a private `Engine`, block by
  block, stops at the end of the song (releasing held notes), renders the tail, and
  `io::writeWav()` writes the result. The app renders with the **live** `EffectPool` at the
  device's format, holding the audio callback lock meanwhile: plugins aren't loaded a second
  time, which matters because some (e.g. LSP's samplers) restore their state on a background
  thread and would still be silent. Every processor is `reset()` before and after, so no tail
  or voice leaks between live playback and the export. Tests use the overload that builds its
  own pool from factories.

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

**Make the engine send something new to instruments** (e.g. pitch bend, CC)
1. Add a `NoteEvent::Kind` in `src/engine/instrument.h`.
2. Emit it in `Engine::renderChunk()` through `addNoteEvent()` (never allocate; the event
   array is fixed).
3. Translate it to MIDI in `PluginInstrument::process()` (`src/plugins/plugin_host.cpp`).
4. Test it with the recording `FakeInstrument` in `tests/instrument_tests.cpp`.

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

**Add a command (and its keyboard shortcut)**
1. Add an id to `commands::ID` in `src/app/commands.h`, at the end of its group. Ids are saved in
   users' `shortcuts.xml`: never renumber or reuse one.
2. Add its name, category, description and default keys (FL's where FL has one) to
   `commands::all()` in `commands.cpp`.
3. Perform it in `MainComponent::perform()`; set active/ticked state in `getCommandInfo()` if
   it has any. Put it in a menu with `addCommandItem` in `getMenuForIndex()` if it belongs there.
4. List its default key in [user-guide.md](user-guide.md).
