#include "engine/engine.h"

namespace jls::engine
{

Engine::~Engine()
{
    collectGarbage();
    while (auto state = incoming.pop())
        delete *state;
    delete current;
}

void Engine::setState(std::unique_ptr<EngineState> state)
{
    pending = std::move(state);
    collectGarbage();
}

void Engine::collectGarbage()
{
    while (auto state = retired.pop())
        delete *state;

    if (pending && incoming.push(pending.get()))
        pending.release();
}

void Engine::prepare(double newSampleRate)
{
    sampleRate = newSampleRate;
    sampler.prepare(newSampleRate);
    metronomeState.prepare(newSampleRate);
}

void Engine::adoptNewestState()
{
    // Only take a new state if the old one can be handed back for deletion.
    while (retired.hasSpace())
    {
        auto next = incoming.pop();
        if (!next)
            return;

        // Stop voices before the old state (which may own their samples) leaves this thread.
        sampler.stateChanged(**next);
        if (current != nullptr)
            retired.push(current);
        current = *next;
    }
}

void Engine::process(float* const* channels, int numChannels, int numSamples)
{
    adoptNewestState();

    const auto block = transportState.advance(numSamples, sampleRate);

    if (current != nullptr)
    {
        const int count = collectTriggers(*current, block, numSamples, triggers);

        // Render in segments so each trigger starts on its exact sample.
        int cursor = 0;
        for (int i = 0; i < count; ++i)
        {
            const auto& trigger = triggers[static_cast<std::size_t>(i)];
            sampler.render(*current, channels, numChannels, cursor, trigger.sampleOffset);
            sampler.trigger(*current, trigger.channelIndex, trigger.pitch, trigger.velocity);
            cursor = trigger.sampleOffset;
        }
        sampler.render(*current, channels, numChannels, cursor, numSamples);
    }

    metronomeState.render(channels, numChannels, numSamples, block);
}

} // namespace jls::engine
