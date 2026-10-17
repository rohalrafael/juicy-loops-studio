#include "ui/controls.h"

#include "BinaryData.h"
#include "ui/theme.h"

#include <cmath>

namespace jls::ui
{
namespace
{

// Icon outlines on a 24 x 24 grid. Most are stroked; play, pause and stop are filled.
juce::Path iconPath(Icon icon, bool& filled)
{
    juce::Path p;
    filled = false;
    switch (icon)
    {
    case Icon::add:
        p.startNewSubPath(12, 5);
        p.lineTo(12, 19);
        p.startNewSubPath(5, 12);
        p.lineTo(19, 12);
        break;
    case Icon::minus:
        p.startNewSubPath(5, 12);
        p.lineTo(19, 12);
        break;
    case Icon::close:
        p.startNewSubPath(6, 6);
        p.lineTo(18, 18);
        p.startNewSubPath(18, 6);
        p.lineTo(6, 18);
        break;
    case Icon::dockSide:
        p.addRoundedRectangle(3.0f, 4.0f, 18.0f, 16.0f, 2.0f);
        p.startNewSubPath(15, 4);
        p.lineTo(15, 20);
        break;
    case Icon::addFolder:
        p.startNewSubPath(3, 17);
        p.lineTo(3, 7);
        p.quadraticTo(3, 5, 5, 5);
        p.lineTo(9, 5);
        p.lineTo(11, 7);
        p.lineTo(19, 7);
        p.quadraticTo(21, 7, 21, 9);
        p.lineTo(21, 17);
        p.quadraticTo(21, 19, 19, 19);
        p.lineTo(5, 19);
        p.quadraticTo(3, 19, 3, 17);
        p.closeSubPath();
        p.startNewSubPath(12, 10);
        p.lineTo(12, 16);
        p.startNewSubPath(9, 13);
        p.lineTo(15, 13);
        break;
    case Icon::play:
        filled = true;
        p.addTriangle(7.0f, 4.5f, 20.0f, 12.0f, 7.0f, 19.5f);
        break;
    case Icon::pause:
        filled = true;
        p.addRoundedRectangle(6.0f, 5.0f, 4.0f, 14.0f, 1.0f);
        p.addRoundedRectangle(14.0f, 5.0f, 4.0f, 14.0f, 1.0f);
        break;
    case Icon::stop:
        filled = true;
        p.addRoundedRectangle(6.0f, 6.0f, 12.0f, 12.0f, 1.5f);
        break;
    case Icon::loop:
        p.addCentredArc(12, 12, 8, 8, 0.0f, -juce::MathConstants<float>::halfPi * 0.95f,
                        juce::MathConstants<float>::halfPi * 0.75f, true);
        p.addCentredArc(12, 12, 8, 8, 0.0f, juce::MathConstants<float>::halfPi * 1.05f,
                        juce::MathConstants<float>::halfPi * 2.75f, true);
        p.startNewSubPath(18, 3);
        p.lineTo(18, 7);
        p.lineTo(14, 7);
        p.startNewSubPath(6, 21);
        p.lineTo(6, 17);
        p.lineTo(10, 17);
        break;
    case Icon::metronome:
        p.startNewSubPath(9, 3);
        p.lineTo(15, 3);
        p.lineTo(19, 21);
        p.lineTo(5, 21);
        p.closeSubPath();
        p.startNewSubPath(12, 16);
        p.lineTo(17, 7);
        break;
    case Icon::keys:
        p.addRoundedRectangle(2.5f, 5.0f, 19.0f, 14.0f, 2.0f);
        for (float x : {7.5f, 12.0f, 16.5f})
        {
            p.startNewSubPath(x, 5);
            p.lineTo(x, 13);
        }
        break;
    case Icon::slice:
        p.addEllipse(3.0f, 3.0f, 6.0f, 6.0f);
        p.addEllipse(3.0f, 15.0f, 6.0f, 6.0f);
        p.startNewSubPath(8.5f, 7.5f);
        p.lineTo(20, 19);
        p.startNewSubPath(8.5f, 16.5f);
        p.lineTo(20, 5);
        break;
    case Icon::list:
        p.startNewSubPath(4, 6);
        p.lineTo(20, 6);
        p.startNewSubPath(4, 12);
        p.lineTo(20, 12);
        p.startNewSubPath(4, 18);
        p.lineTo(14, 18);
        break;
    case Icon::chevronDown:
        p.startNewSubPath(6, 9);
        p.lineTo(12, 15);
        p.lineTo(18, 9);
        break;
    case Icon::none:
        break;
    }
    return p;
}

constexpr int iconLabelGap = 6;

} // namespace

void drawIcon(juce::Graphics& g, Icon icon, juce::Rectangle<float> area, juce::Colour colour)
{
    bool filled = false;
    auto path = iconPath(icon, filled);
    if (path.isEmpty())
        return;
    const float size = std::min(area.getWidth(), area.getHeight());
    const float scale = size / 24.0f;
    path.applyTransform(juce::AffineTransform::scale(scale).translated(
        area.getCentreX() - size / 2.0f, area.getCentreY() - size / 2.0f));
    g.setColour(colour);
    if (filled)
        g.fillPath(path);
    else
        g.strokePath(path, juce::PathStrokeType(std::max(1.2f, 2.0f * scale),
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
}

juce::FontOptions controlFont(int controlHeight)
{
    return juce::FontOptions(controlHeight >= metrics::transportControlHeight ? 13.0f : 12.0f);
}

juce::FontOptions displayFont(float height)
{
    static const auto typeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::IBMPlexMonoMedium_ttf,
        static_cast<std::size_t>(BinaryData::IBMPlexMonoMedium_ttfSize));
    return juce::FontOptions(typeface).withHeight(height);
}

int textWidth(const juce::FontOptions& font, const juce::String& text)
{
    return static_cast<int>(
        std::ceil(juce::GlyphArrangement::getStringWidth(juce::Font(font), text)));
}

// --------------------------------------------------------------------------------- IconButton

IconButton::IconButton(Icon iconToShow, const juce::String& label)
    : juce::Button(label), icon(iconToShow)
{
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
}

void IconButton::setIcon(Icon newIcon)
{
    if (newIcon != icon)
    {
        icon = newIcon;
        repaint();
    }
}

void IconButton::setFlat(bool shouldBeFlat)
{
    flat = shouldBeFlat;
    repaint();
}

int IconButton::preferredWidth(int height) const
{
    const auto label = getButtonText();
    if (label.isEmpty())
        return height;
    int width = 2 * metrics::controlPadding + textWidth(controlFont(height), label);
    if (icon != Icon::none)
        width += metrics::iconSize + iconLabelGap;
    return width;
}

void IconButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    const auto& t = currentTheme();
    const auto& c = t.colours;
    const bool on = getToggleState();
    auto bounds = getLocalBounds().toFloat();

    auto fill = colour(on ? c.activeBackground : (flat ? c.panel : c.panelRaised));
    if (down)
        fill = fill.contrasting(0.15f);
    else if (highlighted)
        fill = fill.contrasting(0.07f);
    if (flat)
    {
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, std::min(4.0f, t.shape.cornerRadius));
    }
    else
    {
        const auto box = bounds.reduced(t.shape.borderWidth / 2.0f);
        g.setColour(fill);
        g.fillRoundedRectangle(box, t.shape.cornerRadius);
        if (t.shape.borderWidth > 0.0f)
        {
            g.setColour(colour(c.outline));
            g.drawRoundedRectangle(box, t.shape.cornerRadius, t.shape.borderWidth);
        }
    }

