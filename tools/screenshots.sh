#!/usr/bin/env bash
# Screenshots of the app for the README and the docs, taken from a project on a private X
# display (Xvfb), so they can be redone whenever the UI changes (see docs/development.md).
# Meant for the sandbox:
#
#   tools/sandbox.sh run tools/screenshots.sh [options] PROJECT.jlp
#
#   --out DIR       where the images go (default /tmp/jls-ui-test/screenshots)
#   --size WxH      window size (default 1600x1000; the scenes' clicks assume about this size)
#   --samples DIR   the folder the Browser shows (default: a few generated one-shots)
#   --theme ID      theme of every scene but "themes" (default blackberry)
#   --only "A B"    just these scenes
#   --play SECONDS  shoot while the project plays, this long after pressing Space (time, play
#                   heads, meters and the scope then show it; the Channel Rack and Piano Roll
#                   play their pattern, in PAT mode); default: stopped
#   --png           keep the PNGs next to the WebP files
#
# Scenes (each starts the app afresh, with settings of its own, never yours):
#   overview      the whole window: Browser, Playlist, Channel Rack, Mixer
#   channel-rack  the Channel Rack
#   piano-roll    the Piano Roll (with the project's scale highlight)
#   mixer         the Mixer
#   browser       the Browser, a folder open and a sample picked (its waveform below)
#   shortcuts     the Keyboard shortcuts dialog
#   themes        the overview in every built-in theme, one image each, plus themes.webp (a grid)
#   features      close-ups for the README's feature list (feature-*.webp), at twice the pixels
#                 (interface size 200 % on a screen twice as large: the same layout, sharp when
#                 shown large): the scale control, the shortcuts window, the Ctrl+Tab setting,
#                 the Browser's list, parts looping under longer ones, and Split by channel
#
# The project decides what's in the pictures: its patterns, notes, arrangement, mixer effects,
# and which pattern is current. Panels are found by their native windows' titles (MainComponent
# names them), so crops follow layout changes; clicks inside panels are relative to them.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="$ROOT/build-docker/src/app/JuicyLoopsStudio_artefacts/Debug/Juicy Loops Studio"
OUT=/tmp/jls-ui-test/screenshots
SIZE=1600x1000
SAMPLES=""
THEME=blackberry
ONLY="overview channel-rack piano-roll mixer browser shortcuts themes features"
KEEP_PNG=0
PLAY=0
PROJECT=""
while [ $# -gt 0 ]; do
    case "$1" in
        --out) OUT="$2"; shift ;;
        --size) SIZE="$2"; shift ;;
        --samples) SAMPLES="$2"; shift ;;
        --theme) THEME="$2"; shift ;;
        --only) ONLY="$2"; shift ;;
        --play) PLAY="$2"; shift ;;
        --png) KEEP_PNG=1 ;;
        -h|--help) sed -n '2,32p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) PROJECT="$1" ;;
    esac
    shift
done
if [ -z "$PROJECT" ] || [ ! -f "$PROJECT" ]; then
    echo "screenshots.sh: give a project file (.jlp); --help for the options" >&2
    exit 2
fi
PROJECT="$(cd "$(dirname "$PROJECT")" && pwd)/$(basename "$PROJECT")"
[ -x "$APP" ] || { echo "screenshots.sh: build the app first ($APP)" >&2; exit 1; }
WIDTH=${SIZE%x*}
HEIGHT=${SIZE#*x}
mkdir -p "$OUT"
WORK="$(mktemp -d)"
trap 'kill "$APP_PID" "$XVFB" 2>/dev/null; rm -rf "$WORK"' EXIT
XVFB=""
APP_PID=""
export DISPLAY=:94

cc -o "$WORK/x11-props" "$ROOT/tools/x11-props.c" -lX11 || exit 1

# Generated one-shots for the Browser, so it isn't empty: simple synthesis, original sounds.
if [ -z "$SAMPLES" ]; then
    SAMPLES="$WORK/Samples"
    python3 -I - "$SAMPLES" <<'EOF'
import math, os, random, struct, sys, wave
root = sys.argv[1]
rate = 44100
def write(path, samples):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, s)) * 30000)) for s in samples))
def kick(length, f0, f1, drive):
    n = int(rate * length); phase = 0.0; out = []
    for i in range(n):
        t = i / rate
        phase += 2 * math.pi * (f1 + (f0 - f1) * math.exp(-t * 30)) / rate
        out.append(math.tanh(drive * math.sin(phase)) * math.exp(-t * 6 / length))
    return out
