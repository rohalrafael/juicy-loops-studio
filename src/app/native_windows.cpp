#include "app/native_windows.h"

#if defined(__linux__) || defined(__FreeBSD__)
#include <X11/Xlib.h>

#include <cstdint>
#include <memory>
#endif

namespace jls::app
{

#if defined(__linux__) || defined(__FreeBSD__)
namespace
{

// Our own connection to the X server (JUCE's is internal); focus and stacking are server-wide.
Display* display()
{
    struct CloseDisplay
    {
        void operator()(Display* d) const { XCloseDisplay(d); }
    };
    static const std::unique_ptr<Display, CloseDisplay> connection{XOpenDisplay(nullptr)};
    return connection.get();
}

Window toWindow(void* handle)
{
    return static_cast<Window>(reinterpret_cast<std::uintptr_t>(handle));
}

} // namespace
#endif

void focusNativeWindow([[maybe_unused]] void* nativeWindow)
{
#if defined(__linux__) || defined(__FreeBSD__)
    if (display() == nullptr || nativeWindow == nullptr)
        return;
    XSetInputFocus(display(), toWindow(nativeWindow), RevertToParent, CurrentTime);
    XFlush(display());
#endif
}

void restackNativeWindows([[maybe_unused]] const std::vector<void*>& frontToBack)
{
#if defined(__linux__) || defined(__FreeBSD__)
    if (display() == nullptr || frontToBack.empty())
        return;
    std::vector<Window> windows;
    windows.reserve(frontToBack.size());
    for (auto* handle : frontToBack)
        if (handle != nullptr)
            windows.push_back(toWindow(handle));
    XRestackWindows(display(), windows.data(), static_cast<int>(windows.size()));
    XFlush(display());
#endif
}

} // namespace jls::app
