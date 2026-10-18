#pragma once

// The transport bar's output display, redrawn about 30 times a second from engine::ScopeBuffer.
// Click it to switch between:
//   - the oscilloscope: the waveform, started on a rising zero crossing so a steady tone stands
//     still instead of scrolling
//   - the spectrum: levels by frequency (log scale, 30 Hz - 16 kHz, -72 - 0 dB), with peaks
//     that fall back slowly

#include "engine/scope.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

namespace jls::ui
{

class Oscilloscope final : public juce::Component,
                           public juce::SettableTooltipClient,
                           private juce::Timer
{
  public:
    enum class Mode
    {
        waveform,
        spectrum,
    };

    explicit Oscilloscope(const engine::ScopeBuffer& source);

    void setMode(Mode newMode);
    Mode getMode() const { return mode; }
    std::function<void(Mode)> onModeChange; // after a click switched it (to remember it)
    std::function<double()> sampleRate;     // of the output; the spectrum's frequency axis

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void visibilityChanged() override;

  private:
    static constexpr int history = engine::ScopeBuffer::size; // samples read each time
    static constexpr int window = 2048; // waveform: samples shown (~43 ms at 48 kHz)
    static constexpr int maxColumns = 512;

    void timerCallback() override;
    void paintWaveform(juce::Graphics& g, juce::Rectangle<float> area);
    void paintSpectrum(juce::Graphics& g, juce::Rectangle<float> area);
    void updateSpectrum(int columns);

    const engine::ScopeBuffer& source;
    Mode mode = Mode::waveform;
    std::array<float, history> samples{};
    int start = 0;     // waveform: first shown sample (the trigger)
    bool quiet = true; // nothing above the noise floor: drawn as a dim flat line
    std::array<float, 2 * history> work{};
    std::array<float, history / 2> bins{};
    std::array<float, maxColumns> levels{}; // spectrum per pixel column, dB, with fall-back
};

} // namespace jls::ui
