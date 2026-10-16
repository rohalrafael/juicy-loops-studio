#pragma once

#include "engine/engine_state.h"
#include "engine/transport.h"

#include <span>

namespace jls::engine
{

inline constexpr int stepsPerBeat = 4; // a step is a 16th note

struct Trigger
{
    int channelIndex = 0; // index into EngineState::channels
    int sampleOffset = 0; // within the block
};

// Finds the steps that start inside `block` and returns how many triggers were written to `out`,
// in order of sample offset. The pattern loops. Excess triggers beyond out.size() are dropped.
int collectTriggers(const EngineState& state, const BlockPosition& block, int numSamples,
                    std::span<Trigger> out);

} // namespace jls::engine
