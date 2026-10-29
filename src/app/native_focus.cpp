#include "app/native_focus.h"

#if defined(__linux__) || defined(__FreeBSD__)
#include <X11/Xlib.h>

#include <cstdint>
#include <memory>
#endif

namespace jls::app
{

void focusNativeWindow([[maybe_unused]] void* nativeWindow)
{
#if defined(__linux__) || defined(__FreeBSD__)
    // Our own connection to the X server (JUCE's is internal); focus is server-wide.
    struct CloseDisplay
    {
        void operator()(Display* d) const { XCloseDisplay(d); }
    };
    static const std::unique_ptr<Display, CloseDisplay> display{XOpenDisplay(nullptr)};
    if (display == nullptr || nativeWindow == nullptr)
        return;
    XSetInputFocus(display.get(),
                   static_cast<Window>(reinterpret_cast<std::uintptr_t>(nativeWindow)),
                   RevertToParent, CurrentTime);
    XFlush(display.get());
#endif
}

} // namespace jls::app
