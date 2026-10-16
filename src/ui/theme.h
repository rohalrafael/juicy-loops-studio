#pragma once

#include "theme/theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

// UI side of theming: the active theme, the LookAndFeel built from it, bundled fonts,
// user theme files and the saved choice. Colours and shapes come from the active theme;
// only layout sizes (metrics) are fixed here.
namespace jls::ui
{

namespace metrics
{
inline constexpr int rowHeight = 30;
inline constexpr int rowGap = 3;
inline constexpr int ledWidth = 14;
inline constexpr int knobSize = 24;
inline constexpr int insertWidth = 28; // the Channel Rack's mixer track box
inline constexpr int nameWidth = 110;
inline constexpr int stepWidth = 20;
inline constexpr int stepGap = 3;
inline constexpr int beatGap = 6; // extra space between groups of 4 steps
} // namespace metrics

inline juce::Colour colour(theme::Argb argb)
{
    return juce::Colour(argb);
}

// The active theme. Valid while a ThemeManager exists (before that: the default theme).
const theme::Theme& currentTheme();

class LookAndFeel final : public juce::LookAndFeel_V4
{
  public:
    void applyTheme(const theme::Theme& theme, juce::Typeface::Ptr bundledFont);

    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& background,
                              bool highlighted, bool down) override;
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float position,
                          float startAngle, float endAngle, juce::Slider&) override;
};

class ThemeManager final : public juce::ChangeBroadcaster
{
  public:
    ThemeManager();
    ~ThemeManager() override;

    const theme::Theme& current() const { return active; }
    // Built-in themes first, then the user's, in file-name order.
    std::vector<theme::Theme> available() const;
    // Switches theme (unknown ids fall back to the default) and remembers the choice.
    void select(const std::string& id);

    juce::File themesFolder() const;
    // Re-reads the themes folder (e.g. after the user edited a file there).
    void rescan();

    // Validates a theme file, copies it into the themes folder and selects it.
    // On success `message` lists any problems that were skipped over.
    struct ImportResult
    {
        bool ok = false;
        juce::String message;
    };
    ImportResult importFile(const juce::File& file);
    juce::Result exportCurrent(const juce::File& destination) const;

    juce::LookAndFeel& lookAndFeel() { return laf; }

  private:
    void apply();
    juce::Typeface::Ptr bundledFont(const std::string& family) const;

    LookAndFeel laf;
    std::unique_ptr<juce::PropertiesFile> settings;
    std::map<juce::String, juce::Typeface::Ptr> fonts; // lower-case family name -> typeface
    std::vector<theme::Theme> userThemes;
    theme::Theme active;
};

} // namespace jls::ui
