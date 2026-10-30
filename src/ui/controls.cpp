#include "ui/controls.h"

#include "BinaryData.h"
#include "ui/theme.h"

#include <array>
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
    case Icon::windowMinimize:
        p.startNewSubPath(6, 16);
        p.lineTo(18, 16);
        break;
    case Icon::windowMaximize:
        p.addRectangle(6.0f, 6.0f, 12.0f, 12.0f);
        break;
    case Icon::windowRestore:
        p.addRectangle(5.0f, 9.0f, 10.0f, 10.0f);
        p.startNewSubPath(9, 9);
        p.lineTo(9, 5);
        p.lineTo(19, 5);
        p.lineTo(19, 15);
        p.lineTo(15, 15);
        break;
    case Icon::channelRack:
        for (float y : {3.5f, 10.0f, 16.5f})
            for (float x : {3.5f, 10.0f, 16.5f})
                p.addRoundedRectangle(x, y, 4.0f, 4.0f, 1.0f);
        break;
    case Icon::playlist:
        p.addRoundedRectangle(3.0f, 4.0f, 10.0f, 4.0f, 1.0f);
        p.addRoundedRectangle(10.0f, 10.0f, 11.0f, 4.0f, 1.0f);
        p.addRoundedRectangle(5.0f, 16.0f, 9.0f, 4.0f, 1.0f);
        break;
    case Icon::pianoRoll:
        p.startNewSubPath(6, 3);
        p.lineTo(6, 21);
        for (float y : {7.0f, 12.0f, 17.0f})
        {
            p.startNewSubPath(2.5f, y);
            p.lineTo(6.0f, y);
        }
        p.startNewSubPath(10, 6);
        p.lineTo(15, 6);
        p.startNewSubPath(13, 12);
        p.lineTo(21, 12);
        p.startNewSubPath(10, 18);
        p.lineTo(16, 18);
        break;
    case Icon::browser:
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
        break;
    case Icon::mixer:
        for (const auto [x, cap] : {std::pair{6.0f, 9.0f}, {12.0f, 15.0f}, {18.0f, 7.0f}})
        {
            p.startNewSubPath(x, 3);
            p.lineTo(x, 21);
            p.startNewSubPath(x - 2.5f, cap);
            p.lineTo(x + 2.5f, cap);
        }
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

float controlTextSize(int controlHeight)
{
    return controlHeight >= metrics::transportControlHeight ? metrics::textLarge
                                                            : metrics::textBody;
}

