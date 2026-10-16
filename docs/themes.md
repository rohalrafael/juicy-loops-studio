# Themes

Juicy Loops Studio ships with four themes: **Juicy** (the default), **Citrus**,
**Watermelon** and **Blueberry**. Pick one under **Options › Theme…**. The choice is
saved in your settings, not in the project.

You can also make your own. A theme is a small JSON file. The quickest start is
**Export current theme…** in the Theme dialog, which writes the active theme with every
setting filled in. Edit that, then use **Import theme file…**.

Imported themes are copied into the themes folder:

| OS      | Themes folder                                                         |
|---------|-----------------------------------------------------------------------|
| Linux   | `$XDG_CONFIG_HOME/JuicyLoopsStudio/themes` (usually `~/.config/...`)   |
| macOS   | `~/Library/Application Support/JuicyLoopsStudio/themes`               |
| Windows | `%APPDATA%\JuicyLoopsStudio\themes`                                   |

Any `.json` file in that folder appears in the Theme dialog. **Open themes folder** takes
you there.

## Format

Only `name` is required. Anything you leave out comes from the Juicy theme, so this is
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
  "name": "Juicy",
  "author": "Juicy Loops Studio",
  "description": "Plum, blood orange and lime",
  "formatVersion": 1,
  "font": "IBM Plex Sans",
  "colours": {
    "background": "#17121c",
    "panel": "#241b2c",
    "panelRaised": "#302439",
    "header": "#1d1624",
    "outline": "#0f0b13",
    "text": "#f5eef7",
    "textDim": "#ad9cb6",
    "accent": "#ff7a45",
    "accentAlt": "#b7f04a",
    "display": "#120e16",
    "displayText": "#b7f04a",
    "displayDim": "#8a7a93",
    "stepOff": ["#392d44", "#4b2f3c"],
    "stepOn": "#ff7a45",
    "playhead": "#f5eef7",
    "led": "#b7f04a",
    "ledOff": "#3a2f44",
    "activeBackground": "#3e2d4a",
    "activeText": "#ff7a45",
    "shadow": "#0000008c"
  },
  "shape": { "cornerRadius": 6, "windowRadius": 8, "stepRadius": 4, "borderWidth": 1, "stepOutline": 0 },
  "effects": { "stepGlow": false, "ledGlow": false, "hardShadow": false }
}
```

### Colours

Write colours as `#rrggbb`, or `#rrggbbaa` to add transparency (`00` is clear, `ff` is
solid).

| Key                | Used for                                                     |
|--------------------|--------------------------------------------------------------|
| `background`       | Window background                                            |
| `panel`            | Panels such as the Channel Rack and dialogs                  |
| `panelRaised`      | Buttons and other raised controls                            |
| `header`           | Title bars and headers                                       |
| `outline`          | Borders and outlines                                         |
| `text`, `textDim`  | Main and secondary text                                      |
| `accent`           | Main highlight colour (knob values, selection)               |
| `accentAlt`        | Second highlight colour                                      |
| `display`          | Recessed areas: tempo display, knob tracks                   |
| `displayText`, `displayDim` | Text on those displays                              |
| `stepOff`          | Two colours for unlit steps; they alternate every beat       |
| `stepOn`           | Lit steps                                                    |
| `playhead`         | Outline on the step that's playing                           |
| `led`, `ledOff`    | Channel mute lights                                          |
| `activeBackground`, `activeText` | Pressed / toggled-on buttons, highlighted menu items |
| `shadow`           | Shadow colour (also the hard shadow, if enabled)             |

### Font

`font` is a family name. These are built in and always available: **IBM Plex Sans**,
**Bricolage Grotesque**, **Sora**, **Outfit**. Any other name is looked up among the
fonts installed on your system. If it isn't found, a default font is used.

### Shape

All sizes are in pixels.

| Key            | Range | Meaning                                          |
|----------------|-------|--------------------------------------------------|
| `cornerRadius` | 0–24  | Corner rounding of buttons and small controls    |
| `windowRadius` | 0–32  | Corner rounding of panels                        |
| `stepRadius`   | 0–12  | Corner rounding of step buttons                  |
| `borderWidth`  | 0–4   | Width of outlines around buttons and panels      |
| `stepOutline`  | 0–4   | Outline around each step button (0 = none)       |

### Effects

| Key          | Value                  | Meaning                                    |
|--------------|------------------------|--------------------------------------------|
| `stepGlow`   | colour, or `false`     | Glow around lit steps                      |
| `ledGlow`    | colour, or `false`     | Glow around lit mute lights                |
| `hardShadow` | `true` / `false`       | Solid offset shadow under panels           |

### Errors

A file is only rejected if it isn't valid JSON or has no `name`. Anything else that's
wrong, like a misspelled key, a bad colour or an out-of-range number, is skipped and
listed when you import, and the default is used for that setting.
