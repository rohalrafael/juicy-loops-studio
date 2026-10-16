#pragma once

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

// Member defaults are the Juicy theme: anything a theme file leaves out falls back to these.
struct Colours
{
    Argb background = 0xff17121c;
    Argb panel = 0xff241b2c;
    Argb panelRaised = 0xff302439;
    Argb header = 0xff1d1624;
    Argb outline = 0xff0f0b13;
    Argb text = 0xfff5eef7;
    Argb textDim = 0xffad9cb6;
    Argb accent = 0xffff7a45;
    Argb accentAlt = 0xffb7f04a;
    Argb display = 0xff120e16; // recessed panels: tempo display, knob tracks
    Argb displayText = 0xffb7f04a;
    Argb displayDim = 0xff8a7a93;
    Argb stepOffA = 0xff392d44; // step buttons alternate A/B every beat
    Argb stepOffB = 0xff4b2f3c;
    Argb stepOn = 0xffff7a45;
    Argb playhead = 0xfff5eef7;
    Argb led = 0xffb7f04a;
    Argb ledOff = 0xff3a2f44;
    Argb activeBackground = 0xff3e2d4a;
    Argb activeText = 0xffff7a45;
    Argb shadow = 0x8c000000;

    bool operator==(const Colours&) const = default;
};

struct Shape
{
    float cornerRadius = 6.0f; // buttons and small controls
    float windowRadius = 8.0f; // panels
    float stepRadius = 4.0f;
    float borderWidth = 1.0f;
    float stepOutline = 0.0f; // outline width around step buttons, in the outline colour

    bool operator==(const Shape&) const = default;
};

struct Effects
{
    std::optional<Argb> stepGlow; // glow around lit steps
    std::optional<Argb> ledGlow;
    bool hardShadow = false; // solid offset shadow under panels instead of none

    bool operator==(const Effects&) const = default;
};

struct Theme
{
    static constexpr int currentFormatVersion = 1;

    std::string id; // "juicy", or "custom:<file name>" for imported themes
    std::string name = "Juicy";
    std::string author = "Juicy Loops Studio";
    std::string description;
    int formatVersion = currentFormatVersion;
    Colours colours;
    std::string font = "IBM Plex Sans";
    Shape shape;
    Effects effects;

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
// for invalid ones). The result's id is left empty; the caller assigns it.
ParseResult parseTheme(std::string_view json, const Theme& fallback = Theme{});

// Writes a complete theme file (every field), pretty-printed.
std::string serializeTheme(const Theme& theme);

// Juicy, Citrus, Watermelon, Blueberry, in menu order. Juicy is the default.
const std::vector<Theme>& builtInThemes();
const Theme& defaultTheme();

} // namespace jls::theme
