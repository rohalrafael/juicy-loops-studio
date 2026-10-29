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
    "header": "#0e0e12",
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
    "header": "#191b1e",
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
        load("blackberry", blackberryJson),
        load("graphite", graphiteJson),
    };
    return themes;
}

const Theme& defaultTheme()
{
    return builtInThemes().front();
}

} // namespace jls::theme
