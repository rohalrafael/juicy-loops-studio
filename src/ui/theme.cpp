#include "ui/theme.h"

#include "BinaryData.h"
#include "ui/controls.h"
#include "ui/title_bar.h"

namespace jls::ui
{
namespace
{

const theme::Theme* activeTheme = nullptr;

constexpr const char* themeSettingKey = "theme";
constexpr const char* customPrefix = "custom:";

} // namespace

const theme::Theme& currentTheme()
{
    return activeTheme != nullptr ? *activeTheme : theme::defaultTheme();
}

// -------------------------------------------------------------------------------- LookAndFeel

juce::FontOptions mediumFont(float size)
{
    return juce::FontOptions(size).withStyle("Medium");
}

juce::FontOptions semiboldFont(float size)
{
    return juce::FontOptions(size).withStyle("SemiBold");
}

juce::Typeface::Ptr LookAndFeel::getTypefaceForFont(const juce::Font& font)
{
    if (family.regular != nullptr &&
        font.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
    {
        const auto style = font.getTypefaceStyle().toLowerCase();
        if ((style.contains("semibold") || style.contains("bold")) && family.semibold != nullptr)
            return family.semibold;
        if (style.contains("medium") && family.medium != nullptr)
            return family.medium;
        return family.regular;
    }
    return juce::LookAndFeel_V4::getTypefaceForFont(font);
}

void LookAndFeel::applyTheme(const theme::Theme& t, const FontFamily* bundledFont)
{
    const auto& c = t.colours;
    setColour(juce::ResizableWindow::backgroundColourId, colour(c.background));
    setColour(juce::DocumentWindow::backgroundColourId, colour(c.background));
    setColour(juce::TextButton::buttonColourId, colour(c.panelRaised));
    setColour(juce::TextButton::buttonOnColourId, colour(c.activeBackground));
    setColour(juce::TextButton::textColourOffId, colour(c.text));
    setColour(juce::TextButton::textColourOnId, colour(c.activeText));
    setColour(juce::ComboBox::backgroundColourId, colour(c.panelRaised));
    setColour(juce::ComboBox::outlineColourId, colour(c.outline));
    setColour(juce::ComboBox::textColourId, colour(c.text));
    setColour(juce::ComboBox::arrowColourId, colour(c.textDim));
    setColour(juce::Label::textColourId, colour(c.text));
    setColour(juce::Slider::thumbColourId, colour(c.accent));
    setColour(juce::Slider::trackColourId, colour(c.display));
    setColour(juce::Slider::backgroundColourId, colour(c.display));
    setColour(juce::Slider::textBoxTextColourId, colour(c.displayText));
    setColour(juce::Slider::textBoxBackgroundColourId, colour(c.display));
    setColour(juce::Slider::textBoxOutlineColourId, colour(c.outline));
    setColour(juce::TextEditor::backgroundColourId, colour(c.display));
    setColour(juce::TextEditor::textColourId, colour(c.text));
    setColour(juce::TextEditor::outlineColourId, colour(c.outline));
    setColour(juce::TextEditor::focusedOutlineColourId, colour(c.accent));
    setColour(juce::PopupMenu::backgroundColourId, colour(c.panel));
    setColour(juce::PopupMenu::textColourId, colour(c.text));
    setColour(juce::PopupMenu::headerTextColourId, colour(c.textDim));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colour(c.activeBackground));
    setColour(juce::PopupMenu::highlightedTextColourId, colour(c.activeText));
    setColour(juce::AlertWindow::backgroundColourId, colour(c.panel));
    setColour(juce::AlertWindow::textColourId, colour(c.text));
    setColour(juce::AlertWindow::outlineColourId, colour(c.outline));
    setColour(juce::TooltipWindow::backgroundColourId, colour(c.panelRaised));
    setColour(juce::TooltipWindow::textColourId, colour(c.text));
    setColour(juce::TooltipWindow::outlineColourId, colour(c.outline));
    setColour(juce::ScrollBar::thumbColourId, colour(c.panelRaised));
    setColour(juce::ListBox::backgroundColourId, colour(c.panel));
    setColour(juce::ListBox::outlineColourId, colour(c.outline));
    setColour(juce::TableHeaderComponent::backgroundColourId, colour(c.header));
    setColour(juce::TableHeaderComponent::textColourId, colour(c.text));
    setColour(juce::TableHeaderComponent::outlineColourId, colour(c.outline));
    setColour(juce::TableHeaderComponent::highlightColourId, colour(c.activeBackground));
    setColour(juce::ListBox::textColourId, colour(c.text));
    setColour(juce::GroupComponent::outlineColourId, colour(c.outline));
    setColour(juce::GroupComponent::textColourId, colour(c.text));
    setColour(juce::ToggleButton::textColourId, colour(c.text));
    setColour(juce::ToggleButton::tickColourId, colour(c.accent));
    setColour(juce::ToggleButton::tickDisabledColourId, colour(c.textDim));

    // Built-in widgets (combo boxes, file choosers, ...) use the V4 colour scheme.
    getCurrentColourScheme().setUIColour(ColourScheme::windowBackground, colour(c.background));
    getCurrentColourScheme().setUIColour(ColourScheme::widgetBackground, colour(c.panelRaised));
    getCurrentColourScheme().setUIColour(ColourScheme::menuBackground, colour(c.panel));
    getCurrentColourScheme().setUIColour(ColourScheme::outline, colour(c.outline));
    getCurrentColourScheme().setUIColour(ColourScheme::defaultText, colour(c.text));
    getCurrentColourScheme().setUIColour(ColourScheme::defaultFill, colour(c.accent));
    getCurrentColourScheme().setUIColour(ColourScheme::highlightedText, colour(c.activeText));
    getCurrentColourScheme().setUIColour(ColourScheme::highlightedFill, colour(c.activeBackground));
    getCurrentColourScheme().setUIColour(ColourScheme::menuText, colour(c.text));

    family = bundledFont != nullptr ? *bundledFont : FontFamily{};
    if (bundledFont != nullptr)
    {
        setDefaultSansSerifTypeface(bundledFont->regular);
    }
    else
    {
        // Not bundled: ask the system for a font with that name (falls back if missing).
        setDefaultSansSerifTypeface(nullptr);
        setDefaultSansSerifTypefaceName(juce::String::fromUTF8(t.font.c_str()));
    }
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                       const juce::Colour& background, bool highlighted, bool down)
{
    const auto& shape = currentTheme().shape;
    auto bounds = button.getLocalBounds().toFloat().reduced(shape.borderWidth / 2.0f);

    auto fill = background;
    if (down)
        fill = fill.contrasting(0.15f);
    else if (highlighted)
        fill = fill.contrasting(0.07f);

    // Segments of a joined control (ui::joinSegments) are square where they meet and share
    // one border line there.
    const bool left = button.isConnectedOnLeft();
    const bool right = button.isConnectedOnRight();
    if (left)
        bounds.setLeft(bounds.getX() - shape.borderWidth / 2.0f);
    juce::Path box;
    box.addRoundedRectangle(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                            shape.cornerRadius, shape.cornerRadius, !left, !right, !left, !right);
    g.setColour(fill);
    g.fillPath(box);
    if (shape.borderWidth > 0.0f)
    {
        g.setColour(colour(currentTheme().colours.outline));
        g.strokePath(box, juce::PathStrokeType(shape.borderWidth));
    }
}

juce::Font LookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return juce::Font(controlFont(buttonHeight));
}

void LookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int,
                               juce::ComboBox& box)
{
    const auto& t = currentTheme();
    const auto bounds =
        juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height))
            .reduced(t.shape.borderWidth / 2.0f);
    auto fill = colour(t.colours.panelRaised);
    if (box.isMouseOver(true) && box.isEnabled())
        fill = fill.contrasting(0.07f);
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, t.shape.cornerRadius);
    if (t.shape.borderWidth > 0.0f)
    {
        g.setColour(colour(t.colours.outline));
        g.drawRoundedRectangle(bounds, t.shape.cornerRadius, t.shape.borderWidth);
    }
    const float icon = static_cast<float>(metrics::iconSize);
    const auto chevron =
        juce::Rectangle<float>(static_cast<float>(width) - icon - 6.0f,
                               (static_cast<float>(height) - icon) / 2.0f, icon, icon);
    drawIcon(g, Icon::chevronDown, chevron,
             colour(t.colours.textDim).withMultipliedAlpha(box.isEnabled() ? 1.0f : 0.5f));
}

juce::Font LookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    // Regular weight: a value, not an action.
    return juce::Font(
        juce::FontOptions(box.getHeight() >= metrics::transportControlHeight ? 14.0f : 13.0f));
}

void LookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(metrics::controlPadding - 2, 1,
                    box.getWidth() - metrics::controlPadding - metrics::iconSize - 6,
                    box.getHeight() - 2);
    label.setBorderSize({});
    label.setFont(getComboBoxFont(box));
}

void LookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar&, int x, int y, int width,
                                int height, bool vertical, int thumbStart, int thumbSize,
                                bool isMouseOver, bool isMouseDown)
{
    const auto& c = currentTheme().colours;
    auto thumb = vertical ? juce::Rectangle<int>(x, thumbStart, width, thumbSize)
                          : juce::Rectangle<int>(thumbStart, y, thumbSize, height);
    const float thickness = isMouseOver || isMouseDown ? 6.0f : 4.0f;
    auto bar = thumb.toFloat();
    bar = vertical ? bar.withSizeKeepingCentre(thickness, bar.getHeight() - 4.0f)
                   : bar.withSizeKeepingCentre(bar.getWidth() - 4.0f, thickness);
    g.setColour(
        colour(isMouseDown ? c.textDim : c.panelRaised).brighter(isMouseOver ? 0.25f : 0.1f));
    g.fillRoundedRectangle(bar, thickness / 2.0f);
}

void LookAndFeel::drawTreeviewPlusMinusBox(juce::Graphics& g, const juce::Rectangle<float>& area,
                                           juce::Colour, bool isOpen, bool isMouseOver)
{
    const auto& c = currentTheme().colours;
    const float size = std::min(static_cast<float>(metrics::iconSize), area.getHeight());
    auto box = area.withSizeKeepingCentre(size, size);
    juce::Graphics::ScopedSaveState state(g);
    if (!isOpen) // the chevron points down when open, right when closed
        g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi,
                                                       box.getCentreX(), box.getCentreY()));
    drawIcon(g, Icon::chevronDown, box, colour(isMouseOver ? c.text : c.textDim));
}

void LookAndFeel::drawKeymapChangeButton(juce::Graphics& g, int width, int height,
                                         juce::Button& button, const juce::String& keyDescription)
{
    const auto& t = currentTheme();
    const auto& c = t.colours;
    auto bounds = juce::Rectangle<float>(static_cast<float>(width), static_cast<float>(height))
                      .reduced(1.0f, 2.0f);
    if (keyDescription.isEmpty())
    {
        // The "+" that adds a key.
        drawIcon(g, Icon::add, bounds.withSizeKeepingCentre(12.0f, 12.0f),
                 colour(button.isOver() ? c.text : c.textDim));
        return;
    }
    g.setColour(colour(c.panelRaised).contrasting(button.isOver() ? 0.07f : 0.0f));
    g.fillRoundedRectangle(bounds, t.shape.cornerRadius);
    g.setColour(colour(c.outline));
    g.drawRoundedRectangle(bounds, t.shape.cornerRadius, 1.0f);
    g.setColour(colour(c.text));
    g.setFont(mediumFont(12.5f));
    g.drawFittedText(keyDescription, bounds.toNearestInt().reduced(4, 0),
                     juce::Justification::centred, 1);
}

void LookAndFeel::drawTableHeaderBackground(juce::Graphics& g, juce::TableHeaderComponent& header)
{
    const auto& c = currentTheme().colours;
    g.fillAll(colour(c.header));
    g.setColour(colour(c.outline));
    g.fillRect(0, header.getHeight() - 1, header.getWidth(), 1);
}