def noise(length, decay, tone=0.0, f=200):
    random.seed(length * 1000 + decay)
    n = int(rate * length)
    return [((random.uniform(-1, 1) * (1 - tone)) + tone * math.sin(2 * math.pi * f * i / rate))
            * math.exp(-i / rate * decay) for i in range(n)]
def bass(length, f, slide=0.0):
    n = int(rate * length); phase = 0.0; out = []
    for i in range(n):
        t = i / rate
        phase += 2 * math.pi * f * (1 + slide * min(1, t * 4)) / rate
        out.append(math.tanh(1.8 * math.sin(phase)) * min(1, t * 200) * math.exp(-t * 1.2))
    return out
def riser(length):
    n = int(rate * length); random.seed(7); phase = 0.0; out = []
    for i in range(n):
        t = i / n
        phase += 2 * math.pi * (200 + 1800 * t * t) / rate
        out.append((0.6 * math.sin(phase) + 0.4 * random.uniform(-1, 1)) * t)
    return out
write(f"{root}/Drums/Kick Punch.wav", kick(0.45, 180, 50, 2.5))
write(f"{root}/Drums/Kick Sub.wav", kick(0.8, 120, 42, 1.4))
write(f"{root}/Drums/Snare Crack.wav", noise(0.3, 18, 0.3, 190))
write(f"{root}/Drums/Clap Tight.wav", noise(0.25, 25))
write(f"{root}/Drums/Hat Closed.wav", noise(0.08, 60))
write(f"{root}/Drums/Hat Open.wav", noise(0.4, 9))
write(f"{root}/808s/808 Long.wav", bass(1.6, 49))
write(f"{root}/808s/808 Slide.wav", bass(1.2, 44, 0.5))
write(f"{root}/FX/Riser.wav", riser(2.0))
write(f"{root}/FX/Impact.wav", kick(1.4, 90, 30, 3.0))
EOF
fi

# One X server at a time, sized for the scenes' scale: 1, or 2 for close-ups (the same layout in
# twice the pixels). It plays a window manager's part: announcing _NET_WM_MOVERESIZE makes the
# app use its invisible resize edges, so the window has no border. -noreset keeps that while no
# client is connected.
SCALE=0
display_at() { # SCALE
    [ "$1" = "$SCALE" ] && return 0
    [ -n "$XVFB" ] && { kill "$XVFB"; wait "$XVFB" 2>/dev/null; }
    SCALE=$1
    SW=$((WIDTH * SCALE))
    SH=$((HEIGHT * SCALE))
    Xvfb :94 -screen 0 "${SW}x${SH}x24" -nolisten tcp -noreset >/dev/null 2>&1 &
    XVFB=$!
    sleep 1
    "$WORK/x11-props" root _NET_SUPPORTED _NET_WM_STATE _NET_WM_STATE_MAXIMIZED_HORZ \
        _NET_WM_STATE_MAXIMIZED_VERT _NET_WM_MOVERESIZE
}

px() { # N: N interface pixels in screen pixels
    echo $(($1 * SCALE))
}

start_app() { # THEME [SCALE]: the app with fresh settings, the project open, filling the screen
    display_at "${2:-1}"
    export XDG_CONFIG_HOME="$WORK/config"
    rm -rf "$XDG_CONFIG_HOME"
    mkdir -p "$XDG_CONFIG_HOME/JuicyLoopsStudio"
    cat >"$XDG_CONFIG_HOME/JuicyLoopsStudio/JuicyLoopsStudio.settings" <<EOF
<?xml version="1.0" encoding="UTF-8"?>

<PROPERTIES>
  <VALUE name="theme" val="$1"/>
  <VALUE name="uiScale" val="$SCALE"/>
  <VALUE name="browserVisible" val="1"/>
  <VALUE name="browserFolders" val="$SAMPLES"/>
</PROPERTIES>
EOF
    "$APP" "$PROJECT" >"$WORK/app.log" 2>&1 &
    APP_PID=$!
    local win=""
    for _ in $(seq 1 50); do
        win=$(xdotool search --name "Juicy Loops Studio" 2>/dev/null | tail -1)
        [ -n "$win" ] && break
        sleep 0.2
    done
    [ -n "$win" ] || { echo "screenshots.sh: the app didn't start" >&2; cat "$WORK/app.log" >&2; exit 1; }
    sleep 2
    xdotool windowmove "$win" 0 0 windowsize "$win" "$SW" "$SH"
    sleep 1.5
    # Keyboard focus: a click on an empty part of the transport bar.
    xdotool mousemove $((SW - $(px 500))) "$(px 60)" click 1
    sleep 0.3
}

