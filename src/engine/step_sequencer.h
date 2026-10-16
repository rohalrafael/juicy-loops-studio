#pragma once

// Turns the musical time covered by one audio block into note triggers with exact sample
// offsets. Pure function of (state, block position): no state, easy to test.

#include "engine/engine_state.h"
#include "engine/transport.h"

#include <span>

namespace jls::engine
{

inline constexpr int stepsPerBeat = 4; // a step is a 16th note
inline constexpr int ticksPerStep = ticksPerBeat / stepsPerBeat;

struct Trigger
{
    int channelIndex = 0; // index into EngineState::channels
    int sampleOffset = 0; // within the block
    int pitch = 60;
    float velocity = 100.0f / 128.0f;
};

// Finds the notes that start inside `block` and returns how many triggers were written to
// `out`, sorted by sample offset. Pattern mode loops the current pattern; song mode plays every
// clip under the playhead (a clip longer than its pattern repeats it) and loops the song.
// Triggers beyond out.size() are dropped.
int collectTriggers(const EngineState& state, const BlockPosition& block, int numSamples,
                    std::span<Trigger> out);

} // namespace jls::engine