void LookAndFeel::drawTableHeaderColumn(juce::Graphics& g, juce::TableHeaderComponent&,
                                        const juce::String& columnName, int, int width, int height,
                                        bool isMouseOver, bool, int columnFlags)
{
    const auto& c = currentTheme().colours;
    if (isMouseOver)
    {
        g.setColour(colour(c.panelRaised).withAlpha(0.5f));
        g.fillRect(0, 0, width, height - 1);
    }
    g.setColour(colour(c.outline));
    g.fillRect(width - 1, 6, 1, height - 12);
    auto area = juce::Rectangle<int>(width, height).reduced(8, 0);
    const bool sorted = (columnFlags & (juce::TableHeaderComponent::sortedForwards |
                                        juce::TableHeaderComponent::sortedBackwards)) != 0;
    if (sorted)
    {
        auto arrow = area.removeFromRight(12).toFloat().withSizeKeepingCentre(10.0f, 10.0f);
        juce::Graphics::ScopedSaveState state(g);
        if ((columnFlags & juce::TableHeaderComponent::sortedBackwards) != 0)
            g.addTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::pi,
                                                           arrow.getCentreX(), arrow.getCentreY()));
        drawIcon(g, Icon::chevronDown, arrow, colour(c.textDim));
    }
    g.setColour(colour(sorted ? c.text : c.textDim));
    g.setFont(semiboldFont(12.5f));
    g.drawFittedText(columnName, area, juce::Justification::centredLeft, 1);
}

