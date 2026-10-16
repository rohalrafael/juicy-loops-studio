#pragma once

#include "engine/engine_state.h"

#include <array>
#include <cstdint>

namespace jls::engine
{

// Polyphonic one-shot sample playback for all channels. Audio thread only; never allocates.
class Sampler
{
  public:
    static constexpr int maxVoices = 64;

    void prepare(double sampleRate);

    // Call whenever the engine switches to a new state, before rendering with it.
    // Stops voices whose channel was removed or whose sample was replaced, so no voice
    // ever points at a sample the current state doesn't keep alive.
    void stateChanged(const EngineState& state);

    void trigger(const EngineState& state, int channelIndex);

    // Adds all sounding voices into channels[*][from, to).
    void render(const EngineState& state, float* const* channels, int numChannels, int from,
                int to);

    int activeVoiceCount() const;

  private:
    struct Voice
    {
        bool active = false;
        std::uint32_t channelId = 0;
        const SampleBuffer* sample = nullptr;
        double position = 0.0;
        double increment = 1.0;
        std::uint64_t startedAt = 0;
    };

    Voice& allocateVoice();

    std::array<Voice, maxVoices> voices{};
    double sampleRate = 44100.0;
    std::uint64_t triggerCounter = 0;
};

} // namespace jls::engine
