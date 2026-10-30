#include "app/native_windows.h"

#if defined(__linux__) || defined(__FreeBSD__)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>

#include <algorithm>
#include <array>

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

// The resize edges (see updateNativeResizeEdges): two windows per corner (along each side) and
// one per side, each with its _NET_WM_MOVERESIZE direction.
struct ResizeEdges
{
    static constexpr int count = 12;
    Window parent = 0;
    std::array<Window, count> windows{};
    std::array<long, count> directions{};
    bool shown = false;
    unsigned int width = 0, height = 0;
};

ResizeEdges& resizeEdges()
{
    static ResizeEdges edges;
    return edges;
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
    const auto& edges = resizeEdges();
    if (edges.parent != 0)
        windows.insert(windows.end(), edges.windows.begin(), edges.windows.end());
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
    XSetWindowAttributes attributes{};
    // Opaque alpha in the top byte, for windows with an ARGB visual.
    attributes.background_pixel = 0xff000000u | rgb;
    attributes.bit_gravity = NorthWestGravity;
    XChangeWindowAttributes(display(), toWindow(nativeWindow), CWBackPixel | CWBitGravity,
                            &attributes);
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

bool canResizeNativeWindowEdges()
{
#if defined(__linux__) || defined(__FreeBSD__)
    if (display() == nullptr)
        return false;
    return hasAtoms(DefaultRootWindow(display()), "_NET_SUPPORTED", "_NET_WM_MOVERESIZE",
                    "_NET_WM_MOVERESIZE");
#else
    return false;
#endif
}

void updateNativeResizeEdges([[maybe_unused]] void* nativeWindow, [[maybe_unused]] bool show)
{
#if defined(__linux__) || defined(__FreeBSD__)
    auto* d = display();
    auto& edges = resizeEdges();
    const auto parent = toWindow(nativeWindow);
    if (d == nullptr || parent == 0)
        return;
    if (edges.parent != parent)
    {
        // _NET_WM_MOVERESIZE directions: top-left 0, top 1, top-right 2, right 3, bottom-right 4,
        // bottom 5, bottom-left 6, left 7. Corners come twice (along both sides).
        constexpr std::array<long, ResizeEdges::count> directions{0, 0, 1, 2, 2, 3,
                                                                  4, 4, 5, 6, 6, 7};
        constexpr std::array<unsigned int, 8> cursors{
            XC_top_left_corner,     XC_top_side,    XC_top_right_corner,   XC_right_side,
            XC_bottom_right_corner, XC_bottom_side, XC_bottom_left_corner, XC_left_side};
        XSetWindowAttributes attributes{};
        attributes.event_mask = ButtonPressMask;
        for (int i = 0; i < ResizeEdges::count; ++i)
        {
            const auto direction = directions[static_cast<std::size_t>(i)];
            attributes.cursor = XCreateFontCursor(d, cursors[static_cast<std::size_t>(direction)]);
            edges.windows[static_cast<std::size_t>(i)] = XCreateWindow(
                d, parent, 0, 0, 1, 1, 0, 0, InputOnly, nullptr /* the parent's visual */,
                CWEventMask | CWCursor, &attributes);
            edges.directions[static_cast<std::size_t>(i)] = direction;
        }
        edges.parent = parent;
        edges.shown = false;
        edges.width = edges.height = 0;
    }

    Window root = None;
    int x = 0, y = 0;
    unsigned int width = 0, height = 0, border = 0, depth = 0;
    if (XGetGeometry(d, parent, &root, &x, &y, &width, &height, &border, &depth) == 0)
        return;
    if (show && (width != edges.width || height != edges.height))
    {
        // Thin enough not to get in the way of the controls along the edges; corners longer, so
        // they're easy to find.
        const int w = static_cast<int>(width), h = static_cast<int>(height);
        constexpr int t = 5;  // thickness
        constexpr int c = 16; // a corner's length along each side
        const std::array<std::array<int, 4>, ResizeEdges::count> rects{{
            {0, 0, c, t},
            {0, 0, t, c},         // top-left
            {c, 0, w - 2 * c, t}, // top
            {w - c, 0, c, t},
            {w - t, 0, t, c},         // top-right
            {w - t, c, t, h - 2 * c}, // right
            {w - c, h - t, c, t},
            {w - t, h - c, t, c},     // bottom-right
            {c, h - t, w - 2 * c, t}, // bottom
            {0, h - t, c, t},
            {0, h - c, t, c},     // bottom-left
            {0, c, t, h - 2 * c}, // left
        }};
        for (std::size_t i = 0; i < rects.size(); ++i)
        {
            const auto& r = rects[i];
            XMoveResizeWindow(d, edges.windows[i], r[0], r[1],
                              static_cast<unsigned int>(std::max(1, r[2])),
                              static_cast<unsigned int>(std::max(1, r[3])));
        }
        edges.width = width;
        edges.height = height;
    }
    if (show != edges.shown)
    {
        for (const auto window : edges.windows)
            show ? XMapWindow(d, window) : XUnmapWindow(d, window);
        edges.shown = show;
    }
    if (show) // above child windows mapped since (a panel just opened)
        for (const auto window : edges.windows)
            XRaiseWindow(d, window);
    XFlush(d);
#endif
}

int nativeEventsFd()
{
#if defined(__linux__) || defined(__FreeBSD__)
    return display() != nullptr ? ConnectionNumber(display()) : -1;
#else
    return -1;
#endif
}

void handleNativeEvents()
{
#if defined(__linux__) || defined(__FreeBSD__)
    auto* d = display();
    if (d == nullptr)
        return;
    const auto& edges = resizeEdges();
    while (XPending(d) > 0)
    {
        XEvent event{};
        XNextEvent(d, &event);
        if (event.type != ButtonPress || event.xbutton.button != Button1)
            continue;
        const auto it = std::find(edges.windows.begin(), edges.windows.end(), event.xbutton.window);
        if (it == edges.windows.end())
            continue;
        // The press grabbed the pointer for us: let go, so the window manager can take it.
        XUngrabPointer(d, event.xbutton.time);
        XEvent message{};
        message.xclient.type = ClientMessage;
        message.xclient.window = edges.parent;
        message.xclient.message_type = XInternAtom(d, "_NET_WM_MOVERESIZE", False);
        message.xclient.format = 32;
        message.xclient.data.l[0] = event.xbutton.x_root;
        message.xclient.data.l[1] = event.xbutton.y_root;
        message.xclient.data.l[2] =
            edges.directions[static_cast<std::size_t>(std::distance(edges.windows.begin(), it))];
        message.xclient.data.l[3] = Button1;
        message.xclient.data.l[4] = 1; // the request comes from an application
        XSendEvent(d, DefaultRootWindow(d), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &message);
        XFlush(d);
    }
#endif
}

} // namespace jls::app
