# Development sandbox (Docker)

A container with everything needed to build, test and drive the app, so tools and test plugins
don't have to be installed on the developer's machine. Defined in `docker/Dockerfile`
(Ubuntu 26.04 LTS, the same family as the main developer's desktop and CI) and run through
`tools/sandbox.sh`.

## What's inside

| For | Packages |
|---|---|
| Building | build-essential (GCC), cmake, ninja-build, clang, clang-format, ccache, git, gh |
| JUCE on Linux | ALSA, JACK, FreeType, fontconfig, X11 and GL development packages |
| UI testing | xvfb, xdotool, imagemagick (see `tools/ui-test.sh`) |
| Audio / MIDI | jackd2 + jack-example-tools (a dummy JACK server runs in the container), alsa-utils |
| Debugging | gdb |
| Test plugins | lsp-plugins (VST3 + LV2) from Ubuntu; Surge XT from its official `.deb` (pinned) |
| Claude Code | installed with the official script into the home volume (updates itself) |

Inside, you have passwordless `sudo`, so anything else is a `sudo apt-get install` away (after `sudo apt-get update`: the image
drops the package lists). Add packages that are needed for good to the Dockerfile.

The Ubuntu release is the Dockerfile's `UBUNTU_VERSION` argument. Stay on an LTS: a binary
works on the release it was built on and on newer ones, not on older ones.

## Using it

```sh
tools/sandbox.sh up                       # build the image if needed, start the container
tools/sandbox.sh run cmake -B build-docker -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
tools/sandbox.sh run cmake --build build-docker
tools/sandbox.sh run ctest --test-dir build-docker --output-on-failure
tools/sandbox.sh shell                    # interactive shell in the repo
tools/sandbox.sh claude                   # Claude Code inside (log in once)
tools/sandbox.sh down                     # stop; `reset` also deletes the home volume
```

- **The repo** is mounted at the same path as on the host, read-write. The sandbox builds into
  `build-docker/` (`JLS_BUILD_DIR`), so it never mixes with the host's `build/`.
- **The home directory** is a Docker volume (`jls-sandbox-home`): the app's settings and plugin
  scan, ccache and the Claude Code login survive restarts. They're separate from the host's, so
  tests can't change your real theme or plugin list.
- **UI tests** work as on the host: `tools/sandbox.sh run tools/ui-test.sh start-x`, `start`,
  `shot NAME`... Screenshots go to `/tmp/jls-ui-test`, which is shared with the host.
- **Audio:** there's no sound card, so `up` starts JACK with the dummy driver and the app plays
  into it. Meters, live playing and plugins all run; nothing is audible. Listening stays a job
  for the real desktop.
- **MIDI:** the host's ALSA sequencer (`/dev/snd/seq`) is passed in. Test with
  `aplaymidi -p 14:0 file.mid` (Midi Through, which the app subscribes to automatically). Host
  MIDI devices are visible too.
- **Network:** `bridge` by default. If Docker can't create its network (e.g. "failed to add the
  host <=> sandbox pair interfaces: operation not supported" after a kernel update, before
  rebooting), use `JLS_SANDBOX_NETWORK=host tools/sandbox.sh up`.

## A binary for the host

`tools/sandbox.sh run tools/dist.sh` builds an optimized app (RelWithDebInfo, so crash
backtraces stay readable) into `build-dist/` and copies it to **`dist/juicy-loops-studio`**,
with `dist/BUILD-INFO.txt` (version, commit, date). Run it on the host:

```sh
./dist/juicy-loops-studio
```

It runs on Ubuntu 26.04 and newer (and on other distributions with glibc 2.43 or newer and the
same libraries); check with `ldd dist/juicy-loops-studio | grep "not found"`. It uses the host's
own settings and plugins.

What it needs at run time is on any desktop install: glibc and libstdc++, ALSA (`libasound2t64`),
FreeType and fontconfig, linked directly; the X11 libraries, loaded when it starts (on Wayland it
runs through XWayland). Optional, loaded only if present: JACK (`libjack.so.0`; on a PipeWire
desktop, install `pipewire-jack` to get the JACK device type, ALSA works without it) and
`libxss1` (keeps the screen saver off).

## Plugins in the sandbox

The sandbox keeps its own plugin list (`~/.config/JuicyLoopsStudio/plugins.xml` in the home
volume). Scan once in Options › Plugins, or copy the host's list in (same packages, same paths):
`docker cp ~/.config/JuicyLoopsStudio/plugins.xml jls-sandbox:/home/$USER/.config/JuicyLoopsStudio/`.

## Working with an AI agent (e.g. Claude Code)

- **Agent on the host, tools in the sandbox:** the agent runs on the host as usual and runs builds,
  tests and installs through `tools/sandbox.sh run ...`. Memory, git pushes (through the host's
  SSH agent) and the conversation stay on the host.
- **Claude inside the sandbox** (`tools/sandbox.sh claude`): everything it runs stays in the
  container, so permission prompts can be loosened. It can't push (no SSH agent or `gh` login in
  there) and it has its own login and memory; the project's CLAUDE.md is shared through the repo.
  It can still change any file in the repo, so review with `git diff` as usual.
