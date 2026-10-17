#!/usr/bin/env bash
# The development sandbox: a Docker container with every tool and test plugin installed, the
# repo mounted at the same path as on the host, and a persistent home volume (settings, plugin
# scan, ccache, Claude Code login). See docs/sandbox.md.
#
#   tools/sandbox.sh build          build the image (also done by `up` when missing)
#   tools/sandbox.sh up             start the container in the background (+ JACK dummy audio)
#   tools/sandbox.sh shell          interactive shell in the repo
#   tools/sandbox.sh run CMD...     run a command in the repo, e.g. run cmake --build build-docker
#   tools/sandbox.sh claude [ARGS]  Claude Code inside the sandbox
#   tools/sandbox.sh down           stop and remove the container (the home volume stays)
#   tools/sandbox.sh reset          also delete the home volume
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE=jls-sandbox:latest
NAME=jls-sandbox
HOME_VOLUME=jls-sandbox-home
USER_NAME="$(id -un)"
# Docker network: "bridge" (default, isolated) or "host" (works without the veth kernel module,
# e.g. after a kernel update before rebooting). JLS_SANDBOX_NETWORK=host tools/sandbox.sh up
NETWORK="${JLS_SANDBOX_NETWORK:-bridge}"

running() { [ "$(docker inspect -f '{{.State.Running}}' "$NAME" 2>/dev/null)" = "true" ]; }

build() {
    docker build --network "$NETWORK" -t "$IMAGE" \
        --build-arg USERNAME="$USER_NAME" --build-arg UID="$(id -u)" --build-arg GID="$(id -g)" \
        "$ROOT/docker"
}

up() {
    running && return 0
    docker image inspect "$IMAGE" >/dev/null 2>&1 || build
    docker rm -f "$NAME" >/dev/null 2>&1 || true
    mkdir -p /tmp/jls-ui-test

    local args=(-d --name "$NAME" --hostname "$NAME" --init --shm-size 512m --network "$NETWORK"
                --ulimit memlock=-1:-1 # JACK clients lock their memory
                -v "$HOME_VOLUME:/home/$USER_NAME"
                -v "$ROOT:$ROOT" -w "$ROOT"
                -v /etc/localtime:/etc/localtime:ro # host time zone (build info, logs)
                # UI-test screenshots land where a Claude on the host can read them too.
                -v /tmp/jls-ui-test:/tmp/jls-ui-test)
    # ALSA sequencer, for MIDI tests (aconnect, aplaymidi). Needs the device's group.
    if [ -e /dev/snd/seq ]; then
        args+=(--device /dev/snd/seq --group-add "$(stat -c %g /dev/snd/seq)")
    fi
    docker run "${args[@]}" "$IMAGE" >/dev/null

    # No sound card in here: a JACK server with the dummy driver gives the app an audio device
    # (meters and live playback work; nothing is audible).
    docker exec -d "$NAME" jackd --no-realtime -d dummy -r 48000 -p 512
    echo "Sandbox running ($NAME). tools/sandbox.sh shell | run CMD | claude | down"
}

exec_in() { # [-it] CMD...
    up
    local tty=()
    if [ "${1:-}" = "-it" ]; then tty=(-it); shift; fi
    docker exec "${tty[@]}" -w "$ROOT" "$NAME" "$@"
}

case "${1:-}" in
    build) build ;;
    up) up ;;
    shell) exec_in -it bash -l ;;
    run) shift; exec_in bash -lc "$(printf '%q ' "$@")" ;;
    claude) shift; exec_in -it claude "$@" ;;
    down) docker rm -f "$NAME" >/dev/null 2>&1 || true ;;
    reset)
        docker rm -f "$NAME" >/dev/null 2>&1 || true
        docker volume rm "$HOME_VOLUME" >/dev/null 2>&1 || true ;;
    *) sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'; exit 1 ;;
esac
