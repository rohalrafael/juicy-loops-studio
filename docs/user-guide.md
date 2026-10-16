# User guide

A quick reference to what's there so far, aimed at people who know FL Studio. Shortcuts and
mouse actions follow FL wherever FL has one.

## Windows

| Window | Key | What it's for |
|---|---|---|
| Playlist | F5 | Arrange patterns into a song |
| Channel Rack | F6 | Channels and their steps; floating, drag it by its title bar |
| Piano Roll | F7 | Notes of the selected channel in the current pattern |
| Mixer | F9 | Insert tracks, levels and effects (docked at the bottom) |

The Playlist and Piano Roll share the main area; F5 and F7 switch between them.

## Keyboard

| Key | Action |
|---|---|
| Space | Play / stop (stop rewinds) |
| L | Switch between pattern (PAT) and song (SONG) mode |
| F4 | New pattern |
| F5 / F6 / F7 / F9 | Playlist / Channel Rack / Piano Roll / Mixer |
| Ctrl+M | Metronome on/off |
| Ctrl+Z | Undo |
| Ctrl+Y or Ctrl+Shift+Z | Redo |

On macOS use Cmd instead of Ctrl.

## Transport bar

- **PAT / SONG:** PAT loops the selected pattern; SONG plays the Playlist and loops at its end.
- **Pattern selector and +:** picks the pattern you're editing and placing; **+** adds one.
- **Play:** play/pause (keeps the position). **Stop** rewinds to the start.
- **Tempo:** click the arrows, or click the number and type a value.
- **Position:** bar:beat:step within whatever is looping.
- **Time signature:** beats per bar (n/4).
- **Metronome:** on/off.

## Channel Rack

| Do this | To |
|---|---|
| Left-click / drag over steps | Turn steps on (or off, if you started on a lit step) |
| Right-click / drag over steps | Turn steps off |
| Click a channel name | Select the channel (the Piano Roll edits it) |
| Right-click a channel name | Piano roll, rename, replace sample, delete |
| Green light | Mute / unmute |
| Pan and volume knobs | Drag up/down; double-click resets |
| Mixer track box | Drag up/down or scroll to route the channel to an insert ("--" = Master) |
| Drop audio files on a row | Replace that channel's sample |
| Drop audio files below the rows, or click + | Add channels |
| Click a note preview | Open the Piano Roll for that channel |

## Playlist

| Do this | To |
|---|---|
| Left-click empty space | Place the selected pattern (snapped), and keep dragging to move it |
| Drag a clip | Move it in time or to another track |
| Drag a clip's right end | Make it longer (the pattern repeats) or shorter |
| Right-click / drag | Delete clips |
| Ctrl + mouse wheel | Zoom |
| Snap (top right) | Bar, beat or step |

## Piano Roll

| Do this | To |
|---|---|
| Left-click empty space | Add a note (same length as the last one), then drag to move it |
| Drag a note | Move it in time and pitch |
| Drag a note's right end | Change its length (new notes use it too) |
| Right-click / drag | Delete notes |
| Velocity lane (bottom) | Drag a stem up/down to set velocity |
| Ctrl + mouse wheel | Zoom |
| Snap (top right) | Beat, step, ½ step, ⅓ step or none |

Note names follow FL: C5 is middle C (MIDI 60) and plays a sample at its original pitch. Note
length doesn't change the sound yet: samples always play to their end, like FL's default sampler.

## Mixer

- **Strips:** Master plus 16 inserts, each with a stereo meter, fader (double-click = 0 dB),
  pan, **M**ute and **S**olo. Click a strip to select it.
- **Effect slots** (right side, for the selected track):
  - Click **+ Add effect** to choose Filter, EQ, Delay, Reverb or Compressor.
  - Click an effect to open its controls; click its light to bypass it.
  - Right-click for edit, bypass, move up/down and remove.
- **Effect panel:** floating; drag it by its title bar. Knobs drag up/down and double-click
  resets them.

## Themes

**Options › Theme…** offers Juicy, Citrus, Watermelon and Blueberry, and can import or export
your own theme files. See [themes.md](themes.md).
