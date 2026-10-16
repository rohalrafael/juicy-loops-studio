#include "theme/theme.h"

#include <stdexcept>

namespace jls::theme
{
namespace
{

// Built-in themes are stored in the same format as user theme files, so they double as
// examples and go through the same parser.

constexpr const char* juicyJson = R"({
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
})";

constexpr const char* citrusJson = R"({
  "name": "Citrus",
  "author": "Juicy Loops Studio",
  "description": "Bright and bold, with thick outlines",
  "formatVersion": 1,
  "font": "Bricolage Grotesque",
  "colours": {
    "background": "#fff4d6",
    "panel": "#fffaf0",
    "panelRaised": "#ffffff",
    "header": "#ffe08a",
    "outline": "#2b1d0e",
    "text": "#2b1d0e",
    "textDim": "#6b5640",
    "accent": "#ff5f1f",
    "accentAlt": "#3fa31b",
    "display": "#2b1d0e",
    "displayText": "#ffd43b",
    "displayDim": "#c9b38f",
    "stepOff": ["#ffe9b0", "#ffd9c7"],
    "stepOn": "#ff5f1f",
    "playhead": "#2b1d0e",
    "led": "#3fa31b",
    "ledOff": "#e8dcc0",
    "activeBackground": "#ffd43b",
    "activeText": "#2b1d0e",
    "shadow": "#2b1d0e"
  },
  "shape": { "cornerRadius": 10, "windowRadius": 14, "stepRadius": 7, "borderWidth": 2, "stepOutline": 2 },
  "effects": { "stepGlow": false, "ledGlow": false, "hardShadow": true }
})";

constexpr const char* watermelonJson = R"({
  "name": "Watermelon",
  "author": "Juicy Loops Studio",
  "description": "Neon glow on near-black",
  "formatVersion": 1,
  "font": "Sora",
  "colours": {
    "background": "#0b1210",
    "panel": "#111c19",
    "panelRaised": "#182723",
    "header": "#0e1714",
    "outline": "#1f3a33",
    "text": "#eafff6",
    "textDim": "#8fb5a8",
    "accent": "#ff3d6e",
    "accentAlt": "#3dffb0",
    "display": "#050a08",
    "displayText": "#3dffb0",
    "displayDim": "#5c8a7b",
    "stepOff": ["#16302a", "#2c1c25"],
    "stepOn": "#ff3d6e",
    "playhead": "#3dffb0",
    "led": "#3dffb0",
    "ledOff": "#1d332d",
    "activeBackground": "#16342c",
    "activeText": "#3dffb0",
    "shadow": "#00000099"
  },
  "shape": { "cornerRadius": 4, "windowRadius": 6, "stepRadius": 2, "borderWidth": 1, "stepOutline": 0 },
  "effects": { "stepGlow": "#ff3d6ecc", "ledGlow": "#3dffb0e6", "hardShadow": false }
})";

constexpr const char* blueberryJson = R"({
  "name": "Blueberry",
  "author": "Juicy Loops Studio",
  "description": "Soft, round and cool",
  "formatVersion": 1,
  "font": "Outfit",
  "colours": {
    "background": "#0f1330",
    "panel": "#171d45",
    "panelRaised": "#222a5c",
    "header": "#141a3d",
    "outline": "#0a0d24",
    "text": "#eef0ff",
    "textDim": "#a3aadf",
    "accent": "#ff4fa3",
    "accentAlt": "#8fa2ff",
    "display": "#0a0e26",
    "displayText": "#ffb8dc",
    "displayDim": "#7f87c4",
    "stepOff": ["#262e66", "#342a66"],
    "stepOn": "#ff4fa3",
    "playhead": "#8fa2ff",
    "led": "#8fa2ff",
    "ledOff": "#2a3266",
    "activeBackground": "#2c3577",
    "activeText": "#ffb8dc",
    "shadow": "#03051499"
  },
  "shape": { "cornerRadius": 14, "windowRadius": 20, "stepRadius": 10, "borderWidth": 1, "stepOutline": 0 },
  "effects": { "stepGlow": "#ff4fa366", "ledGlow": false, "hardShadow": false }
})";

Theme load(const char* id, const char* json)
{
    auto result = parseTheme(json);
    if (!result.theme || !result.warnings.empty())
        throw std::logic_error(std::string("built-in theme '") + id + "' is invalid");
    result.theme->id = id;
    return *result.theme;
}

} // namespace

const std::vector<Theme>& builtInThemes()
{
    static const std::vector<Theme> themes{
        load("juicy", juicyJson),
        load("citrus", citrusJson),
        load("watermelon", watermelonJson),
        load("blueberry", blueberryJson),
    };
    return themes;
}

const Theme& defaultTheme()
{
    return builtInThemes().front();
}

} // namespace jls::theme
