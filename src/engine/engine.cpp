// Audio-thread processing order per block (see docs/architecture.md, "Audio callback"):
//   1. adopt the newest snapshot (old one goes back to the UI thread for deletion)
//   2. advance the transport, collect note triggers for the block
//   3. render sampler voices into their mixer-track buses, split at trigger offsets
//   4. run each insert's effects + fader/pan + meter, sum into the Master
//   5. Master effects + fader/pan + meter, add to the output; metronome on top

#include "engine/engine.h"

#include <algorithm>
#include <cmath>

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

void Engine::prepare(double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;
    maxBlock = std::max(64, maxBlockSize);
    sampler.prepare(newSampleRate);
    metronomeState.prepare(newSampleRate);

    busMemory.assign(static_cast<std::size_t>(maxMixerTracks * 2 * maxBlock), 0.0f);
    for (std::size_t i = 0; i < buses.size(); ++i)
        buses[i] = busMemory.data() + i * static_cast<std::size_t>(maxBlock);

    if (current != nullptr)
        for (auto& track : current->tracks)
            for (auto& effect : track.effects)
                if (effect.processor)
                    effect.processor->prepare(newSampleRate, maxBlock);

    preparedRate.store(newSampleRate);
    preparedBlock.store(maxBlock);
}

float Engine::takePeak(int track, int side)
{
    if (track < 0 || track >= maxMixerTracks || side < 0 || side > 1)
        return 0.0f;
    return peaks[static_cast<std::size_t>(track * 2 + side)].exchange(0.0f);
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
    if (maxBlock == 0)
        return; // not prepared

    // The mixer buses hold maxBlock samples: split larger blocks.
    for (int offset = 0; offset < numSamples; offset += maxBlock)
        renderChunk(channels, numChannels, offset, std::min(maxBlock, numSamples - offset));
}

void Engine::renderChunk(float* const* channels, int numChannels, int offset, int numSamples)
{
    const auto block = transportState.advance(numSamples, sampleRate);
    const int numTracks = current == nullptr ? 1
                                             : std::clamp(static_cast<int>(current->tracks.size()),
                                                          1, maxMixerTracks);

    for (int i = 0; i < numTracks * 2; ++i)
        std::fill_n(buses[static_cast<std::size_t>(i)], numSamples, 0.0f);

    if (current != nullptr)
    {
        const int count = collectTriggers(*current, block, numSamples, triggers);

        // Render in segments so each trigger starts on its exact sample.
        int cursor = 0;
        for (int i = 0; i < count; ++i)
        {
            const auto& trigger = triggers[static_cast<std::size_t>(i)];
            sampler.render(*current, buses.data(), numTracks * 2, cursor, trigger.sampleOffset);
            sampler.trigger(*current, trigger.channelIndex, trigger.pitch, trigger.velocity);
            cursor = trigger.sampleOffset;
        }
        sampler.render(*current, buses.data(), numTracks * 2, cursor, numSamples);
    }

    // Inserts feed the Master.
    float* masterLeft = buses[0];
    float* masterRight = buses[1];
    for (int t = 1; t < numTracks; ++t)
    {
        processTrack(t, numSamples);
        const float* left = buses[static_cast<std::size_t>(2 * t)];
        const float* right = buses[static_cast<std::size_t>(2 * t + 1)];
        for (int i = 0; i < numSamples; ++i)
        {
            masterLeft[i] += left[i];
            masterRight[i] += right[i];
        }
    }
    processTrack(0, numSamples);

    // Master to the output (mono outputs get the left side).
    for (int i = 0; i < numSamples; ++i)
    {
        channels[0][offset + i] += masterLeft[i];
        if (numChannels > 1)
            channels[1][offset + i] += masterRight[i];
    }

    std::array<float*, 2> out{channels[0] + offset,
                              numChannels > 1 ? channels[1] + offset : nullptr};
    metronomeState.render(out.data(), std::min(numChannels, 2), numSamples, block);
}

void Engine::processTrack(int track, int numSamples)
{
    float* left = buses[static_cast<std::size_t>(2 * track)];
    float* right = buses[static_cast<std::size_t>(2 * track + 1)];

    float gainLeft = 1.0f;
    float gainRight = 1.0f;
    if (current != nullptr && track < static_cast<int>(current->tracks.size()))
    {
        const auto& state = current->tracks[static_cast<std::size_t>(track)];
        for (const auto& effect : state.effects)
            if (effect.enabled && effect.processor != nullptr)
                effect.processor->process(left, right, numSamples, effect.params);
        gainLeft = state.gainLeft;
        gainRight = state.gainRight;
    }

    float peakLeft = 0.0f;
    float peakRight = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        left[i] *= gainLeft;
        right[i] *= gainRight;
        peakLeft = std::max(peakLeft, std::abs(left[i]));
        peakRight = std::max(peakRight, std::abs(right[i]));
    }

    // Keep the highest level until the UI reads it.
    auto& pl = peaks[static_cast<std::size_t>(2 * track)];
    auto& pr = peaks[static_cast<std::size_t>(2 * track + 1)];
    if (peakLeft > pl.load(std::memory_order_relaxed))
        pl.store(peakLeft, std::memory_order_relaxed);
    if (peakRight > pr.load(std::memory_order_relaxed))
        pr.store(peakRight, std::memory_order_relaxed);
}

} // namespace jls::engine
