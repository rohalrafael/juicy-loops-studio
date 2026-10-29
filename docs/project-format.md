# Project file format (.jlp)

A project is one JSON file (`.jlp`; `.jls` before 2026-10-17, same format), readable and
diff-friendly. Samples aren't embedded: a project
references them, like FL Studio does. Code: `src/model/project_file.{h,cpp}`.

## Example (shortened)

```json
{
  "format": "juicy-loops-studio-project",
  "version": 1,
  "tempo": 130.0,
  "beatsPerBar": 4,
  "loopChannels": true,
  "channels": [
    { "id": 1, "name": "Kick", "sample": { "builtin": "kick" },
      "volume": 0.8, "pan": 0.0, "muted": false, "mixerTrack": 1 },
    { "id": 5, "name": "Vox", "sample": { "path": "/home/me/songs/samples/vox.wav",
                                          "relativePath": "samples/vox.wav" },
      "volume": 0.8, "pan": 0.0, "muted": false, "mixerTrack": 5 },
    { "id": 6, "name": "Surge XT", "instrument": { "id": "VST3-Surge XT-...", "name": "Surge XT",
                                                    "state": "VkMyIQ..." },
      "volume": 0.8, "pan": 0.0, "muted": false, "mixerTrack": 6 }
  ],
  "patterns": [
    { "id": 1, "name": "Pattern 1", "gridLength": 16,
      "notes": [ { "channel": 1, "notes": [[0, 24, 60, 0.78125], [96, 24, 60, 0.78125]] } ] }
  ],
  "playlist": { "tracks": 16, "clips": [ { "pattern": 1, "track": 0, "start": 0, "length": 16 } ] },
  "mixer": { "tracks": [
    { "name": "Master", "volumeDb": 0.0, "pan": 0.0, "width": 1.0, "muted": false, "solo": false,
      "effects": [] },
    { "name": "Insert 1", "volumeDb": -3.0, "pan": 0.0, "width": 1.0, "muted": false, "solo": false,
      "effects": [ { "id": 1, "type": "reverb", "enabled": true,
                     "params": { "Size": 0.6, "Damping": 0.4, "Width": 1.0, "Mix": 0.25 } } ] }
  ] },
  "view": { "currentPattern": 1, "selectedChannel": 1, "selectedMixerTrack": 1, "songMode": false,
            "scale": "minor", "scaleRoot": 9 },
  "nextIds": { "channel": 7, "pattern": 2, "effect": 2 }
}
```

## Fields

| Field | Meaning |
|---|---|
| `format`, `version` | Always `"juicy-loops-studio-project"`; format version (currently 1) |
| `tempo` | BPM, 10 to 999 |
| `beatsPerBar` | Time signature numerator (n/4), 1 to 16 |
| `loopChannels` | The Channel Rack's Loop switch (default `true`) |
| `channels[]` | In Channel Rack order. `id` is referenced by patterns. `mixerTrack` is 0 for Master, otherwise an insert |
| `channels[].sample` | `{"builtin": "kick"/"clap"/"hat"/"snare"}`, or `{"path": absolute, "relativePath": relative to the project file}`, or `null` |
| `channels[].audioClip` | `true` for the channel behind audio clips (hidden in the Channel Rack); its `sample` is what the clips play |
| `channels[].sampler` | Sampler settings when not default: `stretch` (`off` / `resample` / `stretch`), `originalTempo` (BPM, 0 = not set), `pitch` (semitones) |
| `channels[].loop` | Whether the channel repeats its notes when the pattern is longer (default `true`) |
| `channels[].instrument` | Instead of `sample`, for a channel played by a plugin: `{"id": host identifier, "name": display name, "state": base64 of the plugin's own state}`. Kept even if the plugin is missing |
| `patterns[]` | In pattern-selector order. `gridLength`: steps the Channel Rack shows (older files: `minLength`); the pattern plays as far as its notes, in whole bars. `minPlayLength` (optional, steps): the shortest it plays, set by Split by channel |
| `patterns[].notes[]` | Per channel: a list of `[start, length, pitch, velocity]`. Start and length are in ticks (96 per beat, 24 per step); pitch is MIDI (60 = C5); velocity is 0 to 1 |
| `playlist.clips[]` | `start` and `length` in steps. Pattern clips have `pattern` (a clip longer than its pattern repeats it); audio clips have `audio`, the audio-clip channel's id, and `offset` (steps into the sample, from a left-edge trim) |
| `mixer.tracks[]` | Index 0 is the Master, then inserts 1 to 16. `volumeDb` runs from -60 (silence) to +6. `width` is the stereo width, 0 (mono) to 2 (twice as wide); 1, the default for older files, leaves the sound as it is |
| `mixer.tracks[].effects[]` | In processing order. `type` is an effect id (`filter`, `eq`, `delay`, `reverb`, `compressor`, `softclip`); `params` are by name, in real units |
| `effects[].plugin` | For `"type": "plugin"`: `{"id": host identifier, "name": display name, "state": base64 of the plugin's own state}`. Kept even if the plugin is missing |
| `view` | Selections, PAT/SONG mode, `showAudioClips` (Channel Rack filter) and the Piano Roll scale (`scale`: `none`/`major`/`minor`, `scaleRoot`: 0 = C ... 11 = B), restored on open |
| `nextIds` | Id counters for new channels, patterns and effects |

## Loading rules

- **Rejected:** only files that aren't JSON or don't carry the `format` tag.
- **Repaired, with a warning shown after opening:**
  - values out of range are clamped
  - unknown effects, notes for missing channels, and clips for missing patterns or audio-clip
    channels are skipped
  - duplicate ids are dropped
- **Samples** are looked up next to the project first (`relativePath`), then at the absolute
  `path`, so a project folder with its samples can be moved or shared. A missing sample leaves
  the channel silent but keeps its reference, so saving again doesn't lose it.
- **Effect parameters** are matched by name. A missing parameter gets its default, so new
  parameters can be added to an effect without breaking old projects.
- **Newer versions:** a newer `version` still loads, with a warning that some parts may be missing.
