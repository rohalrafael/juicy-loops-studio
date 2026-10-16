#pragma once

#include "engine/sample_buffer.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace jls::engine
{

// Immutable snapshot of everything the audio thread needs to play the project.
// Built on the UI thread, handed to the audio thread through Engine::setState().
struct ChannelState
{
    std::uint32_t id = 0;
    std::shared_ptr<const SampleBuffer> sample;
    float gainLeft = 1.0f;
    float gainRight = 1.0f;
    bool muted = false;
    std::vector<std::uint8_t> steps; // one entry per pattern step, nonzero = on
};

struct EngineState
{
    std::vector<ChannelState> channels;
    int patternLengthInSteps = 16;
};

} // namespace jls::engine
