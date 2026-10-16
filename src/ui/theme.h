#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// All colours and sizes used by the UI live here, so the look can be changed in one place.
// Placeholder look for now; the visual design is still to be decided with the user.
namespace jls::ui::theme
{

namespace colours
{
inline const juce::Colour background{0xff2b3035};
inline const juce::Colour panel{0xff363c42};
inline const juce::Colour panelOutline{0xff1f2327};
inline const juce::Colour text{0xffdfe3e6};
inline const juce::Colour textDim{0xff8a949c};
inline const juce::Colour accent{0xffff9a3c};

// Step buttons alternate colour every 4 steps (one beat) so the grid is easy to read.
inline const juce::Colour stepOffA{0xff4a5158};
inline const juce::Colour stepOffB{0xff5b4b52};
inline const juce::Colour stepOn{0xffd8dde1};
inline const juce::Colour playhead{0xffff9a3c};

inline const juce::Colour ledOn{0xff7cdc5a};
inline const juce::Colour ledOff{0xff3b4146};
inline const juce::Colour knobTrack{0xff23272b};
inline const juce::Colour knobValue{0xffff9a3c};
inline const juce::Colour dropHighlight{0x40ff9a3c};
} // namespace colours

namespace metrics
{
inline constexpr int rowHeight = 30;
inline constexpr int rowGap = 3;
inline constexpr int ledWidth = 14;
inline constexpr int knobSize = 24;
inline constexpr int nameWidth = 110;
inline constexpr int stepWidth = 20;
inline constexpr int stepGap = 3;
inline constexpr int beatGap = 6; // extra space between groups of 4 steps
inline constexpr float cornerRadius = 3.0f;
} // namespace metrics

// Applies the theme to standard JUCE widgets and draws knobs.
class LookAndFeel final : public juce::LookAndFeel_V4
{
  public:
    LookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float position,
                          float startAngle, float endAngle, juce::Slider&) override;
};

} // namespace jls::ui::theme
