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
| Ctrl+T | Typing keyboard to piano on/off (the **Keys** button) |
| Z S X D C V G B H N J M | Play C5 to B5 on the selected channel (when Keys is on) |
| Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P | Play C6 to E7 |
| Ctrl+N / Ctrl+O | New project / open project |
| Ctrl+S / Ctrl+Shift+S | Save / save as |
| Ctrl+R | Export the song as a WAV file |
| Ctrl+Z | Undo |
| Ctrl+Y or Ctrl+Shift+Z | Redo |

On macOS use Cmd instead of Ctrl.

**Playing live.** With **Keys** on (the default), the letter rows play the selected channel like
a piano, as in FL: the bottom row is the white and black keys from C5, the top row continues
from C6. Shortcuts with Ctrl still work (Ctrl+Z undoes, it doesn't play a note), and L stays
the PAT/SONG switch. **MIDI keyboards** are picked up automatically when plugged in (also
while the app runs) and play the selected channel too, on any MIDI channel, with velocity,
sustain pedal, mod wheel and pitch bend. Switch devices off in Options › Audio settings. Live
notes play instruments and samples alike, also while the transport is stopped; they aren't
recorded yet.

## Projects and export

- **File › Save** writes a `.jls` project file. The title bar shows the project name, with `*`
  when there are unsaved changes. You're asked before unsaved changes would be lost (New, Open,
  Quit).
- **Samples:** projects reference your sample files instead of copying them. Keep samples in a
  folder next to the project to move or share them together; they're found there first.
- **Missing samples:** if a project's samples can't be found, the channels stay silent and you
  get a list of what's missing.
- **File › Export WAV** (Ctrl+R) renders the song (or the current pattern, if the Playlist is
  empty) to a 24-bit stereo WAV, plus 2 seconds for reverb and delay tails. The metronome is
  never included.

Details of the file format: [project-format.md](project-format.md).

## Transport bar

- **PAT / SONG:** PAT loops the selected pattern; SONG plays the Playlist and loops at its end.
- **Pattern selector and +:** picks the pattern you're editing and placing; **+** adds one.
- **Play:** play/pause (keeps the position). **Stop** rewinds to the start.
- **Tempo:** - / + change it by 1 BPM; click the number to type an exact value (up to three
  decimals).
- **Position:** bar:beat:step within whatever is looping.
- **Clock** (below the position): current time / total time, as minutes:seconds.milliseconds.
  In SONG mode that's the song (the Playlist); in PAT mode, the selected pattern.
- **Time signature:** beats per bar (n/4).
- **Metronome:** on/off.
- **Keys:** typing keyboard to piano on/off (Ctrl+T).

Plugins follow the tempo, time signature and position, so arpeggiators, tempo-synced delays
and LFOs stay in time with the song. In PAT mode they see the pattern as a loop, in SONG mode
the whole song.

## Channel Rack

| Do this | To |
|---|---|
| Left-click / drag over steps | Turn steps on (or off, if you started on a lit step) |
| Right-click / drag over steps | Turn steps off |
| Click a channel name | Select the channel (the Piano Roll edits it). For a plugin instrument, also shows or hides its window |
| Right-click a channel name | Piano roll, show plugin, rename, replace sample, use/replace an instrument plugin, delete |
| Green light | Mute / unmute |
| Pan and volume knobs | Drag up/down; double-click resets |
| Mixer track box | Drag up/down or scroll to route the channel to an insert ("--" = Master) |
| Drop audio files on a row | Replace that channel's sample |
| Drop audio files below the rows | Add channels |
| Click + | Add a channel: **Sample…** or **Instrument ›** (a plugin synth) |
| Click a note preview | Open the Piano Roll for that channel |
| 16 \| 32 \| 64 (title bar) | How many steps the rack shows |
| Drag the handle after the last step | Show more or fewer steps, 4 at a time |
| **Loop** (title bar) | Loop on/off for the whole rack |
| Circular arrow next to a name | Loop on/off for that channel |

