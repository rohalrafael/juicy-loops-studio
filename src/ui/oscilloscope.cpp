#include "ui/oscilloscope.h"

#include "ui/controls.h"
#include "ui/theme.h"

#include <cmath>

namespace jls::ui
{
namespace
{
constexpr float lowestHz = 30.0f;
constexpr float highestHz = 16000.0f;
constexpr float floorDb = -72.0f;
constexpr float fallPerFrame = 1.5f; // dB a peak drops each redraw
} // namespace

Oscilloscope::Oscilloscope(const engine::ScopeBuffer& scopeSource) : source(scopeSource)
{
    levels.fill(floorDb);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setMode(Mode::waveform);
}

void Oscilloscope::setMode(Mode newMode)
{
    mode = newMode;
    levels.fill(floorDb);
    setTooltip(mode == Mode::waveform ? "Output waveform (click for the spectrum)"
                                      : "Output spectrum (click for the waveform)");
    quiet = false; // redraw once in the new mode
    repaint();
}

void Oscilloscope::mouseDown(const juce::MouseEvent&)
{
    setMode(mode == Mode::waveform ? Mode::spectrum : Mode::waveform);
    if (onModeChange)
        onModeChange(mode);
}

void Oscilloscope::visibilityChanged()
{
    if (isVisible())
        startTimerHz(30);
    else
        stopTimer();
}

void Oscilloscope::timerCallback()
{
    source.read(samples.data(), history);
    float peak = 0.0f;
    for (float s : samples)
        peak = std::max(peak, std::abs(s));
    const bool nowQuiet = peak < 1.0e-4f;
    // Still silent and nothing left to fall back: nothing to redraw.
    const bool settled =
        mode == Mode::waveform ||
        std::all_of(levels.begin(), levels.end(), [](float l) { return l <= floorDb; });
    if (nowQuiet && quiet && settled)
        return;
    quiet = nowQuiet;
    if (mode == Mode::waveform)
        start = engine::findScopeTrigger(samples.data(), history, window);
    else
        updateSpectrum(std::clamp(getWidth() - 12, 1, maxColumns));
    repaint();
}

void Oscilloscope::updateSpectrum(int columns)
{
    engine::spectrumDb(samples.data(), history, work.data(), bins.data());
    const double rate = sampleRate ? sampleRate() : 48000.0;
    const double binHz = rate > 0.0 ? rate / history : 48000.0 / history;
    const int lastBin = static_cast<int>(bins.size()) - 1;
    for (int x = 0; x < columns; ++x)
    {
        // Log frequency axis: each column covers a range of bins (at least one).
        const auto hzAt = [columns](double position)
        { return lowestHz * std::pow(highestHz / lowestHz, position / columns); };
        const int from = std::clamp(static_cast<int>(hzAt(x) / binHz), 1, lastBin);
        const int to = std::clamp(static_cast<int>(hzAt(x + 1) / binHz), from, lastBin);
        float level = floorDb;
        for (int b = from; b <= to; ++b)
            level = std::max(level, bins[static_cast<std::size_t>(b)]);
        auto& shown = levels[static_cast<std::size_t>(x)];
        shown = std::max(level, shown - fallPerFrame);
    }
}

void Oscilloscope::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    paintDisplayBox(g, bounds);
    const auto area = bounds.reduced(6.0f, 4.0f);
    if (mode == Mode::waveform)
        paintWaveform(g, area);
    else
        paintSpectrum(g, area);
}

void Oscilloscope::paintWaveform(juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& c = currentTheme().colours;
    const float mid = area.getCentreY();
    if (quiet)
    {
        g.setColour(colour(c.displayDim).withAlpha(0.6f));
        g.fillRect(area.getX(), mid - 0.5f, area.getWidth(), 1.0f);
        return;
    }
    // One point per pixel column: the sample with the largest swing in that column, so peaks
    // don't disappear between pixels.
    juce::Path line;
    const int columns = std::max(1, static_cast<int>(area.getWidth()));
    const float half = area.getHeight() / 2.0f;
    for (int x = 0; x < columns; ++x)
    {
        const int from = start + x * window / columns;
        const int to = std::max(from + 1, start + (x + 1) * window / columns);
        float value = 0.0f;
        for (int i = from; i < to && i < history; ++i)
            if (std::abs(samples[static_cast<std::size_t>(i)]) > std::abs(value))
                value = samples[static_cast<std::size_t>(i)];
        const float y = mid - std::clamp(value, -1.0f, 1.0f) * half;
        const float px = area.getX() + static_cast<float>(x);
        if (x == 0)
            line.startNewSubPath(px, y);
        else
            line.lineTo(px, y);
    }
    g.setColour(colour(c.accentAlt));
    g.strokePath(line, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
}

void Oscilloscope::paintSpectrum(juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& c = currentTheme().colours;
    const int columns = std::clamp(static_cast<int>(area.getWidth()), 1, maxColumns);
    const auto yFor = [&area](float db)
    {
        const float t = std::clamp((db - floorDb) / -floorDb, 0.0f, 1.0f);
        return area.getBottom() - t * area.getHeight();
    };
    juce::Path line;
    for (int x = 0; x < columns; ++x)
    {
        const float px = area.getX() + static_cast<float>(x);
        const float y = yFor(levels[static_cast<std::size_t>(x)]);
        if (x == 0)
            line.startNewSubPath(px, y);
        else
            line.lineTo(px, y);
    }
    juce::Path fill(line);
    fill.lineTo(area.getX() + static_cast<float>(columns - 1), area.getBottom());
    fill.lineTo(area.getX(), area.getBottom());
    fill.closeSubPath();
    g.setColour(colour(c.accentAlt).withAlpha(0.22f));
    g.fillPath(fill);
    g.setColour(colour(c.accentAlt));
    g.strokePath(line, juce::PathStrokeType(1.25f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
}

} // namespace jls::ui
