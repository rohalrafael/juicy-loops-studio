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
| Browser | Alt+F8 | Your sample folders, with previews (docked at the left or right) |
| Sampler | (click a sample channel) | A sample's time-stretching and pitch (a floating panel; reopens where you left it) |

The main window has its own title bar, as in FL (Windows and Linux; macOS keeps its native one):
the menus on the left, the project name in the middle, minimize / maximize / close on the right.
Drag the empty part of the bar to move the window, double-click it to maximize or restore, and
drag the window's edges to resize it. Dialogs and separate plugin windows have the same style of
title bar.

Plugin editors and the Sampler float inside the main window: drag them by their title bar,
close them with ×, and click one to bring it to the front. On Linux the windows stack as in FL:
clicking the Playlist, Piano Roll, Mixer, Browser or Channel Rack brings it in front of a plugin
panel, and the plugin comes back with a click on it or on its channel's name (a second click on
the name, while the plugin is in front, closes it). On Windows and macOS plugin panels still
always stay in front (the plugin draws in its own native window). If a plugin
misbehaves inside the app, switch **Options › Settings › Plugin windows** to *Separate
windows*.

Clicking inside a plugin gives it the keyboard (for its own text fields and keys). The app takes
the keyboard back as soon as the mouse leaves the plugin panel, so typed notes and shortcuts work
again without a click; the keyboard button in a plugin panel's title bar does the same while the
mouse is still on the panel.

The Playlist and Piano Roll share the main area; F5 and F7 switch between them. The Channel
Rack floats in front of them while you work in it; clicking the Playlist or Piano Roll (or
opening one with F5 / F7) puts the rack behind it, as in FL. F6 or Ctrl+Tab brings it back to
the front (F6 again hides it).

## Keyboard

**Focus.** The arrow keys and Enter act on the focused area, which shows a line in the accent
colour along its top edge:
the Browser, the Channel Rack, the Playlist / Piano Roll, or the Mixer. Click an area to focus
it, or cycle with **Ctrl+Tab** / **Ctrl+Shift+Tab** through the areas that are showing. Open
plugin panels are stops too: Ctrl+Tab to one brings it to the front (the keyboard stays with the
app; click into the plugin to type in it), and opening a plugin, clicking its title bar or its
channel's name focuses it. In Settings › Ctrl+Tab order choose **Fixed** (Browser, Channel Rack,
Playlist, Mixer, then the open plugins) or **Last used first**, which works like Alt+Tab: one press goes back to the area you came from (twice:
back again), and keeping Ctrl held while pressing Tab again goes further back. In the Channel
Rack ↑ / ↓ select the previous / next channel; in the Mixer ← / → select the previous / next
track. The selected channel's row is highlighted: that's where the Browser loads samples.

Shortcuts also work while the Sampler or a plugin window has the keyboard (a plugin may keep
keys it uses itself).

