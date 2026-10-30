# Juicy Loops Studio

A free, open-source DAW for Linux, Windows and macOS, with a workflow that will feel
familiar to FL Studio users: Channel Rack and step sequencer, Piano Roll,
pattern-based Playlist, and Mixer.

Juicy Loops Studio is an independent project. It is not affiliated with or endorsed
by Image-Line, and it contains no Image-Line code or assets.

**Status:** early, but you can make whole tracks:

- **Channel Rack** with step sequencer, looping channels, built-in drum sounds and your own samples
- **Piano Roll** with velocity, scale highlighting, selection, copy/paste and a note slicer
- **Playlist** for arranging patterns and **audio clips** (with waveforms, trimming, a picker panel)
- **Sampler** settings per sample: time-stretching (Off / Resample / Stretch) and pitch
- **Browser** for your sample folders, with previews and keyboard navigation
- **Mixer** with 16 inserts, meters and built-in effects (Filter, EQ, Delay, Reverb, Compressor,
  Soft Clipper)
- **VST3 / LV2 plugins**: effects on mixer inserts, instruments on channels (with tempo sync)
- **Live playing** from the computer keyboard or a MIDI keyboard
- **Projects** (`.jlp`) and **export to WAV**
- **Keyboard shortcuts for everything**, FL Studio's by default, all customizable (with
  import/export)
- Seven colour themes (Blackberry, Graphite, Nightshade, Bloodline, Chrome, Ice, Dracula), plus
  custom theme files

Not there yet: recording, automation, mixer sends. See the [roadmap](docs/roadmap.md).

Documentation: [user guide](docs/user-guide.md), [all docs](docs/README.md). Contributing (people
and AI agents): start with [AGENTS.md](AGENTS.md).

## Building

Requirements: CMake ≥ 3.24, a C++20 compiler (GCC 11+, Clang 14+, MSVC 2022), git.

On Linux you also need the ALSA, JACK, FreeType, fontconfig and X11 development
packages. Ubuntu / Debian: `sudo apt install build-essential cmake ninja-build libasound2-dev
libjack-jackd2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxext-dev libxrandr-dev
libxinerama-dev libxcursor-dev libxcomposite-dev libxi-dev libxrender-dev libgl1-mesa-dev`.
Arch: `sudo pacman -S cmake ninja alsa-lib jack2 freetype2 fontconfig libx11 libxrandr libxinerama libxcursor libxcomposite libxi`

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

The app binary ends up under `build/src/app/JuicyLoopsStudio_artefacts/`. To build without
installing anything, use the Docker sandbox (`tools/sandbox.sh`, see
[docs/sandbox.md](docs/sandbox.md)); more in [docs/development.md](docs/development.md).

## License

AGPLv3. See [LICENSE](LICENSE). This follows from JUCE's open-source license.
