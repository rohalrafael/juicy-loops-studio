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

- Agent CLI (2026-10-18, proposed, postponed): a headless `juicy-loops-cli` (project info,
  render to WAV, validate) and later an opt-in localhost control channel in the app (run
  commands by string id, read project state, load / save, screenshots), possibly wrapped as
  an MCP server.
- Earlier, deferred: recording live input into the Piano Roll; mixer sends; FL's undo scheme.
