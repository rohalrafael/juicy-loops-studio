#pragma once

// The transport bar's oscilloscope: the app's output as a line in a display box, redrawn about
// 30 times a second from engine::ScopeBuffer. It starts on a rising zero crossing, so a steady
// tone stands still instead of scrolling.

#include "engine/scope.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace jls::ui
{

class Oscilloscope final : public juce::Component,
                           public juce::SettableTooltipClient,
                           private juce::Timer
{
  public:
    explicit Oscilloscope(const engine::ScopeBuffer& source);

    void paint(juce::Graphics& g) override;
    void visibilityChanged() override;

  private:
    static constexpr int history = engine::ScopeBuffer::size; // samples read each time
    static constexpr int window = 2048; // samples shown (~43 ms at 48 kHz: a few bass cycles)

    void timerCallback() override;

    const engine::ScopeBuffer& source;
    std::array<float, history> samples{};
    int start = 0;     // first shown sample (the trigger)
    bool quiet = true; // nothing above the noise floor: drawn as a dim flat line
};

} // namespace jls::ui
