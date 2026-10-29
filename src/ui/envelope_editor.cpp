#include "ui/envelope_editor.h"

#include "ui/controls.h"
#include "ui/theme.h"

namespace jls::ui
{
namespace
{

constexpr int graphHeight = 64;
constexpr int knobSize = 34;
const std::array<const char*, 6> names{"Delay", "Attack", "Hold", "Decay", "Sustain", "Release"};

double& field(model::Envelope& e, std::size_t i)
{
    switch (i)
    {
    case 0:
        return e.delay;
    case 1:
        return e.attack;
    case 2:
        return e.hold;
    case 3:
        return e.decay;
    case 4:
        return e.sustain;
    default:
        return e.release;
    }
}

juce::String valueText(std::size_t knob, double value)
{
    if (knob == 4)
        return juce::String(juce::roundToInt(value * 100.0)) + " %";
    if (value < 1.0)
        return juce::String(juce::roundToInt(value * 1000.0)) + " ms";
    return juce::String(value, 2) + " s";
}

} // namespace

EnvelopeEditor::EnvelopeEditor()
{
    for (std::size_t i = 0; i < knobs.size(); ++i)
    {
        auto& knob = knobs[i];
        knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        if (i == 4)
            knob.setRange(0.0, 1.0);
        else
        {
            knob.setRange(0.0, 10.0);
            knob.setSkewFactorFromMidPoint(0.5); // half a second at the top of the dial
        }
        knob.setValue(field(current, i), juce::dontSendNotification);
        knob.setDoubleClickReturnValue(true, field(current, i));
        knob.setWantsKeyboardFocus(false);
        knob.setTooltip(names[i]);
        knob.onDragStart = [this] { gesture = newGestureId ? newGestureId() : 0; };
        knob.onValueChange = [this, i]
        {
            field(current, i) = knobs[i].getValue();
            repaint();
            if (onChange)
                onChange(current, gesture);
        };
        addAndMakeVisible(knob);
    }
}

void EnvelopeEditor::setEnvelope(const model::Envelope& envelope)
{
    if (envelope == current)
        return;
    current = envelope;
    for (std::size_t i = 0; i < knobs.size(); ++i)
        knobs[i].setValue(field(current, i), juce::dontSendNotification);
    repaint();
}

void EnvelopeEditor::resized()
{
    auto area = getLocalBounds();
    graph = area.removeFromTop(graphHeight);
    area.removeFromTop(8);
    const int column = area.getWidth() / static_cast<int>(knobs.size());
    for (auto& knob : knobs)
        knob.setBounds(area.removeFromLeft(column).removeFromTop(knobSize).withSizeKeepingCentre(
            knobSize, knobSize));
}

void EnvelopeEditor::paint(juce::Graphics& g)
{
    const auto& t = currentTheme();
    paintDisplayBox(g, graph.toFloat());

    // The shape: each time stage gets width by its length (sustain a fixed stretch), so short
    // stages stay visible.
    const auto& e = current;
    const auto area = graph.toFloat().reduced(8.0f, 8.0f);
    const std::array<double, 6> lengths{
        std::sqrt(e.delay),  std::sqrt(e.attack), std::sqrt(e.hold), std::sqrt(e.decay), 0.6,
        std::sqrt(e.release)};
    double total = 0.0;
    for (double l : lengths)
        total += l;
    const float unit = area.getWidth() / static_cast<float>(std::max(total, 1.0e-6));
    const auto y = [&area](double level)
    { return area.getBottom() - static_cast<float>(level) * area.getHeight(); };
    float x = area.getX();
    juce::Path shape;
    shape.startNewSubPath(x, y(0.0));
    x += static_cast<float>(lengths[0]) * unit;
    shape.lineTo(x, y(0.0));
    x += static_cast<float>(lengths[1]) * unit;
    shape.lineTo(x, y(1.0));
    x += static_cast<float>(lengths[2]) * unit;
    shape.lineTo(x, y(1.0));
    x += static_cast<float>(lengths[3]) * unit;
    shape.lineTo(x, y(e.sustain));
    x += static_cast<float>(lengths[4]) * unit;
    shape.lineTo(x, y(e.sustain));
    x += static_cast<float>(lengths[5]) * unit;
    shape.lineTo(x, y(0.0));

    juce::Path fill(shape);
    fill.closeSubPath();
    const auto ink = colour(t.colours.accentAlt).withMultipliedAlpha(e.on ? 1.0f : 0.35f);
    g.setColour(ink.withAlpha(0.18f * ink.getFloatAlpha()));
    g.fillPath(fill);
    g.setColour(ink);
    g.strokePath(shape, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Labels and values under the knobs.
    for (std::size_t i = 0; i < knobs.size(); ++i)
    {
        const auto below = knobs[i].getBounds().withY(knobs[i].getBottom() + 2).withHeight(30);
        const auto column = below.withSizeKeepingCentre(getWidth() / 6, 30);
        g.setColour(colour(t.colours.text));
        g.setFont(mediumFont(metrics::textSmall));
        g.drawText(names[i], column.withHeight(15), juce::Justification::centred, false);
        g.setColour(colour(t.colours.textDim));
        g.setFont(juce::FontOptions(metrics::textTiny));
        auto copy = current;
        g.drawText(valueText(i, field(copy, i)), column.withTrimmedTop(15),
                   juce::Justification::centred, false);
    }
}

} // namespace jls::ui
