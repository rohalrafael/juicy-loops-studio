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

- A GitHub README that shows the app off (2026-10-30, maintainer's request): what it does, why
  it feels natural to FL users, what it adds, with screenshots, including the themes. Steps:
  1. A showcase project: a full, good-looking beat (original, our own sounds) to photograph.
  2. A screenshot script (sandbox, Xvfb, fixed window size) that regenerates every README image
     from that project, so the images never go stale; small WebP files in `docs/images/`.
     Done: `tools/screenshots.sh` (2026-10-30); images not taken for real yet.
  3. More themes (direction to agree first), and a theme gallery image. Themes done (seven
     built in, 2026-10-30); the gallery is the script's `themes` scene.
  4. The README itself: hero shot, "Coming from FL Studio" (same concepts, same shortcuts),
     what's different or extra (each claim about FL checked against its current manual),
     install, build. FL named only descriptively, with the existing not-affiliated note.
  Maintainer's answers: images in the repo are fine; the README stresses how familiar it is to
  FL users and where it makes things easier (scale highlighting, Ctrl+Tab, key bindings) rather
  than what FL lacks; themes in a dark, neon, underground trap / EDM direction (black + pink,
  white + baby blue as starting points, drafts shown 2026-10-30); no final screenshots until the
  maintainer's remaining UI changes are in.

- Rethink Ctrl+Tab (2026-10-29, maintainer's note, to discuss): the whole focus / Ctrl+Tab
  model deserves a fresh look before adding more to it. (The window buttons' LEDs now just show
  which windows are open, 2026-10-30.)
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

