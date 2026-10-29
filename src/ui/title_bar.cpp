#include "ui/title_bar.h"

#include "ui/theme.h"

namespace jls::ui
{
namespace
{
constexpr int windowButtonWidth = 44;
} // namespace

TitleButton::TitleButton(Icon iconToShow, bool isClose)
    : juce::Button({}), icon(iconToShow), close(isClose)
{
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
}

void TitleButton::setIcon(Icon newIcon)
{
    icon = newIcon;
    repaint();
}

void TitleButton::paintButton(juce::Graphics& g, bool highlighted, bool down)
{
    const auto& c = currentTheme().colours;
    if (highlighted || down)
    {
        auto fill = close ? colour(c.accent) : colour(c.panelRaised);
        g.setColour(down ? fill.darker(0.15f) : fill);
        g.fillRect(getLocalBounds());
    }
    const auto ink = close && (highlighted || down) ? colour(c.background) : colour(c.text);
    const auto iconSize = static_cast<float>(metrics::iconSize);
    drawIcon(g, icon, getLocalBounds().toFloat().withSizeKeepingCentre(iconSize, iconSize), ink);
}

void useOwnTitleBar(juce::DocumentWindow* window)
{
#if !JUCE_MAC
    if (window == nullptr)
        return;
    window->setUsingNativeTitleBar(false);
    window->setTitleBarHeight(TitleBar::height);
#else
    juce::ignoreUnused(window);
#endif
}

TitleBar::TitleBar(juce::MenuBarModel& menus) : menuBar(&menus)
{
    addAndMakeVisible(menuBar);
    minimise.setTooltip("Minimize");
    maximise.setTooltip("Maximize");
    close.setTooltip("Close");
    minimise.onClick = [this]
    {
        if (auto* w = window())
            w->setMinimised(true);
    };
    maximise.onClick = [this] { toggleMaximised(); };
    close.onClick = [this]
    {
        if (auto* w = window())
            w->closeButtonPressed();
    };
    for (auto* b : {&minimise, &maximise, &close})
        addAndMakeVisible(b);
}

juce::DocumentWindow* TitleBar::window() const
{
    return findParentComponentOfClass<juce::DocumentWindow>();
}

void TitleBar::toggleMaximised()
{
    // JUCE's "full screen" for a window is maximized: the screen's usable area (a real
    // maximize on Windows).
    if (auto* w = window())
        w->setFullScreen(!w->isFullScreen());
    updateMaximiseIcon();
}

void TitleBar::updateMaximiseIcon()
{
    const auto* w = window();
    const bool maximised = w != nullptr && w->isFullScreen();
    maximise.setIcon(maximised ? Icon::windowRestore : Icon::windowMaximize);
    maximise.setTooltip(maximised ? "Restore" : "Maximize");
}

void TitleBar::paint(juce::Graphics& g)
{
    const auto& t = currentTheme();
    g.fillAll(colour(t.colours.header));
    g.setColour(colour(t.colours.outline));
    g.fillRect(0, getHeight() - 1, getWidth(), 1);

    // The window's name, centred in the bar (not just in the space between menus and buttons).
    if (const auto* w = window())
    {
        const int side = std::max(menuBar.getRight(), getWidth() - minimise.getX()) + 8;
        auto area = getLocalBounds().reduced(side, 0);
        if (area.getWidth() < 80) // not enough room in the middle: whatever is free
            area = getLocalBounds().withLeft(menuBar.getRight() + 8).withRight(minimise.getX() - 8);
        g.setColour(colour(t.colours.textDim));
        g.setFont(mediumFont(metrics::textBody));
        g.drawText(w->getName(), area, juce::Justification::centred, true);
    }
}

void TitleBar::resized()
{
    auto area = getLocalBounds().withTrimmedBottom(1);
    close.setBounds(area.removeFromRight(windowButtonWidth));
    maximise.setBounds(area.removeFromRight(windowButtonWidth));
    minimise.setBounds(area.removeFromRight(windowButtonWidth));

    // The menu bar only as wide as its items, so the rest of the bar drags the window. Item
    // widths depend on the bar's height, so that's set first.
    menuBar.setBounds(area.withWidth(area.getWidth() / 2).reduced(0, 3));
    int width = 6;
    auto* model = menuBar.getModel();
    if (model != nullptr)
    {
        const auto names = model->getMenuBarNames();
        for (int i = 0; i < names.size(); ++i)
            width += getLookAndFeel().getMenuBarItemWidth(menuBar, i, names[i]);
    }
    menuBar.setBounds(area.removeFromLeft(width).reduced(0, 3));
    updateMaximiseIcon();
}

void TitleBar::mouseDown(const juce::MouseEvent& e)
{
    if (auto* w = window(); w != nullptr && !w->isFullScreen())
        dragger.startDraggingComponent(w, e.getEventRelativeTo(w));
}

void TitleBar::mouseDrag(const juce::MouseEvent& e)
{
    if (auto* w = window(); w != nullptr && !w->isFullScreen())
        dragger.dragComponent(w, e.getEventRelativeTo(w), nullptr);
}

void TitleBar::mouseDoubleClick(const juce::MouseEvent&)
{
    toggleMaximised();
}

} // namespace jls::ui
