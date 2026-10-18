#include "ui/oscilloscope.h"

#include "ui/controls.h"
#include "ui/theme.h"

#include <cmath>

namespace jls::ui
{

Oscilloscope::Oscilloscope(const engine::ScopeBuffer& scopeSource) : source(scopeSource)
{
    setInterceptsMouseClicks(false, false);
    setTooltip("Output");
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
    start = engine::findScopeTrigger(samples.data(), history, window);
    float peak = 0.0f;
    for (int i = start; i < start + window; ++i)
        peak = std::max(peak, std::abs(samples[static_cast<std::size_t>(i)]));
    const bool nowQuiet = peak < 1.0e-4f;
    if (nowQuiet && quiet)
        return; // still silent: nothing to redraw
    quiet = nowQuiet;
    repaint();
}

void Oscilloscope::paint(juce::Graphics& g)
{
    const auto& c = currentTheme().colours;
    const auto bounds = getLocalBounds().toFloat();
    paintDisplayBox(g, bounds);
    const auto area = bounds.reduced(6.0f, 4.0f);
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
        for (int i = from; i < to; ++i)
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

} // namespace jls::ui
