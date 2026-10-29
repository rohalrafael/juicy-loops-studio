// Themes: parsing and writing theme files, defaults, the built-in themes.

#include "theme/theme.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

using namespace jls::theme;

namespace
{

bool hasWarningContaining(const ParseResult& r, const std::string& text)
{
    return std::any_of(r.warnings.begin(), r.warnings.end(),
                       [&](const std::string& w) { return w.find(text) != std::string::npos; });
}

} // namespace

TEST_CASE("Colours parse from #rrggbb and #rrggbbaa")
{
    CHECK(parseColour("#ff7a45") == 0xffff7a45u);
    CHECK(parseColour("#FF7A45") == 0xffff7a45u);
    CHECK(parseColour("#0000008c") == 0x8c000000u);
    CHECK(parseColour("#ff3d6ecc") == 0xccff3d6eu);

    CHECK_FALSE(parseColour("ff7a45").has_value());
    CHECK_FALSE(parseColour("#ff7a4").has_value());
    CHECK_FALSE(parseColour("#ff7a4g").has_value());
    CHECK_FALSE(parseColour("").has_value());
    CHECK_FALSE(parseColour("red").has_value());
}

TEST_CASE("Colours format back to text")
{
    CHECK(formatColour(0xffff7a45u) == "#ff7a45");
    CHECK(formatColour(0x8c000000u) == "#0000008c");
    for (const char* text : {"#17121c", "#ff3d6ecc", "#00000000"})
        CHECK(formatColour(*parseColour(text)) == text);
}

TEST_CASE("The built-in themes load, Blackberry first and default")
{
    const auto& themes = builtInThemes();
    REQUIRE(themes.size() == 2);
    CHECK(themes[0].id == "blackberry");
    CHECK(themes[1].id == "graphite");
    CHECK(defaultTheme().id == "blackberry");
    CHECK(themes[1].colours.accent == 0xff5aa9ffu);
}

TEST_CASE("The Blackberry theme file matches the built-in defaults")
{
    // Theme's member defaults are the fallback for every file, so they must equal Blackberry.
    Theme defaults;
    defaults.id = "blackberry";
    defaults.description = "Near-black with lavender, mint and pink";
    CHECK(defaultTheme() == defaults);
}

TEST_CASE("A minimal theme only needs a name; the rest falls back")
{
    const auto r = parseTheme(R"({ "name": "Tiny", "colours": { "accent": "#5ee0d0" } })");
    REQUIRE(r.theme);
    CHECK(r.warnings.empty());
    CHECK(r.theme->name == "Tiny");
    CHECK(r.theme->colours.accent == 0xff5ee0d0u);
    CHECK(r.theme->colours.background == defaultTheme().colours.background);
    CHECK(r.theme->author.empty());
}

TEST_CASE("Unusable files are rejected with a reason")
{
    CHECK(parseTheme("not json").error == "Not a valid JSON file.");
    CHECK_FALSE(parseTheme("[1, 2]").theme);
    CHECK_FALSE(parseTheme(R"({ "colours": {} })").theme);
    CHECK(parseTheme(R"({ "name": "" })").error.find("name") != std::string::npos);
}

TEST_CASE("Bad values are skipped with a warning, not fatal")
{
    const auto r = parseTheme(R"({
        "name": "Messy",
        "colours": { "accent": "orange", "text": 42, "stepOff": ["#111111"], "sparkle": "#ffffff" },
        "extra": true
    })");
    REQUIRE(r.theme);
    const auto& t = *r.theme;
    const auto& d = defaultTheme();

    CHECK(t.colours.accent == d.colours.accent);
    CHECK(t.colours.text == d.colours.text);
    CHECK(t.colours.stepOffA == d.colours.stepOffA);

    CHECK(hasWarningContaining(r, "colours.accent"));
    CHECK(hasWarningContaining(r, "colours.text"));
    CHECK(hasWarningContaining(r, "colours.stepOff"));
    CHECK(hasWarningContaining(r, "colours.sparkle"));
    CHECK(hasWarningContaining(r, "extra"));
}

TEST_CASE("Themes from a newer version still load, with a warning")
{
    const auto r = parseTheme(R"({ "name": "Future", "formatVersion": 7 })");
    REQUIRE(r.theme);
    CHECK(hasWarningContaining(r, "newer version"));
    CHECK(r.theme->formatVersion == Theme::currentFormatVersion);
}

TEST_CASE("Version 1 themes load their colours; font, shapes and effects are skipped quietly")
{
    // Themes used to set a font, corner radii and glows; now they only set colours.
    const auto r = parseTheme(R"({
        "name": "Old", "formatVersion": 1, "font": "Sora",
        "colours": { "accent": "#ff7a45" },
        "shape": { "cornerRadius": 6, "windowRadius": 8 },
        "effects": { "stepGlow": "#ff000080", "hardShadow": true }
    })");
    REQUIRE(r.theme);
    CHECK(r.warnings.empty());
    CHECK(r.theme->colours.accent == 0xffff7a45u);
    CHECK(r.theme->formatVersion == Theme::currentFormatVersion);
}

TEST_CASE("Exported themes contain only colours")
{
    const auto text = serializeTheme(defaultTheme());
    CHECK(text.find("\"colours\"") != std::string::npos);
    CHECK(text.find("\"formatVersion\": 2") != std::string::npos);
    for (const char* gone : {"\"font\"", "\"shape\"", "\"effects\""})
        CHECK(text.find(gone) == std::string::npos);
}

TEST_CASE("Exported themes read back identically")
{
    for (const auto& builtIn : builtInThemes())
    {
        INFO(builtIn.name);
        const auto r = parseTheme(serializeTheme(builtIn));
        REQUIRE(r.theme);
        CHECK(r.warnings.empty());
        auto copy = *r.theme;
        copy.id = builtIn.id;
        CHECK(copy == builtIn);
    }
}

TEST_CASE("Themes without a chrome colour use their header colour for it")
{
    // "chrome" is newer than "header", which used to colour the title and transport bars too.
    const auto r = parseTheme(R"({ "name": "Old", "colours": { "header": "#123456" } })");
    REQUIRE(r.theme);
    CHECK(r.warnings.empty());
    CHECK(r.theme->colours.chrome == 0xff123456u);

    const auto withChrome =
        parseTheme(R"({ "name": "New", "colours": { "header": "#123456", "chrome": "#abcdef" } })");
    REQUIRE(withChrome.theme);
    CHECK(withChrome.theme->colours.chrome == 0xffabcdefu);
}