void LookAndFeel::drawTickBox(juce::Graphics& g, juce::Component&, float x, float y, float w,
                              float h, bool ticked, bool isEnabled, bool isMouseOverButton, bool)
{
    const auto& t = currentTheme();
    const auto& c = t.colours;
    const float size = std::min({w, h, 16.0f});
    const auto box = juce::Rectangle<float>(x, y, w, h).withSizeKeepingCentre(size, size);
    const float radius = std::min(4.0f, t.shape.cornerRadius);
    g.setColour(colour(ticked ? c.accent : c.panelRaised)
                    .withMultipliedAlpha(isEnabled ? 1.0f : 0.5f)
                    .brighter(isMouseOverButton ? 0.1f : 0.0f));
    g.fillRoundedRectangle(box, radius);
    if (!ticked)
    {
        g.setColour(colour(c.outline).brighter(0.2f));
        g.drawRoundedRectangle(box.reduced(0.5f), radius, 1.0f);
        return;
    }
    juce::Path tick;
    tick.startNewSubPath(box.getX() + size * 0.25f, box.getCentreY());
    tick.lineTo(box.getX() + size * 0.43f, box.getY() + size * 0.7f);
    tick.lineTo(box.getRight() - size * 0.22f, box.getY() + size * 0.3f);
    g.setColour(colour(c.background));
    g.strokePath(tick, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
}

void LookAndFeel::drawLevelMeter(juce::Graphics& g, int width, int height, float level)
{
    const auto& t = currentTheme();
    const auto& c = t.colours;
    const auto bounds =
        juce::Rectangle<float>(static_cast<float>(width), static_cast<float>(height));
    paintDisplayBox(g, bounds);
    // Segments like the mixer's meters: lit up to the level, the top ones in the accent colour.
    constexpr int segments = 24;
    const auto inner = bounds.reduced(4.0f, 3.0f);
    const float segmentWidth = inner.getWidth() / segments;
    const int lit = static_cast<int>(std::pow(std::clamp(level, 0.0f, 1.0f), 0.5f) * segments);
    for (int i = 0; i < segments; ++i)
    {
        const auto segment = juce::Rectangle<float>(inner.getX() + i * segmentWidth, inner.getY(),
                                                    segmentWidth - 2.0f, inner.getHeight());
        const auto on = colour(i >= segments * 5 / 6 ? c.accent : c.led);
        g.setColour(i < lit ? on : colour(c.ledOff));
        g.fillRoundedRectangle(segment, 1.5f);
    }
}

void LookAndFeel::drawMenuBarBackground(juce::Graphics&, int, int, bool, juce::MenuBarComponent&)
{
    // Transparent: the title bar behind it is the background.
}

void LookAndFeel::drawMenuBarItem(juce::Graphics& g, int width, int height, int,
                                  const juce::String& itemText, bool isMouseOverItem,
                                  bool isMenuOpen, bool, juce::MenuBarComponent& bar)
{
    const auto& t = currentTheme();
    if (isMenuOpen || isMouseOverItem)
    {
        g.setColour(colour(t.colours.panelRaised));
        g.fillRoundedRectangle(
            juce::Rectangle<float>(static_cast<float>(width), static_cast<float>(height))
                .reduced(1.0f, 0.0f),
            t.shape.cornerRadius);
    }
    g.setColour(colour(isMenuOpen ? t.colours.activeText : t.colours.text));
    g.setFont(getMenuBarFont(bar, 0, itemText));
    g.drawText(itemText, 0, 0, width, height, juce::Justification::centred, false);
}

juce::Font LookAndFeel::getMenuBarFont(juce::MenuBarComponent&, int, const juce::String&)
{
    return juce::Font(mediumFont(13.0f));
}

void LookAndFeel::drawDocumentWindowTitleBar(juce::DocumentWindow& window, juce::Graphics& g, int w,
                                             int h, int, int, const juce::Image*, bool)
{
    const auto& t = currentTheme();
    g.setColour(colour(t.colours.header));
    g.fillRect(0, 0, w, h);
    g.setColour(colour(t.colours.outline));
    g.fillRect(0, h - 1, w, 1);
    g.setColour(colour(t.colours.text));
    g.setFont(semiboldFont(14.0f));
    g.drawText(window.getName(), 12, 0, w - 12 - 3 * h, h, juce::Justification::centredLeft, true);
}

juce::Button* LookAndFeel::createDocumentWindowButton(int buttonType)
{
    switch (buttonType)
    {
    case juce::DocumentWindow::minimiseButton:
        return new TitleButton(Icon::windowMinimize, false);
    case juce::DocumentWindow::maximiseButton:
        return new TitleButton(Icon::windowMaximize, false);
    default:
        return new TitleButton(Icon::close, true);
    }
}

void LookAndFeel::positionDocumentWindowButtons(juce::DocumentWindow&, int titleBarX, int titleBarY,
                                                int titleBarW, int titleBarH,
                                                juce::Button* minimiseButton,
                                                juce::Button* maximiseButton,
                                                juce::Button* closeButton, bool)
{
    // Right-aligned, full height, like the main window's title bar.
    const int width = titleBarH * 11 / 8;
    int x = titleBarX + titleBarW;
    for (auto* button : {closeButton, maximiseButton, minimiseButton})
        if (button != nullptr)
        {
            x -= width;
            button->setBounds(x, titleBarY, width, titleBarH - 1);
        }
}

void LookAndFeel::drawResizableFrame(juce::Graphics& g, int w, int h, const juce::BorderSize<int>&)
{
    g.setColour(colour(currentTheme().colours.outline));
    g.drawRect(0, 0, w, h, 1);
}

juce::Font LookAndFeel::getAlertWindowTitleFont()
{
    return juce::Font(semiboldFont(15.0f));
}

juce::Font LookAndFeel::getAlertWindowMessageFont()
{
    return juce::Font(juce::FontOptions(14.0f));
}

juce::Font LookAndFeel::getAlertWindowFont()
{
    return juce::Font(juce::FontOptions(14.0f));
}

int LookAndFeel::getAlertWindowButtonHeight()
{
    return metrics::controlHeight + 2;
}

void LookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto& t = currentTheme();
    const auto bounds =
        juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height))
            .reduced(0.5f);
    g.setColour(colour(t.colours.panelRaised));
    g.fillRoundedRectangle(bounds, t.shape.cornerRadius);
    g.setColour(colour(t.colours.outline));
    g.drawRoundedRectangle(bounds, t.shape.cornerRadius, 1.0f);
    g.setColour(colour(t.colours.text));
    g.setFont(juce::FontOptions(13.0f));
    g.drawFittedText(text, juce::Rectangle<int>(width, height).reduced(8, 4),
                     juce::Justification::centredLeft, 6);
}

