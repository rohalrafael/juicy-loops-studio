# Juicy Loops Studio

A free, open-source DAW for Linux, Windows and macOS, with a workflow that will feel
familiar to FL Studio users: Channel Rack and step sequencer, Piano Roll,
pattern-based Playlist, and Mixer.

Juicy Loops Studio is an independent project. It is not affiliated with or endorsed
by Image-Line, and it contains no Image-Line code or assets.

**Status:** early but usable for making beats:

- **Channel Rack** with step sequencer, built-in drum sounds, your own samples (drag and drop)
- **Patterns** and the **Playlist** for arranging songs (PAT / SONG modes)
- **Piano Roll** with velocity
- **Mixer** with 16 inserts, meters and built-in effects (Filter, EQ, Delay, Reverb, Compressor, Soft Clipper)
- **VST3 / LV2 effect plugins** on mixer inserts
- **Save / open projects** (`.jls`) and **export to WAV**
- Four themes (Juicy, Citrus, Watermelon, Blueberry), plus custom theme files
- FL Studio's shortcuts: Space, L, F4–F9, Ctrl+M, Ctrl+Z, Ctrl+S, Ctrl+R

Not there yet: plugin instruments, recording, automation. See the roadmap in
[CLAUDE.md](CLAUDE.md).

Documentation: [user guide](docs/user-guide.md), [architecture](docs/architecture.md),
[theme files](docs/themes.md).

## Building

Requirements: CMake ≥ 3.24, a C++20 compiler (GCC 11+, Clang 14+, MSVC 2022), git.

On Linux you also need the ALSA, JACK, FreeType, fontconfig and X11 development
packages. Arch: `sudo pacman -S cmake ninja alsa-lib jack2 freetype2 fontconfig libx11 libxrandr libxinerama libxcursor libxcomposite libxi`

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

The app binary ends up under `build/src/app/JuicyLoopsStudio_artefacts/`.

## License

AGPLv3. See [LICENSE](LICENSE). This follows from JUCE's open-source license.
