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
binary in `dist/` that runs on a host with the same distribution. Details: [sandbox.md](sandbox.md).

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
- To screenshot a window on a real X/XWayland session without the rest of the screen:
  `import -window "Juicy Loops Studio" out.png`. Don't `pkill -f` the app path from a shell
  script: it matches the script itself.

Sound can't be checked this way: export a WAV and analyse it (length, level, pitch), or listen.

## CI

`.github/workflows/ci.yml`: pushes and pull requests build and test on Linux (with ccache); the
full Ubuntu / Windows / macOS matrix runs on manual dispatch (`gh workflow run ci.yml`).

## Editing tips

- When changing files with scripts, check that every search string matched before writing:
  clang-format reflows lines, and a silent non-match once left new commands unregistered.
- Debug crashes on Linux with
  `coredumpctl debug <pid> --debugger-arguments="-batch -ex 'thread 1' -ex 'bt 40'"`.
