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
    // When the note ends, in the same (unwrapped) tick timeline as the transport: the note's own
    // end, cut at the end of its clip or of the pattern loop. Instruments get a note-off there.
    double endTick = 0.0;
    // Audio clips: the channel's sample plays from `clipOffsetTicks` into the clip (0 = from its
    // start) for `lengthTicks`, then stops; it's cut when the transport stops or jumps.
    bool audioClip = false;
    double clipOffsetTicks = 0.0;
    double lengthTicks = 0.0;
};

// Finds the notes that start inside `block` and returns how many triggers were written to
// `out`, sorted by sample offset. Pattern mode loops the current pattern; song mode plays every
// clip under the playhead (a clip longer than its pattern repeats it) and loops the song.
// Triggers beyond out.size() are dropped. Note ends are not reported here: the engine keeps
// each started note until its Trigger::endTick (see Engine).
int collectTriggers(const EngineState& state, const BlockPosition& block, int numSamples,
                    std::span<Trigger> out);

// Audio clips playing at song position `tick` that started before it (Play pressed, or a jump,
// into the middle of them): one trigger each, at sample offset 0, with clipOffsetTicks set.
// Song mode only. Clips starting exactly at `tick` are left to collectTriggers().
int collectClipsUnderPlayhead(const EngineState& state, double tick, std::span<Trigger> out);

} // namespace jls::engine
