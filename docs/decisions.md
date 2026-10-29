# Decision log

Why things are the way they are, newest last. Each entry records *what* was decided and *why*,
plus known gaps at the time (the current list is in `roadmap.md`). Add an entry when you make a decision others would otherwise
second-guess (a convention, a trade-off, a dependency, a behaviour copied from FL or deliberately
not). Older entries describe the project as it was then; later entries may supersede them.

- 2026-10-16: Language set to **C++** (over Rust) for its plugin-hosting and DAW
  ecosystem.
- 2026-10-16: **JUCE 9 + CMake**, deps via FetchContent. JUCE 9's open-source
  license is AGPLv3 (not GPLv3), so the project is **AGPLv3**.
- 2026-10-16: Engine is a separate JUCE-free library so it can be unit-tested
  without a GUI.
- 2026-10-16: Transport semantics follow FL: the Play button toggles play/pause;
  Space and the Stop button stop *and rewind* to the start.
- 2026-10-16: Metronome: 1760 Hz accent on bar downbeats, 880 Hz on other beats,
  30 ms decaying sine. Off by default; Ctrl+M toggles it (FL's shortcut). Time
  signature is numerator only (n/4) for now.
- 2026-10-16: Playhead is computed from an integer sample count since the last tempo
  change (not by summing per-block increments), and trigger offsets are rounded. This
  fixed a real drift bug and keeps triggers exact to the sample; there's a test for it.
- 2026-10-16: Channel Rack v1: steps belong to the pattern (keyed by channel id), 16 steps =
  1 bar of 16ths, pattern loops. Default channels Kick/Clap/Hat/Snare use *synthesized*
  sounds (no shipped samples). Left-click toggles/paints steps, right-click erases. The
  channel name button opens a menu (rename / replace sample / delete). Dropping audio files
  on a row replaces its sample; dropping elsewhere adds channels. Volume default 80%,
  balance-style pan. Undo is Ctrl+Z, redo Ctrl+Y / Ctrl+Shift+Z. (FL's real scheme differs:
  Ctrl+Z toggles last undo/redo, Ctrl+Alt+Z steps back. Revisit.)
- Known gaps: no channel settings window, no pattern length UI (model supports 1-64),
  samples load synchronously on the UI thread, no channel reordering or selection.
- 2026-10-16: Look: maintainer picked **Juicy** (plum #17121c/#241b2c, blood orange #ff7a45,
  lime #b7f04a) as the main direction. Further mockups (Citrus, Watermelon, Blueberry) are
  deliberately *not* FL-like. Plan: themes become data, selectable in Options › Theme,
  with import/export of custom theme JSON files. A theme sets colours *and* font, corner
  radii, border width and glow; anything missing falls back to the default. Format
  draft is on the mockup canvas's ThemePicker board.
- 2026-10-16: Theme system built. Built-ins Juicy (default), Citrus, Watermelon,
  Blueberry are JSON strings in `src/theme/builtin_themes.cpp`, parsed like user files.
  `Theme`'s member defaults == Juicy (tested). Options › Theme… opens ThemeDialog (cards,
  import/export/open folder). User themes live in `<settings dir>/themes/*.json`, ids
  `custom:<file>`. Settings: `$XDG_CONFIG_HOME/JuicyLoopsStudio/` on Linux (JUCE's
  default would be a folder directly in $HOME). Fonts are bundled as **static** Regular
  TTFs: variable fonts loaded at their default axis (Outfit = Thin). Menu bar: File/Edit/
  Options/Help (native on macOS). Format doc: `docs/themes.md`.
- 2026-10-16: Patterns + Playlist. `Project` has `patterns` (never empty; steps keyed by
  channel id) and `playlist.clips` {pattern, track, startStep, lengthInSteps}; positions
  are in steps (16ths). A clip longer than its pattern **repeats** it. Song length = last
  clip end rounded up to a bar; SONG mode loops it. `currentPattern` and `songMode` are
  **view state**: `Document::setViewState()` changes them without an undo step, and
  undo/redo keeps them. Time signature moved into `Project::beatsPerBar` (transport mirrors
  it for the metronome). Playlist mouse: left-click places + drag moves, right edge resizes,
  right-click deletes, Ctrl+wheel zooms; snap Bar/Beat/Step. Channel Rack is a floating
  panel over the Playlist (drag its title bar; F6 toggles). Keys: L = PAT/SONG, F4 = new
  pattern. Patterns menu: new/clone/rename/delete.
- 2026-10-16: Keyboard focus: shortcuts live in MainComponent and most widgets don't take
  focus, so focus must come back after menus and dialogs: `menuBarActivated(false)`, a click
  anywhere (mouse listener on all children), and a timer safety net (only while our window
  is the active one). Found by UI testing, along with step painting skipping steps on fast
  drags (now interpolated).
- Known gaps: no seeking (clicking the ruler), no pattern length UI, no track names/mute,
  no clip selection or multi-select, the playlist has a fixed 16 tracks.
- 2026-10-16: Piano Roll. Patterns store **notes** per channel (`Note`: startTick,
  lengthTicks, pitch, velocity; 96 ticks/beat, 24/step). A Channel Rack step is a note at
  C5 (60), one step long, on a step boundary (`Note::isStep`). A channel with any other
  notes shows a mini note preview in the rack instead of steps; clicking it opens the Piano
  Roll, as in FL. Pattern length = notes rounded up to whole bars (min 1 bar; see 2026-10-17)
  (`Project::lengthInSteps(pattern)`). Engine works in ticks; the sequencer collects note
  starts per block (pattern loop / clip repeats / song loop split) and sorts by offset.
  Sampler pitch = resample by 2^((p-60)/12); velocity gain = v / (100/128). **Note length
  isn't audible yet**: samples play to the end, like FL's default sampler.
  `Project::selectedChannel` is view state (left-click a channel name; right-click = menu).
  F7 shows the Piano Roll for it, F5 the Playlist (they share the main area). FL note names
  (C5 = 60). Piano Roll mouse mirrors the Playlist: click adds + drag moves, right edge
  resizes (sets the length for new notes), right-click deletes, Ctrl+wheel zooms; snap
  Beat/Step/half/third/None; velocity lane sets the nearest stem (and chord) within 8 px.
- Known gaps (Piano Roll): no note selection/multi-select/copy, no audition when clicking
  keys, no cut-at-note-end option, can't set pattern length by hand.
- 2026-10-16: Mixer. `Project::mixer.tracks` = Master + 16 inserts (fader dB, pan, mute,
  solo, up to 8 `EffectSlot`s {id, type, params, enabled}). Channels have `mixerTrack`
  (defaults Kick..Snare -> 1..4, new channels -> next free insert, 0 = Master). Engine: the
  sampler renders into per-track stereo buses; inserts run effects, fader/pan, meter, then
  sum into Master; Master goes to the output; metronome bypasses the mixer. Effect DSP
  state lives in `EffectProcessor`s owned by `engine::EffectPool` (UI thread, keyed by
  effect id) and is shared into each snapshot via shared_ptr, so tails survive edits; the
  audio thread never copies or releases those pointers. Pool is reset when the device
  format changes (MainComponent timer). Effects are written from scratch (engine stays
  JUCE-free) and describe their params (`ParamInfo`) so the UI builds knobs generically.
  Meters: `Engine::takePeak()` (atomic max, read-and-reset by the UI). Mixer docked at the
  bottom (F9), selected track's slots on the right, `EffectPanel` floats.
- Known gaps (Mixer): no sends/routing between inserts, no renaming tracks, no param
  smoothing (fast knob moves can click), no tempo-synced delay.
- 2026-10-16: Projects + export. `.jls` = JSON (`model/project_file`, nlohmann now a top-level
  dependency shared by theme + model). Samples are referenced (absolute + relative path;
  relative tried first), built-ins by name; effect params saved by name; loading clamps,
  skips unknown entries with warnings, keeps missing sample references. Floats are saved
  rounded to 6 decimals. Tempo moved into `Project::tempo` (undoable; transport mirrors it).
  `Document` has revision-based `isModified()`, `markSaved()`, `reset()`. Export =
  `model::renderProject()` (private Engine + EffectPool, song once + 2 s tail, current pattern
  if the Playlist is empty) + `io::writeWav()` (24-bit). File menu: Ctrl+N/O/S/Shift+S/R,
  title shows name and `*`, Save / Don't save / Cancel prompt on New/Open/Quit.
- Known gaps (files): export runs on the UI thread (blocks briefly, and holds live audio), no recent-files list, no
  autosave, no "collect samples into project folder". Clipping: solved 2026-10-17 with a **Soft
  Clipper** effect (maintainer's choice over a limiter), on the Master in new projects
  (`makeDefaultProject`). Linear below the knee, tanh shoulder up to the ceiling (default
  -0.3 dB); no oversampling yet.
- 2026-10-17: Plugin hosting phase 1 (effects). `src/plugins/plugin_host` (JUCE,
  `JUCE_PLUGINHOST_VST3/LV2=1`; CLAP isn't in JUCE). `EffectSlot` type `"plugin"` + pluginId /
  pluginName / pluginState (base64). `EffectPool::setPluginFactory` creates them;
  `EffectProcessor::saveState()` lets plugins report state; failures are cached in the pool.
  `PluginWindow` shows the editor (it holds the processor; the editor is deleted first).
  Plugin states are synced into the project before save/export. Known list in
  `<settings>/plugins.xml`, crash file `plugin-scan-crash.txt`. In-process hosting only. All
  JUCE dialogs set `dialogBackgroundColour` from the theme (the default is light grey).
- 2026-10-17: Tested with LSP plugins (`lsp-plugins-vst3/-lv2`, 198 each): scanning both
  formats, adding via grouped menu (manufacturer > category > plugin; LV2's " Plugin"
  category suffix stripped), VST3 editor window, processing (export peak matched the
  plugin's -39 dB output), state through save/reopen/export. The LV2 editor stayed blank
  on Xvfb only; on the maintainer's Hyprland desktop it shows (an Xvfb artifact).
- 2026-10-17: **Crash fixed in JUCE:** closing an LV2 plugin window segfaulted (maintainer report,
  core dump via `coredumpctl debug`). JUCE 9.0.4 `XEmbedComponent` queues `callAsync([this]
  ...)` on ConfigureNotify; the window was deleted before it ran. Fixed by
  `patches/juce-xembed-use-after-free.patch` (SafePointer check), applied idempotently by
  `cmake/apply_patch.cmake` as JUCE's FetchContent PATCH_COMMAND. Patches keep JUCE's CRLF
  line endings (`.gitattributes: *.patch -text`). Report upstream and drop the patch when
  JUCE ships a fix. Debug crashes with `coredumpctl debug <pid> --debugger-arguments="-batch
  -ex 'thread 1' -ex 'bt 40'"`. Message boxes use NoIcon (JUCE's icons
  are huge and off-theme).

- 2026-10-17: Plugin instruments (phase 2). `Channel::pluginId/pluginName/pluginState` (project
  key `instrument`, replaces `sample`); `setInstrument()` / `setSample()` switch kinds.
  Engine-side `InstrumentProcessor` gets sorted `NoteEvent`s per block (JUCE-free, testable with
  a fake). **No stuck notes** by construction: the sequencer gives each trigger an absolute
  `endTick` (cut at clip/pattern-repeat end) and the *engine* holds the note until then (fixed
  512-entry array); same key retriggered = note-off first; stop/pause/jump = note-offs + all-notes
  -off; a removed channel/replaced plugin drops its held notes. Instruments live in `EffectPool`
  keyed by channel id. UI: + menu (Sample / Instrument), row menu (use/replace instrument, show
  plugin), clicking an instrument channel's name toggles its window (FL-like). The grouped plugin
  menu is shared (`ui/plugin_menu`). `PluginWindow` takes (owner, plugin) and closes when its
  owner no longer matches the pool's. Plugins get no AudioPlayHead yet. **Export now renders with
  the live EffectPool** (under the audio callback lock, processors reset before/after): fresh
  instances were silent because LSP Sampler loads its sample asynchronously. Verified on Xvfb with
  LSP Sampler Stereo + a 440 Hz WAV: notes at steps 0 and 8 cut after one step (+10 ms fade),
  identical after save/reopen. Known: delete + undo of an instrument channel restores the plugin
  state as of the last save.

- 2026-10-17: Surge XT (VST3, `surge-xt`) tested: editor renders on Xvfb, export plays C5 =
  261.6 Hz with releases on time; maintainer reports its window is "a bit wonky" on Hyprland but works.
- 2026-10-17: Live input. `Engine::sendLive(LiveEvent)` from any non-audio thread (senders
  serialized by a mutex, SPSC queue to the audio thread, applied at chunk start). Note-offs match
  by **pitch** (ends on the channel it started on); sustain lifted where it went down; live notes
  remapped/dropped on snapshot change like sequenced ones; sampler channels just trigger. Typing
  keyboard = FL layout (Z row C5, Q row C6, skipping L and punctuation), **on by default**,
  Ctrl+T / "Keys" button / Options menu; polled via `isKeyCurrentlyDown` in `keyStateChanged`,
  released on focus loss. MIDI: all inputs omni, new devices auto-enabled
  (`MidiDeviceListConnection`); notes, CCs, pitch bend. Not saved: device choices. Not yet:
  recording, octave shift for the typing keyboard, highlighting played keys.

- 2026-10-17: Play head + clock. `engine::PlayPosition` (playing, bpm, beats per bar, beat within
  the loop, loop length, seconds, samples) computed per chunk and passed via
  `setPlayPosition()` to every effect/instrument before `process()`; `plugins::HostPlayHead`
  turns it into JUCE's `AudioPlayHead` (owned by each wrapper, declared before the instance).
  Position is within what loops (pattern/song), reported as a loop from 0. Verified with LSP
  Artistic Delay's SYNC: showed 130 then 100 BPM (LSP latches the value until SYNC is re-pressed).
  Clock under bar:beat:step: `model::formatTime` "M:SS.mmm", current / total of what loops.
  Tempo -/+ buttons step 1 BPM (range interval 1, custom snap keeps typed values to 0.001).

- 2026-10-17: Rulers seek (`Transport::seek`; Playlist ruler -> SONG mode, Piano Roll ruler -> PAT
  mode, snapped). Piano Roll audition (keys and notes, while held; drag = new pitch) via live
  events. Channel Rack: length box 16/32/64 (= `Pattern::minLengthInSteps`, maintainer's "4/8/16" meant
  groups of 4 steps) + drag handle (groups of 4); steps shrink to fit (20 -> 8 px). **Looping**
  (maintainer's choices): rack Loop switch (`Project::loopChannels`) + per-channel `Channel::loop`, both
  default on; loop length automatic = notes rounded up to 1/2/4/8.. bars; repeats expanded in
  `toEngineState`; greyed in the rack; clicking a greyed step `fillLoop`s first. Limitation: a
  looping channel can't end in empty bars (switch its loop off).

- 2026-10-17 (correction from the maintainer): the rack's 16|32|64 (joined buttons, not a drop-down)
  and handle set only the **shown** steps (`Pattern::gridSteps`, file key `gridLength`, old
  `minLength` read as it). What plays = notes rounded up to whole bars ("only the first 16 are
  used until I add a note in 17-32, even with loop on"). Steps past it are greyed pattern
  repeats; clicking one fills the repeat in first.

- 2026-10-17: Scale highlighter (backlog 1): `model::Scale` none/major/minor + `scaleRoot`, view
  state (kept through undo, saved in `view`), `isInScale()`. Rows: in-scale lighter, others
  darker, root tinted with the accent; colours derived from `lane`/`accent` (no new theme keys).
  Every key labelled (not just C).

- 2026-10-17: Selection (backlog 2): Ctrl+drag rubber band in the Piano Roll and the Playlist
  (accentAlt outline), drag a selected item = move all (Piano Roll: right end = resize all),
  Delete / Ctrl+A / Esc via `keyCommand()`. Selection = indices kept in the view (not model/undo).
  A click outside a selection only deselects, as in FL. Copy/paste: Ctrl+C/X/V (`model/clipboard`),
  pasted one block (whole beats / bars) after the copy, then after each paste; pastes selected.

- 2026-10-17: Note slicer (backlog 3): Piano Roll "Slice" button + piece size (1/4, 1/8, 1/8 T,
  1/16, 1/16 T, 1/32, 1/64), shortcut Alt+U (FL's chop key); selected notes, or all if none;
  pieces stay selected. `model::sliceNote()`.

- 2026-10-17: **Customizable shortcuts** (maintainer's request: "for borderline everything"). Commands in
  `src/app/commands` (stable ids, saved in `shortcuts.xml`; never renumber) on JUCE's
  ApplicationCommandManager; MainComponent = command target; menus via `addCommandItem`; editor =
  JUCE KeyMappingEditorComponent (Options > Keyboard shortcuts, Ctrl+Shift+K). Tempo up/down
  (Ctrl+Up/Down) by a step and a fine step (Ctrl+Shift+Up/Down), **defaults 5 and 1 BPM** (maintainer's
  choice), changeable in Options > Settings (Ctrl+,). New defaults: Alt+Up/Down slice size,
  Alt+Right/Left snap, Alt+L / Alt+1/2/3 rack loop / 16/32/64, Numpad +/- pattern, F2 rename,
  Ctrl+Shift+C clone, Shift+Space play/pause, Ctrl+Q quit. Views take `ui::EditAction`s.
  Import/Export of full shortcut sets in the shortcuts window; tooltips of controls with commands
  show their current keys (`updateShortcutHints()`, refreshed on mapping changes).

- 2026-10-17: Browser panel (backlog). `ui/browser` (TreeView of added folders, docks left/right,
  Alt+F8), previews via `engine::PreviewPlayer` (lock-free, after the Master; raw sample pointer
  kept alive by `retired` + `consumed()`), background loading (ThreadPool, generation counter).
  Click = first 5 s (Settings: preview length), Shift+click = whole, waveform click = from there,
  Play/Pause button. Drag "file:<path>" onto the Channel Rack (DragAndDropTarget). Commands:
  toggle browser, switch side, add folder, preview play/pause. Loader still caps at 120 s.
  Next in the backlog: audio clips (drop on the Playlist), which this enables.

- 2026-10-17: Keyboard navigation + focus areas (maintainer request). Browser: Up/Down move (and preview
  samples), Right/Left open/close, Enter = open folder or load the sample into the selected
  channel; Ctrl+click = load. Selected channel row highlighted. Focus areas (Browser, Channel
  Rack, Playlist/Piano Roll, Mixer) with an accent outline; click to focus, Ctrl+Tab /
  Ctrl+Shift+Tab cycle (Alt+Tab is usually the desktop's). All rebindable commands ("Navigation").
  Fixed on the way: plain arrow keys were swallowed as typing-keyboard notes (key code truncated
  to a char); the settings file now saves within 1 s (was only on clean exit).

- 2026-10-17: Replacing a channel's sample renames the channel after it (FL). Browser Ctrl+Up/Down =
  move + load into the selected channel + preview (FL); so **tempo moved to Shift+Up/Down** (fine
  stays Ctrl+Shift+Up/Down). Focus order setting: Fixed or Last used first (Alt+Tab-like, commits
  on modifier release). Fixed: the Settings window's Browser section wasn't shown (an unmatched
  edit); it and a Focus section are there now.

- 2026-10-17: Audio clips (backlog, option b). Engine: song-time sample clips, mid-clip start on
  play/jump (`collectClipsUnderPlayhead`), cut at clip end and on stop/jump with a 128-sample fade
  (`Sampler::triggerClip/stopClips`). Model: `Channel::audioClip`, `Clip::audio`, one channel per
  file (`audioClipChannel`), hidden in the rack (View > Channel rack options > Show audio clips).
  Playlist: drop files / Browser samples, waveform drawing, picker panel (patterns + audio
  clips; click = brush, drag = place). Song mode only; no time-stretching (tempo changes move the
  clip's grid position, not the sample's speed); no left-edge trim/offset yet.

- 2026-10-17: Sampler window + time-stretching (maintainer agreed: built-in channel window, FL-like, not
  a plugin). Modes Off / Resample / Stretch (FL's others are proprietary élastique/E3), original
  tempo, pitch. Stretch = Signalsmith Stretch 1.4.0 (MIT), offline in `StretchCache`, resampled
  meanwhile. Audio clips default to Stretch at the drop tempo. Clicking a sample channel's name
  toggles its Sampler (FL); double-click an audio clip opens it. Left-edge trim
  (`Clip::offsetSteps`). Verified by export: 2 s 110 Hz clip at 130 -> 160 BPM: Stretch 1.63 s
  at 110 Hz, Resample 1.63 s at 135.4 Hz.

- 2026-10-17: Projects are `.jlp` (like FL's `.flp`); `.jls` still opens, and saving one writes
  a `.jlp` beside it instead of overwriting the original.

- 2026-10-18: Piano Roll editing keys (FL's): Shift+arrows move the selection (a semitone / the
  snap), Ctrl+B duplicates (selection or all notes; Playlist: selected clips) right after its
  block without touching the clipboard, Shift+D shortens to a step. Clicking a note selects it
  (Shift+click toggles); right-click also deselects. So **tempo moved again, to Alt+Shift+Up/Down**
  (fine stays Ctrl+Shift+Up/Down), and keys with Shift no longer play typing-keyboard notes.
  Ctrl+Enter loads the Browser's sample into the selected channel from any focus area.
  The Channel Rack's stacking follows the focus: focusing (or showing) the Playlist / Piano Roll
  puts it behind, F6 brings it back. Fixed: dragging the rack up got stuck because JUCE's
  `mouseDownPosition` is relative to the component's *current* position, so after moving up the
  press no longer lay in the title bar; the rack also couldn't be dragged under the transport
  bar where its title bar was out of reach (now kept inside the area below it).

- 2026-10-18: **Split by channel**, done better than FL (maintainer's brief): FL stacks the new
  clips on top of each other and the parts come out at different sizes when channels loop. Here
  (`Project::splitByChannel`): one part per channel with notes, the original removed (maintainer's
  follow-up; FL keeps it); each part as
  long as the original (looping channels' repeats written out with `fillLoop`, plus the new
  `Pattern::minLengthSteps`, saved as `minPlayLength`); each part's clips on their own track,
  the nearest free one below the original's for every clip of the pattern on that track (tracks
  added if needed). A test renders a song before and after and compares it sample by sample.
  Patterns menu and right-click in the picker.

- 2026-10-18: Own window frame (backlog item, FL-like): the main window has no system title bar;
  `ui::TitleBar` holds the menus (only as wide as they are, so the rest drags the window), the
  name and the window buttons. Maximize uses JUCE's setFullScreen (the screen's usable area; a
  real maximize on Windows). Dialogs and separate plugin windows use LookAndFeel-drawn title
  bars. macOS keeps the native title bar and menu bar. On Hyprland, tiled windows ignore move /
  maximize / minimize requests (the compositor decides), which is expected.

- 2026-10-18: All UI text one pixel larger (12 -> 13 px in panels, 13 -> 14 px in the transport
  bar, and so on), the first step against soft text on Windows; the rest is an open question in
  roadmap.md. **Reverted** the same day: the maintainer found the UI too big (Settings >
  Interface size stays for anyone who wants it larger). **No VST2 hosting**: it needs Steinberg's VST2 headers (no longer licensed) or a
  clean-room reimplementation; not needed for now (Serum 2 is VST3; 32-bit-only plugins would
  need a bridge anyway).

- 2026-10-18: Playhead artifacts (Piano Roll, Playlist): the timer repainted strips around the
  position it computed, but the paint asked the transport again a moment later and drew the line
  where playback had got to, outside the cleared strip. Now the paint draws it where the timer
  recorded it (`shownPlayheadX()`). Laggy panel dragging: measured on Xvfb with temporary paint
  timers, ~22 fps because the Sampler panel was redrawn from scratch every frame (~9 ms); floating
  panels and the Channel Rack are now cached images (`setBufferedToImage`), so a drag only copies
  them. Piano Roll: white-key rows (no scale) and scale rows share one lighter shade, black /
  out-of-scale rows are darker, grid lines are lighter so they show on both; clicking a note
  makes new notes that long (FL).

- 2026-10-18: From the maintainer's FL-vs-JLS comparison on Windows:
  - **Loud and gritty:** a busy default beat peaked at +4.6 dBFS before the Master's Soft
    Clipper, whose knee 0.5 also saturated everything above about -6 dB. Built-in drums are now
    ~7 dB quieter (`headroom` in drum_synth.cpp: the same beat peaks at -2.3 dBFS, the clipper
    doesn't touch it) and new projects get knee 0.1. The Sampler's linear interpolation became
    4-point cubic. Saved projects keep their clipper settings.
  - **Cut itself + cut groups** (FL's Cut / Cut by), per sample channel; cuts fade over 64
    samples.
  - **Envelope:** FL-style DAHDSR with an on/off switch (maintainer's choice); off = one-shot
    as before, on = note length matters (release at the note's end; live notes at note-off).
  - **Delete** with the Channel Rack focused deletes the selected channel; **D** in the
    channel menu deletes it (`ui::MenuKeys` watches keys while a menu is open, since JUCE's
    menus ignore letters).
  - **Blurry text** (Windows, 100 % scaling): JUCE draws text with grayscale anti-aliasing
    (no ClearType), which we can't change; Settings › Interface size (100-150 %) scales the
    whole UI.
  - CI's automatic runs are off for now (maintainer's choice); manual dispatch still works.

- 2026-10-18: The Sampler and plugin editors float **inside** the main window (maintainer's
  choice, FL-like; separate OS windows caused the focus bug, Hyprland placing them, and couldn't
  stack with the panels). `ui::FloatingPanel` (shared header, close, drag, kept below the
  transport bar). Plugins keep a fallback, Settings › Plugin windows = Separate windows. Found
  on Xvfb: `XEmbedComponent` doesn't follow an ancestor's move (call `updateEmbeddedBounds()`),
  the panel must be placed before it's added (or the native window shows up at 0,0 first), and
  the window is repainted once the editor has settled. Plugin panels stay on top: JUCE can't
  paint over a native child window. Ctrl+Up/Down in the Piano Roll (focused) moves the
  selection an octave; in the Browser they still load the previous / next sample.

- 2026-10-18: Ctrl+click or right-click a channel's light = solo (`Project::soloChannel`, FL's
  toggle: again unmutes all; audio-clip channels untouched). Channel menu › Fill every 1/2/4/8
  steps **replaces** the steps (maintainer's choice) across the shown grid; disabled for melodic
  channels. Clicking the output display switches to a spectrum (own FFT in the engine, log axis
  30 Hz-16 kHz, -72-0 dB, peaks fall 1.5 dB per frame), remembered as `scopeMode`. Shortcuts
  now also work in the Sampler and plugin windows (`forwardShortcuts`: the key-mapping set as
  a key listener), and the main window takes the keyboard at startup: opening a channel's
  window used to leave Ctrl+A / Ctrl+Tab dead until the main window was clicked. Follow-up: on
  Hyprland (focus follows the mouse) moving back to the main window focused it without a click
  while JUCE's focused component stayed in the Sampler, so keys were dropped. Now the main window
  forwards shortcuts too, and the 30 Hz safety net takes the keyboard back whenever our window
  is focused but JUCE's focus is elsewhere. Reproduced on Xvfb with `xdotool windowfocus`.

- 2026-10-18: Oscilloscope at the right of the transport bar (from the mockups): the engine copies
  the final output (mono mix) into `engine::ScopeBuffer`, a ring of atomics (lock-free, no
  allocation); `ui::Oscilloscope` reads the newest 4096 samples at 30 Hz, starts at the last
  rising zero crossing (`findScopeTrigger`) and draws ~43 ms in the second accent colour. It
  takes the room left in the bar and is the first thing hidden when the window is narrow.

- 2026-10-18: Right-click / right-drag over greyed steps only acts on looped *notes*; empty greyed
  steps stay looped (they used to make the whole loop real). Plugins and Audio settings dialogs
  framed like the others (`ui::DialogFrame`), with themed table headers, check boxes and level
  meter. Note: erasing every looped note of a bar leaves that bar empty, so (by the "only what
  has notes plays" rule) the channel loops its shorter part again and the copies reappear.

- 2026-10-18: Clicking a greyed (looped) step no longer toggles it: left-click makes the loop
  real and sets the step on (a looped note stays), right-click makes it real and sets it off.
  Dialogs and the Sampler window restyled as forms (`ui::FormPanel`, `NumberField` replacing
  JUCE's -/+ sliders, dialog footers, key chips in the shortcuts window). After comparing with
  the mockups: real **Medium / SemiBold** font files (cut from the variable fonts with fontTools,
  picked by style in `LookAndFeel::getTypefaceForFont`; the double-drawn titles are gone) and
  docked panels drawn as **cards** (rounded, bordered, 8 px apart, the focused one's border in
  the accent colour).

- 2026-10-18: UI polish, maintainer picked mockup **option A (compact)**: one control height
  per area (panel headers 36 px with 26 px controls; transport 32 px), drawn icons instead of
  typed symbols, segmented PAT | SONG and 16 | 32 | 64, drop-downs styled like buttons,
  recessed display boxes for tempo and clock (IBM Plex Mono, bundled, OFL), one shared panel
  header (`ui/controls`). The transport bar sheds labels, then the clock's time, then the time
  signature when narrow. New built-in theme **Blackberry** (near-black, pastel lavender / mint
  / pink, Outfit). Titles are drawn twice half a pixel apart for a semibold look, since the
  bundled fonts are Regular only. Fixed on the way: the effect panel had the same stuck-drag
  bug as the Channel Rack.

- 2026-10-18: Playlist: clicking a clip selects it, Shift+click toggles, right-click also
  deselects (all like the Piano Roll); double-clicking a
  pattern clip (or a pattern in the picker) selects it and brings the Channel Rack to the front
  (FL); double-clicking an audio clip in the picker opens its Sampler. "Split by channel" is in
  the backlog.

- 2026-10-18: First Windows build since the early milestones (maintainer wanted an .exe to try):
  `gh workflow run ci.yml -f platforms=windows` builds only Windows and uploads a single
  self-contained `.exe` (static MSVC runtime; imports only system DLLs). Fixes it needed: the
  Signalsmith headers as system includes (also what had broken Linux CI since the
  time-stretching commits: Ubuntu's GCC flagged `-Wpedantic` in them), C4324 disabled around
  `SpscQueue`'s intended `alignas(64)` padding, missing `<algorithm>` includes, JACK only on
  Linux. All tests pass on Windows. Development stays Linux-first; dispatch Windows only when
  needed.

- 2026-10-29: The sandbox moved from Arch to **Ubuntu 26.04 LTS** (the maintainer's desktop is
  now Ubuntu 26.10). A binary runs on the glibc it was built against and newer, so building on
  the LTS gives a `dist/` binary for 26.04, 26.10 and later (and most Ubuntu users), where
  building on 26.10 would shut 26.04 out. Same GCC (15.2) either way; the LTS image changes less
  and is supported until 2031. Surge XT isn't in Ubuntu, so it comes from its pinned official
  `.deb`. Docker 29's BuildKit refuses `docker build --network bridge`, so `sandbox.sh` passes
  `--network` to builds only for `host`.

- 2026-10-29: **A professional look; themes are colours only** (maintainer's choices from a look
  study: real screenshots of each idea behind prototype switches, judged before / after). The UI
  read as playful because of its structure as much as its colours: rounded cards floating on a
  coloured background, a loud focus frame, completely flat controls, phone-style faders. Kept:
  - **Docked panels** edge to edge with 2 px seams, square corners, the transport bar as a
    full-width strip; focus is a 2 px accent line along the panel's top instead of a frame.
  - **Depth**: surfaces lit from above (`fillRaised`: gradient + top highlight), recessed
    displays (inner shadow), a sheen on headers, a shadow under the floating Channel Rack.
  - **Console controls**: faders with a groove, scale ticks and a rectangular cap; knobs with a
    raised body inside a thin value ring.
  - **LED step keys**: the key stays grey, a small LED on it lights (picked over pads, flat
    squares, velocity bars and nine other variations; familiar to FL users).
  - **Themes set colours only.** Shapes (3 px controls, 4 px floating panels, 2 px steps) and
    the font (IBM Plex Sans) are fixed: the maintainer found rounded themes (Juicy) didn't fit,
    and one look is easier to keep consistent. Theme format version 2; version 1 files still
    import, their `font` / `shape` / `effects` skipped quietly. The `shadow` colour now colours
    the rack's shadow.
  - **Built-ins: Blackberry (default) and Graphite** (neutral grey, blue accent, amber
    displays; the maintainer didn't want orange). Juicy, Citrus, Watermelon and Blueberry are
    gone, with the Bricolage Grotesque, Sora and Outfit fonts; a saved choice of one falls back
    to Blackberry.
  Not taken up yet: denser controls, flush mixer strips, per-track colours (prototyped on the
  `look-experiments` branch).

- 2026-10-29: **Channel Rack steps are always full size** (maintainer: 32 / 64 squeezed the
  buttons). A long pattern makes the rack wider, past the window's edge, as a floating window
  would; the header's controls stay at the right edge of the visible part (re-laid out when the
  rack moves or the window resizes), so 16 | 32 | 64 and close stay reachable. Supersedes the
  2026-10-17 "steps shrink to fit (20 -> 8 px)". Dragging from the Playlist's picker shows a chip
  with the item's name instead of JUCE's default snapshot of the whole picker.

- 2026-10-29: **Inter, 1 px larger** (maintainer: text wasn't legible enough). Compared on 1:1
  screenshots and in the app (prototype switches): every size +1 / +2 px, Medium body text with
  brighter dim text, Inter at the same size and Inter +1 px. Inter +1 px won: Inter is drawn for
  screens with a taller x-height, so with JUCE's greyscale anti-aliasing it stays clear at small
  sizes, and +1 px (13 px in panels, 14 px in the transport bar) fits the denser layout, unlike
  the 2026-10-18 attempt with the old one. Control sizes are unchanged. IBM Plex Sans is gone;
  IBM Plex Mono stays for numbers. Inter is OFL, bundled as static instances cut at the small
  optical size (opsz 14).
  Then a **type scale**: the 49 literal sizes (ten different ones, some half a pixel apart) became
  five named sizes in `ui::metrics`, so a change like this is one line next time. Half-pixel
  strays snapped to their neighbours (picker and form rows 13.5 -> 14 px, the rest unseen).

- 2026-10-29: **Clicking a looped step is a setting** (maintainer's request): Options › Settings ›
  Clicking a looped step, stored as `loopedStepClick`. *Discards the loop (FL)*, the default:
  only the clicked channel's repeats go, its loop grows to include the new step, the bars in
  between are empty, other channels keep repeating; right-click and erase-drags leave greyed
  steps alone (with Loop on, dropping copies without adding a note would only bring them back).
  *Copies the loop in*: the 2026-10-18 behaviour, unchanged. The setting is an app preference,
  not project data, like the other Settings.

- 2026-10-29: **Window buttons in the transport bar** (maintainer's request and choices from
  mockups): Playlist, Channel rack, Piano roll, Mixer and Browser, in shortcut order, right-aligned
  next to the oscilloscope. They run the window commands. An **LED** on each shows the Ctrl+Tab
  order (brightest for the focused area, 60 / 35 / 18 % further along, dark when hidden; the same
  LED as the step keys), picked over a colour tint, underlines and numbers. Icons are drawn like
  the others (no image files, nothing from FL). Labels only when the oscilloscope still fits
  beside them (the maintainer wanted the buttons next to it), so a 1600 px window shows icons and
  a wider one icons and labels.

- 2026-10-29: **Piano Roll look** (maintainer's pick from four prototype designs, "A"): the pure
  white keys with every key labelled and the high-contrast striped grid felt rough. Now: ivory
  keys (from the theme's text colour; the black keys in its background colour), labels only on C
  and the scale's root, plus the key under the mouse, on the keyboard or its row in the grid
  (maintainer's idea); dark keys as a setting (Settings › Keys). A quiet grid: rows a touch apart,
  the root faintly tinted, line weights by bar / beat / step, all moved towards the theme's text
  colour so they work on any theme. Notes lit from above (`fillRaised`), duller and darker when
  soft, a dark label only from 34 px wide; the velocity lane with guides and the notes' colours.

- 2026-10-29: **On/off switches are lights** (maintainer's pick: "the dot like in the Channel
  Rack", over LED keys, LED + label, a sliding switch, a power symbol and a check box). The
  Sampler's Cut itself and Volume envelope and the effect panel's bypass were buttons labelled
  "On" whether on or off. They're now `LedToggle`s: the same round light as a channel's mute
  light (`paintLed`, which the mute light uses too), lit in the theme's `led` colour when on.

- 2026-10-29: **Note edits are silent while playing** (maintainer's request): clicking, adding or
  dragging notes in the Piano Roll previews them only while stopped or paused
  (`PianoRollView::previewNote`), and a preview still sounding stops as soon as playback starts
  (the view's timer). Clicking a keyboard key still sounds while playing: that's a deliberate "let
  me hear this note".

- 2026-10-29: **Selecting in the Piano Roll and the Playlist** (maintainer's requests). A click in
  empty space with a selection now deselects *and* adds a note / places a clip, reversing the
  2026-10-17 "only deselects, as in FL". Ctrl+click without dragging (under 4 px) toggles the
  note or clip under the mouse in the selection, like Shift+click; it used to start an empty
  rubber band, which cleared the selection.
