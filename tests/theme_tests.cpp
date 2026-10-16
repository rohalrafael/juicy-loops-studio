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

TEST_CASE("The four built-in themes load, Juicy first and default")
{
    const auto& themes = builtInThemes();
    REQUIRE(themes.size() == 4);
    CHECK(themes[0].id == "juicy");
    CHECK(themes[1].id == "citrus");
    CHECK(themes[2].id == "watermelon");
    CHECK(themes[3].id == "blueberry");
    CHECK(defaultTheme().id == "juicy");

    CHECK(themes[1].font == "Bricolage Grotesque");
    CHECK(themes[1].effects.hardShadow);
    CHECK(themes[2].effects.stepGlow == 0xccff3d6eu);
    CHECK(themes[3].shape.windowRadius == 20.0f);
}

TEST_CASE("The Juicy theme file matches the built-in defaults")
{
    // Theme's member defaults are the fallback for every file, so they must equal Juicy.
    Theme defaults;
    defaults.id = "juicy";
    defaults.description = "Plum, blood orange and lime";
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
    CHECK(r.theme->font == defaultTheme().font);
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
        "shape": { "cornerRadius": 100, "stepRadius": "big" },
        "effects": { "stepGlow": "#nothex", "hardShadow": "yes" },
        "extra": true
    })");
    REQUIRE(r.theme);
    const auto& t = *r.theme;
    const auto& d = defaultTheme();

    CHECK(t.colours.accent == d.colours.accent);
    CHECK(t.colours.text == d.colours.text);
    CHECK(t.colours.stepOffA == d.colours.stepOffA);
    CHECK(t.shape.cornerRadius == 24.0f); // clamped to max
    CHECK(t.shape.stepRadius == d.shape.stepRadius);
    CHECK_FALSE(t.effects.stepGlow);
    CHECK_FALSE(t.effects.hardShadow);

    CHECK(hasWarningContaining(r, "colours.accent"));
    CHECK(hasWarningContaining(r, "colours.text"));
    CHECK(hasWarningContaining(r, "colours.stepOff"));
    CHECK(hasWarningContaining(r, "colours.sparkle"));
    CHECK(hasWarningContaining(r, "shape.cornerRadius"));
    CHECK(hasWarningContaining(r, "shape.stepRadius"));
    CHECK(hasWarningContaining(r, "effects.stepGlow"));
    CHECK(hasWarningContaining(r, "effects.hardShadow"));
    CHECK(hasWarningContaining(r, "extra"));
}

TEST_CASE("Themes from a newer version still load, with a warning")
{
    const auto r = parseTheme(R"({ "name": "Future", "formatVersion": 7 })");
    REQUIRE(r.theme);
    CHECK(hasWarningContaining(r, "newer version"));
    CHECK(r.theme->formatVersion == Theme::currentFormatVersion);
}

TEST_CASE("Glow can be switched off with false or null")
{
    Theme glowing = defaultTheme();
    glowing.effects.stepGlow = 0xffff0000u;
    CHECK_FALSE(parseTheme(R"({ "name": "a", "effects": { "stepGlow": false } })", glowing)
                    .theme->effects.stepGlow);
    CHECK_FALSE(parseTheme(R"({ "name": "a", "effects": { "stepGlow": null } })", glowing)
                    .theme->effects.stepGlow);
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
