// A few X11 jobs the test scripts need and the usual tools don't do (xprop -set can only store a
// single atom). Built on the fly by tools/gnome-maximize-test.sh and tools/screenshots.sh:
//
//   x11-props root|WINDOW PROPERTY ATOM...   sets an atom-list property
//   x11-props root|WINDOW PROPERTY           deletes it
//   x11-props WINDOW --edges                 counts its shown input-only children (the main
//                                            window's resize edges, app/native_windows)
//
// With them a script can play a window manager's part on Xvfb: announce what it supports
// (_NET_SUPPORTED on the root) and set a window's state (_NET_WM_STATE).
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int countEdges(Display* d, Window w)
{
    Window root, parent, *children = NULL;
    unsigned int n = 0, shown = 0;
    if (!XQueryTree(d, w, &root, &parent, &children, &n))
        return 1;
    for (unsigned int i = 0; i < n; ++i)
    {
        XWindowAttributes a;
        if (XGetWindowAttributes(d, children[i], &a))
            shown += a.class == InputOnly && a.map_state == IsViewable;
    }
    if (children != NULL)
        XFree(children);
    printf("%u\n", shown);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "usage: x11-props root|WINDOW PROPERTY [ATOM...] | WINDOW --edges\n");
        return 2;
    }
    Display* d = XOpenDisplay(NULL);
    if (d == NULL)
    {
        fprintf(stderr, "x11-props: can't open the display\n");
        return 1;
    }
    const Window w =
        strcmp(argv[1], "root") == 0 ? DefaultRootWindow(d) : (Window)strtoul(argv[1], NULL, 0);
    int result = 0;
    if (strcmp(argv[2], "--edges") == 0)
        result = countEdges(d, w);
    else
    {
        const Atom property = XInternAtom(d, argv[2], False);
        if (argc == 3)
            XDeleteProperty(d, w, property);
        else
        {
            Atom atoms[32];
            int n = 0;
            for (int i = 3; i < argc && n < 32; ++i)
                atoms[n++] = XInternAtom(d, argv[i], False);
            XChangeProperty(d, w, property, XA_ATOM, 32, PropModeReplace, (unsigned char*)atoms, n);
        }
    }
    XCloseDisplay(d);
    return result;
}
