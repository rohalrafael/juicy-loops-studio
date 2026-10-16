#include "ui/theme.h"

namespace jls::ui::theme
{

LookAndFeel::LookAndFeel()
{
    using namespace colours;
    setColour(juce::ResizableWindow::backgroundColourId, background);
    setColour(juce::DocumentWindow::backgroundColourId, background);
    setColour(juce::TextButton::buttonColourId, panel);
    setColour(juce::TextButton::buttonOnColourId, accent.darker(0.3f));
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::TextButton::textColourOnId, text);
    setColour(juce::ComboBox::backgroundColourId, panel);
    setColour(juce::ComboBox::outlineColourId, panelOutline);
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::Label::textColourId, text);
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxBackgroundColourId, panel);
    setColour(juce::Slider::textBoxOutlineColourId, panelOutline);
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accent.darker(0.3f));
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                   float position, float startAngle, float endAngle,
                                   juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2.0f);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float lineWidth = std::max(2.0f, radius * 0.22f);
    const float arcRadius = radius - lineWidth / 2.0f;

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour(colours::knobTrack);
    g.strokePath(track, juce::PathStrokeType(lineWidth, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Bipolar knobs (pan) fill from the centre, others from the start.
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float fromAngle = bipolar ? (startAngle + endAngle) / 2.0f : startAngle;
    const float toAngle = startAngle + position * (endAngle - startAngle);

    juce::Path value;
    value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, fromAngle, toAngle, true);
    g.setColour(slider.isEnabled() ? colours::knobValue : colours::textDim);
    g.strokePath(value, juce::PathStrokeType(lineWidth, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    const juce::Point<float> tip(centre.x + arcRadius * 0.6f * std::sin(toAngle),
                                 centre.y - arcRadius * 0.6f * std::cos(toAngle));
    g.setColour(colours::text);
    g.drawLine({centre, tip}, lineWidth * 0.7f);
}

} // namespace jls::ui::theme
