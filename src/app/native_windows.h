#pragma once

// The app's own native windows, past JUCE, on Linux (X11; XWayland on Wayland desktops). JUCE
// won't move the X input focus while it's anywhere inside our window (a plugin's embedded window
// included), and it can't restack child windows (ComponentPeer::toFront only asks the window
// manager to activate top-level ones). Kept apart so Xlib's macros stay out of other files.
// Elsewhere these do nothing.

#include <cstdint>
#include <vector>

namespace jls::app
{

// Sets the input focus to `nativeWindow` (a ComponentPeer's native handle). Call only while the
// app has the focus: it doesn't check.
void focusNativeWindow(void* nativeWindow);

// Stacks sibling child windows in this order, the first on top (see AreaStack).
void restackNativeWindows(const std::vector<void*>& frontToBack);

// The colour (0xRRGGBB) the X server fills a window with where it's exposed, before the app
// repaints there: where a child window was just hidden or moved away.
void setNativeWindowBackground(void* nativeWindow, std::uint32_t rgb);

// Maximizes or restores a top-level window through the window manager (_NET_WM_STATE), as the
// system's own title bar would. JUCE only resizes a window without a native title bar, which the
// window manager doesn't count as maximized: JUCE then forgot it was, and a window the window
// manager had maximized (Super+Up) couldn't be restored.
void setNativeWindowMaximised(void* nativeWindow, bool maximised);
// Whether the window manager has the window maximized.
bool isNativeWindowMaximised(void* nativeWindow);
// Makes JUCE read the window's geometry and the window manager's state again, by sending the
// window a ConfigureNotify. JUCE reads whether it's maximized at each resize, but GNOME updates
// that state just after resizing, so JUCE lagged a step: a resize border on the maximized window
// and none (no resizing at the edges) after restoring it.
void refreshNativeWindowState(void* nativeWindow);
// Whether a window manager that maximizes on request is running (it lists the maximized states
// in _NET_SUPPORTED); without one, JUCE's resize is all there is.
bool canMaximiseNativeWindows();

} // namespace jls::app