**Pattern length and looping.** 16 | 32 | 64 and the handle only set how many steps you see.
The pattern *plays* as far as its notes go, rounded up to whole bars: with notes only in the
first 16 steps it plays 16 steps and starts over, even when 32 are shown. Steps past that are
greyed and show what will be heard there (the repeat). Clicking a greyed step makes that bar
real: the repeat is copied in first, so nothing you hear changes, and the pattern now plays up
to there. With **Loop** on (the default), a channel whose notes are shorter than the pattern
repeats them: its notes are rounded up to 1, 2, 4, 8... bars, and that block plays again and
again. Example: a 4-bar bass line with a 1-bar drum beat gives 4 bars of drums. Those repeats
are greyed too. Because the loop length comes from the notes, a looping channel can't end with
empty bars; switch its loop off (the arrow) to have it play once per pattern. Long patterns
shrink the step buttons to fit.

**Instrument plugins (VST3 / LV2).** Scan for plugins first (Options › Plugins…). Then add one
with **+ › Instrument**, or turn an existing channel into one from its right-click menu. Its
window opens right away; click the channel name to show or hide it later. Instrument channels
play their notes like any channel: steps play C5 for one step, and the Piano Roll sets pitch,
length and velocity (note length matters here, unlike for samples). Volume, pan, mute and the
mixer track apply after the plugin. Notes are always released: when a note ends, when its
clip ends, and when you stop or pause. Settings are saved in the project; if the plugin isn't
installed, the channel stays silent and keeps them.

## Playlist

| Do this | To |
|---|---|
| Left-click empty space | Place the selected pattern (snapped), and keep dragging to move it |
| Drag a clip | Move it in time or to another track |
| Drag a clip's right end | Make it longer (the pattern repeats) or shorter |
| Right-click / drag | Delete clips |
| Ctrl + mouse wheel | Zoom |
| Snap (top right) | Bar, beat or step |
| Click / drag the ruler | Move the playhead there (snapped); switches to SONG mode |
| Ctrl + drag | Select the clips in a rectangle (outlined) |
| Drag a selected clip | Move all selected clips together |
| Delete / Ctrl+A / Esc | Delete the selected clips / select all / deselect |

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
| Click / drag the ruler | Move the playhead there (snapped); switches to PAT mode |
| Ctrl + drag | Select the notes in a rectangle (outlined) |
| Drag a selected note | Move all selected notes together (its right end resizes them all) |
| Delete / Ctrl+A / Esc | Delete the selected notes / select all / deselect |
| Slice (top right) or Alt+U | Cut the selected notes (all notes if none are selected) into pieces of the chosen size: 1/4, 1/8, 1/8 T, 1/16, 1/16 T, 1/32, 1/64 |
| Click and hold a key on the left | Play that note (slide to play others) |
| Scale (top right) | Highlight a scale: root note + Major / Minor (Off = plain keys) |

The slicer is for rolls (hi-hats, snares): draw one long note, slice it, then shape the pieces'
velocities. The pieces stay selected, so slicing again makes them finer.
With a selection, a click elsewhere only deselects (it doesn't add a note), as in FL.
Clicking a note (or adding one) plays it on the channel for as long as you hold the button;
while you drag it up or down you hear the new pitch.

Every key is labelled; note names follow FL: C5 is middle C (MIDI 60) and plays a sample at its
original pitch. With a scale chosen, its notes' rows are lighter, the others darker (their key
labels faded), and the root's rows are tinted. The scale is saved with the project. Note
length doesn't change the sound yet: samples always play to their end, like FL's default sampler.

## Mixer

- **Strips:** Master plus 16 inserts, each with a stereo meter, fader (double-click = 0 dB),
  pan, **M**ute and **S**olo. Click a strip to select it.
- **Effect slots** (right side, for the selected track):
  - Click **+ Add effect** to choose Filter, EQ, Delay, Reverb, Compressor or Soft Clipper.
  - Click an effect to open its controls; click its light to bypass it.
  - Right-click for edit, bypass, move up/down and remove.
- **Plugins (VST3 / LV2):** first scan for them in **Options › Plugins…** (Options › Scan). Then
  **+ Add effect › Plugins** lists them. Clicking a plugin's slot opens its own window. Its
  settings are saved in the project. If a project uses a plugin that isn't installed, the slot
  shows "(missing)", passes audio through unchanged and keeps the plugin's settings.
- **Soft Clipper on the Master:** new projects start with one, so loud mixes saturate gently
  instead of hard-clipping. It changes nothing below its knee; remove or bypass it if you like.
- **Effect panel:** floating; drag it by its title bar. Knobs drag up/down and double-click
  resets them.

## Themes

**Options › Theme…** offers Juicy, Citrus, Watermelon and Blueberry, and can import or export
your own theme files. See [themes.md](themes.md).
