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
};

struct PatternState
{
    int lengthInSteps = 16;
    // steps[channelIndex][step], nonzero = on. Indexed like EngineState::channels.
    std::vector<std::vector<std::uint8_t>> steps;
};

struct ClipState
{
    int patternIndex = 0; // into EngineState::patterns
    int startStep = 0;    // position in the song, in steps
    int lengthInSteps = 16;
};

struct EngineState
{
    std::vector<ChannelState> channels;
    std::vector<PatternState> patterns;
    std::vector<ClipState> clips;

    bool songMode = false;     // false: loop patterns[currentPattern]; true: play clips
    int currentPattern = 0;    // index into patterns
    int songLengthInSteps = 0; // song mode loops after this; 0 = nothing to play
};

} // namespace jls::engine
