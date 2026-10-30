#include "theme/theme.h"

#include <stdexcept>

namespace jls::theme
{
namespace
{

// Built-in themes are stored in the same format as user theme files, so they double as
// examples and go through the same parser.

constexpr const char* blackberryJson = R"({
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
})";

constexpr const char* graphiteJson = R"({
  "name": "Graphite",
  "author": "Juicy Loops Studio",
  "description": "Neutral graphite with a blue accent and amber displays",
  "formatVersion": 2,
  "colours": {
    "background": "#111214",
    "panel": "#1f2124",
    "panelRaised": "#2b2e32",
    "chrome": "#373a3e",
    "header": "#2b2e32",
    "outline": "#0a0b0c",
    "text": "#e3e5e8",
    "textDim": "#8d929a",
    "accent": "#5aa9ff",
    "accentAlt": "#5cc46a",
    "display": "#0c0d0f",
    "displayText": "#ffb35c",
    "displayDim": "#8a8f97",
    "stepOff": ["#33363b", "#3a363b"],
    "stepOn": "#5aa9ff",
    "playhead": "#e3e5e8",
    "led": "#5cc46a",
    "ledOff": "#34373b",
    "activeBackground": "#2f3540",
    "activeText": "#7dbbff",
    "shadow": "#000000a0",
    "lane": "#18191c",
    "grid": "#25282c",
    "clip": "#4b5b72",
    "clipText": "#eef2f6"
  }
})";

constexpr const char* nightshadeJson = R"({
  "name": "Nightshade",
  "author": "Juicy Loops Studio",
  "description": "Black with hot pink",
  "formatVersion": 2,
  "colours": {
    "background": "#060508",
    "panel": "#0f0d11",
    "panelRaised": "#19151b",
    "chrome": "#231d25",
    "header": "#18141a",
    "outline": "#2a2229",
    "text": "#f6eef4",
    "textDim": "#9b8b97",
    "accent": "#ff3d9a",
    "accentAlt": "#ffb3d6",
    "display": "#040305",
    "displayText": "#ff6fb5",
    "displayDim": "#7a6672",
    "stepOff": ["#211b23", "#2b1f29"],
    "stepOn": "#ff3d9a",
    "playhead": "#fff0f7",
    "led": "#ff5fae",
    "ledOff": "#2a2329",
    "activeBackground": "#2f1526",
    "activeText": "#ff7ab8",
    "shadow": "#000000b0",
    "lane": "#0b090c",
    "grid": "#1b161d",
    "clip": "#ff4fa3",
    "clipText": "#1a0812"
  }
})";

constexpr const char* bloodlineJson = R"({
  "name": "Bloodline",
  "author": "Juicy Loops Studio",
  "description": "Black with deep red",
  "formatVersion": 2,
  "colours": {
    "background": "#050405",
    "panel": "#0e0b0c",
    "panelRaised": "#181314",
    "chrome": "#221a1b",
    "header": "#171213",
    "outline": "#2a1e20",
    "text": "#f3ebeb",
    "textDim": "#968686",
    "accent": "#ff2a3d",
    "accentAlt": "#ff8a8a",
    "display": "#030202",
    "displayText": "#ff3b4a",
    "displayDim": "#7a6262",
    "stepOff": ["#211a1b", "#2a1d1f"],
    "stepOn": "#ff2a3d",
    "playhead": "#fff1f1",
    "led": "#ff3b4a",
    "ledOff": "#2a2122",
    "activeBackground": "#2e1215",
    "activeText": "#ff5a68",
    "shadow": "#000000b5",
    "lane": "#0a0808",
    "grid": "#1b1516",
    "clip": "#c3142a",
    "clipText": "#fff0f0"
  }
})";

constexpr const char* chromeJson = R"({
  "name": "Chrome",
  "author": "Juicy Loops Studio",
  "description": "Black and silver, an ice-blue light",
  "formatVersion": 2,
  "colours": {
    "background": "#060708",
    "panel": "#0f1012",
    "panelRaised": "#1a1c1f",
    "chrome": "#25282c",
    "header": "#191b1e",
    "outline": "#272a2f",
    "text": "#eef2f6",
    "textDim": "#8b939c",
    "accent": "#d6e0ea",
    "accentAlt": "#9fd6ff",
    "display": "#040506",
    "displayText": "#e8f1ff",
    "displayDim": "#6d7680",
    "stepOff": ["#1f2125", "#262a2f"],
    "stepOn": "#e3eaf2",
    "playhead": "#ffffff",
    "led": "#9fd6ff",
    "ledOff": "#272a2f",
    "activeBackground": "#24282e",
    "activeText": "#ffffff",
    "shadow": "#000000b0",
    "lane": "#0b0c0e",
    "grid": "#1a1c20",
    "clip": "#c9d3de",
    "clipText": "#0d1014"
  }
})";

constexpr const char* iceJson = R"({
  "name": "Ice",
  "author": "Juicy Loops Studio",
  "description": "Black with baby blue",
  "formatVersion": 2,
  "colours": {
    "background": "#05070a",
    "panel": "#0d1015",
    "panelRaised": "#161b22",
    "chrome": "#1f262f",
    "header": "#151a21",
    "outline": "#222a34",
    "text": "#eef5fc",
    "textDim": "#8593a3",
    "accent": "#7cc6ff",
    "accentAlt": "#c4e6ff",
    "display": "#03050a",
    "displayText": "#8fd0ff",
    "displayDim": "#61707f",
    "stepOff": ["#1b2129", "#202833"],
    "stepOn": "#7cc6ff",
    "playhead": "#f0f8ff",
    "led": "#8fd0ff",
    "ledOff": "#232b35",
    "activeBackground": "#14283c",
    "activeText": "#9fd6ff",
    "shadow": "#000000b0",
    "lane": "#090c10",
    "grid": "#171d24",
    "clip": "#7cc6ff",
    "clipText": "#06131f"
  }
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
        load("blackberry", blackberryJson), load("graphite", graphiteJson),
        load("nightshade", nightshadeJson), load("bloodline", bloodlineJson),
        load("chrome", chromeJson),         load("ice", iceJson),
    };
    return themes;
}

const Theme& defaultTheme()
{
    return builtInThemes().front();
}

} // namespace jls::theme
