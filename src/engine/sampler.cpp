#include "engine/sampler.h"

#include <algorithm>
#include <cmath>

namespace jls::engine
{
namespace
{

const ChannelState* findChannel(const EngineState& state, std::uint32_t id)
{
    for (const auto& channel : state.channels)
        if (channel.id == id)
            return &channel;
    return nullptr;
}

} // namespace

void Sampler::prepare(double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    for (auto& voice : voices)
        voice.active = false;
}

void Sampler::stateChanged(const EngineState& state)
{
    for (auto& voice : voices)
    {
        if (!voice.active)
            continue;
        const auto* channel = findChannel(state, voice.channelId);
        if (channel == nullptr || channel->sample.get() != voice.sample)
            voice.active = false;
    }
}

Sampler::Voice& Sampler::allocateVoice()
{
    for (auto& voice : voices)
        if (!voice.active)
            return voice;

    // All busy: steal the oldest.
    return *std::min_element(voices.begin(), voices.end(), [](const Voice& a, const Voice& b)
                             { return a.startedAt < b.startedAt; });
}

void Sampler::trigger(const EngineState& state, int channelIndex, int pitch, float velocity)
{
    if (channelIndex < 0 || channelIndex >= static_cast<int>(state.channels.size()))
        return;

    const auto& channel = state.channels[static_cast<std::size_t>(channelIndex)];
    if (!channel.sample || channel.sample->numFrames() == 0)
        return;

    auto& voice = allocateVoice();
    voice.active = true;
    voice.channelId = channel.id;
    voice.sample = channel.sample.get();
    voice.position = 0.0;
    voice.increment = channel.sample->sampleRate / sampleRate *
                      std::exp2((std::clamp(pitch, 0, 127) - 60) / 12.0);
    voice.gain = std::clamp(velocity, 0.0f, 1.0f) / (100.0f / 128.0f);
    voice.startedAt = ++triggerCounter;
}

void Sampler::render(const EngineState& state, float* const* buses, int numBuses, int from, int to)
{
    if (numBuses <= 0 || from >= to)
        return;

    for (auto& voice : voices)
    {
        if (!voice.active)
            continue;

        const auto* channel = findChannel(state, voice.channelId);
        if (channel == nullptr)
        {
            voice.active = false;
            continue;
        }

        const auto& sample = *voice.sample;
        const int lastFrame = sample.numFrames() - 1;
        const auto& left = sample.channels[0];
        const auto& right = sample.numChannels() > 1 ? sample.channels[1] : sample.channels[0];
        const float gainLeft = channel->muted ? 0.0f : channel->gainLeft * voice.gain;
        const float gainRight = channel->muted ? 0.0f : channel->gainRight * voice.gain;
        const int track = 2 * channel->mixerTrack < numBuses ? channel->mixerTrack : 0;
        float* outLeft = buses[2 * track];
        float* outRight = 2 * track + 1 < numBuses ? buses[2 * track + 1] : nullptr;

        for (int i = from; i < to; ++i)
        {
            if (voice.position >= lastFrame)
            {
                voice.active = false;
                break;
            }

            // Linear interpolation, also handles sample-rate conversion.
            const auto index = static_cast<std::size_t>(voice.position);
            const auto frac = static_cast<float>(voice.position - static_cast<double>(index));
            const float l = left[index] + frac * (left[index + 1] - left[index]);
            const float r = right[index] + frac * (right[index + 1] - right[index]);

            outLeft[i] += l * gainLeft;
            if (outRight != nullptr)
                outRight[i] += r * gainRight;

            voice.position += voice.increment;
        }
    }
}

int Sampler::activeVoiceCount() const
{
    return static_cast<int>(
        std::count_if(voices.begin(), voices.end(), [](const Voice& v) { return v.active; }));
}

} // namespace jls::engine
