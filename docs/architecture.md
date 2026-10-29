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
            │ src/app   JUCE application: MainComponent (owns Document,│
            │           Engine, EffectPool), commands + shortcuts,     │
            │           menus, LiveInput (keyboard/MIDI), StretchCache │
            └───────────────┬───────────────────────┬──────────────────┘
                            │                       │
            ┌───────────────▼──────────┐   ┌────────▼─────────────────┐
            │ src/ui    JUCE views:    │   │ src/io  file I/O, src/   │
            │ ChannelRack, Playlist,   │   │ plugins  VST3/LV2 hosting│
            │ PianoRoll, Mixer,        │   │ (JUCE)                   │
            │ Browser, Sampler, dialogs│   └──────────────────────────┘
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
   │ Sampler, Metronome, effects, instruments, EffectPool,    │
   │ PreviewPlayer, ScopeBuffer, time-stretching, DrumSynth,  │
   │ SpscQueue,                                               │
   │ EngineState snapshot types                               │
   └──────────────────────────────────────────────────────────┘
```

Dependencies only point downwards. Third-party code comes in through CMake FetchContent (JUCE,
Catch2, nlohmann/json, Signalsmith Stretch). Fixes we need in it live in `patches/` and are applied automatically
at configure time ([patches/README.md](../patches/README.md)). `jls_engine`, `jls_model` and `jls_theme` are static
libraries without JUCE. The `JuicyLoopsStudio` app target compiles `src/app`, `src/ui` and
`src/io` together with JUCE. `jls_tests` links only the JUCE-free libraries.

## Threads

| Thread | Runs | May |
|---|---|---|
| **UI (message) thread** | All JUCE components, `Document` edits, `toEngineState()`, `EffectPool`, `Engine::setState()` / `collectGarbage()` | Allocate, block, do file I/O |
| **Audio thread** | `Engine::process()` (from the audio device callback) | Only read its current snapshot, write its own preallocated state and atomics |
| **MIDI input thread(s)** | `LiveInput::handleIncomingMidiMessage()` | Only call `Engine::sendLive()` and read an atomic |
| **Background workers** | Browser sample loading, `StretchCache` time-stretching (one thread each) | Work on their own data; results go back to the UI thread with `MessageManager::callAsync` |

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
| `channels` | Name, a sample *or* a plugin instrument, volume, pan, mute, mixer track, loop switch, Sampler settings (stretch mode, original tempo, pitch); `audioClip` marks the hidden channels behind audio clips |
| `patterns` | Notes per channel id (start, length, pitch, velocity, in ticks); `gridSteps` for the Channel Rack |
| `playlist.clips` | A pattern or an audio clip on a track, from which step, for how long; a pattern clip longer than its pattern repeats it; an audio clip can start part-way into its sample (`offsetSteps`) |
| `mixer.tracks` | Master + 16 inserts: fader, pan, mute, solo, effect slots |
| settings | tempo, beats per bar, `loopChannels` |
| view state | `currentPattern`, `selectedChannel`, `selectedMixerTrack`, `songMode`, the Piano Roll scale, `showAudioClips` |

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
  depend on buffer size. Reads samples with 4-point cubic (Hermite) interpolation. Without an
  envelope a sample plays to its end (note length ignored, FL's default); with the DAHDSR
  envelope on, each voice steps the envelope per sample and releases at the note's end (the
  trigger's `endTick` as seconds) or, for live notes, at `releaseLive()`. Cutting: a trigger
  fades out (64 samples) the channel's own voices when `cutItself`, and every voice whose
  channel's `cutBy` equals its `cutGroup`.
- **Instruments:** triggers on plugin-instrument channels become note events instead (see
  below).
- **Audio clips:** a clip starting in the span becomes one trigger for its channel's sample
  (see *Audio clips* below).
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
6. Metronome clicks and the Browser's sample preview go straight to the output, bypassing the
   mixer.
7. The finished output (mono mix) is copied into `ScopeBuffer`, a lock-free ring of atomics
   that the transport bar's oscilloscope (`ui::Oscilloscope`) reads at 30 Hz; its spectrum mode
   runs `engine::spectrumDb` (a small radix-2 FFT, Hann window) on the UI thread.

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

### Editing: pattern length, looping, seeking, selection, clipboard, slicer, audition

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

### Audio clips

- **Model:** an audio clip is a `Clip` with `audio` set to a `Channel` whose `audioClip` flag is
  on (`Project::audioClipChannel()` makes or reuses one per file). The channel brings the mixer
  routing, volume, pan and mute; the Channel Rack hides it unless `Project::showAudioClips`.
- **Snapshot:** `ClipState::audioChannel` (index) instead of `patternIndex`.
- **Timing:** `collectTriggers` emits one `Trigger` (`audioClip`, `lengthTicks`) when a clip
  starts. When playback starts or jumps, `collectClipsUnderPlayhead()` finds clips already
  running at the playhead and `clipOffsetTicks` says how far in. The engine converts ticks to
  seconds at the current tempo and calls `Sampler::triggerClip()` (start frame, maximum length).
- **Ending:** clip voices count down `remaining` output samples and fade out over the last
  128; `Sampler::stopClips()` (on stop or a jump) shortens every clip voice to that fade.
  Ordinary sample triggers still ring out.
- **UI:** the Playlist grid is a drop target for files (desktop or Browser) and for picker
  items (`"pattern:<id>"`, `"audio:<id>"`); `PlaylistView::placeAt()` handles all of them. The
  waveform peaks come from `ui/waveform.h`, cached per sample.

### Sampler: speed and time-stretching

- **Settings:** `model::SamplerSettings` on each channel (mode, original tempo, pitch).
  `Project::playbackSpeed()` / `stretchFactor()` / `playedSeconds()` hold the maths.
- **Off / Resample:** just a speed (`ChannelState::speed`) that the Sampler multiplies into its
  increment and into an audio clip's start position. No extra work.
- **Stretch:** the sample is stretched offline with Signalsmith Stretch
  (`engine::stretchSample`, MIT, fetched by CMake) to `stretchFactor` and pitch, and that buffer
  plays at speed 1. `toEngineState()` gets it from a provider; in the app that's
  `StretchCache` (one background thread; results for old tempos are dropped, and their queued
  jobs skipped). A finished job calls `publishState()` again. Until then the channel plays
  resampled. Export asks for every stretch synchronously (`getNow`).
- **Window:** `ui::SamplerPanel` in a `SamplerWindow`, one at a time, owned by `MainComponent`
  and refreshed (or closed) on every document change.
- **Trim:** `Clip::offsetSteps` / `ClipState::offsetTicks` add to an audio clip's start offset.

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
- **Focus areas:** `MainComponent::Area` (Browser, Channel Rack, main view, Mixer, a plugin
  panel) says where the navigation commands (arrows, Enter) go; JUCE's keyboard focus stays on
  the window. Ctrl+Tab walks `FocusStop`s: an area, or one open plugin panel (`focusPlugin`;
  `tabOrder()` lists the stops showing; separate plugin windows aren't stops). A click in an
  area or on a plugin panel focuses it (`mouseDown`; not when the click's own action already
  chose the focus, like the rack's note preview opening the Piano Roll), as does bringing a
  plugin up
  (`bringPluginToFront`); Ctrl+Tab cycles (`cycleFocus`); each panel draws its own focus line
  from its "focused" property (`updateFocusFrames`), and `navigate()` dispatches (a plugin
  takes none). With Settings › Ctrl+Tab order = "Last used first", `recentAreas` keeps the
  stops most-recent-first; while the modifier is held the order is frozen and `cycleStep`
  walks it, and the release commits (`modifierKeysChanged`, and the timer, since a native area
  doesn't pass modifier changes up to us). The timer also drops closed plugin panels
  (`forgetClosedPlugins`).
  `raiseFocusedArea()` also sets the floating Channel Rack's stacking: in front while it has the
  focus, behind the Playlist / Piano Roll while they do.
- **Note keys vs commands:** the typing keyboard only claims plain character keys (codes below
  128) without modifiers; special keys' codes can end in a letter's byte (Down = ...'T').
- **Editing commands** (copy, duplicate, move, slice...) become a `ui::EditAction` for the
  Piano Roll or the Playlist (`editInMainView`); each view's `edit()` handles the ones it
  understands.
- **Settings** used by commands (tempo steps) live in the settings file; Options › Settings
  (`ui/settings_dialog`) edits them.

## UI

`MainComponent` arranges:
- the menu bar (native on macOS) and the transport bar
- the **Browser** docked at the left or right (Alt+F8)
- the main area, showing either the **Playlist** (F5, with its picker panel) or the
  **Piano Roll** (F7)
- the **Mixer** docked below (F9)
- floating panels inside the window, dragged by their title bars and kept below the transport
  bar: the **Channel Rack** (F6), the **effect panel**, and `ui::FloatingPanel`s holding the
  **Sampler** (`SamplerPanel`) and plugin editors (`plugins::PluginPanel`). A plugin editor is
  a native window on Linux (`juce::XEmbedComponent`): it follows only its own moves, so the
  panel calls `updateEmbeddedBounds()` when it moves. `plugins::PluginView` hides whether an editor is a panel or a
  separate `PluginWindow` (Settings › Plugin windows). A click into an editor gives the plugin
  the keyboard; `MainComponent::returnKeyboard` takes it back when the mouse leaves the plugin
  panels (the timer) or from the panel's keyboard button. JUCE won't move the X input focus while
  it's inside our window (a plugin's child window counts), so `app/native_windows` sets it with
  Xlib (the app links libX11 for this). The same file maximizes and restores the main window
  through the window manager for our title bar (`ui::TitleBar::setMaximised`), since JUCE's full
  screen only resizes a window without the system's title bar.
- **Areas as native windows (Linux).** JUCE can't draw over a plugin's native window, so on
  Linux every area that can overlap a plugin panel (Browser, Playlist, Piano Roll, Mixer,
  Channel Rack, effect panel, Sampler and the plugin panels themselves) is a native child window
  of the main window, and `app::AreaStack` keeps their stacking order: `raise()` restacks them
  with Xlib (`XRestackWindows`; JUCE's `toFront()` only asks the window manager, which doesn't
  restack child windows) and moves the area forward in JUCE's own desktop list, which drag and
  drop follows. `MainComponent` places areas through the stack (`setBounds`, `toAreaSpace`)
  since they aren't its children; it listens to their mouse events, and the main window's
  `hitTest` says "not here" where an area covers it (`coversPoint`) so drags find the area.
  Keys still go to whichever component has JUCE's focus, in any of our windows; the shortcut
  safety net spares text fields inside areas (`holds`). Child windows can't be
  semi-transparent, so floating panels are opaque and the Channel Rack has no shadow there.
  Rules that keep them from glitching (seen on GNOME's XWayland, not on Xvfb): place a panel
  before it shows (`EffectPanel::load`, the Sampler), or it flashes where it was; panels drag
  with `ui::PanelDragger`, since JUCE's `ComponentDragger` puts a window at the mouse's screen
  position while ours are placed relative to the main window; when an area hides or closes,
  `repaintBelow` repaints what it covered, and the main window's X background is the seams'
  colour (`setBackground`), so nothing stale shows meanwhile.
  Elsewhere (and with `JLS_NO_NATIVE_AREAS=1`) the areas are ordinary children and plugin
  panels stay in front.
- separate windows: the Settings / Keyboard shortcuts / Theme / Plugins / Audio settings
  dialogs, and plugin editors when set to separate windows
- window frames: no system title bars (except on macOS). The main window's top row is
  `ui::TitleBar` (menus, name, minimize / maximize / close, drag to move, double-click to
  maximize; the window keeps JUCE's resizable border). Other windows call
  `ui::useOwnTitleBar()` and get the title bar the LookAndFeel draws
  (`drawDocumentWindowTitleBar`, `createDocumentWindowButton` -> `ui::TitleButton`).
  Turning the native title bar off makes JUCE create a new native window, so UI tests pick the
  last window with the app's name.

Views are plain JUCE components that read the `Document` in `refresh()` and change it only
through `Document::apply()` / `setViewState()`.

**Keyboard focus.** Shortcuts are handled in `MainComponent::keyPressed` (see *Commands and
shortcuts*), and most widgets don't take focus. Focus is steered back after menus (`menuBarActivated`), on clicks (a mouse
listener on all children) and by a timer while the window is active.

**Theming.** A theme is colours only (`ui::currentTheme().colours`). Components read them in
`paint()`, so switching themes repaints live. Everything else about the look is fixed, the same
in every theme: sizes and corner radii (`ui::metrics`) and the font (Inter, IBM Plex
Mono for numbers). `ThemeManager`:
- builds the `LookAndFeel` from the active theme
- loads the bundled font once (`LookAndFeel::setFont`)
- remembers the choice in the settings file (a theme that no longer exists falls back to
  Blackberry, the default)
- manages user theme files

The look is a console's: docked panels edge to edge with 2 px seams, surfaces lit from above
(`fillRaised()`: a gradient and a highlight along the top edge), recessed displays, console
faders and knobs, step keys with an LED. The theme file format is in [themes.md](themes.md).

**Controls.** `ui/controls` holds the shared pieces, so every panel lines up:
- sizes in `ui::metrics`: panel headers 36 px with 26 px controls, the transport bar's 32 px
  controls, 14 px icons, a 6 px gap, and the type scale every text uses: `textTiny` (11 px),
  `textSmall` (12), `textBody` (13), `textLarge` (14), `textHeading` (16)
- `drawIcon()`: vector icons (add, close, play, loop, metronome...), no image files
- `IconButton`: icon, icon + label or label; toggles show the active colours
- `LedToggle` / `paintLed()`: on/off switches drawn as a round light, the same as a channel's
  mute light (lit in the theme's `led` colour)
- `WindowButton`: the transport bar's window buttons, an `IconButton` with an LED showing the
  window's place in the Ctrl+Tab order (`MainComponent::tabSteps`, refreshed on focus changes and
  by the 30 Hz timer, since windows are shown and hidden from many places)
- `joinSegments()`: buttons drawn as one segmented control (PAT | SONG, 16 | 32 | 64)
- `paintPanelHeader()` + `HeaderLayout`: every panel's header (title, dim subtitle, controls
  from the right)
- `paintDisplayBox()` and `NumberField`: recessed number boxes (tempo, settings, the Sampler),
  with the bundled IBM Plex Mono so digits keep their width
- `paintCardFrame()`: docked panels' frame (square, a thin border; an accent line along the top
  while focused, from the "focused" property MainComponent sets)
- `fillRaised()`: a raised surface lit from above, for buttons, step keys, knobs and fader caps;
  `fillRaisedOutlined()` adds the outline, `paintOutlinedBox()` is the flat equivalent
  (drop-downs, tooltips), and `buttonFill()` shades a fill for hover and press
- `FormPanel`: dialogs and the Sampler window as sections of label + control rows and notes
- fonts: `mediumFont()` / `semiboldFont()` ask for a weight by style name; `ThemeManager` loads
  Inter in Regular, Medium and SemiBold
The `LookAndFeel` draws the rest to match: drop-downs, segment edges, scrollbars, tooltips and
tree arrows. Mockups of the layout are on the design canvas (option A, "Compact").

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
on Windows. Next to the settings file: `themes/` (user themes), `shortcuts.xml` (changed
shortcuts), `plugins.xml` (scanned plugins). Projects are saved wherever the user chooses
(`.jlp`; older `.jls` files still open, and saving one writes a `.jlp` beside it).

## Testing

- **Unit tests** (`tests/`, Catch2, run with `ctest`) cover the JUCE-free code, one file per
  area (each file says what it covers at the top):
  - timing to the sample (transport drift, sequencer, metronome, audio clips)
  - instruments with a recording fake: note-ons/offs, no stuck notes, live input, play head
  - the Sampler: speed, Resample / Stretch maths, real time-stretching quality
  - effects, the EffectPool, mixer routing, previews
  - the model: notes, looping, scales, slicing, copy/paste, undo; project files; export; themes
- **UI and sound** are checked by driving the app on Xvfb and by exporting and analysing WAVs;
  see [development.md](development.md), which also covers CI.

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
3. Add it to every built-in theme JSON (`builtin_themes.cpp`). The tests check that each
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
3. Give it a show/hide command (see *Add a command* below) and put that in the View menu.
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