juce::FontOptions controlFont(int controlHeight)
{
    return mediumFont(controlTextSize(controlHeight));
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

juce::Colour IconButton::inkColour() const
{
    const auto& c = currentTheme().colours;
    if (!isEnabled())
        return colour(c.textDim).withMultipliedAlpha(0.6f);
    return colour(getToggleState() ? c.activeText : c.text);
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

// ---------------------------------------------------------------------------------------- LED

void paintLed(juce::Graphics& g, juce::Rectangle<float> area, bool on, bool hovered)
{
    const auto& c = currentTheme().colours;
    constexpr float size = 9.0f;
    const auto led = area.withSizeKeepingCentre(size, size);
    // Unlit, a dark recessed lamp with a rim, so it shows on any background (a highlighted row
    // or strip can be as light as the rim).
    g.setColour(colour(on ? c.led : c.display));
    g.fillEllipse(led);
    g.setColour(hovered ? colour(c.textDim) : colour(on ? c.outline : c.ledOff));
    g.drawEllipse(led, 1.0f);
}

void LedButton::setOn(bool shouldBeOn)
{
    if (on != shouldBeOn)
    {
        on = shouldBeOn;
        repaint();
    }
}

void LedButton::paint(juce::Graphics& g)
{
    paintLed(g, getLocalBounds().toFloat(), on);
}

void LedButton::mouseDown(const juce::MouseEvent& e)
{
    if (onClick)
        onClick(e.mods.isCommandDown() || e.mods.isPopupMenu());
}

EdgeResizer::EdgeResizer(bool isHorizontal) : horizontal(isHorizontal)
{
    setMouseCursor(horizontal ? juce::MouseCursor::LeftRightResizeCursor
                              : juce::MouseCursor::UpDownResizeCursor);
    setRepaintsOnMouseActivity(true);
}

void EdgeResizer::paint(juce::Graphics& g)
{
    if (!isMouseOverOrDragging())
        return;
    g.setColour(colour(currentTheme().colours.accent).withAlpha(0.6f));
    const auto bounds = getLocalBounds().toFloat();
    g.fillRect(horizontal ? bounds.withSizeKeepingCentre(2.0f, bounds.getHeight())
                          : bounds.withSizeKeepingCentre(bounds.getWidth(), 2.0f));
}

void EdgeResizer::mouseDown(const juce::MouseEvent&)
{
    start = juce::Desktop::getMousePosition();
    if (onStart)
        onStart();
}

void EdgeResizer::mouseDrag(const juce::MouseEvent&)
{
    const auto moved = juce::Desktop::getMousePosition() - start;
    if (onDrag)
        onDrag(horizontal ? moved.x : moved.y);
}

void EdgeResizer::mouseUp(const juce::MouseEvent&)
{
    if (onEnd)
        onEnd();
}

void EdgeResizer::mouseDoubleClick(const juce::MouseEvent&)
{
    if (onReset)
        onReset();
}

LedToggle::LedToggle() : juce::Button({})
{
    setClickingTogglesState(true);
}

void LedToggle::paintButton(juce::Graphics& g, bool highlighted, bool)
{
    paintLed(g, getLocalBounds().toFloat(), getToggleState(), highlighted);
}

// ------------------------------------------------------------------------------- WindowButton

namespace
{
constexpr float windowLedInset = 9.0f; // from the button's sides
constexpr float windowLedTop = 5.0f;
constexpr float windowLedHeight = 3.0f;
} // namespace

void WindowButton::setOpen(bool isOpen)
{
    if (isOpen != open)
    {
        open = isOpen;
        repaint();
    }
}

juce::Colour WindowButton::inkColour() const
{
    const auto& c = currentTheme().colours;
    return colour(open ? c.text : c.textDim);
}

juce::Rectangle<float> WindowButton::contentArea() const
{
    // Below the LED.
    return getLocalBounds().toFloat().withTrimmedTop(windowLedTop);
}

void WindowButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    IconButton::paintButton(g, highlighted, down);

    // The LED, like the step keys': the theme's step colour while the window is open.
    const auto bounds = getLocalBounds().toFloat();
    juce::Path led;
    led.addRoundedRectangle(bounds.getX() + windowLedInset, bounds.getY() + windowLedTop,
                            bounds.getWidth() - 2.0f * windowLedInset, windowLedHeight,
                            windowLedHeight / 2.0f);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillPath(led);
    if (!open)
        return;
    const auto lit = colour(currentTheme().colours.stepOn);
    juce::DropShadow(lit, 6, {}).drawForPath(g, led);
    g.setColour(lit.brighter(0.3f));
    g.fillPath(led);
}

void IconButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    const auto& c = currentTheme().colours;
    const bool on = getToggleState();
    auto bounds = getLocalBounds().toFloat();

    auto base = c.panelRaised;
    if (on)
        base = c.activeBackground;
    else if (flat)
        base = c.panel;
    const auto fill = buttonFill(colour(base), highlighted, down);
    if (flat)
    {
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, metrics::cornerRadius);
    }
    else
    {
        const auto box = bounds.reduced(metrics::borderWidth / 2.0f);
        juce::Path shape;
        shape.addRoundedRectangle(box, metrics::cornerRadius);
        fillRaised(g, shape, fill);
        g.setColour(colour(c.outline));
        g.drawRoundedRectangle(box, metrics::cornerRadius, metrics::borderWidth);
    }

    const auto ink = inkColour();
    bounds = contentArea();
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

// ----------------------------------------------------------------------------------- MenuKeys

MenuKeys::MenuKeys(std::vector<std::pair<char, std::function<void()>>> menuKeys)
    : keys(std::move(menuKeys))
{
    for (const auto& [key, action] : keys)
        wasDown.push_back(juce::KeyPress::isKeyCurrentlyDown(key));
    startTimerHz(60);
}

MenuKeys::~MenuKeys()
{
    stopTimer();
}

