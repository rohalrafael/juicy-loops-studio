#include "theme/theme.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>

namespace jls::theme
{
namespace
{

using nlohmann::ordered_json;

int hexValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// One table drives parsing and serializing, so the two can't drift apart.
struct ColourField
{
    const char* key;
    Argb Colours::* member;
};

constexpr std::array colourFields{
    ColourField{"background", &Colours::background},
    ColourField{"panel", &Colours::panel},
    ColourField{"panelRaised", &Colours::panelRaised},
    ColourField{"header", &Colours::header},
    ColourField{"outline", &Colours::outline},
    ColourField{"text", &Colours::text},
    ColourField{"textDim", &Colours::textDim},
    ColourField{"accent", &Colours::accent},
    ColourField{"accentAlt", &Colours::accentAlt},
    ColourField{"display", &Colours::display},
    ColourField{"displayText", &Colours::displayText},
    ColourField{"displayDim", &Colours::displayDim},
    ColourField{"stepOn", &Colours::stepOn},
    ColourField{"playhead", &Colours::playhead},
    ColourField{"led", &Colours::led},
    ColourField{"ledOff", &Colours::ledOff},
    ColourField{"activeBackground", &Colours::activeBackground},
    ColourField{"activeText", &Colours::activeText},
    ColourField{"shadow", &Colours::shadow},
    ColourField{"lane", &Colours::lane},
    ColourField{"grid", &Colours::grid},
    ColourField{"clip", &Colours::clip},
    ColourField{"clipText", &Colours::clipText},
};

struct ShapeField
{
    const char* key;
    float Shape::* member;
    float min;
    float max;
};

constexpr std::array shapeFields{
    ShapeField{"cornerRadius", &Shape::cornerRadius, 0.0f, 24.0f},
    ShapeField{"windowRadius", &Shape::windowRadius, 0.0f, 32.0f},
    ShapeField{"stepRadius", &Shape::stepRadius, 0.0f, 12.0f},
    ShapeField{"borderWidth", &Shape::borderWidth, 0.0f, 4.0f},
    ShapeField{"stepOutline", &Shape::stepOutline, 0.0f, 4.0f},
};

class Reader
{
  public:
    explicit Reader(ParseResult& r) : result(r) {}

    void warn(std::string message) { result.warnings.push_back(std::move(message)); }

    void colour(const ordered_json& obj, const std::string& path, const char* key, Argb& out)
    {
        const auto it = obj.find(key);
        if (it == obj.end())
            return;
        if (!it->is_string())
        {
            warn(path + key + ": expected a colour like \"#ff7a45\"");
            return;
        }
        if (const auto c = parseColour(it->get<std::string>()))
            out = *c;
        else
            warn(path + key + ": \"" + it->get<std::string>() + "\" is not a colour");
    }

    void number(const ordered_json& obj, const std::string& path, const ShapeField& field,
                Shape& shape)
    {
        const auto it = obj.find(field.key);
        if (it == obj.end())
            return;
        if (!it->is_number())
        {
            warn(path + field.key + ": expected a number");
            return;
        }
        const auto value = it->get<double>();
        const auto clamped = std::clamp(static_cast<float>(value), field.min, field.max);
        if (static_cast<double>(clamped) != value)
            warn(path + field.key + ": " + std::to_string(value) + " is out of range, using " +
                 std::to_string(clamped));
        shape.*field.member = clamped;
    }

    void text(const ordered_json& obj, const char* key, std::string& out)
    {
        const auto it = obj.find(key);
        if (it == obj.end())
            return;
        if (it->is_string())
            out = it->get<std::string>();
        else
            warn(std::string(key) + ": expected text");
    }

    void optionalColour(const ordered_json& obj, const std::string& path, const char* key,
                        std::optional<Argb>& out)
    {
        const auto it = obj.find(key);
        if (it == obj.end())
            return;
        if (it->is_null() || (it->is_boolean() && !it->get<bool>()))
            out.reset();
        else if (!it->is_string())
            warn(path + key + ": expected a colour or false");
        else if (const auto c = parseColour(it->get<std::string>()))
            out = *c;
        else
            warn(path + key + ": \"" + it->get<std::string>() + "\" is not a colour");
    }

    void unknownKeys(const ordered_json& obj, const std::string& path,
                     std::initializer_list<std::string_view> known)
    {
        for (const auto& [key, value] : obj.items())
            if (std::find(known.begin(), known.end(), key) == known.end())
                warn(path + key + ": unknown setting, ignored");
    }

