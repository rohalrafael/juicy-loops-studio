#include "app/area_stack.h"

#include "app/native_windows.h"

#include <algorithm>
#include <cstdlib>

namespace jls::app
{

namespace
{

bool useNativeAreas()
{
#if JUCE_LINUX || JUCE_BSD
    return std::getenv("JLS_NO_NATIVE_AREAS") == nullptr;
#else
    return false;
#endif
}

} // namespace

AreaStack::AreaStack(juce::Component& hostComponent) : host(hostComponent), native(useNativeAreas())
{
}

AreaStack::~AreaStack()
{
    for (auto* area : order)
        area->removeComponentListener(this);
}

void AreaStack::add(juce::Component& area)
{
    if (std::find(order.begin(), order.end(), &area) != order.end())
        return;
    order.push_back(&area);
    area.addComponentListener(this);
    if (!native)
    {
        host.addChildComponent(area);
        return;
    }
    // Not a child of the host: its mouse listener wouldn't see the area's events otherwise.
    area.addMouseListener(&host, true);
    if (attachedTo != nullptr)
    {
        attach(area);
        restack();
    }
}

void AreaStack::remove(juce::Component& area)
{
    const auto it = std::find(order.begin(), order.end(), &area);
    if (it == order.end())
        return;
    order.erase(it);
    wanted.erase(&area);
    area.removeComponentListener(this);
    if (native)
    {
        area.removeMouseListener(&host);
        area.removeFromDesktop();
    }
    else
    {
        host.removeChildComponent(&area);
    }
}

void AreaStack::raise(juce::Component& area)
{
    const auto it = std::find(order.begin(), order.end(), &area);
    if (it == order.end())
        return;
    if (!native)
    {
        area.toFront(false);
        return;
    }
    order.erase(it);
    order.push_back(&area);
    restack();
    // JUCE keeps its own order of desktop windows, which its look-ups across windows follow (a
    // drag finds its drop target in it): bring the area forward there too.
    if (!raising)
    {
        raising = true;
        area.toFront(false);
        raising = false;
    }
}

bool AreaStack::isCovered(const juce::Component& area, const juce::Component* ignoring) const
{
    if (!area.isShowing())
        return false;
    const auto bounds = area.getScreenBounds();
    if (!native)
    {
        // Children above it in the host, the same idea.
        for (int i = host.getIndexOfChildComponent(&area) + 1; i < host.getNumChildComponents();
             ++i)
            if (auto* above = host.getChildComponent(i);
                above != ignoring && above->isVisible() &&
                std::find(order.begin(), order.end(), above) != order.end() &&
                above->getScreenBounds().intersects(bounds))
                return true;
        return false;
    }
    const auto it = std::find(order.begin(), order.end(), &area);
    for (auto above = it == order.end() ? order.end() : it + 1; above != order.end(); ++above)
        if (*above != ignoring && (*above)->isShowing() &&
            (*above)->getScreenBounds().intersects(bounds))
            return true;
    return false;
}

void AreaStack::setBounds(juce::Component& area, juce::Rectangle<int> bounds)
{
    if (!native)
    {
        area.setBounds(bounds);
        return;
    }
    wanted[&area] = bounds;
    if (area.isOnDesktop())
        area.setBounds(toAreaSpace(bounds));
}

juce::Rectangle<int> AreaStack::toAreaSpace(juce::Rectangle<int> inHost) const
{
    if (!native)
        return inHost;
    // Native areas are placed in the host window's coordinates.
    if (auto* peer = host.getPeer())
        return peer->getComponent().getLocalArea(&host, inHost);
    return inHost;
}

bool AreaStack::coversPoint(juce::Point<int> screenPos) const
{
    if (!native)
        return false;
    return std::any_of(order.begin(), order.end(),
                       [screenPos](const juce::Component* area)
                       {
                           return area->isOnDesktop() && area->isShowing() &&
                                  area->getScreenBounds().contains(screenPos);
                       });
}

bool AreaStack::holds(const juce::Component* component) const
{
    return component != nullptr &&
           std::any_of(order.begin(), order.end(), [component](const juce::Component* area)
                       { return area == component || area->isParentOf(component); });
}

void AreaStack::update()
{
    if (!native)
        return;
    auto* peer = host.getPeer();
    auto* handle = peer != nullptr ? peer->getNativeHandle() : nullptr;
    if (handle == attachedTo)
        return;
    attachedTo = handle;
    if (handle == nullptr)
        return;
    for (auto* area : order)
        attach(*area);
    restack();
}

void AreaStack::attach(juce::Component& area)
{
    // Keys stay with the main window (shortcuts, typed notes): JUCE hands them to whichever
    // component has the keyboard focus, in any of our windows.
    const bool visible = area.isVisible();
    area.addToDesktop(juce::ComponentPeer::windowIgnoresKeyPresses, attachedTo);
    if (const auto it = wanted.find(&area); it != wanted.end())
        area.setBounds(toAreaSpace(it->second));
    area.setVisible(visible);
}

void AreaStack::restack()
{
    std::vector<void*> frontToBack;
    for (auto it = order.rbegin(); it != order.rend(); ++it)
        if (auto* peer = (*it)->getPeer(); peer != nullptr && (*it)->isOnDesktop())
            frontToBack.push_back(peer->getNativeHandle());
    restackNativeWindows(frontToBack);
}

void AreaStack::componentBroughtToFront(juce::Component& component)
{
    // toFront() on a native area only asks the window manager, which doesn't restack child
    // windows: do it here.
    if (native && !raising)
    {
        raising = true;
        raise(component);
        raising = false;
    }
}

void AreaStack::componentBeingDeleted(juce::Component& component)
{
    order.erase(std::remove(order.begin(), order.end(), &component), order.end());
    wanted.erase(&component);
}

} // namespace jls::app