    auto ink = colour(on ? c.activeText : c.text);
    if (!isEnabled())
        ink = colour(c.textDim).withMultipliedAlpha(0.6f);
    const auto label = getButtonText();
    const auto iconSize = static_cast<float>(metrics::iconSize);
    if (label.isEmpty())
    {
        drawIcon(g, icon, bounds.withSizeKeepingCentre(iconSize, iconSize), ink);
        return;
    }
    // Icon and label, centred together.
    const auto font = controlFont(getHeight());
    const float labelWidth = static_cast<float>(textWidth(font, label));
    const float contentWidth =
        labelWidth + (icon != Icon::none ? iconSize + static_cast<float>(iconLabelGap) : 0.0f);
    auto content =
        bounds.withSizeKeepingCentre(std::min(contentWidth, bounds.getWidth()), bounds.getHeight());
    if (icon != Icon::none)
    {
        drawIcon(g, icon,
                 content.removeFromLeft(iconSize).withSizeKeepingCentre(iconSize, iconSize), ink);
        content.removeFromLeft(static_cast<float>(iconLabelGap));
    }
    g.setColour(ink);
    g.setFont(font);
    g.drawText(label, content, juce::Justification::centredLeft, false);
}

// ------------------------------------------------------------------- Segments and headers

void joinSegments(std::initializer_list<juce::Button*> buttons)
{
    const int count = static_cast<int>(buttons.size());
    int i = 0;
    for (auto* button : buttons)
    {
        int edges = 0;
        if (i > 0)
            edges |= juce::Button::ConnectedOnLeft;
        if (i < count - 1)
            edges |= juce::Button::ConnectedOnRight;
        button->setConnectedEdges(edges);
        ++i;
    }
}

