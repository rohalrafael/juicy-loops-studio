#include "app/native_windows.h"

#if defined(__linux__) || defined(__FreeBSD__)
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <algorithm>

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

void setNativeWindowBackground([[maybe_unused]] void* nativeWindow,
                               [[maybe_unused]] std::uint32_t rgb)
{
#if defined(__linux__) || defined(__FreeBSD__)
    if (display() == nullptr || nativeWindow == nullptr)
        return;
    // Opaque alpha in the top byte, for windows with an ARGB visual.
    XSetWindowBackground(display(), toWindow(nativeWindow), 0xff000000u | rgb);
    XFlush(display());
#endif
}

void setNativeWindowMaximised([[maybe_unused]] void* nativeWindow, [[maybe_unused]] bool maximised)
{
#if defined(__linux__) || defined(__FreeBSD__)
    if (display() == nullptr || nativeWindow == nullptr)
        return;
    auto* d = display();
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.window = toWindow(nativeWindow);
    event.xclient.message_type = XInternAtom(d, "_NET_WM_STATE", False);
    event.xclient.format = 32;
    event.xclient.data.l[0] = maximised ? 1 : 0; // _NET_WM_STATE_ADD / _REMOVE
    event.xclient.data.l[1] =
        static_cast<long>(XInternAtom(d, "_NET_WM_STATE_MAXIMIZED_HORZ", False));
    event.xclient.data.l[2] =
        static_cast<long>(XInternAtom(d, "_NET_WM_STATE_MAXIMIZED_VERT", False));
    event.xclient.data.l[3] = 1; // the request comes from an application
    XSendEvent(d, DefaultRootWindow(d), False, SubstructureRedirectMask | SubstructureNotifyMask,
               &event);
    XFlush(d);
#endif
}

#if defined(__linux__) || defined(__FreeBSD__)
namespace
{

// Whether `window`'s atom-list property `property` holds both `first` and `second`.
bool hasAtoms(Window window, const char* property, const char* first, const char* second)
{
    auto* d = display();
    Atom type = None;
    int format = 0;
    unsigned long count = 0, remaining = 0;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(d, window, XInternAtom(d, property, False), 0, 1024, False, XA_ATOM,
                           &type, &format, &count, &remaining, &data) != Success ||
        data == nullptr)
        return false;
    bool found = false;
    if (type == XA_ATOM && format == 32)
    {
        const auto* atoms = reinterpret_cast<const Atom*>(data);
        const auto has = [&](const char* name)
        { return std::find(atoms, atoms + count, XInternAtom(d, name, False)) != atoms + count; };
        found = has(first) && has(second);
    }
    XFree(data);
    return found;
}

} // namespace
#endif

void refreshNativeWindowState([[maybe_unused]] void* nativeWindow)
{
#if defined(__linux__) || defined(__FreeBSD__)
    if (display() == nullptr || nativeWindow == nullptr)
        return;
    auto* d = display();
    const auto window = toWindow(nativeWindow);
    Window root = None;
    int x = 0, y = 0;
    unsigned int width = 0, height = 0, border = 0, depth = 0;
    if (XGetGeometry(d, window, &root, &x, &y, &width, &height, &border, &depth) == 0)
        return;
    XEvent event{};
    event.xconfigure.type = ConfigureNotify;
    event.xconfigure.event = window;
    event.xconfigure.window = window;
    event.xconfigure.x = x;
    event.xconfigure.y = y;
    event.xconfigure.width = static_cast<int>(width);
    event.xconfigure.height = static_cast<int>(height);
    XSendEvent(d, window, False, StructureNotifyMask, &event);
    XFlush(d);
#endif
}

bool isNativeWindowMaximised([[maybe_unused]] void* nativeWindow)
{
#if defined(__linux__) || defined(__FreeBSD__)
    if (display() == nullptr || nativeWindow == nullptr)
        return false;
    return hasAtoms(toWindow(nativeWindow), "_NET_WM_STATE", "_NET_WM_STATE_MAXIMIZED_HORZ",
                    "_NET_WM_STATE_MAXIMIZED_VERT");
#else
    return false;
#endif
}

bool canMaximiseNativeWindows()
{
#if defined(__linux__) || defined(__FreeBSD__)
    if (display() == nullptr)
        return false;
    return hasAtoms(DefaultRootWindow(display()), "_NET_SUPPORTED", "_NET_WM_STATE_MAXIMIZED_HORZ",
                    "_NET_WM_STATE_MAXIMIZED_VERT");
#else
    return false;
#endif
}

} // namespace jls::app
