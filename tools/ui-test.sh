#!/usr/bin/env bash
# Drive the app on a private, invisible X display (Xvfb) for UI testing.
# Nothing touches the real desktop (works the same under Wayland).
#
# Needs: xorg-server-xvfb, xdotool, imagemagick.
#
#   tools/ui-test.sh start-x            start the virtual display :99 (1280x800)
#   tools/ui-test.sh start              launch the app on it (window at 0,0)
#   tools/ui-test.sh click X Y [BUTTON] click (1 = left, 3 = right)
#   tools/ui-test.sh drag X1 Y1 X2 Y2   left-drag in small steps
#   tools/ui-test.sh key KEYS           e.g. space, ctrl+z, F4
#   tools/ui-test.sh shot NAME          screenshot to $OUT/NAME.png
#   tools/ui-test.sh stop | stop-x
#
# Note: the app uses your real settings file, so a theme picked during a test sticks.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="$ROOT/build/src/app/JuicyLoopsStudio_artefacts/Debug/Juicy Loops Studio"
OUT="${OUT:-/tmp/jls-ui-test}"
mkdir -p "$OUT"
export DISPLAY=:99

case "${1:-}" in
start-x)
    Xvfb :99 -screen 0 1280x800x24 -nolisten tcp >"$OUT/xvfb.log" 2>&1 &
    echo $! >"$OUT/xvfb.pid"
    sleep 1
    ;;
stop-x) kill "$(cat "$OUT/xvfb.pid")" 2>/dev/null || true ;;
start)
    "$APP" >"$OUT/app.log" 2>&1 &
    echo $! >"$OUT/app.pid"
    sleep 3
    xdotool windowmove "$(xdotool search --name "Juicy Loops Studio" | head -1)" 0 0
    sleep 0.5
    ;;
stop)
    kill "$(cat "$OUT/app.pid")" 2>/dev/null || true
    sleep 0.5
    ;;
shot) import -window root "$OUT/$2.png" ;;
click)
    xdotool mousemove "$2" "$3" click "${4:-1}"
    sleep 0.3
    ;;
drag)
    xdotool mousemove "$2" "$3" mousedown 1
    for i in 1 2 3 4 5 6 7 8; do
        xdotool mousemove $(($2 + ($4 - $2) * i / 8)) $(($3 + ($5 - $3) * i / 8))
        sleep 0.05
    done
    xdotool mouseup 1
    sleep 0.3
    ;;
key)
    xdotool key "$2"
    sleep 0.3
    ;;
*)
    sed -n '2,17p' "$0"
    exit 1
    ;;
esac
