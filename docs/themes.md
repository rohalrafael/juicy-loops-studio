# Themes

A theme sets the app's **colours**. Shapes (corner radii, borders), sizes and the font
(Inter) are the same in every theme, so the app looks consistent whichever you pick.

Juicy Loops Studio ships with two themes: **Blackberry** (the default: near-black with
lavender, mint and pink) and **Graphite** (neutral grey with a blue accent and amber displays).
Pick one under **Options › Theme…**. The choice is saved in your settings, not in the project.

You can also make your own. A theme is a small JSON file. The quickest start is
**Export current theme…** in the Theme dialog, which writes the active theme with every
colour filled in. Edit that, then use **Import theme file…**.

Imported themes are copied into the themes folder:

| OS      | Themes folder                                                         |
|---------|-----------------------------------------------------------------------|
| Linux   | `$XDG_CONFIG_HOME/JuicyLoopsStudio/themes` (usually `~/.config/...`)   |
| macOS   | `~/Library/Application Support/JuicyLoopsStudio/themes`               |
| Windows | `%APPDATA%\JuicyLoopsStudio\themes`                                   |

Any `.json` file in that folder appears in the Theme dialog. **Open themes folder** takes
you there.

## Format

Only `name` is required. Any colour you leave out comes from the Blackberry theme, so this is
a complete theme:

```json
{
  "name": "Mint",
  "colours": { "accent": "#5ee0d0", "stepOn": "#5ee0d0" }
}
```

A full file:

```json
{
  "name": "Blackberry",
  "author": "Juicy Loops Studio",
  "description": "Near-black with lavender, mint and pink",
  "formatVersion": 2,
  "colours": {
    "background": "#0a0a0d",
    "panel": "#121216",
    "panelRaised": "#1b1b21",
    "chrome": "#28282e",
    "header": "#1b1b21",
    "outline": "#24242c",
    "text": "#ececf2",
    "textDim": "#8e8e9c",
    "accent": "#b9a7ff",
    "accentAlt": "#9fe8c8",
    "display": "#070709",
    "displayText": "#9fe8c8",
    "displayDim": "#6c6c7a",
    "stepOff": ["#23232c", "#2b2634"],
    "stepOn": "#b9a7ff",
    "playhead": "#f4f2ff",
    "led": "#9fe8c8",
    "ledOff": "#26262e",
    "activeBackground": "#252135",
    "activeText": "#b9a7ff",
    "shadow": "#000000a6",
    "lane": "#0e0e12",
    "grid": "#1b1b22",
    "clip": "#f2b5c6",
    "clipText": "#1b1218"
  }
}
```

### Colours

Write colours as `#rrggbb`, or `#rrggbbaa` to add transparency (`00` is clear, `ff` is
solid).

| Key                | Used for                                                     |
|--------------------|--------------------------------------------------------------|
| `background`       | Window background, and the seams between docked panels       |
| `panel`            | Panels such as the Channel Rack and dialogs                  |
| `panelRaised`      | Buttons and other raised controls                            |
| `chrome`           | The title bar, dialogs' title bars and the transport bar: the lightest surface |
| `header`           | Panel headers (Playlist, Channel Rack...) and dialog footers, a step between the chrome and the content |
| `outline`          | Borders and outlines                                         |
| `text`, `textDim`  | Main and secondary text                                      |
| `accent`           | Main highlight colour (knob values, selection, the focused panel's top line) |
| `accentAlt`        | Second highlight colour (level meters)                       |
| `display`          | Recessed areas: tempo display, knob tracks, fader grooves    |
| `displayText`, `displayDim` | Text on those displays                              |
| `stepOff`          | Two colours for the step keys; they alternate every beat     |
| `stepOn`           | The lit LED on a step key                                    |
| `playhead`         | Outline on the step that's playing                           |
| `led`, `ledOff`    | Channel mute lights                                          |
| `activeBackground`, `activeText` | Pressed / toggled-on buttons, highlighted menu items |
| `shadow`           | The shadow under the floating Channel Rack (not on Linux)    |
| `lane`, `grid`     | Playlist track background and bar lines                      |
| `clip`, `clipText` | Pattern clips in the Playlist and their labels               |

### Older theme files

Version 1 theme files (`"formatVersion": 1`) also set a `font`, a `shape` (corner radii,
borders) and `effects` (glows, a hard shadow). They still import: their colours are used and
those three settings are skipped without a warning.

Theme files from before `chrome` existed use their `header` colour for it, as the title and
transport bars did then.

### Errors

A file is only rejected if it isn't valid JSON or has no `name`. Anything else that's
wrong, like a misspelled key or a bad colour, is skipped and listed when you import, and the
default is used for that colour.