Every shortcut can be changed in **Options › Keyboard shortcuts…** (Ctrl+Shift+K): click **+**
next to a command and press the keys, click a key to change or remove it, or **reset to
defaults**. A command can have several keys; if a key is already taken you're asked whether to
move it. Your changes are saved (only what differs from the defaults) in `shortcuts.xml` in the
settings folder. **Export…** saves your whole shortcut set to a file and **Import…** loads one
(replacing the current keys), to share sets or move them to another computer. The menus show
the current keys, and so do the tooltips of buttons that have a shortcut (hover, e.g. the
Channel Rack's 16 | 32 | 64: "(Alt+1)"). The defaults, FL's where FL has one:

| Key | Action |
|---|---|
| Space / Shift+Space | Play-stop (stop rewinds) / play-pause |
| L | Switch between pattern (PAT) and song (SONG) mode |
| Ctrl+M | Metronome on/off |
| Alt+Shift+↑ / ↓ | Tempo up / down by the tempo step (5 BPM) |
| Ctrl+Shift+↑ / ↓ | Tempo up / down by the fine step (1 BPM) |
| F4 / Ctrl+Shift+C / F2 | New / clone / rename pattern |
| Numpad + / − | Next / previous pattern |
| F5 / F6 / F7 / F9 | Playlist / Channel Rack / Piano Roll / Mixer |
| Alt+F8 | Browser |
| Ctrl+C / X / V | Copy / cut / paste (Piano Roll, Playlist, or the Channel Rack's selected channel) |
| Delete or Backspace / Ctrl+A / Esc | Delete selection / select all / deselect |
| Esc (Sampler or plugin focused) | Close it: the Sampler after opening or clicking it, a plugin panel while it has the focus (View › Close window) |
| Ctrl+B | Duplicate: copy the selection and paste it right after (Piano Roll or Playlist) |
| Shift+↑ / ↓ | Move the selected notes (all if none are selected) up / down a semitone (Piano Roll) |
| Ctrl+↑ / ↓ | Piano Roll focused: move the selected notes (or all) up / down an octave (Browser focused: previous / next sample into the channel) |
| Shift+← / → | Move the selected notes (or all) earlier / later by the snap (Piano Roll) |
| Shift+D | Shorten the selected notes (all if none are selected) to one step |
| Ctrl+L | Legato: stretch the selected notes (or all) until the next note; chords end together (Piano Roll) |
| Alt+U | Slice notes (Piano Roll) |
| Alt+↑ / Alt+↓ | Slice size smaller / larger |
| Alt+→ / Alt+← | Snap finer / coarser (Piano Roll or Playlist) |
| Alt+L / Alt+1 / 2 / 3 | Channel Rack loop on/off / show 16 / 32 / 64 steps |
| Ctrl+T | Typing keyboard to piano on/off (the **Keys** button) |
| Z S X D C V G B H N J M | Play C5 to B5 on the selected channel (when Keys is on) |
| Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P | Play C6 to E7 |
| Ctrl+N / Ctrl+O / Ctrl+Q | New project / open project / quit |
| Ctrl+S / Ctrl+Shift+S | Save / save as |
| Ctrl+R | Export the song as a WAV file |
| Ctrl+Z / Ctrl+Y or Ctrl+Shift+Z | Undo / redo |
| Ctrl+, | Settings (tempo steps, preview length) |
| Ctrl+Tab / Ctrl+Shift+Tab | Focus the next / previous area |
| ↑ ↓ ← → / Enter | Move / open or load in the focused area |
| Ctrl+Enter | Load the Browser's selected sample into the selected channel, whichever area has the focus |

While **Keys** is on, the typing keyboard's letter keys play notes even if you give one of them
to a command; use them with Ctrl, Alt or Shift for shortcuts. The tempo steps are set in
**Options › Settings…**.

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

- **Opening from outside the app:** `juicy-loops-studio song.jlp` starts the app with that project
  open.
- **File › Save** writes a `.jlp` project file. Projects saved before the rename (`.jls`) still
  open; saving one writes a `.jlp` beside it and leaves the old file alone. The title bar shows the project name, with `*`
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
- **Play** (▶, ❚❚ while playing): play/pause (keeps the position). **Stop** (■) rewinds to
  the start.
- **Tempo:** − / + or the mouse wheel change it by 1 BPM; click the number to type an exact
  value (up to three decimals).
- **Position:** bar:beat:step within whatever is looping.
- **Clock** (next to the position): current time / total time, as
  minutes:seconds.milliseconds. In SONG mode that's the song (the Playlist); in PAT mode, the
  selected pattern.

- **Oscilloscope** (right end): the output as you hear it, steady on a zero crossing. Click it
  for the **spectrum** (levels by frequency, 30 Hz to 16 kHz), and again for the waveform; the
  choice is remembered.

- **Time signature:** beats per bar (n/4).
- **Metronome:** on/off.
- **Keys:** typing keyboard to piano on/off (Ctrl+T).
- **Window buttons** (right end, next to the oscilloscope): **Playlist** (F5), **Channel rack**
  (F6), **Piano roll** (F7), **Mixer** (F9) and **Browser** (Alt+F8), the same as their keys and
  the View menu. The LED on each is lit while that window is open, dark (and its label dimmed)
  while it's closed. The buttons show
  their names when there's room beside the oscilloscope, otherwise just their icons (the tooltip
  names the window and its key).

**Panel sizes.** Drag the Browser's inner edge, the Mixer's top edge or the Playlist picker's
right edge to make them wider or taller (a line in the accent colour shows the edge under the
mouse); double-click the edge to reset. The sizes are remembered.

In a narrow window the oscilloscope goes first, then Metronome and Keys show only their icons,
then the clock's time and the time signature are hidden.

Plugins follow the tempo, time signature and position, so arpeggiators, tempo-synced delays
and LFOs stay in time with the song. In PAT mode they see the pattern as a loop, in SONG mode
the whole song.

## Browser

The Browser (**Alt+F8**, View › Browser) lists folders of samples. It docks on the left; **>**
moves it to the right (**<** back), **×** hides it. Your folders, its side and whether it's open
are remembered.

| Do this | To |
|---|---|
| **+** (title bar) | Add a folder (right-click a folder at the top level to remove it) |
| Click a folder | Open / close it |
| Click a sample | Show its waveform and play a short preview (the first 5 s; Settings › Click preview length) |
| Shift+click a sample | Play all of it |
| Click in the waveform | Play from there to the end |
| Play / Pause (bottom) | Play the whole sample, pause, and resume where it paused |
| Drag a sample onto the Channel Rack | Add it as a channel (or onto a row: replace that channel's sample) |
| Drag a sample onto the Playlist | Place it as an audio clip |
| Ctrl+click a sample | Load it into the selected channel (the highlighted row in the Channel Rack) |
| ↑ / ↓ (Browser focused) | Move through the list; samples preview as you land on them |
| → / ← | Open / close a folder (← on a file goes to its folder) |
| Enter | Open or close a folder; on a sample: load it into the selected channel |
| Ctrl+Enter | Load the selected sample into the selected channel, even while another area has the focus |
| Ctrl+↑ / Ctrl+↓ | Previous / next sample, loaded into the selected channel right away (and previewed): audition samples in the song as it plays |

Loading a sample into a channel renames the channel after the sample, as in FL.

Previews go straight to your speakers, past the mixer and the song, so they're heard while the
song plays or not. Samples load in the background, so long files don't freeze the app; files
longer than 2 minutes are cut at 2 minutes (for now).

## Channel Rack

| Do this | To |
|---|---|
| Left-click / drag over steps | Turn steps on (or off, if you started on a lit step); a step that's on lights the LED on its key |
| Right-click / drag over steps | Turn steps off |
| Click a channel name | Select the channel (the Piano Roll edits it), and show or hide its window: the plugin's for an instrument, the Sampler for a sample |
| Right-click a channel name | Piano roll, show plugin / Sampler, Fill, **Clear** (remove its notes from this pattern), **Duplicate** (a copy of the channel right after it: its sample or plugin with the plugin's current settings, volume, pan and mixer track, without notes), rename, replace sample, use/replace an instrument plugin, delete (press **D** while the menu is open). Clear and Duplicate are also in View › Channel rack options, for the selected channel |
| Delete (Channel Rack focused) | Delete the selected channel (Ctrl+Z brings it back) |
| Ctrl+C / Ctrl+X / Ctrl+V (Channel Rack focused) | Copy or cut the selected channel's notes in this pattern; paste them into the selected channel (also another channel or pattern), replacing its notes |
| Right-click › Fill | Replace the channel's notes with one on every step, every 2, 4 or 8 steps (hi-hats), across the steps shown; also on a channel with melodic notes (undo brings them back) |
| Green light | Mute / unmute |
| Ctrl+click or right-click the green light | Solo: mute every other channel (again: all play) |
| Pan and volume knobs | Drag up/down; double-click resets |
| Mixer track box | Drag up/down or scroll to route the channel to an insert ("--" = Master) |
| Drop audio files on a row | Replace that channel's sample |
| Drop audio files below the rows | Add channels |
| Click + | Add a channel: **Sample…** or **Instrument ›** (a plugin synth) |
| Click a note preview | Open the Piano Roll for that channel |
| 16 \| 32 \| 64 (title bar) | How many steps the rack shows. It doesn't grow by itself: a Piano Roll part longer than that still plays in full, but the rack shows only these steps |
| Drag the handle after the last step | Show more or fewer steps, 4 at a time |
| **Loop** (title bar) | Loop on/off for the whole rack |
| Circular arrow at the end of a row | Loop on/off for that channel |

**Pattern length and looping.** 16 | 32 | 64 and the handle only set how many steps you see.
The pattern *plays* as far as its notes go, rounded up to whole bars: with notes only in the
first 16 steps it plays 16 steps and starts over, even when 32 are shown. Steps past that are
greyed and show what will be heard there (the repeat). What clicking a greyed step does is set in
**Options › Settings › Clicking a looped step**:
- **Discards the loop (FL)**, the default: the step you click becomes a real note and the pattern
  now plays up to there. That channel's repeats are dropped (its other new steps are empty);
  other channels keep repeating. Right-click leaves greyed steps alone.
- **Copies the loop in**: the repeat is copied in first, so nothing else you hear changes, then
  left-click keeps or adds a note on that step; right-click (or right-drag) removes looped notes,
  and leaves empty greyed steps alone. With **Loop** on (the default), a channel whose notes are shorter than the pattern
repeats them: its notes are rounded up to 1, 2, 4, 8... bars, and that block plays again and
again. Example: a 4-bar bass line with a 1-bar drum beat gives 4 bars of drums. Those repeats
are greyed too. Because the loop length comes from the notes, a looping channel can't end with
empty bars; switch its loop off (the arrow) to have it play once per pattern. Steps are always full
size: at 32 or 64 steps the rack gets wider, past the window's edge if need be. Drag it by its
title bar to see the rest; its Loop, 16 | 32 | 64 and close buttons stay in view.

**Instrument plugins (VST3 / LV2).** Scan for plugins first (Options › Plugins…). Then add one
with **+ › Instrument**, or turn an existing channel into one from its right-click menu. Its
editor opens right away (a panel in the main window); click the channel name to show or hide it later. Instrument channels
play their notes like any channel: steps play C5 for one step, and the Piano Roll sets pitch,
length and velocity (note length matters here, unlike for samples). Volume, pan, mute and the
mixer track apply after the plugin. Notes are always released: when a note ends, when its
clip ends, and when you stop or pause. Settings are saved in the project; if the plugin isn't
installed, the channel stays silent and keeps them.

## Playlist

| Do this | To |
|---|---|
| Left-click empty space | Place the selected pattern (snapped), and keep dragging to move it (with clips selected, it also deselects them) |
| Click a clip | Select it (just it); drag to move it in time or to another track |
| Shift + click a clip | Add it to the selection, or take it out |
| Drag a clip's right end | Make it longer (the pattern repeats) or shorter |
| Right-click / drag | Deselect, and delete the clips under the mouse |
| Double-click a pattern clip | Open that pattern in the Channel Rack (brought to the front) |
| Ctrl + mouse wheel | Zoom |
| Snap (top right) | Bar, beat or step |
| Click / drag the ruler | Move the playhead there (snapped); switches to SONG mode |
| Drop a sample (from the Browser or your desktop) | Place it as an audio clip, as long as the sample |
| **Picker** (title bar) | Show / hide the picker: all patterns and audio clips |
| Click an item in the picker | Make it what clicking empty space places |
| Double-click an item in the picker | A pattern: open it in the Channel Rack; an audio clip: its Sampler |
| Right-click a pattern in the picker | **Split by channel** (also in the Patterns menu, for the selected pattern) |
| Drag an item from the picker | Place it where you drop it (a chip with its name follows the mouse) |
| Ctrl + drag | Select the clips in a rectangle (outlined) |
| Ctrl + click a clip | Add it to the selection, or take it out (like Shift + click) |
| Drag a selected clip | Move all selected clips together |
| Delete / Ctrl+A / Esc | Delete the selected clips / select all / deselect |
| Patterns › Make unique | Give the selected clips their own copy of their pattern ("Beat 2"), so editing it doesn't change the pattern's other clips (as in FL) |
| Ctrl+C / Ctrl+X / Ctrl+V | Copy / cut the selected clips; paste them right after the copied bars (again: after the last paste) |
| Ctrl+B | Duplicate the selected clips right after their bars (the copies get selected: press again to keep going) |

**Split by channel** replaces a pattern with one pattern per channel, named after the channel
("Kick", or "Kick 2" if a pattern already has that name),
in its place in the list (the original is removed). Unlike FL it keeps the arrangement tidy and
sounding the same:
- each part is exactly as long as the original: a channel that looped a shorter part gets its
  repeats written out, and a part whose notes end early keeps the full length
- in the Playlist, every clip of the pattern is replaced by one clip per part, each on its own
  track: the first on the original clip's track, the others on the nearest tracks below that
  are free for that stretch of time (tracks are added at the bottom if needed). Copies of the
  pattern on the same track all use the same tracks.
It's one step to undo.

**Audio clips.** Dropping a sample on the Playlist makes an audio clip: the sample plays at that
point in the song, shown with its waveform. Move, resize (shorter clips cut the sample off),
select, copy, paste and delete them like pattern clips. Drag a clip's **left edge** to start
it later in the sample (trim). **Double-click** a clip to open its Sampler. Each sample becomes one audio-clip
channel, used by all its clips: its volume, pan, mute and mixer track (a new insert) apply to
all of them. These channels are hidden from the Channel Rack; **View › Channel rack options ›
Show audio clips** lists them there. Pressing Play (or jumping with the ruler) inside a clip
plays it from that point; stopping fades it out. Audio clips play in SONG mode only.

## Sampler

Every sample channel (drums and audio clips alike) has a **Sampler** window: click the channel's
name in the Channel Rack (again to hide it), use "Sampler..." in its right-click menu, or
double-click an audio clip. It shows the sample's waveform and its time stretching:

| Setting | Does |
|---|---|
| Mode **Off** | Plays at the sample's own speed whatever the tempo |
| Mode **Resample** | Speed follows the tempo, and the pitch moves with it (like a record) |
| Mode **Stretch** | Length follows the tempo, the pitch stays (high-quality time-stretching) |
| Original tempo | The tempo the sample is at; Resample / Stretch fit it to the song's tempo from there. **Use project tempo** sets it to the current tempo |
| Pitch | Transpose in semitones (in Stretch mode without changing the length) |

Audio clips start in **Stretch** at the tempo they were dropped at, so they stay in time when
you change the tempo. Drum samples start in Off. Stretching is worked out in the background
after a tempo change; until it's ready (usually a moment) the sample plays resampled so it stays
in time. Exports always use the finished stretch.

The right-hand column has cutting and the envelope:

| Setting | Does |
|---|---|
| Cut itself | A new note stops the channel's note that's still ringing (short fade, no click). Like the envelope's switch, it's a light: lit = on, click to switch |
| Cut group / Cut by | Notes on a channel stop every channel whose *Cut by* is its *Cut group*. Closed and open hi-hat: give both the same number in both, and each stops the other |
| Volume envelope | On: shapes the volume with Delay, Attack, Hold, Decay, Sustain and Release, and note length matters (the release starts where the note ends; a held key on the typing keyboard or MIDI holds it). Off: the sample plays to its end, as before. Knobs: drag up / down, double-click resets |

## Piano Roll

| Do this | To |
|---|---|
| Left-click empty space | Add a note (same length as the last one), then drag to move it |
| Click a note | Select it (just it); drag to move it in time and pitch. New notes take its length, as in FL |
| Shift + click a note | Add it to the selection, or take it out |
| Drag a note's right end | Change its length (new notes use it too) |
| Right-click / drag | Deselect, and delete the notes under the mouse |
| Velocity lane (bottom) | Drag a stem up/down to set velocity |
| Ctrl + mouse wheel | Zoom |
| Snap (top right) | Beat, step, ½ step, ⅓ step or none |
| Click / drag the ruler | Move the playhead there (snapped); switches to PAT mode |
| Ctrl + drag | Select the notes in a rectangle (outlined) |
| Ctrl + click a note | Add it to the selection, or take it out (like Shift + click) |
| Drag a selected note | Move all selected notes together (its right end resizes them all) |
| Delete / Ctrl+A / Esc | Delete the selected notes / select all / deselect |
| Ctrl+C / Ctrl+X / Ctrl+V | Copy / cut the selected notes; paste them right after the copied beats (again: after the last paste), also into another channel or pattern |
| Shift + arrows | Move the selected notes (all if none are selected): ↑ / ↓ a semitone, ← / → by the snap |
| Ctrl + ↑ / ↓ | Move the selected notes (or all) an octave |
| Ctrl+L | Legato: each selected note (or every note) lasts until the next one starts; notes starting together end together; the last ones keep their length |
| Ctrl+B | Duplicate the selected notes (all if none are selected) right after their beats; the copies get selected, so pressing again keeps going. The clipboard is left alone |
| Shift+D | Shorten the selected notes (all if none are selected) to one step |
| Slice (top right) or Alt+U | Cut the selected notes (all notes if none are selected) into pieces of the chosen size: 1/4, 1/8, 1/8 T, 1/16, 1/16 T, 1/32, 1/64 |
| Click and hold a key on the left | Play that note (slide to play others) |
| Scale (title bar, after "Piano roll") | One control: click **● Scale** (the light or the word) to turn the scale highlight on / off; then the key (choosing one turns the highlight on) and Major / Minor. Key and mode are kept, greyed, while it's off; new projects start with Settings › Default scale |

The slicer is for rolls (hi-hats, snares): draw one long note, slice it, then shape the pieces'
velocities. The pieces stay selected, so slicing again makes them finer.
With a selection, a click in empty space deselects and adds a note there, in one go.
Clicking a note (or adding one) plays it on the channel for as long as you hold the button;
while you drag it up or down you hear the new pitch. That's only while the song is stopped or paused: during playback, editing notes is silent (and a preview still sounding stops when playback starts), while clicking a key on the keyboard still plays it.

The keyboard labels each C and, with a scale on, the scale's root (in the accent colour); point
at any key, or at its row in the grid, to see its name. Note names follow FL: C5 is middle C
(MIDI 60) and plays a sample at its original pitch. The keys are ivory by default; Settings ›
Keys switches to dark keys. The grid is quiet on purpose: rows differ only slightly (with a
scale chosen, its notes' rows are a touch lighter and the root's faintly tinted), and the lines
get stronger from steps to beats to bars. Notes are brighter the louder they are, and show their
name when they're wide enough. The scale is saved with the project. For
samples, note length only matters with the Sampler's volume envelope on; otherwise they play to
their end, like FL's default sampler.

## Mixer

- **Strips:** Master plus 16 inserts, drawn like a console: the track's number and name, a
  stereo meter with a dB scale, the fader (double-click = 0 dB) with its level below it, **pan**
  and **width** knobs, and the track's green **on light**, as in FL: click it to turn the track
  off (mute) or on; right-click or Ctrl+click it to solo the track (every other insert off;
  again: all on). Click a strip to select it.
- **Width** makes a track's stereo image narrower or wider: all the way left is mono, the middle
  (100 %, double-click) leaves it as it comes in, all the way right (200 %) is twice as wide.
- **Effect slots** (right side, for the selected track):
  - Click **+ Add effect** to choose Filter, EQ, Delay, Reverb, Compressor or Soft Clipper.
  - Click an effect to open its controls; click its light to bypass it (the effect's panel has
    the same light, left of its close button).
  - Right-click for edit, bypass, move up/down and remove.
- **Plugins (VST3 / LV2):** first scan for them in **Options › Plugins…**. Then
  **+ Add effect › Plugins** lists them. Clicking a plugin's slot opens its editor (a panel in the main window). Its
  settings are saved in the project. If a project uses a plugin that isn't installed, the slot
  shows "(missing)", passes audio through unchanged and keeps the plugin's settings.
- **Soft Clipper on the Master:** new projects start with one, so loud mixes saturate gently
  instead of hard-clipping. It changes nothing below its knee (new projects: knee 0.1, so it only
  touches the last dB below the ceiling); remove or bypass it if you like.
- **Effect panel:** floating; drag it by its title bar. Knobs drag up/down and double-click
  resets them.

## Settings

**Options › Settings…** (Ctrl+,):

| Setting | Does |
|---|---|
| Tempo step / Fine tempo step | How far Tempo up / down (Alt+Shift+↑ / ↓) and the fine versions (Ctrl+Shift+↑ / ↓) move the tempo: 5 and 1 BPM by default |
| Click preview length | How much of a sample a click in the Browser plays (5 s by default) |
| Ctrl+Tab order | Fixed, or Last used first (like Alt+Tab) |
| Plugin windows | Inside the app (panels, the default) or separate windows |
| Clicking a looped step | Discards the loop (as in FL, the default) or copies the loop in first (see *Channel Rack*) |
| Keys (Piano Roll) | Ivory (the default) or dark keys in the panel colours |
| Default scale (Piano Roll) | Major (the default) or Minor: the scale new projects start with |
| Interface size | Scales the whole interface, text included: 100, 110, 125 or 150 % |

Shortcuts have their own window, **Options › Keyboard shortcuts…** (see *Keyboard* above); audio
and MIDI devices are in **Options › Audio settings…**.

## Themes

**Options › Theme…** offers **Blackberry** (the default) and **Graphite**, and can import or
export your own theme files. Themes change the colours only; the shapes and the font are the same
in every theme. See [themes.md](themes.md).