void MenuKeys::timerCallback()
{
    for (std::size_t i = 0; i < keys.size(); ++i)
    {
        const bool down = juce::KeyPress::isKeyCurrentlyDown(keys[i].first);
        if (down && !wasDown[i])
        {
            stopTimer();
            juce::PopupMenu::dismissAllActiveMenus();
            // After the menu has closed (its callback may delete this object).
            juce::MessageManager::callAsync(keys[i].second);
            return;
        }
        wasDown[i] = down;
    }
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
    // A slight sheen, lit from above like the rest.
    g.setGradientFill(
        juce::ColourGradient::vertical(colour(t.colours.header).brighter(0.08f), area.getY(),
                                       colour(t.colours.header).darker(0.1f), area.getBottom()));
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
    g.fillRect(area.withTop(area.getBottom() - metrics::borderWidth));
    // A lit top edge, like a bevel: headers read as the top of their panel.
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.fillRect(area.withHeight(1.0f).reduced(topRadius, 0.0f));

    auto text = header.withTrimmedLeft(metrics::controlPadding);
    if (textRight >= 0)
        text.setRight(std::min(text.getRight(), textRight));
    const auto titleFont = semiboldFont(metrics::textLarge);
    const int titleWidth = std::min(text.getWidth(), textWidth(titleFont, title) + 2);
    g.setColour(colour(t.colours.text));
    g.setFont(titleFont);
    g.drawText(title, text.removeFromLeft(titleWidth), juce::Justification::centredLeft, true);
    if (subtitle.isNotEmpty() && text.getWidth() > 20)
    {
        text.removeFromLeft(metrics::controlGap + 2);
        g.setColour(colour(t.colours.textDim));
        g.setFont(juce::FontOptions(metrics::textBody));
        g.drawText(subtitle, text, juce::Justification::centredLeft, true);
    }
}

void paintHeaderLabel(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area)
{
    g.setColour(colour(currentTheme().colours.textDim));
    g.setFont(juce::FontOptions(metrics::textBody));
    g.drawText(text, area, juce::Justification::centredRight, false);
}

int headerLabelWidth(const juce::String& text)
{
    return textWidth(juce::FontOptions(metrics::textBody), text) + 6;
}

HeaderLayout::HeaderLayout(juce::Rectangle<int> header)
{
    // Controls sit as far from the right edge as from the top, so they look centred.
    const int inset = std::max(0, (header.getHeight() - metrics::controlHeight) / 2);
    area = header.withTrimmedRight(inset).withTrimmedLeft(metrics::controlPadding);
}

juce::Rectangle<int> HeaderLayout::fromRight(int width)
{
    auto r = area.removeFromRight(width).withSizeKeepingCentre(width, metrics::controlHeight);
    area.removeFromRight(metrics::controlGap);
    return r;
}

juce::Rectangle<int> HeaderLayout::fromLeft(int width)
{
    auto r = area.removeFromLeft(width).withSizeKeepingCentre(width, metrics::controlHeight);
    area.removeFromLeft(metrics::controlGap);
    return r;
}

void paintCardFrame(juce::Graphics& g, juce::Component& panel)
{
    const auto& t = currentTheme();
    const auto bounds = panel.getLocalBounds().toFloat();
    // Square, edge to edge; focus is a line along the top instead of a frame.
    g.setColour(colour(t.colours.outline));
    g.drawRect(bounds, metrics::borderWidth);
    if (panel.getProperties()["focused"])
    {
        g.setColour(colour(t.colours.accent));
        g.fillRect(bounds.withHeight(2.0f));
    }
}

void paintFloatingPanel(juce::Graphics& g, juce::Component& panel, const juce::String& title,
                        const juce::String& subtitle, int textRight)
{
    // Outside the rounded corners, the window background: a native area window (app/area_stack)
    // has nothing behind it there, so they'd come out black.
    g.fillAll(colour(currentTheme().colours.background));
    g.setColour(colour(currentTheme().colours.panel));
    g.fillRoundedRectangle(panel.getLocalBounds().toFloat(), metrics::windowRadius);
    paintPanelHeader(g, panel.getLocalBounds().removeFromTop(metrics::panelHeaderHeight), title,
                     subtitle, textRight, metrics::windowRadius);
}

void paintFloatingBorder(juce::Graphics& g, juce::Component& panel)
{
    g.setColour(colour(currentTheme().colours.outline));
    g.drawRoundedRectangle(panel.getLocalBounds().toFloat().reduced(metrics::borderWidth / 2.0f),
                           metrics::windowRadius, metrics::borderWidth);
}

