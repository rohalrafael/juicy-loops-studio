#include "engine/step_sequencer.h"

#include <algorithm>
#include <cmath>

namespace jls::engine
{

int collectTriggers(const EngineState& state, const BlockPosition& block, int numSamples,
                    std::span<Trigger> out)
{
    if (!block.playing || numSamples <= 0 || block.endBeat <= block.startBeat ||
        state.patternLengthInSteps <= 0)
        return 0;

    const double startStep = block.startBeat * stepsPerBeat;
    const double endStep = block.endBeat * stepsPerBeat;
    const double span = endStep - startStep;
    int count = 0;

    // Every whole step s with startStep <= s < endStep begins in this block.
    for (auto step = static_cast<long long>(std::ceil(startStep));
         static_cast<double>(step) < endStep; ++step)
    {
        const auto patternStep = static_cast<std::size_t>(step % state.patternLengthInSteps);
        const double fraction = (static_cast<double>(step) - startStep) / span;
        const int offset =
            std::clamp(static_cast<int>(std::lround(fraction * numSamples)), 0, numSamples - 1);

        for (std::size_t ch = 0; ch < state.channels.size(); ++ch)
        {
            const auto& channel = state.channels[ch];
            if (channel.muted || patternStep >= channel.steps.size() ||
                channel.steps[patternStep] == 0)
                continue;
            if (count == static_cast<int>(out.size()))
                return count;
            out[static_cast<std::size_t>(count++)] = {static_cast<int>(ch), offset};
        }
    }
    return count;
}

} // namespace jls::engine
