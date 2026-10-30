# Development

## Build, test, run

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug   # first run fetches JUCE, Catch2, nlohmann/json, Signalsmith Stretch
cmake --build build
ctest --test-dir build --output-on-failure
"./build/src/app/JuicyLoopsStudio_artefacts/Debug/Juicy Loops Studio"
```

Options: `-DJLS_BUILD_APP=OFF` (engine + tests only, no JUCE), `-DJLS_BUILD_TESTS=OFF`.
Use `-DCMAKE_CXX_COMPILER_LAUNCHER=ccache` for faster rebuilds.

JUCE is fetched by CMake and patched (`patches/`, applied by `cmake/apply_patch.cmake`); see
`patches/README.md` for what each patch fixes and when it can go.

## The sandbox (recommended)

`tools/sandbox.sh` runs a Docker container with the toolchain, Xvfb/xdotool/ImageMagick, JACK
(dummy driver), alsa-utils, gdb and test plugins (LSP, Surge XT): nothing to install on your
machine. It builds into `build-docker/`. `tools/sandbox.sh run tools/dist.sh` makes an optimized
binary in `dist/` that runs on Ubuntu 26.04 or newer. Details: [sandbox.md](sandbox.md).

## Testing the UI

Unit tests cover the engine and model; UI changes are checked by driving the app on a private
X display (`tools/ui-test.sh`: Xvfb on `:99`, xdotool, ImageMagick) and looking at screenshots:

```sh
tools/ui-test.sh start-x; tools/ui-test.sh start
tools/ui-test.sh click 320 427; tools/ui-test.sh key ctrl+z; tools/ui-test.sh shot name
tools/ui-test.sh stop; tools/ui-test.sh stop-x
```

Never drive your real desktop with xdotool (on Wayland it doesn't work, and elsewhere it moves
your pointer). Things to know about Xvfb (no window manager):

- Nothing has keyboard focus at start: click an empty spot of the transport bar (e.g. 1050,56)
  before sending keys.
- Windows can open *behind* others, and a modal message box blocks all input until dismissed.
  List windows with `xdotool search --onlyvisible --name ""` + `getwindowname`; bring one up
  with `xdotool windowraise`. Dialogs have no title bar; JUCE logs harmless `BadAtom` errors.
- Plugins may show their own popups (LSP shows a one-time greeting).
- The app uses the real settings file of the user running it (`~/.config/JuicyLoopsStudio/`):
  reset what you changed (e.g. the theme) after testing.
- Maximize and restore depend on the window manager, and Xvfb has none.
  `tools/gnome-maximize-test.sh` plays GNOME's part (the resize first, `_NET_WM_STATE` just
  after) and checks the window's edges follow (no border; resize edges only while restored).
  For real resizing by the edges, run the app under `openbox` (not in the sandbox image:
  `apt install openbox` in the container as root, gone on the next `tools/sandbox.sh build`).
  To see what the X server shows before the app repaints (flicker), stop the app with
  `kill -STOP`, resize it, screenshot, then `kill -CONT`. Faking a window manager by hand: start Xvfb with
  `-noreset`, list `_NET_WM_STATE` in the root's `_NET_SUPPORTED` before the app starts (JUCE
  looks the atoms up once), and set atom lists with Xlib (`xprop -set` stores a single atom).
- To screenshot a window on a real X/XWayland session without the rest of the screen:
  `import -window "Juicy Loops Studio" out.png`. Don't `pkill -f` the app path from a shell
  script: it matches the script itself.

- F-keys sent by name (`xdotool key F9`) arrive as Alt+F9 with Xvfb's default keymap: send
  keycodes instead (F5 71, F6 72, F7 73, F8 74, F9 75).
- The areas' native windows are titled with their names ("Playlist", "Channel rack", "Mixer",
  ...): `xdotool search --onlyvisible --name '^Mixer$'` + `xwininfo` give a panel's position.

### Screenshots for the README and docs

`tools/screenshots.sh` photographs the app from a project: the whole window, the Channel Rack,
Piano Roll, Mixer, Browser (a generated sample folder unless you give one), the overview in
every built-in theme plus a grid of them, and close-ups for the README's features. Each scene starts
the app with settings of its own (never yours), a borderless window of a fixed size, and writes
WebP files:

```sh
tools/sandbox.sh run tools/screenshots.sh path/to/project.jlp               # all scenes
tools/sandbox.sh run tools/screenshots.sh --only "mixer themes" --out docs/images project.jlp
```

The project decides what's in the pictures (its patterns, notes, arrangement, effects, current
pattern); `--help` lists the options. Panels are cropped by their windows' titles, so the
crops follow layout changes; it warns when a panel is larger than the screen.

The README's images come from the showcase project, a beat and a drum kit that
`tools/showcase-project.py` synthesises (no audio in the repo). To redo them after a UI change:

```sh
python3 tools/showcase-project.py /tmp/jls-ui-test/showcase
tools/sandbox.sh run tools/screenshots.sh --play 9 --out docs/images \
    --samples "/tmp/jls-ui-test/showcase/Neon Ruins Kit" "/tmp/jls-ui-test/showcase/Neon Ruins.jlp"
```

`--play SECONDS` shoots while the project plays (the sandbox's JACK dummy device drives it), so
the time display, play heads, meters and scope show it. The Channel Rack and Piano Roll scenes
switch to PAT mode first, since in song mode their play heads give way to the Playlist's; the
Browser scene and the close-ups of dialogs stay stopped.

The `features` scene takes the README's close-ups (`feature-*.webp`): the app at interface size
200 % on a screen twice as large, so the layout is the one the other scenes see but the crops
have twice the pixels and stay sharp shown large. Its crops are in interface pixels from a
panel's or dialog's corner (`shot_part`), so a layout change may need them adjusted: check the
images after a UI change.

Sound can't be checked this way: export a WAV and analyse it (length, level, pitch), or listen.

## CI

`.github/workflows/ci.yml` builds and tests every pull request and every push to `main` on
Ubuntu 26.04, Ubuntu 24.04 and Windows. `main` is protected: changes reach it only through pull requests, and
a pull request merges once the **CI result** check passes. Changes that only touch `docs/`,
Markdown files, `LICENSE` or `.clang-format` skip the build (CI result still passes).

Each build uploads the app, kept for 14 days; download it from the run's page or with
`gh run download`:
- `juicy-loops-studio-linux-x64.tar.gz`: the binary, built on 24.04 so it runs there and on
  anything newer (Linux Mint 22, Pop!_OS 24.04, Ubuntu 26.04); 26.04 builds only to test the
  newer compiler
- `juicy-loops-studio-windows-x64.zip`: a single `.exe` with the MSVC runtime linked in

macOS isn't built or tested for now.

**Releases.** Pushing a tag like `v0.1.0` builds both and publishes a GitHub release with the
two packages and notes generated from the merged pull requests:

```sh
git tag v0.1.0 && git push origin v0.1.0
```

## Editing tips

- When changing files with scripts, check that every search string matched before writing:
  clang-format reflows lines, and a silent non-match once left new commands unregistered.
- Debug crashes on Linux with
  `coredumpctl debug <pid> --debugger-arguments="-batch -ex 'thread 1' -ex 'bt 40'"`.