void fillRaised(juce::Graphics& g, const juce::Path& shape, juce::Colour fill)
{
    const auto b = shape.getBounds();
    g.setGradientFill(juce::ColourGradient::vertical(fill.brighter(0.12f), b.getY(),
                                                     fill.darker(0.22f), b.getBottom()));
    g.fillPath(shape);
    juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(shape);
    g.setColour(juce::Colours::white.withAlpha(0.09f));
    g.fillRect(b.getX(), b.getY() + 1.0f, b.getWidth(), 1.0f);
}

void fillRaisedOutlined(juce::Graphics& g, const juce::Path& shape, juce::Colour fill)
{
    fillRaised(g, shape, fill);
    g.setColour(colour(currentTheme().colours.outline));
    g.strokePath(shape, juce::PathStrokeType(metrics::borderWidth));
}

void paintOutlinedBox(juce::Graphics& g, juce::Rectangle<float> box, juce::Colour fill)
{
    g.setColour(fill);
    g.fillRoundedRectangle(box, metrics::cornerRadius);
    g.setColour(colour(currentTheme().colours.outline));
    g.drawRoundedRectangle(box, metrics::cornerRadius, metrics::borderWidth);
}

juce::Colour buttonFill(juce::Colour base, bool highlighted, bool down)
{
    if (down)
        return base.contrasting(0.15f);
    if (highlighted)
        return base.contrasting(0.07f);
    return base;
}

void paintDisplayBox(juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto& t = currentTheme();
    const auto box = area.reduced(metrics::borderWidth / 2.0f);
    g.setColour(colour(t.colours.display));
    g.fillRoundedRectangle(box, metrics::cornerRadius);
    {
        // Recessed: a shadow falling in from the top edge.
        constexpr float shadowDepth = 6.0f;
        juce::Path p;
        p.addRoundedRectangle(box, metrics::cornerRadius);
        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(p);
        g.setGradientFill(juce::ColourGradient::vertical(
            juce::Colours::black.withAlpha(0.55f), box.getY(), juce::Colours::transparentBlack,
            box.getY() + shadowDepth));
        g.fillRect(box.withHeight(shadowDepth));
    }
    g.setColour(colour(t.colours.outline));
    g.drawRoundedRectangle(box, metrics::cornerRadius, metrics::borderWidth);
}

// -------------------------------------------------------------------------------- NumberField

namespace
{

constexpr int displayInset = 5;

// The side of the square -/+ buttons: 5 px clear of the top and bottom.
int nudgeSize(int height)
{
    return height - 10;
}

// The space the unit text takes, its gaps included.
int unitWidth(const juce::String& unit)
{
    return unit.isEmpty() ? 0 : textWidth(juce::FontOptions(metrics::textTiny), unit) + 10;
}

} // namespace

NumberField::NumberField(const juce::String& unitText, int decimalPlaces,
                         const juce::String& widestText)
    : unit(unitText), decimals(decimalPlaces), widest(widestText)
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

void NumberField::setRange(double lowest, double highest)
{
    minimum = lowest;
    maximum = highest;
    value = std::clamp(value, minimum, maximum);
    updateLabel();
}

void NumberField::setValue(double newValue)
{
    value = std::clamp(newValue, minimum, maximum);
    updateLabel();
}

void NumberField::setTooltip(const juce::String& tooltip)
{
    juce::SettableTooltipClient::setTooltip(tooltip);
    for (auto* part : std::initializer_list<juce::SettableTooltipClient*>{&number, &down, &up})
        part->setTooltip(tooltip);
}

float NumberField::numberHeight() const
{
    return std::round(static_cast<float>(std::max(getHeight(), 20)) * 0.5f);
}

int NumberField::preferredWidth(int height) const
{
    const auto font = displayFont(std::round(static_cast<float>(height) * 0.5f));
    return 2 * displayInset + 2 * nudgeSize(height) + textWidth(font, widest) + 10 +
           unitWidth(unit);
}

void NumberField::change(double newValue)
{
    const double scale = std::pow(10.0, decimals);
    const double rounded = std::clamp(std::round(newValue * scale) / scale, minimum, maximum);
    if (std::abs(rounded - value) < 1.0e-9)
        return;
    value = rounded;
    updateLabel();
    if (onChange)
        onChange(value);
}

