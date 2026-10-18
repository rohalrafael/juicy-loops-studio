// Voices are preallocated; triggering takes a free voice or steals the oldest. Playback reads the
// sample at (sample rate ratio * 2^(semitones/12) * speed) with 4-point cubic (Hermite)
// interpolation, which handles both sample-rate conversion and pitch with much less grit than
// linear. The envelope is a per-voice DAHDSR state machine stepped every sample.

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

void Sampler::cutFor(const EngineState& state, const ChannelState& channel)
{
    for (auto& voice : voices)
    {
        if (!voice.active || voice.remaining >= 0 || voice.cutFade >= 0)
            continue; // audio clips aren't cut; already fading
        bool cut = channel.cutItself && voice.channelId == channel.id;
        if (!cut && channel.cutGroup != 0)
            if (const auto* other = findChannel(state, voice.channelId))
                cut = other->cutBy == channel.cutGroup;
        if (cut)
            voice.cutFade = cutFadeSamples;
    }
}

void Sampler::trigger(const EngineState& state, int channelIndex, int pitch, float velocity,
                      double noteSeconds, bool live)
{
    if (channelIndex < 0 || channelIndex >= static_cast<int>(state.channels.size()))
        return;

    const auto& channel = state.channels[static_cast<std::size_t>(channelIndex)];
    if (!channel.sample || channel.sample->numFrames() == 0)
        return;

    cutFor(state, channel);

    auto& voice = allocateVoice();
    voice.active = true;
    voice.channelId = channel.id;
    voice.sample = channel.sample.get();
    voice.position = 0.0;
    voice.increment = channel.sample->sampleRate / sampleRate *
                      std::exp2((std::clamp(pitch, 0, 127) - 60) / 12.0) * channel.speed;
    voice.gain = std::clamp(velocity, 0.0f, 1.0f) / (100.0f / 128.0f);
    voice.startedAt = ++triggerCounter;
    voice.remaining = -1;
    voice.pitch = pitch;
    voice.live = live;
    voice.cutFade = -1;
    voice.noteLeft = noteSeconds >= 0.0 ? std::llround(noteSeconds * sampleRate) : -1;
    voice.envelope = channel.envelope;
    voice.level = 0.0f;
    enterStage(voice, Voice::Stage::delay);
}

void Sampler::releaseLive(int pitch)
{
    for (auto& voice : voices)
        if (voice.active && voice.live && (pitch < 0 || voice.pitch == pitch))
        {
            voice.live = false;
            voice.noteLeft = 0; // releases on the next sample
        }
}

void Sampler::enterStage(Voice& voice, Voice::Stage stage) const
{
    const auto& e = voice.envelope;
    const auto samples = [this](float seconds)
    { return std::max(0LL, std::llround(static_cast<double>(seconds) * sampleRate)); };
    voice.stage = stage;
    switch (stage)
    {
    case Voice::Stage::delay:
        voice.stageLeft = samples(e.delay);
        break;
    case Voice::Stage::attack:
        voice.stageLeft = samples(e.attack);
        break;
    case Voice::Stage::hold:
        voice.stageLeft = samples(e.hold);
        break;
    case Voice::Stage::decay:
        voice.stageLeft = samples(e.decay);
        break;
    case Voice::Stage::release:
        voice.stageLeft = std::max(1LL, samples(e.release));
        voice.releaseStep = voice.level / static_cast<float>(voice.stageLeft);
        break;
    case Voice::Stage::sustain:
    case Voice::Stage::done:
        voice.stageLeft = 0;
        break;
    }
}

