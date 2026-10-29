#pragma once

// A panel that floats inside the main window, like FL's channel and plugin windows: the shared
// header (title, dim subtitle, close button), dragged by its title bar and kept below the
// transport bar, brought to the front when clicked. It holds one content component (the Sampler,
// a plugin's editor) and sizes itself around it, following the content when that resizes itself
// (plugin editors do, e.g. when zoomed).

#include "ui/controls.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace jls::ui
{

class FloatingPanel : public juce::Component, private juce::ComponentListener
{
  public:
    explicit FloatingPanel(std::unique_ptr<juce::Component> content);
    ~FloatingPanel() override;

    void setTitle(const juce::String& title, const juce::String& subtitle = {});
    juce::Component* content() const { return held.get(); }
    // Deletes the content now (a plugin editor must go before its plugin is released).
    void deleteContent();

    // Where it may be dragged (the area below the transport bar); its title bar stays inside.
    void setMovableArea(juce::Rectangle<int> area);
    // First placement: centred in the movable area, offset by `cascade` steps so several
    // panels don't stack exactly.
    void placeInitially(int cascade);

    std::function<void()> onClose; // the close button (the owner deletes or hides the panel)
    // Shows a keyboard button in the title bar that runs `handler`: plugin panels use it to give
    // the keyboard back to the app, which a plugin's editor keeps after a click into it.
    void setKeyboardReturn(std::function<void()> handler);

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    void moved() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

  private:
    void componentMovedOrResized(juce::Component& c, bool moved, bool resized) override;
    void fitToContent();
    void keepReachable();
    void updateEmbeddedWindows();

    std::unique_ptr<juce::Component> held;
    juce::String title, subtitle;
    IconButton closeButton{Icon::close};
    IconButton keyboardButton{Icon::keys}; // shown with setKeyboardReturn()
    juce::ComponentDragger dragger;
    juce::Rectangle<int> movableArea;
    bool moving = false;
};

// Moves a panel dragged by its title bar (a FloatingPanel, the Channel Rack) as little as needed
// to keep its title bar inside `area` and some of its width in view, so it can always be grabbed
// again. Does nothing while `area` is empty.
void keepTitleBarReachable(juce::Component& panel, juce::Rectangle<int> area);

} // namespace jls::ui