void NumberField::updateLabel()
{
    auto text = juce::String(value, decimals);
    if (showPlus && value > 0.0)
        text = "+" + text;
    number.setText(text, juce::dontSendNotification);
}

void NumberField::paint(juce::Graphics& g)
{
    paintDisplayBox(g, getLocalBounds().toFloat());
    if (unit.isEmpty())
        return;
    const auto& c = currentTheme().colours;
    g.setColour(colour(c.displayDim));
    g.setFont(juce::FontOptions(metrics::textTiny));
    g.drawText(unit, number.getRight() + 4, 0, up.getX() - number.getRight() - 6, getHeight(),
               juce::Justification::centredLeft);
}

void NumberField::resized()
{
    auto area = getLocalBounds().reduced(displayInset, 0);
    const int nudge = nudgeSize(getHeight());
    down.setBounds(area.removeFromLeft(nudge).withSizeKeepingCentre(nudge, nudge));
    up.setBounds(area.removeFromRight(nudge).withSizeKeepingCentre(nudge, nudge));
    area.removeFromRight(unitWidth(unit));
    number.setBounds(area);
    number.setFont(displayFont(numberHeight()));
}

void NumberField::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (std::abs(w.deltaY) > 0.0f)
        change(value + (w.deltaY > 0.0f ? step : -step));
}

void NumberField::lookAndFeelChanged()
{
    const auto& c = currentTheme().colours;
    number.setFont(displayFont(numberHeight()));
    number.setColour(juce::Label::textColourId, colour(c.displayText));
    number.setColour(juce::Label::backgroundWhenEditingColourId, colour(c.display));
    number.setColour(juce::Label::textWhenEditingColourId, colour(c.text));
    number.setColour(juce::Label::outlineWhenEditingColourId, colour(c.accent));
    number.setColour(juce::TextEditor::highlightColourId, colour(c.activeBackground));
}

// ------------------------------------------------------------------------------ Forms, dialogs

void paintSectionHeading(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area)
{
    const auto& c = currentTheme().colours;
    const auto font = semiboldFont(metrics::textSmall).withKerningFactor(0.08f);
    g.setColour(colour(c.textDim));
    g.setFont(font);
    const auto upper = text.toUpperCase();
    g.drawText(upper, area, juce::Justification::bottomLeft, false);
    // A hairline from the heading to the right edge.
    const int x = area.getX() + textWidth(font, upper) + 10;
    if (x < area.getRight())
    {
        g.setColour(colour(c.outline));
        g.fillRect(x, area.getBottom() - 6, area.getRight() - x, 1);
    }
}

void paintNote(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area)
{
    g.setColour(colour(currentTheme().colours.textDim));
    g.setFont(juce::FontOptions(metrics::textBody));
    g.drawFittedText(text, area, juce::Justification::topLeft, 6, 1.0f);
}

int noteHeight(const juce::String& text, int width)
{
    juce::AttributedString attributed(text);
    attributed.setFont(juce::FontOptions(metrics::textBody));
    juce::TextLayout layout;
    layout.createLayout(attributed, static_cast<float>(std::max(40, width)));
    return static_cast<int>(std::ceil(layout.getHeight())) + 4;
}

void paintDialogFooter(juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto& t = currentTheme();
    g.setColour(colour(t.colours.header));
    g.fillRect(area);
    g.setColour(colour(t.colours.outline));
    g.fillRect(area.withHeight(1));
}

DialogFrame::DialogFrame(std::unique_ptr<juce::Component> c, const juce::String& h,
                         const juce::String& n, int contentWidth, int contentHeight)
    : content(std::move(c)), heading(h), note(n)
{
    addAndMakeVisible(*content);
    setSize(contentWidth + 2 * FormPanel::margin,
            contentHeight + topHeight() + 2 * FormPanel::margin);
}

int DialogFrame::topHeight() const
{
    const int width = std::max(200, getWidth() - 2 * FormPanel::margin);
    return 24 + (note.isEmpty() ? 4 : noteHeight(note, width) + 10);
}

