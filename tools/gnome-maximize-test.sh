#!/usr/bin/env bash
# Maximize and restore the main window the way GNOME does it, on Xvfb, and check the window's
# edges follow (see docs/development.md). GNOME resizes the window first and sets its
# _NET_WM_STATE just after, which JUCE alone misses. There's no window manager here: this script
# plays its part (xdotool resizes, a few lines of Xlib set the state). Meant for the sandbox:
#
#   tools/sandbox.sh run tools/gnome-maximize-test.sh [app binary] [--no-moveresize]
#
# Like GNOME, it announces _NET_WM_MOVERESIZE: the window should have no border, and its
# invisible resize edges (app/native_windows) should be there while restored and gone while
# maximized. With --no-moveresize, JUCE's resizable border is checked instead: 4 px restored,
# 1 px maximized. Prints the window's left edge (6 pixels, at y + 300) and the number of edges
# in each state.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="$ROOT/build-docker/src/app/JuicyLoopsStudio_artefacts/Debug/Juicy Loops Studio"
MOVERESIZE=_NET_WM_MOVERESIZE
for arg in "$@"; do
    case "$arg" in
        --no-moveresize) MOVERESIZE="" ;;
        *) APP="$arg" ;;
    esac
done
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
export DISPLAY=:98

# xs WINDOW|root PROPERTY [ATOM...]: sets an atom-list property, or deletes it (no atoms).
# (xprop -set can only store a single atom.) xs WINDOW edges: counts its shown input-only
# children (the resize edges).
cat >"$WORK/xs.c" <<'EOF'
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char** argv)
{
    Display* d = XOpenDisplay(NULL);
    Window w = strcmp(argv[1], "root") == 0 ? DefaultRootWindow(d) : strtoul(argv[1], 0, 0);
    if (strcmp(argv[2], "edges") == 0)
    {
        Window root, parent, *children;
        unsigned int n, shown = 0;
        XQueryTree(d, w, &root, &parent, &children, &n);
        for (unsigned int i = 0; i < n; ++i)
        {
            XWindowAttributes a;
            XGetWindowAttributes(d, children[i], &a);
            shown += a.class == InputOnly && a.map_state == IsViewable;
        }
        printf("%u\n", shown);
        return 0;
    }
    Atom prop = XInternAtom(d, argv[2], False);
    if (argc == 3)
        XDeleteProperty(d, w, prop);
    else
    {
        Atom atoms[16];
        int n = 0;
        for (int i = 3; i < argc && n < 16; ++i)
            atoms[n++] = XInternAtom(d, argv[i], False);
        XChangeProperty(d, w, prop, XA_ATOM, 32, PropModeReplace, (unsigned char*)atoms, n);
    }
    XCloseDisplay(d);
    return 0;
}
EOF
cc -o "$WORK/xs" "$WORK/xs.c" -lX11 || exit 1
XS="$WORK/xs"

# -noreset: Xvfb otherwise resets (dropping the root's properties) when its last client leaves.
Xvfb :98 -screen 0 1600x1000x24 -nolisten tcp -noreset >/dev/null 2>&1 &
XVFB=$!
sleep 1
MAXIMIZED="_NET_WM_STATE_MAXIMIZED_HORZ _NET_WM_STATE_MAXIMIZED_VERT"
# What a window manager announces. JUCE looks the atoms up once at start, so they must exist.
"$XS" root _NET_SUPPORTED _NET_WM_STATE $MAXIMIZED $MOVERESIZE
"$APP" >"$WORK/app.log" 2>&1 &
APP_PID=$!
sleep 5
WIN=$(xdotool search --name "Juicy Loops Studio" | tail -1)
xdotool windowsize "$WIN" 1200 800 windowmove "$WIN" 150 100
sleep 1.5

edge() { # NAME X Y: the 6 pixels from the window's left edge
    import -window root "$WORK/$1.png"
    local row=""
    for dx in 0 1 2 3 4 5; do
        row+=" $(convert "$WORK/$1.png" -crop "1x1+$(($2 + dx))+$(($3 + 300))" \
            -format '%[hex:p{0,0}]' info:)"
    done
    printf '%-10s%s   edges: %s\n' "$1" "$row" "$("$XS" "$WIN" edges)" >&2
    echo "$row"
}

start=$(edge start 150 100)
edges_start=$("$XS" "$WIN" edges)
xdotool windowmove "$WIN" 0 0 windowsize "$WIN" 1600 1000
sleep 0.05
"$XS" "$WIN" _NET_WM_STATE $MAXIMIZED
sleep 1.5
maximized=$(edge maximized 0 0)
edges_maximized=$("$XS" "$WIN" edges)
xdotool windowsize "$WIN" 1200 800 windowmove "$WIN" 150 100
sleep 0.05
"$XS" "$WIN" _NET_WM_STATE
sleep 1.5
restored=$(edge restored 150 100)
edges_restored=$("$XS" "$WIN" edges)

kill "$APP_PID"
sleep 0.3
kill "$XVFB"

if [ -n "$MOVERESIZE" ]; then
    if [ "$maximized" != "$start" ] || [ "$restored" != "$start" ]; then
        echo "FAIL: the window's edge changed (a border showed)" >&2
        exit 1
    fi
    if [ "$edges_start" != 12 ] || [ "$edges_maximized" != 0 ] || [ "$edges_restored" != 12 ]; then
        echo "FAIL: resize edges $edges_start / $edges_maximized / $edges_restored (want 12 / 0 / 12)" >&2
        exit 1
    fi
    echo "OK: no border; resize edges while restored, none while maximized" >&2
    exit 0
fi
if [ "$maximized" = "$start" ]; then
    echo "FAIL: the border didn't change when maximized" >&2
    exit 1
fi
if [ "$restored" != "$start" ]; then
    echo "FAIL: the border didn't come back when restored" >&2
    exit 1
fi
echo "OK: border while restored, none while maximized" >&2