juce::Rectangle<int> LookAndFeel::getTooltipBounds(const juce::String& tipText,
                                                   juce::Point<int> screenPos,
                                                   juce::Rectangle<int> parentArea)
{
    juce::TextLayout layout;
    juce::AttributedString text(tipText);
    text.setFont(juce::FontOptions(13.0f));
    layout.createLayoutWithBalancedLineLengths(text, 360.0f);
    const int w = static_cast<int>(std::ceil(layout.getWidth())) + 18;
    const int h = static_cast<int>(std::ceil(layout.getHeight())) + 10;
    return juce::Rectangle<int>(
               screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
               screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w,
               h)
        .constrainedWithin(parentArea);
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                   float position, float startAngle, float endAngle,
                                   juce::Slider& slider)
{
    const auto& c = currentTheme().colours;
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2.0f);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float lineWidth = std::max(2.0f, radius * 0.22f);
    const float arcRadius = radius - lineWidth / 2.0f;
    const juce::PathStrokeType stroke(lineWidth, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour(colour(c.display));
    g.strokePath(track, stroke);

    // Bipolar knobs (pan) fill from the centre, others from the start.
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float fromAngle = bipolar ? (startAngle + endAngle) / 2.0f : startAngle;
    const float toAngle = startAngle + position * (endAngle - startAngle);

    juce::Path value;
    value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, fromAngle, toAngle, true);
    g.setColour(slider.isEnabled() ? colour(c.accent) : colour(c.textDim));
    g.strokePath(value, stroke);

    const juce::Point<float> tip(centre.x + arcRadius * 0.6f * std::sin(toAngle),
                                 centre.y - arcRadius * 0.6f * std::cos(toAngle));
    g.setColour(colour(c.text));
    g.drawLine({centre, tip}, lineWidth * 0.7f);
}

// ------------------------------------------------------------------------------- ThemeManager

ThemeManager::ThemeManager() : active(theme::defaultTheme())
{
    // Each family in the weights the UI uses. The Medium and SemiBold files are cut from the
    // variable fonts and keep the family's name, so they're filed by family here, not by name.
    const auto load = [](const char* data, int size)
    { return juce::Typeface::createSystemTypefaceFor(data, static_cast<size_t>(size)); };
    const auto add = [this](const char* name, juce::Typeface::Ptr regular,
                            juce::Typeface::Ptr medium, juce::Typeface::Ptr semibold)
    { fonts[juce::String(name).toLowerCase()] = {regular, medium, semibold}; };
    using namespace BinaryData;
    add("IBM Plex Sans", load(IBMPlexSans_ttf, IBMPlexSans_ttfSize),
        load(IBMPlexSansMedium_ttf, IBMPlexSansMedium_ttfSize),
        load(IBMPlexSansSemiBold_ttf, IBMPlexSansSemiBold_ttfSize));
    add("Bricolage Grotesque", load(BricolageGrotesque_ttf, BricolageGrotesque_ttfSize),
        load(BricolageGrotesqueMedium_ttf, BricolageGrotesqueMedium_ttfSize),
        load(BricolageGrotesqueSemiBold_ttf, BricolageGrotesqueSemiBold_ttfSize));
    add("Sora", load(Sora_ttf, Sora_ttfSize), load(SoraMedium_ttf, SoraMedium_ttfSize),
        load(SoraSemiBold_ttf, SoraSemiBold_ttfSize));
    add("Outfit", load(Outfit_ttf, Outfit_ttfSize), load(OutfitMedium_ttf, OutfitMedium_ttfSize),
        load(OutfitSemiBold_ttf, OutfitSemiBold_ttfSize));

    juce::PropertiesFile::Options options;
    options.applicationName = "JuicyLoopsStudio";
    options.folderName = "JuicyLoopsStudio";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    // Write changes within a second, not only on a clean exit (a crash or kill would lose them).
    options.millisecondsBeforeSaving = 1000;
#if JUCE_LINUX || JUCE_BSD
    // JUCE's default on Linux is a folder directly in $HOME; follow the XDG convention instead.
    const auto xdg = juce::SystemStats::getEnvironmentVariable("XDG_CONFIG_HOME", {});
    const auto configDir = xdg.isNotEmpty() ? juce::File(xdg) : juce::File("~/.config");
    settings = std::make_unique<juce::PropertiesFile>(
        configDir.getChildFile("JuicyLoopsStudio/JuicyLoopsStudio.settings"), options);
#else
    settings = std::make_unique<juce::PropertiesFile>(options);
#endif

    activeTheme = &active;
    rescan();
    select(settings->getValue(themeSettingKey, "juicy").toStdString());
}