float Sampler::nextEnvelopeLevel(Voice& voice) const
{
    const auto& e = voice.envelope;
    // The note ended: release from wherever the envelope is.
    if (voice.noteLeft == 0 && voice.stage != Voice::Stage::release &&
        voice.stage != Voice::Stage::done)
        enterStage(voice, Voice::Stage::release);
    if (voice.noteLeft > 0)
        --voice.noteLeft;

    // Zero-length stages pass straight through.
    for (int guard = 0; guard < 6; ++guard)
    {
        switch (voice.stage)
        {
        case Voice::Stage::delay:
            if (voice.stageLeft > 0)
            {
                --voice.stageLeft;
                return 0.0f;
            }
            enterStage(voice, Voice::Stage::attack);
            continue;
        case Voice::Stage::attack:
            if (voice.stageLeft > 0)
            {
                voice.level += (1.0f - voice.level) / static_cast<float>(voice.stageLeft--);
                return voice.level;
            }
            voice.level = 1.0f;
            enterStage(voice, Voice::Stage::hold);
            continue;
        case Voice::Stage::hold:
            if (voice.stageLeft > 0)
            {
                --voice.stageLeft;
                return voice.level = 1.0f;
            }
            enterStage(voice, Voice::Stage::decay);
            continue;
        case Voice::Stage::decay:
            if (voice.stageLeft > 0)
            {
                voice.level += (e.sustain - voice.level) / static_cast<float>(voice.stageLeft--);
                return voice.level;
            }
            voice.level = e.sustain;
            enterStage(voice, e.sustain > 0.0f ? Voice::Stage::sustain : Voice::Stage::done);
            continue;
        case Voice::Stage::sustain:
            return voice.level = e.sustain;
        case Voice::Stage::release:
            if (voice.stageLeft > 0)
            {
                --voice.stageLeft;
                voice.level = std::max(0.0f, voice.level - voice.releaseStep);
                return voice.level;
            }
            enterStage(voice, Voice::Stage::done);
            continue;
        case Voice::Stage::done:
            return 0.0f;
        }
    }
    return voice.level;
}

void Sampler::triggerClip(const EngineState& state, int channelIndex, double startSeconds,
                          double lengthSeconds)
{
    if (channelIndex < 0 || channelIndex >= static_cast<int>(state.channels.size()))
        return;
    const auto& channel = state.channels[static_cast<std::size_t>(channelIndex)];
    if (!channel.sample || channel.sample->numFrames() == 0)
        return;
    // Song seconds into the clip -> frames into the sample, at the channel's playback speed.
    const double startFrame =
        std::max(0.0, startSeconds) * channel.sample->sampleRate * channel.speed;
    if (startFrame >= channel.sample->numFrames() - 1)
        return; // the clip is longer than its sample and we're past the sound

    auto& voice = allocateVoice();
    voice.active = true;
    voice.channelId = channel.id;
    voice.sample = channel.sample.get();
    voice.position = startFrame;
    voice.increment = channel.sample->sampleRate / sampleRate * channel.speed;
    voice.gain = 1.0f;
    voice.startedAt = ++triggerCounter;
    voice.remaining =
        std::max(1LL, static_cast<long long>(std::llround(lengthSeconds * sampleRate)));
}

void Sampler::stopClips()
{
    for (auto& voice : voices)
        if (voice.active && voice.remaining >= 0)
            voice.remaining = std::min(voice.remaining, clipFadeSamples);
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
            if (voice.position >= lastFrame || voice.remaining == 0 || voice.cutFade == 0)
            {
                voice.active = false;
                break;
            }
            // Audio clips fade out over their last few samples instead of clicking.
            float fade = 1.0f;
            if (voice.remaining > 0)
            {
                fade = std::min(1.0f, static_cast<float>(voice.remaining) / clipFadeSamples);
                --voice.remaining;
            }
            // So do cut notes, over a shorter time.
            if (voice.cutFade > 0)
                fade *= static_cast<float>(voice.cutFade--) / cutFadeSamples;
            if (voice.envelope.on)
            {
                fade *= nextEnvelopeLevel(voice);
                if (voice.stage == Voice::Stage::done)
                {
                    voice.active = false;
                    break;
                }
            }

            // 4-point cubic (Hermite) interpolation, also handles sample-rate conversion.
            const auto index = static_cast<int>(voice.position);
            const auto frac = static_cast<float>(voice.position - static_cast<double>(index));
            const auto at = [lastFrame](const std::vector<float>& data, int k)
            { return data[static_cast<std::size_t>(std::clamp(k, 0, lastFrame))]; };
            const auto hermite = [&](const std::vector<float>& data)
            {
                const float y0 = at(data, index - 1);
                const float y1 = at(data, index);
                const float y2 = at(data, index + 1);
                const float y3 = at(data, index + 2);
                const float c1 = 0.5f * (y2 - y0);
                const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
                const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
                return ((c3 * frac + c2) * frac + c1) * frac + y1;
            };
            const float l = hermite(left);
            const float r = hermite(right);

            outLeft[i] += l * gainLeft * fade;
            if (outRight != nullptr)
                outRight[i] += r * gainRight * fade;

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
