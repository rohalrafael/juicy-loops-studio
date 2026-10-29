#include "ui/floating_panel.h"

#include "ui/theme.h"

#include <juce_gui_extra/juce_gui_extra.h>

namespace jls::ui
{
namespace
{
constexpr int border = 1;       // around the content, inside the frame
constexpr int cascadeStep = 24; // offset between panels opened one after another
} // namespace

FloatingPanel::FloatingPanel(std::unique_ptr<juce::Component> content) : held(std::move(content))
{
    closeButton.setTooltip("Close");
    closeButton.onClick = [this]
    {
        if (onClose)
            onClose();
    };
    addAndMakeVisible(closeButton);
    addAndMakeVisible(*held);
    // Kept as an image: dragging the panel then only copies it instead of drawing every label,
    // knob and waveform again for each frame (measured: ~9 ms per frame for the Sampler).
    setBufferedToImage(true);
    held->addComponentListener(this);
    held->addMouseListener(this, true); // a click anywhere in it brings the panel to the front
    fitToContent();
}

FloatingPanel::~FloatingPanel()
{
    deleteContent();
}

void FloatingPanel::deleteContent()
{
    if (held != nullptr)
    {
        held->removeComponentListener(this);
        held->removeMouseListener(this);
        held.reset();
    }
}

void FloatingPanel::setTitle(const juce::String& newTitle, const juce::String& newSubtitle)
{
    if (newTitle == title && newSubtitle == subtitle)
        return;
    title = newTitle;
    subtitle = newSubtitle;
    repaint(0, 0, getWidth(), metrics::panelHeaderHeight);
}

void FloatingPanel::fitToContent()
{
    if (held == nullptr)
        return;
    setSize(held->getWidth() + 2 * border, held->getHeight() + metrics::panelHeaderHeight + border);
    // Plugin editors often pick their size a moment after opening: keep the title bar
    // reachable and the frame drawn at the new size.
    keepReachable();
    repaint();
}

void FloatingPanel::componentMovedOrResized(juce::Component& c, bool, bool wasResized)
{
    if (&c == held.get() && wasResized)
        fitToContent();
}

void FloatingPanel::setMovableArea(juce::Rectangle<int> area)
{
    movableArea = area;
    keepReachable();
}

void FloatingPanel::placeInitially(int cascade)
{
    const auto area = movableArea.isEmpty() && getParentComponent() != nullptr
                          ? getParentComponent()->getLocalBounds()
                          : movableArea;
    setTopLeftPosition(area.getCentreX() - getWidth() / 2 + cascade * cascadeStep,
                       std::max(area.getY(), area.getCentreY() - getHeight() / 2) +
                           cascade * cascadeStep);
    keepReachable();
}

void FloatingPanel::keepReachable()
{
    keepTitleBarReachable(*this, movableArea);
}

void keepTitleBarReachable(juce::Component& panel, juce::Rectangle<int> area)
{
    // JUCE's constrainer only keeps a few pixels on screen and lets the title bar leave the top
    // (under the transport bar), where it can't be grabbed again: the drag got "stuck".
    if (area.isEmpty())
        return;
    constexpr int keepVisible = 80; // of the width, at the left or right edge
    const int x = std::clamp(panel.getX(), area.getX() - panel.getWidth() + keepVisible,
                             std::max(area.getX(), area.getRight() - keepVisible));
    const int y = std::clamp(panel.getY(), area.getY(),
                             std::max(area.getY(), area.getBottom() - metrics::panelHeaderHeight));
    if (x != panel.getX() || y != panel.getY())
        panel.setTopLeftPosition(x, y);
}

void FloatingPanel::paint(juce::Graphics& g)
{
    paintFloatingPanel(g, *this, title, subtitle, closeButton.getX() - metrics::controlGap);
}

void FloatingPanel::paintOverChildren(juce::Graphics& g)
{
    paintFloatingBorder(g, *this);
}

void FloatingPanel::resized()
{
    HeaderLayout header(getLocalBounds().removeFromTop(metrics::panelHeaderHeight));
    closeButton.setBounds(header.fromRight(metrics::controlHeight));
    if (held != nullptr)
        held->setTopLeftPosition(border, metrics::panelHeaderHeight);
}

void FloatingPanel::moved()
{
    updateEmbeddedWindows();
}

void FloatingPanel::updateEmbeddedWindows()
{
#if JUCE_LINUX || JUCE_BSD
    // A plugin editor on Linux is a native window inside a juce::XEmbedComponent, which only
    // follows moves of itself, not of the panel around it: tell it, or it stays behind when the
    // panel is dragged.
    std::function<void(juce::Component&)> visit = [&visit](juce::Component& c)
    {
        if (auto* embedded = dynamic_cast<juce::XEmbedComponent*>(&c))
            embedded->updateEmbeddedBounds();
        for (auto* child : c.getChildren())
            visit(*child);
    };
    if (held != nullptr)
        visit(*held);
#endif
}

void FloatingPanel::mouseDown(const juce::MouseEvent& e)
{
    toFront(false);
    // Remembered: e.mouseDownPosition is relative to where the panel is now (see ChannelRack).
    // Clicks in the content arrive here too (mouse listener); only the title bar drags.
    moving = e.eventComponent == this && e.y < metrics::panelHeaderHeight;
    if (moving)
        dragger.startDraggingComponent(this, e);
}

void FloatingPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (!moving)
        return;
    dragger.dragComponent(this, e, nullptr);
    keepReachable();
}

} // namespace jls::ui
