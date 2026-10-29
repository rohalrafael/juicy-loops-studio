#pragma once

// Gives the keyboard to one of the app's own native windows, past JUCE: on Linux, JUCE won't
// move the X input focus while it's anywhere inside our window, a plugin's embedded window
// included, so keys went on to the plugin. Kept apart so Xlib's macros stay out of other files.

namespace jls::app
{

// Sets the input focus to `nativeWindow` (a ComponentPeer's native handle). Linux only; does
// nothing elsewhere. Call only while the app has the focus: it doesn't check.
void focusNativeWindow(void* nativeWindow);

} // namespace jls::app
