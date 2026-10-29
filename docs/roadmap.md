# Roadmap

## Milestones

1. [x] Repo skeleton: CMake, JUCE, Catch2, CI on all three OSes
2. [x] Audio device setup + transport (play/pause/stop, tempo, beats per bar)
       + sample-accurate metronome (Ctrl+M)
3. [x] Channel Rack with step sequencer + a built-in sampler
4. [x] Pattern system + Playlist (PAT/SONG modes)
5. [x] Piano Roll
6. [x] Mixer (inserts, built-in effects). Sends between tracks still to do.
7. [x] Plugin hosting: VST3/LV2 **effects** (phase 1) and **instruments** (phase 2), live input
       (typing keyboard + MIDI), play head (tempo/position) for plugins. Still to do: recording
       live input, out-of-process hosting
8. [x] Project save/load (.jlp JSON), WAV export

## Backlog

Agreed features not started yet (newest first). Maintainers decide what's next; ask before
starting one.

- Plugin windows can go to the background (2026-10-29, maintainer's request): today a plugin's
  panel always stays in front of the Playlist, Channel Rack and the rest, because on Linux the
  plugin draws in its own native window that the app can't draw over (user guide, *Windows*).
  Needs a way to lower or hide the native window when another area comes to the front (and to
  restore it), probably per platform.
- Plugin windows in the Ctrl+Tab order (2026-10-29): an open plugin window becomes a stop when
  cycling with Ctrl+Tab (closed ones don't). Depends on the item above, since Ctrl+Tab away from
  a plugin should put it behind. The window buttons' LEDs only cover the five main windows; the
  plugin panel's own focus line can show that it has the focus.
- Typing notes stop after clicking inside a plugin (2026-10-29, maintainer's report with a VST3):
  the plugin's native window takes the keyboard, so the typing keyboard no longer plays notes
  until the main window is clicked. Shortcuts are forwarded from other windows
  (`MainComponent::forwardShortcuts`), but notes are played in `MainComponent::keyStateChanged`,
  which only sees keys while the main window has them. FL has the same problem at times and a
  button to give the keyboard back. Decided: both, the keyboard returns automatically when the
  mouse leaves the plugin's window, and plugin panels' title bars get a button for it.

- Agent CLI (2026-10-18, proposed, postponed): a headless `juicy-loops-cli` (project info,
  render to WAV, validate) and later an opt-in localhost control channel in the app (run
  commands by string id, read project state, load / save, screenshots), possibly wrapped as
  an MCP server.
- Earlier, deferred: recording live input into the Piano Roll; mixer sends; FL's undo scheme.

## Known gaps

Smaller shortcomings, gathered from the "Known gaps" notes in `decisions.md` and checked against
the code on 2026-10-29 (the rest of those notes are done, or superseded like pattern length,
which follows the notes). Not agreed work yet: candidates for the backlog.

- **Channel Rack:** no reordering channels (FL: Alt+↑ / ↓); one selected channel only.
- **Playlist:** tracks have no names, mute or solo; no way to add tracks by hand.
- **Mixer:** no renaming tracks; a fixed 16 inserts; no parameter smoothing (fast fader or
  knob moves can click); the Delay's time is in ms only (no tempo sync).
- **Samples:** dropping or replacing samples and opening projects decode on the UI thread (only
  the Browser decodes in the background); samples are cut at 2 minutes.
- **Files:** export blocks the UI and holds live audio; no recent files; no autosave; no
  "collect samples into the project folder".
- **Plugins:** hosted in-process, so a crashing plugin takes the app down.
- **Soft Clipper:** no oversampling (heavy drive on bright material can alias).

## Open questions

To discuss before working on them.

- **Text sharpness on Windows** (2026-10-18). The maintainer finds our text blurrier than FL's
  at 100 % display scaling. Cause: JUCE draws text with grayscale anti-aliasing; FL gets
  Windows' ClearType (subpixel) rendering. Done so far: real Medium / SemiBold weights, Settings
  > Interface size, and (2026-10-29, also for Linux) Inter instead of IBM Plex Sans with all UI
  text 1 px larger, picked over larger sizes and heavier weights from 1:1 screenshots. Options
  left:
  - a setting (not a theme option: themes are colours only since 2026-10-29) to use the system
    UI font (Segoe UI on Windows), which is hinted for small sizes on screen
  - patching JUCE's Direct2D text rendering to use ClearType where the background is opaque
    (the real fix for the softness; needs several Windows builds to test, and ClearType can
    fringe on semi-transparent areas)

