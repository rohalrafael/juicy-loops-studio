#include "engine/step_sequencer.h"

#include <algorithm>
#include <cmath>

namespace jls::engine
{
namespace
{

// Adds a trigger for every unmuted channel whose step `patternStep` is on.
// Returns false once `out` is full.
bool emitPatternStep(const EngineState& state, const PatternState& pattern, int patternStep,
                     int offset, std::span<Trigger> out, int& count)
{
    const auto numChannels = std::min(state.channels.size(), pattern.steps.size());
    for (std::size_t ch = 0; ch < numChannels; ++ch)
    {
        const auto& row = pattern.steps[ch];
        if (state.channels[ch].muted || patternStep >= static_cast<int>(row.size()) ||
            row[static_cast<std::size_t>(patternStep)] == 0)
            continue;
        if (count == static_cast<int>(out.size()))
            return false;
        out[static_cast<std::size_t>(count++)] = {static_cast<int>(ch), offset};
    }
    return true;
}

} // namespace

int collectTriggers(const EngineState& state, const BlockPosition& block, int numSamples,
                    std::span<Trigger> out)
{
    if (!block.playing || numSamples <= 0 || block.endBeat <= block.startBeat)
        return 0;

    const double startStep = block.startBeat * stepsPerBeat;
    const double endStep = block.endBeat * stepsPerBeat;
    const double span = endStep - startStep;
    int count = 0;

    // Every whole step s with startStep <= s < endStep begins in this block.
    for (auto step = static_cast<long long>(std::ceil(startStep));
         static_cast<double>(step) < endStep; ++step)
    {
        const double fraction = (static_cast<double>(step) - startStep) / span;
        const int offset =
            std::clamp(static_cast<int>(std::lround(fraction * numSamples)), 0, numSamples - 1);

        if (!state.songMode)
        {
            if (state.currentPattern < 0 ||
                state.currentPattern >= static_cast<int>(state.patterns.size()))
                return count;
            const auto& pattern = state.patterns[static_cast<std::size_t>(state.currentPattern)];
            if (pattern.lengthInSteps <= 0)
                return count;
            const auto patternStep = static_cast<int>(step % pattern.lengthInSteps);
            if (!emitPatternStep(state, pattern, patternStep, offset, out, count))
                return count;
            continue;
        }

        // Song mode: the arrangement loops; every clip under the playhead plays its pattern
        // (looping the pattern if the clip is longer than it).
        if (state.songLengthInSteps <= 0)
            return count;
        const auto songStep = static_cast<int>(step % state.songLengthInSteps);
        for (const auto& clip : state.clips)
        {
            if (songStep < clip.startStep || songStep >= clip.startStep + clip.lengthInSteps ||
                clip.patternIndex < 0 ||
                clip.patternIndex >= static_cast<int>(state.patterns.size()))
                continue;
            const auto& pattern = state.patterns[static_cast<std::size_t>(clip.patternIndex)];
            if (pattern.lengthInSteps <= 0)
                continue;
            const int patternStep = (songStep - clip.startStep) % pattern.lengthInSteps;
            if (!emitPatternStep(state, pattern, patternStep, offset, out, count))
                return count;
        }
    }
    return count;
}

} // namespace jls::engine
