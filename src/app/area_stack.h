#pragma once

// The stacking of the areas that can overlap a plugin's editor: the Playlist, Piano Roll, Channel
// Rack, Mixer, Browser, the Sampler, the effect panel and plugin panels.
//
// A plugin's editor is a native window, which JUCE can't draw over. So on Linux each area gets a
// native child window of the main window too (Component::addToDesktop with the main window as
// parent), and the X server stacks them, plugins included, like FL's windows: a plugin can go
// behind the Playlist and come to the front again. The title bar and transport bar stay in the
// main window, which is always below its children; areas never overlap them.
//
// Elsewhere (Windows and macOS, until their restacking is written and tested) and with the
// environment variable JLS_NO_NATIVE_AREAS set, areas stay ordinary child components of the
// host, as before, and plugin panels stay in front.

#include <juce_gui_basics/juce_gui_basics.h>

#include <map>
#include <vector>

namespace jls::app
{

class AreaStack final : private juce::ComponentListener
{
  public:
    // `host`: the component that lays the areas out (MainComponent).
    explicit AreaStack(juce::Component& host);
    ~AreaStack() override;

    bool isNative() const { return native; }

    // Takes `area` in, in front of the others (its visibility is left as it is). The host's mouse
    // listener gets its events too, as for its own children.
    void add(juce::Component& area);
    // Areas are forgotten when deleted; this is for taking one out earlier.
    void remove(juce::Component& area);

    // Stacking. Calling toFront() on an area raises it here too.
    void raise(juce::Component& area);
    // Whether a visible area above `area` overlaps it: toggles (clicking a channel's name for its
    // plugin, F6 for the rack) bring a covered window to the front instead of closing it, as in FL.
    // `ignoring`: an area that doesn't count, e.g. the Channel Rack when its own button was
    // clicked (it's necessarily on top there).
    bool isCovered(const juce::Component& area, const juce::Component* ignoring = nullptr) const;

    // Places `area` at `bounds` in the host's coordinates.
    void setBounds(juce::Component& area, juce::Rectangle<int> bounds);
    // A rectangle in the host's coordinates in the areas' own (movable areas, centring).
    juce::Rectangle<int> toAreaSpace(juce::Rectangle<int> inHost) const;
    // Whether a native area is showing at `screenPos`: the host's window says it isn't there,
    // so JUCE's look-ups across windows (drag and drop) go on to the area.
    bool coversPoint(juce::Point<int> screenPos) const;
    // Whether `component` is one of the areas or inside one (native areas aren't the host's
    // children, so the host's isParentOf() doesn't see them).
    bool holds(const juce::Component* component) const;

    // Attaches the areas to the host's window, again if it was recreated. Call often (a timer):
    // the window doesn't exist yet while the host is built.
    void update();

  private:
    void componentBroughtToFront(juce::Component& component) override;
    void componentBeingDeleted(juce::Component& component) override;
    void attach(juce::Component& area);
    void restack();

    juce::Component& host;
    const bool native;
    void* attachedTo = nullptr;          // the host window's native handle, once attached
    std::vector<juce::Component*> order; // back to front
    std::map<juce::Component*, juce::Rectangle<int>> wanted; // host-space bounds
    bool raising = false;
};

} // namespace jls::app
