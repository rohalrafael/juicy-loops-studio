#pragma once

// The metronome: sample-accurate clicks (accented on each bar's first beat), found in the beat
// span the transport reports for every block.

#include "engine/transport.h"

#include <atomic>

namespace jls::engine
{

// Sample-accurate click on every beat, with a higher-pitched accent on each bar's downbeat.
//
// Threading: setEnabled()/setVolume() from any thread; prepare() before audio starts;
// render() only from the audio thread. render() never locks or allocates.
class Metronome
{
  public:
    static constexpr double accentFrequency = 1760.0; // A6
    static constexpr double beatFrequency = 880.0;    // A5
    static constexpr double clickSeconds = 0.03;

    void prepare(double sampleRate);

    void setEnabled(bool shouldBeEnabled);
    bool isEnabled() const;
    void toggleEnabled();

    // Linear gain, 0..1.
    void setVolume(float gain);
    float volume() const;

    // Adds clicks for the beats in `block` to the output (does not clear it).
    void render(float* const* channels, int numChannels, int numSamples,
                const BlockPosition& block);

  private:
    void startClick(bool accent);
    void renderClick(float* const* channels, int numChannels, int from, int to);

    std::atomic<bool> enabled{false};
    std::atomic<float> gain{0.5f};

    // Audio-thread state.
    double sampleRate = 44100.0;
    double phase = 0.0;
    double phaseIncrement = 0.0;
    float envelope = 0.0f;
    float decay = 0.0f;
    int samplesLeft = 0;
};

} // namespace jls::engine