int buttonWidth(const juce::String& text, int height)
{
    return std::max(height + 6, 2 * metrics::controlPadding + textWidth(controlFont(height), text));
}

void paintPanelHeader(juce::Graphics& g, juce::Rectangle<int> header, const juce::String& title,
                      const juce::String& subtitle, int textRight, float topRadius)
{
    const auto& t = currentTheme();
    const auto area = header.toFloat();
    g.setColour(colour(t.colours.header));
    if (topRadius > 0.0f)
    {
        juce::Path top;
        top.addRoundedRectangle(area.getX(), area.getY(), area.getWidth(), area.getHeight(),
                                topRadius, topRadius, true, true, false, false);
        g.fillPath(top);
    }
    else
    {
        g.fillRect(area);
    }
    g.setColour(colour(t.colours.outline));
    g.fillRect(area.withTop(area.getBottom() - std::max(1.0f, t.shape.borderWidth)));

    auto text = header.withTrimmedLeft(metrics::controlPadding);
    if (textRight >= 0)
        text.setRight(std::min(text.getRight(), textRight));
    const juce::FontOptions titleFont(13.0f, juce::Font::bold);
    const int titleWidth = std::min(text.getWidth(), textWidth(titleFont, title) + 2);
    // The bundled fonts are Regular only: a second pass half a pixel over makes a semibold.
    g.setColour(colour(t.colours.text));
    g.setFont(titleFont);
    const auto titleArea = text.removeFromLeft(titleWidth).toFloat();
    g.drawText(title, titleArea, juce::Justification::centredLeft, true);
    g.drawText(title, titleArea.translated(0.45f, 0.0f), juce::Justification::centredLeft, true);
    if (subtitle.isNotEmpty() && text.getWidth() > 20)
    {
        text.removeFromLeft(metrics::controlGap + 2);
        g.setColour(colour(t.colours.textDim));
        g.setFont(juce::FontOptions(12.0f));
        g.drawText(subtitle, text, juce::Justification::centredLeft, true);
    }
}

void paintHeaderLabel(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area)
{
    g.setColour(colour(currentTheme().colours.textDim));
    g.setFont(juce::FontOptions(12.0f));
    g.drawText(text, area, juce::Justification::centredRight, false);
}

int headerLabelWidth(const juce::String& text)
{
    return textWidth(juce::FontOptions(12.0f), text) + 6;
}

HeaderLayout::HeaderLayout(juce::Rectangle<int> header, int controlHeight)
    : height(controlHeight > 0 ? controlHeight : metrics::controlHeight)
{
    // Controls sit as far from the right edge as from the top, so they look centred.
    const int inset = std::max(0, (header.getHeight() - height) / 2);
    area = header.withTrimmedRight(inset).withTrimmedLeft(metrics::controlPadding);
}

juce::Rectangle<int> HeaderLayout::fromRight(int width)
{
    auto r = area.removeFromRight(width).withSizeKeepingCentre(width, height);
    area.removeFromRight(metrics::controlGap);
    return r;
}

juce::Rectangle<int> HeaderLayout::fromLeft(int width)
{
    auto r = area.removeFromLeft(width).withSizeKeepingCentre(width, height);
    area.removeFromLeft(metrics::controlGap);
    return r;
}

