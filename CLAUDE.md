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
src/app/      JUCE app shell: window, transport bar, MainComponent (owns Document + Engine)
src/engine/   Real-time, JUCE-free: Engine (mixer buses), Transport, StepSequencer, Sampler,
              Metronome, DrumSynth, effects (Filter/EQ/Delay/Reverb/Compressor), EffectPool,
              SpscQueue, EngineState snapshot
src/model/    JUCE-free project data: Project (channels, patterns, playlist), Document (undo)
src/theme/    JUCE-free theme data + JSON theme files (nlohmann/json) + 4 built-in themes
src/ui/       JUCE components: ChannelRack (floating), PlaylistView, PianoRollView, MixerView +
              EffectPanel, ThemeManager +
              LookAndFeel (ui/theme), ThemeDialog
src/io/       JUCE-based file I/O: SampleLoader (WAV/AIFF/FLAC/Ogg/MP3 -> SampleBuffer)
src/plugins/  Plugin hosting + built-in instruments/effects                     [planned]
tests/        Catch2 unit tests (link engine/model only, never JUCE)
```

Targets: `jls_engine`, `jls_model` and `jls_theme` (static libs, plain C++, **no JUCE**), the
`JuicyLoopsStudio` app (compiles `src/app`, `src/ui`, `src/io` with JUCE), `jls_tests`.
Includes are rooted at `src/`, e.g. `#include "engine/transport.h"`. Namespaces: `jls`,
`jls::engine`, `jls::model`, `jls::theme`, `jls::ui`, `jls::io`.
Resources: `resources/fonts/` (bundled OFL fonts, compiled in as `BinaryData`).

Data flow: UI → `model::Document::apply()` (undoable edit) → `onChange` → MainComponent
rebuilds `model::toEngineState()` → `Engine::setState()` pushes it through a lock-free
queue → audio thread adopts it at the start of the next block and sends the old one back;
`Engine::collectGarbage()` (UI timer) deletes it. The audio thread never allocates/frees.
When a state is replaced, the Sampler stops voices whose channel/sample is gone *before*
the old state is retired, so voices never point at freed samples.

Audio callback flow (per block): `Engine::process()` adopts any new state, then
`Transport::advance()` moves the playhead and returns a
`BlockPosition` (the beat span `[startBeat, endBeat)` the block covers). Anything that
must happen "on a beat" (metronome now, sequencer later) takes that span and finds the
exact sample offset, so timing doesn't depend on block size. Engine components render
into raw `float* const*` channel arrays, not JUCE buffers.

Key rules:
- **UI and engine are separate.** The UI edits the model; the engine reads a
  real-time-safe snapshot of it. No direct UI → audio thread calls.
- **No locks, allocation, file I/O or logging on the audio thread.** Use lock-free
  queues/FIFOs to communicate with it.
- The model has undo/redo built in from the start (whole-`Project` snapshots; samples are
  `shared_ptr` so copies are cheap). Every user edit goes through `Document::apply()`;
  continuous gestures (drag-painting steps, turning a knob) pass one gesture id so they
  undo as a single step. Serialization is still to do (roadmap 8).
- UI look comes from the active **theme** (`ui::currentTheme()`); never hard-code colours,
  radii or fonts in components. Read them in `paint()`, since themes switch at runtime
  (ThemeManager calls `sendLookAndFeelChange()` on all windows). Only layout sizes are
  fixed (`ui::metrics`). New themeable properties go in `theme::Theme`, in the parse/serialize
  tables in `theme.cpp`, in all 4 built-in JSONs and in `docs/themes.md`. The user wants a
  say in visual design. Mockups: https://claude.ai/artifact/V8rnfPk2sneMEfNvescNi1
  (private canvas).

## Build & run

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug   # first run fetches JUCE + Catch2
cmake --build build
ctest --test-dir build --output-on-failure
"./build/src/app/JuicyLoopsStudio_artefacts/Debug/Juicy Loops Studio"
```

Options: `-DJLS_BUILD_APP=OFF` (engine + tests only, skips JUCE), `-DJLS_BUILD_TESTS=OFF`.
CI (`.github/workflows/ci.yml`): pushes/PRs build + test on **Linux only** (with ccache);
the full Ubuntu/Windows/macOS matrix runs only on manual dispatch
(`gh workflow run ci.yml`). The repo is private with limited CI minutes (macOS 10×,
Windows 2×), so don't trigger the full matrix casually. Batch commits before pushing,
and run it before milestones or after platform-specific changes.

## Conventions

- C++20, format with `.clang-format` (Allman braces, 4 spaces, 100 cols).
- Engine code builds with `-Wall -Wextra -Wpedantic -Werror` (`/W4 /WX` on MSVC).
- Prefer RAII and value types; no raw `new`/`delete`.
- Names: `PascalCase` types, `camelCase` functions/variables, `snake_case` files.

## Roadmap

1. [x] Repo skeleton: CMake, JUCE, Catch2, CI on all three OSes
2. [x] Audio device setup + transport (play/pause/stop, tempo, beats per bar)
       + sample-accurate metronome (Ctrl+M)
3. [x] Channel Rack with step sequencer + a built-in sampler
4. [x] Pattern system + Playlist (PAT/SONG modes)
5. [x] Piano Roll
6. [x] Mixer (inserts, built-in effects). Sends between tracks still to do.
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
- 2026-10-16: Look: user picked **Juicy** (plum #17121c/#241b2c, blood orange #ff7a45,
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
  Roll, as in FL. Pattern length = `minLengthInSteps` (16), grown to whole bars to fit notes
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

## Dev environment notes

- The user is happy to install tools on request. Ask instead of working around a
  missing tool.
- Installed locally (Arch): cmake, ninja, clang (clang-format/clang-tidy), gh, ImageMagick.
- **UI testing: use `tools/ui-test.sh`** (Xvfb display :99 + xdotool + ImageMagick). The
  user is on **Wayland (Hyprland)**: xdotool can't click the app on the real desktop
  (synthetic events are ignored, and real injection would move the user's pointer), so
  never drive the live session. Test on Xvfb instead, read screenshots, then `stop`/`stop-x`.
  The app uses the real settings file, so reset `theme` to `juicy` after testing themes.
  Without a window manager, JUCE logs harmless `BadAtom` errors and dialogs have no title
  bar.
- In Xvfb nothing has keyboard focus at start (no window manager): click an empty spot of
  the transport bar (e.g. 1050,56) before sending keys.
- On the real desktop Hyprland tiles the window (e.g. 941x501) a few seconds after launch;
  wait about 6 s before taking screenshots there.
- Screenshot the app (XWayland) without capturing the rest of the screen:
  `import -window "Juicy Loops Studio" out.png`, then Read the PNG. Start the app with `&`
  and `kill $!`. **Don't `pkill -f`** with the app path: it matches and kills its own shell.
  To shoot another theme, set `val` in `~/.config/JuicyLoopsStudio/JuicyLoopsStudio.settings`
  before launch and put it back to `juicy` afterwards.
- `grep` here is ugrep and differs from GNU grep for some patterns; use Python for tricky
  text extraction.
- Pushing: Claude may push. Its shell doesn't inherit `SSH_AUTH_SOCK`; the agent socket
  lives in `~/.ssh/agent/`: `SSH_AUTH_SOCK=$(ls -t ~/.ssh/agent/s.* | head -1) git push`.
- Windows/macOS CI is skipped for now (Linux-native first). Don't dispatch the full
  matrix unless the user asks.