  private:
    ParseResult& result;
};

} // namespace

std::optional<Argb> parseColour(std::string_view text)
{
    if (text.size() != 7 && text.size() != 9)
        return std::nullopt;
    if (text[0] != '#')
        return std::nullopt;

    Argb rgba = 0;
    for (std::size_t i = 1; i < text.size(); ++i)
    {
        const int v = hexValue(text[i]);
        if (v < 0)
            return std::nullopt;
        rgba = (rgba << 4) | static_cast<Argb>(v);
    }

    if (text.size() == 7)
        return 0xff000000u | rgba;
    return (rgba >> 8) | ((rgba & 0xffu) << 24); // #rrggbbaa -> 0xaarrggbb
}

std::string formatColour(Argb colour)
{
    const unsigned alpha = colour >> 24;
    const unsigned rgb = colour & 0xffffffu;
    char buffer[10];
    if (alpha == 0xff)
        std::snprintf(buffer, sizeof(buffer), "#%06x", rgb);
    else
        std::snprintf(buffer, sizeof(buffer), "#%06x%02x", rgb, alpha);
    return buffer;
}

ParseResult parseTheme(std::string_view json, const Theme& fallback)
{
    ParseResult result;
    const auto root = ordered_json::parse(json, nullptr, false);
    if (root.is_discarded())
    {
        result.error = "Not a valid JSON file.";
        return result;
    }
    if (!root.is_object())
    {
        result.error = "A theme file must contain a JSON object ({ ... }).";
        return result;
    }

    Reader read(result);
    Theme theme = fallback;
    theme.id.clear();

    const auto name = root.find("name");
    if (name == root.end() || !name->is_string() || name->get<std::string>().empty())
    {
        result.error = "The theme has no \"name\".";
        return result;
    }
    theme.name = name->get<std::string>();
    theme.author.clear();
    theme.description.clear();
    read.text(root, "author", theme.author);
    read.text(root, "description", theme.description);
    read.text(root, "font", theme.font);

    if (const auto v = root.find("formatVersion"); v != root.end())
    {
        if (v->is_number_integer() && v->get<int>() > Theme::currentFormatVersion)
            read.warn("This theme was made for a newer version of Juicy Loops Studio; some "
                      "settings may be ignored.");
        else if (!v->is_number_integer())
            read.warn("formatVersion: expected a whole number");
    }
    theme.formatVersion = Theme::currentFormatVersion;

    if (const auto c = root.find("colours"); c != root.end())
    {
        if (!c->is_object())
            read.warn("colours: expected an object");
        else
        {
            for (const auto& field : colourFields)
                read.colour(*c, "colours.", field.key, theme.colours.*field.member);

            if (const auto steps = c->find("stepOff"); steps != c->end())
            {
                if (steps->is_array() && steps->size() == 2 && (*steps)[0].is_string() &&
                    (*steps)[1].is_string() && parseColour((*steps)[0].get<std::string>()) &&
                    parseColour((*steps)[1].get<std::string>()))
                {
                    theme.colours.stepOffA = *parseColour((*steps)[0].get<std::string>());
                    theme.colours.stepOffB = *parseColour((*steps)[1].get<std::string>());
                }
                else
                    read.warn(
                        "colours.stepOff: expected two colours, like [\"#392d44\", \"#4b2f3c\"]");
            }

            read.unknownKeys(*c, "colours.", {"background", "panel",       "panelRaised",
                                              "header",     "outline",     "text",
                                              "textDim",    "accent",      "accentAlt",
                                              "display",    "displayText", "displayDim",
                                              "stepOff",    "stepOn",      "playhead",
                                              "led",        "ledOff",      "activeBackground",
                                              "activeText", "shadow",      "lane",
                                              "grid",       "clip",        "clipText"});
        }
    }

    if (const auto s = root.find("shape"); s != root.end())
    {
        if (!s->is_object())
            read.warn("shape: expected an object");
        else
        {
            for (const auto& field : shapeFields)
                read.number(*s, "shape.", field, theme.shape);
            read.unknownKeys(
                *s, "shape.",
                {"cornerRadius", "windowRadius", "stepRadius", "borderWidth", "stepOutline"});
        }
    }

    if (const auto e = root.find("effects"); e != root.end())
    {
        if (!e->is_object())
            read.warn("effects: expected an object");
        else
        {
            read.optionalColour(*e, "effects.", "stepGlow", theme.effects.stepGlow);
            read.optionalColour(*e, "effects.", "ledGlow", theme.effects.ledGlow);
            if (const auto h = e->find("hardShadow"); h != e->end())
            {
                if (h->is_boolean())
                    theme.effects.hardShadow = h->get<bool>();
                else
                    read.warn("effects.hardShadow: expected true or false");
            }
            read.unknownKeys(*e, "effects.", {"stepGlow", "ledGlow", "hardShadow"});
        }
    }

    read.unknownKeys(
        root, "",
        {"name", "author", "description", "formatVersion", "font", "colours", "shape", "effects"});

    result.theme = std::move(theme);
    return result;
}

std::string serializeTheme(const Theme& theme)
{
    ordered_json colours = ordered_json::object();
    for (const auto& field : colourFields)
    {
        colours[field.key] = formatColour(theme.colours.*field.member);
        if (std::string_view(field.key) == "displayDim") // keep the file in a readable order
            colours["stepOff"] = {formatColour(theme.colours.stepOffA),
                                  formatColour(theme.colours.stepOffB)};
    }

    ordered_json shape = ordered_json::object();
    for (const auto& field : shapeFields)
        shape[field.key] = theme.shape.*field.member;

    auto optionalColour = [](const std::optional<Argb>& c)
    { return c ? ordered_json(formatColour(*c)) : ordered_json(false); };

    ordered_json root = {
        {"name", theme.name},
        {"author", theme.author},
        {"description", theme.description},
        {"formatVersion", Theme::currentFormatVersion},
        {"font", theme.font},
        {"colours", colours},
        {"shape", shape},
        {"effects",
         {{"stepGlow", optionalColour(theme.effects.stepGlow)},
          {"ledGlow", optionalColour(theme.effects.ledGlow)},
          {"hardShadow", theme.effects.hardShadow}}},
    };
    return root.dump(2) + "\n";
}

} // namespace jls::theme