ThemeManager::~ThemeManager()
{
    if (activeTheme == &active)
        activeTheme = nullptr;
}

std::vector<theme::Theme> ThemeManager::available() const
{
    auto all = theme::builtInThemes();
    all.insert(all.end(), userThemes.begin(), userThemes.end());
    return all;
}

juce::File ThemeManager::themesFolder() const
{
    return settings->getFile().getParentDirectory().getChildFile("themes");
}

void ThemeManager::rescan()
{
    userThemes.clear();
    auto files = themesFolder().findChildFiles(juce::File::findFiles, false, "*.json");
    files.sort();
    for (const auto& file : files)
    {
        auto result = theme::parseTheme(file.loadFileAsString().toStdString());
        if (!result.theme)
            continue; // unusable files are reported when imported, skipped here
        result.theme->id = customPrefix + file.getFileName().toStdString();
        userThemes.push_back(std::move(*result.theme));
    }
}

void ThemeManager::select(const std::string& id)
{
    const auto all = available();
    const auto it = std::find_if(all.begin(), all.end(), [&](const auto& t) { return t.id == id; });
    active = it != all.end() ? *it : theme::defaultTheme();

    settings->setValue(themeSettingKey, juce::String(active.id));
    settings->saveIfNeeded();
    apply();
}

const FontFamily* ThemeManager::bundledFont(const std::string& family) const
{
    const auto it = fonts.find(juce::String::fromUTF8(family.c_str()).toLowerCase());
    return it != fonts.end() && it->second.regular != nullptr ? &it->second : nullptr;
}

void ThemeManager::apply()
{
    laf.applyTheme(active, bundledFont(active.font));

    // Repaint everything that's on screen with the new look.
    auto& desktop = juce::Desktop::getInstance();
    for (int i = 0; i < desktop.getNumComponents(); ++i)
        if (auto* window = desktop.getComponent(i))
            window->sendLookAndFeelChange();

    sendChangeMessage();
}

ThemeManager::ImportResult ThemeManager::importFile(const juce::File& file)
{
    const auto parsed = theme::parseTheme(file.loadFileAsString().toStdString());
    if (!parsed.theme)
        return {false, "Couldn't import " + file.getFileName() + ": " +
                           juce::String::fromUTF8(parsed.error.c_str())};

    const auto folder = themesFolder();
    if (!folder.createDirectory())
        return {false, "Couldn't create the themes folder: " + folder.getFullPathName()};

    auto destination = file;
    if (file.getParentDirectory() != folder)
    {
        destination =
            folder.getNonexistentChildFile(file.getFileNameWithoutExtension(), ".json", false);
        if (!file.copyFileTo(destination))
            return {false, "Couldn't copy the theme into " + folder.getFullPathName()};
    }

    rescan();
    select(customPrefix + destination.getFileName().toStdString());

    juce::String message;
    if (!parsed.warnings.empty())
    {
        message << "Imported \"" << juce::String::fromUTF8(parsed.theme->name.c_str())
                << "\", but some settings were skipped:\n";
        for (std::size_t i = 0; i < parsed.warnings.size() && i < 8; ++i)
            message << "\n- " << juce::String::fromUTF8(parsed.warnings[i].c_str());
        if (parsed.warnings.size() > 8)
            message << "\n(and " << static_cast<int>(parsed.warnings.size() - 8) << " more)";
    }
    return {true, message};
}

juce::Result ThemeManager::exportCurrent(const juce::File& destination) const
{
    if (destination.replaceWithText(juce::String::fromUTF8(theme::serializeTheme(active).c_str())))
        return juce::Result::ok();
    return juce::Result::fail("Couldn't write " + destination.getFullPathName());
}

} // namespace jls::ui