play() { # [pat]: with --play, start playback (in PAT mode with "pat") and let it run
    [ "$PLAY" != 0 ] || return 0
    # The Channel Rack and Piano Roll only show a play head while their pattern plays: in song
    # mode it belongs to the Playlist.
    [ "${1:-}" = pat ] && xdotool key l
    xdotool key space
    sleep "$PLAY"
}

stop_app() {
    kill "$APP_PID" 2>/dev/null
    wait "$APP_PID" 2>/dev/null
    APP_PID=""
    sleep 0.3
}

# Keys by keycode: Xvfb's default keymap makes xdotool add Alt to F-keys sent by name.
key_f() { # N: function key FN
    local codes=(0 67 68 69 70 71 72 73 74 75 76 95 96)
    xdotool key "${codes[$1]}"
    sleep 1
}

panel() { # NAME: "X Y W H" of a panel's native window, clipped to the screen; empty if hidden
    local id
    id=$(xdotool search --onlyvisible --name "^$1\$" 2>/dev/null | head -1)
    [ -n "$id" ] || return 0
    xwininfo -id "$id" | awk -v sw="$SW" -v sh="$SH" '
        /Absolute upper-left X/ {x=$NF} /Absolute upper-left Y/ {y=$NF}
        /Width:/ {w=$NF} /Height:/ {h=$NF}
        END { if (x+w > sw || y+h > sh) print "clipped" > "/dev/stderr"
              if (x+w > sw) w = sw-x; if (y+h > sh) h = sh-y; print x, y, w, h }' 2>"$WORK/clipped"
    if [ -s "$WORK/clipped" ]; then
        echo "  ($1 is larger than the screen: cut off at its edge)" >&2
    fi
}