void DialogFrame::paint(juce::Graphics& g)
{
    g.fillAll(colour(currentTheme().colours.panel));
    auto area = getLocalBounds().reduced(FormPanel::margin);
    paintSectionHeading(g, heading, area.removeFromTop(24));
    if (note.isNotEmpty())
        paintNote(g, note, area.removeFromTop(topHeight() - 24).withTrimmedTop(6));
}

void DialogFrame::resized()
{
    content->setBounds(getLocalBounds().reduced(FormPanel::margin).withTrimmedTop(topHeight()));
}

void FormPanel::addSection(const juce::String& heading)
{
    items.push_back({.kind = Item::Kind::section, .text = heading});
}

void FormPanel::addRow(const juce::String& label,
                       std::initializer_list<std::pair<juce::Component*, int>> controls)
{
    for (const auto& [control, width] : controls)
        addAndMakeVisible(control);
    items.push_back({.kind = Item::Kind::row, .text = label, .controls = controls});
}

int FormPanel::addNote(const juce::String& text)
{
    items.push_back({.kind = Item::Kind::note, .text = text});
    return static_cast<int>(items.size()) - 1;
}

void FormPanel::setNote(int index, const juce::String& text)
{
    if (index < 0 || index >= static_cast<int>(items.size()))
        return;
    auto& item = items[static_cast<std::size_t>(index)];
    if (item.text == text)
        return;
    item.text = text;
    repaint(item.bounds);
}

void FormPanel::addComponent(juce::Component& component, int height)
{
    addAndMakeVisible(component);
    items.push_back({.kind = Item::Kind::component, .height = height, .component = &component});
}

void FormPanel::addPainted(int height,
                           std::function<void(juce::Graphics&, juce::Rectangle<int>)> painter)
{
    items.push_back({.kind = Item::Kind::painted, .height = height, .painter = std::move(painter)});
}

int FormPanel::heightOf(std::size_t index, int width) const
{
    const auto& item = items[index];
    switch (item.kind)
    {
    case Item::Kind::section:
        return index == 0 ? 24 : 34; // no gap above the first heading
    case Item::Kind::row:
        return metrics::controlHeight + 8;
    case Item::Kind::note:
        return noteHeight(item.text, width) + 2;
    case Item::Kind::painted:
    case Item::Kind::component:
        return item.height + 8;
    }
    return 0;
}

int FormPanel::preferredHeight(int width) const
{
    int h = 2 * margin;
    for (std::size_t i = 0; i < items.size(); ++i)
        h += heightOf(i, width - 2 * margin);
    return h;
}

juce::Rectangle<int> FormPanel::itemsArea() const
{
    auto area = getLocalBounds();
    if (itemsWidth > 0)
        area = area.withWidth(itemsWidth);
    return area.reduced(margin);
}

void FormPanel::resized()
{
    auto area = itemsArea();
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        auto& item = items[i];
        item.bounds = area.removeFromTop(heightOf(i, area.getWidth()));
        if (item.kind == Item::Kind::component)
            item.component->setBounds(item.bounds.withTrimmedBottom(8));
        if (item.kind != Item::Kind::row)
            continue;
        auto line = item.bounds.withTrimmedLeft(labelWidth);
        for (const auto& [control, width] : item.controls)
        {
            control->setBounds(
                line.removeFromLeft(width).withSizeKeepingCentre(width, metrics::controlHeight));
            line.removeFromLeft(metrics::controlGap);
        }
    }
}

void FormPanel::paint(juce::Graphics& g)
{
    const auto& c = currentTheme().colours;
    g.fillAll(colour(c.panel));
    for (const auto& item : items)
    {
        switch (item.kind)
        {
        case Item::Kind::section:
            paintSectionHeading(g, item.text, item.bounds.withTrimmedBottom(6));
            break;
        case Item::Kind::row:
            g.setColour(colour(c.text));
            g.setFont(juce::FontOptions(metrics::textLarge));
            g.drawText(item.text, item.bounds.withWidth(labelWidth - 8),
                       juce::Justification::centredLeft, true);
            break;
        case Item::Kind::note:
            paintNote(g, item.text, item.bounds);
            break;
        case Item::Kind::painted:
            if (item.painter)
                item.painter(g, item.bounds.withTrimmedBottom(8));
            break;
        case Item::Kind::component:
            break;
        }
    }
}

} // namespace jls::ui
