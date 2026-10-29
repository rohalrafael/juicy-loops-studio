#pragma once

// The app's own native windows, past JUCE, on Linux (X11; XWayland on Wayland desktops). JUCE
// won't move the X input focus while it's anywhere inside our window (a plugin's embedded window
// included), and it can't restack child windows (ComponentPeer::toFront only asks the window
// manager to activate top-level ones). Kept apart so Xlib's macros stay out of other files.
// Elsewhere these do nothing.

#include <vector>

namespace jls::app
{

// Sets the input focus to `nativeWindow` (a ComponentPeer's native handle). Call only while the
// app has the focus: it doesn't check.
void focusNativeWindow(void* nativeWindow);

// Stacks sibling child windows in this order, the first on top (see AreaStack).
void restackNativeWindows(const std::vector<void*>& frontToBack);

} // namespace jls::app