shot() { # NAME [X Y W H]: the screen (or that part of it) to NAME.webp
    xdotool mousemove "$SW" "$SH" # no hover highlight in the picture
    sleep 0.6
    import -window root "$WORK/full.png"
    local png="$WORK/$1.png"
    if [ $# -ge 5 ]; then
        convert "$WORK/full.png" -crop "${4}x${5}+${2}+${3}" +repage "$png"
    else
        cp "$WORK/full.png" "$png"
    fi
    convert "$png" -quality 90 -define webp:method=6 "$OUT/$1.webp"
    [ "$KEEP_PNG" = 1 ] && cp "$png" "$OUT/$1.png"
    echo "  $1.webp" >&2
}

shot_panel() { # FILE PANEL
    local geometry
    geometry=$(panel "$2")
    if [ -z "$geometry" ]; then
        echo "screenshots.sh: $2 isn't showing; skipped $1" >&2
        return
    fi
    # shellcheck disable=SC2086
    shot "$1" $geometry
}

shot_part() { # FILE WINDOW X Y W H: part of a panel or dialog, in interface pixels from its corner
    local geometry x y w h
    geometry=$(panel "$2" 2>/dev/null)
    if [ -z "$geometry" ]; then
        echo "screenshots.sh: $2 isn't showing; skipped $1" >&2
        return
    fi
    read -r x y w h <<<"$geometry"
    local cx=$(px "$3") cy=$(px "$4") cw=$(px "$5") ch=$(px "$6")
    [ $((cx + cw)) -gt "$w" ] && cw=$((w - cx))
    [ $((cy + ch)) -gt "$h" ] && ch=$((h - cy))
    shot "$1" $((x + cx)) $((y + cy)) "$cw" "$ch"
}

scene_overview() {
    start_app "$THEME"
    key_f 9 # Mixer
    play
    shot overview
    stop_app
}

scene_channel_rack() {
    start_app "$THEME"
    play pat
    shot_panel channel-rack "Channel rack"
    stop_app
}

scene_piano_roll() {
    start_app "$THEME"
    key_f 7
    play pat
    shot_panel piano-roll "Piano roll"
    stop_app
}

scene_mixer() {
    start_app "$THEME"
    key_f 9
    play
    shot_panel mixer "Mixer"
    stop_app
}

open_first_sample() { # in the Browser: the first folder, then its first sample (previewed)
    local geometry x y
    geometry=$(panel "Browser")
    read -r x y _ _ <<<"$geometry"
    # The tree's first row, under the header: open it, then pick its first sample with the
    # keyboard (the Browser previews it and draws its waveform).
    xdotool mousemove $((x + $(px 60))) $((y + $(px 50))) click 1
    sleep 0.4
    xdotool key Right; sleep 0.3
    xdotool key Down; sleep 0.3
    xdotool key Right; sleep 0.3
    xdotool key Down; sleep 1
}

scene_browser() {
    start_app "$THEME"
    open_first_sample
    shot_panel browser "Browser"
    stop_app
}

scene_shortcuts() {
    start_app "$THEME"
    xdotool key ctrl+shift+k
    sleep 1.5
    local id
    id=$(xdotool search --onlyvisible --name "Keyboard shortcuts" 2>/dev/null | head -1)
    if [ -n "$id" ]; then
        local geometry
        geometry=$(xwininfo -id "$id" | awk '/Absolute upper-left X/ {x=$NF} /Absolute upper-left Y/ {y=$NF}
            /Width:/ {w=$NF} /Height:/ {h=$NF} END {print x, y, w, h}')
        # shellcheck disable=SC2086
        shot shortcuts $geometry
    else
        echo "screenshots.sh: the Keyboard shortcuts dialog didn't open; skipped" >&2
    fi
    stop_app
}

scene_themes() {
    local ids
    ids=$(grep -o 'load("[a-z-]*"' "$ROOT/src/theme/builtin_themes.cpp" | cut -d'"' -f2)
    local tiles=()
    for id in $ids; do
        start_app "$id"
        key_f 9
        play
        shot "theme-$id"
        tiles+=("$WORK/theme-$id.png")
        stop_app
    done
    # The grid: two columns, each tile labelled with its theme's name.
    local labelled=()
    for tile in "${tiles[@]}"; do
        local id name
        id=$(basename "$tile" .png)
        id=${id#theme-}
        name=$(sed -n "/${id}Json = R\"(/,/\"name\"/p" "$ROOT/src/theme/builtin_themes.cpp" |
            grep -o '"name": "[^"]*"' | cut -d'"' -f4)
        [ -n "$name" ] || name=$id
        convert "$tile" -resize 800x \( -size 800x44 xc:'#0d0d10' -fill '#e8e8ee' \
            -font "$ROOT/resources/fonts/inter/Inter-Medium.ttf" -pointsize 22 -gravity west -annotate +16+0 "$name" \) -append \
            "$WORK/label-$id.png"
        labelled+=("$WORK/label-$id.png")
    done
    montage "${labelled[@]}" -tile 2x -geometry +12+12 -background '#08080a' "$WORK/themes.png"
    convert "$WORK/themes.png" -quality 90 -define webp:method=6 "$OUT/themes.webp"
    [ "$KEEP_PNG" = 1 ] && cp "$WORK/themes.png" "$OUT/themes.png"
    echo "  themes.webp" >&2
}

# Close-ups, at scale 2. Crops are in interface pixels, from the panel's or dialog's corner.
scene_features() {
    start_app "$THEME" 2
    key_f 7
    local geometry x y
    geometry=$(panel "Piano roll")
    read -r x y _ _ <<<"$geometry"
    # Scroll down to lower notes, so the melody sits under the title bar.
    xdotool mousemove $((x + $(px 600))) $((y + $(px 400))) click --repeat 4 5
    sleep 0.5
    play pat
    shot_part feature-scale "Piano roll" 0 0 760 400
    stop_app

    start_app "$THEME" 2
    xdotool key ctrl+shift+k
    sleep 1.5
    shot_part feature-shortcuts "Keyboard shortcuts" 0 0 568 470
    stop_app

    start_app "$THEME" 2
    xdotool key ctrl+comma
    sleep 1.5
    shot_part feature-ctrl-tab "Settings" 0 548 442 92 # the Focus section
    stop_app

    start_app "$THEME" 2
    open_first_sample
    shot_part feature-browser "Browser" 0 0 260 300
    stop_app

    start_app "$THEME" 2
    xdotool key alt+3 # 64 steps: the shorter drum part shows its repeats
    play pat
    shot_part feature-loop "Channel rack" 115 40 1100 296
    stop_app

    start_app "$THEME" 2
    geometry=$(panel "Playlist")
    read -r x y _ _ <<<"$geometry"
    # Right-click the current pattern in the picker: its menu's only item is Split by channel.
    xdotool mousemove $((x + $(px 34))) $((y + $(px 88))) click 3
    sleep 0.6
    xdotool key Down Return
    sleep 1
    key_f 6 # the Channel Rack, in front, hides
    shot_part feature-split "Playlist" 0 0 1010 330
    stop_app
}

echo "Screenshots of $(basename "$PROJECT") at ${WIDTH}x${HEIGHT} -> $OUT" >&2
for scene in $ONLY; do
    fn="scene_${scene//-/_}"
    if declare -F "$fn" >/dev/null; then
        "$fn"
    else
        echo "screenshots.sh: no scene \"$scene\"" >&2
    fi
done