void paintDisplayBox(juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& t = currentTheme();
    const auto box = area.reduced(t.shape.borderWidth / 2.0f);
    g.setColour(colour(t.colours.display));
    g.fillRoundedRectangle(box, t.shape.cornerRadius);
    if (t.shape.borderWidth > 0.0f)
    {
        g.setColour(colour(t.colours.outline));
        g.drawRoundedRectangle(box, t.shape.cornerRadius, t.shape.borderWidth);
    }
}

// ------------------------------------------------------------------------------- TempoDisplay

namespace
{
constexpr int displayInset = 5;
constexpr int unitWidth = 30; // "BPM"
} // namespace

TempoDisplay::TempoDisplay()
{
    for (auto* button : {&down, &up})
    {
        button->setFlat(true);
        addAndMakeVisible(*button);
    }
    down.onClick = [this] { change(value - step); };
    up.onClick = [this] { change(value + step); };

    number.setEditable(true, false, false);
    number.setJustificationType(juce::Justification::centredRight);
    number.setBorderSize({});
    number.setMouseCursor(juce::MouseCursor::IBeamCursor);
    number.onTextChange = [this]
    {
        const auto text = number.getText().trim();
        if (text.containsAnyOf("0123456789"))
            change(text.getDoubleValue());
        updateLabel();
    };
    addAndMakeVisible(number);
    lookAndFeelChanged();
    updateLabel();
}

void TempoDisplay::setRange(double lowest, double highest)
{
    minimum = lowest;
    maximum = highest;
}

void TempoDisplay::setValue(double bpm)
{
    value = std::clamp(bpm, minimum, maximum);
    updateLabel();
}

void TempoDisplay::setTooltip(const juce::String& tooltip)
{
    juce::SettableTooltipClient::setTooltip(tooltip);
    for (auto* part : std::initializer_list<juce::SettableTooltipClient*>{&number, &down, &up})
        part->setTooltip(tooltip);
}

int TempoDisplay::preferredWidth(int height) const
{
    const int nudge = height - 10;
    return 2 * displayInset + 2 * nudge + textWidth(displayFont(16.0f), "000.000") + 8 + unitWidth;
}

void TempoDisplay::change(double bpm)
{
    // Typed values keep three decimals.
    const double rounded = std::clamp(std::round(bpm * 1000.0) / 1000.0, minimum, maximum);
    if (std::abs(rounded - value) < 1.0e-9)
        return;
    value = rounded;
    updateLabel();
    if (onChange)
        onChange(value);
}

void TempoDisplay::updateLabel()
{
    number.setText(juce::String(value, 3), juce::dontSendNotification);
}

void TempoDisplay::paint(juce::Graphics& g)
{
    paintDisplayBox(g, getLocalBounds().toFloat());
    const auto& c = currentTheme().colours;
    g.setColour(colour(c.displayDim));
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("BPM", up.getX() - unitWidth, 0, unitWidth - 4, getHeight(),
               juce::Justification::centred);
}

void TempoDisplay::resized()
{
    auto area = getLocalBounds().reduced(displayInset, 0);
    const int nudge = getHeight() - 10;
    down.setBounds(area.removeFromLeft(nudge).withSizeKeepingCentre(nudge, nudge));
    up.setBounds(area.removeFromRight(nudge).withSizeKeepingCentre(nudge, nudge));
    area.removeFromRight(unitWidth);
    number.setBounds(area);
}

void TempoDisplay::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (std::abs(w.deltaY) > 0.0f)
        change(value + (w.deltaY > 0.0f ? step : -step));
}

void TempoDisplay::lookAndFeelChanged()
{
    const auto& c = currentTheme().colours;
    number.setFont(displayFont(16.0f));
    number.setColour(juce::Label::textColourId, colour(c.displayText));
    number.setColour(juce::Label::backgroundWhenEditingColourId, colour(c.display));
    number.setColour(juce::Label::textWhenEditingColourId, colour(c.text));
    number.setColour(juce::Label::outlineWhenEditingColourId, colour(c.accent));
    number.setColour(juce::TextEditor::highlightColourId, colour(c.activeBackground));
}

} // namespace jls::ui
