#pragma once

// Theme data (colours) and its JSON file format. Plain C++: the JUCE side is in ui/theme.h.
// Themes only set colours; shapes and fonts are the same for every theme (ui::metrics).

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Theme data and theme files. Plain C++ (no JUCE) so it can be unit-tested.
// The JSON file format is documented in docs/themes.md.
namespace jls::theme
{

using Argb = std::uint32_t; // 0xAARRGGBB

// Member defaults are the Blackberry theme: anything a theme file leaves out falls back to these.
struct Colours
{
    Argb background = 0xff0a0a0d;
    Argb panel = 0xff121216;
    Argb panelRaised = 0xff1b1b21;
    Argb header = 0xff0e0e12;
    Argb outline = 0xff24242c;
    Argb text = 0xffececf2;
    Argb textDim = 0xff8e8e9c;
    Argb accent = 0xffb9a7ff;
    Argb accentAlt = 0xff9fe8c8;
    Argb display = 0xff070709; // recessed panels: tempo display, knob tracks
    Argb displayText = 0xff9fe8c8;
    Argb displayDim = 0xff6c6c7a;
    Argb stepOffA = 0xff23232c; // step buttons alternate A/B every beat
    Argb stepOffB = 0xff2b2634;
    Argb stepOn = 0xffb9a7ff; // the lit LED of a step
    Argb playhead = 0xfff4f2ff;
    Argb led = 0xff9fe8c8;
    Argb ledOff = 0xff26262e;
    Argb activeBackground = 0xff252135;
    Argb activeText = 0xffb9a7ff;
    Argb shadow = 0xa6000000;
    Argb lane = 0xff0e0e12; // Playlist track background
    Argb grid = 0xff1b1b22; // Playlist bar lines
    Argb clip = 0xfff2b5c6; // pattern clips
    Argb clipText = 0xff1b1218;

    bool operator==(const Colours&) const = default;
};

struct Theme
{
    // 2: colours only (version 1 also had a font, shapes and effects; they're ignored now).
    static constexpr int currentFormatVersion = 2;

    std::string id; // "blackberry", or "custom:<file name>" for imported themes
    std::string name = "Blackberry";
    std::string author = "Juicy Loops Studio";
    std::string description;
    int formatVersion = currentFormatVersion;
    Colours colours;

    bool operator==(const Theme&) const = default;
};

// "#rrggbb" or "#rrggbbaa" (alpha last, as in CSS). Returns nullopt if malformed.
std::optional<Argb> parseColour(std::string_view text);
// "#rrggbb" when opaque, otherwise "#rrggbbaa".
std::string formatColour(Argb colour);

struct ParseResult
{
    std::optional<Theme> theme;        // empty if the file couldn't be used at all
    std::string error;                 // why theme is empty
    std::vector<std::string> warnings; // problems that were skipped over
};

// Reads a theme file. Missing or invalid values fall back to `fallback` (with a warning
// for invalid ones). Version 1's "font", "shape" and "effects" are skipped without a warning.
// The result's id is left empty; the caller assigns it.
ParseResult parseTheme(std::string_view json, const Theme& fallback = Theme{});

// Writes a complete theme file (every field), pretty-printed.
std::string serializeTheme(const Theme& theme);

// Blackberry, Graphite, in menu order. Blackberry is the default.
const std::vector<Theme>& builtInThemes();
const Theme& defaultTheme();

} // namespace jls::theme
