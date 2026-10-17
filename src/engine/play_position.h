#pragma once

// Where playback is, as effects and instruments see it (hosted plugins turn it into JUCE's
// AudioPlayHead, so arpeggiators and tempo-synced LFOs/delays follow the song).

namespace jls::engine
{

struct PlayPosition
{
    bool playing = false;
    double bpm = 130.0;
    int beatsPerBar = 4;          // time signature n/4
    double beat = 0.0;            // quarter notes from the loop start, at the start of the block
    double loopLengthBeats = 0.0; // what's looping (the pattern, or the song); 0 = nothing
    double seconds = 0.0;         // `beat` as time
    long long sample = 0;         // `beat` in samples

    double barStartBeat() const; // start of the bar `beat` is in
};

inline double PlayPosition::barStartBeat() const
{
    const int bar = beatsPerBar > 0 ? static_cast<int>(beat / beatsPerBar) : 0;
    return static_cast<double>(bar * beatsPerBar);
}

} // namespace jls::engine
