#include "engine/preview_player.h"

#include <algorithm>

namespace jls::engine
{

std::uint64_t PreviewPlayer::play(const SampleBuffer* s, double startFrame, double endFrame)
{
    const Command command{s, startFrame, endFrame, sent + 1};
    if (!commands.push(command))
        return 0;
    return ++sent;
}

std::uint64_t PreviewPlayer::stop()
{
    return play(nullptr, 0.0, 0.0);
}

void PreviewPlayer::prepare(double sampleRate)
{
    deviceRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

void PreviewPlayer::render(float* const* out, int numChannels, int numSamples)
{
    while (auto command = commands.pop())
    {
        sample = command->sample;
        cursor = command->start;
        end = sample != nullptr ? std::min(command->end, static_cast<double>(sample->numFrames()))
                                : 0.0;
        consumedCommand.store(command->number);
    }
    if (sample == nullptr || numChannels <= 0)
    {
        playing.store(false);
        return;
    }

    // Linear interpolation, which also converts the sample rate (as in the Sampler).
    const auto& left = sample->channels[0];
    const auto& right = sample->numChannels() > 1 ? sample->channels[1] : sample->channels[0];
    const double increment = sample->sampleRate / deviceRate;
    const double last = std::min(end, static_cast<double>(sample->numFrames() - 1));
    for (int i = 0; i < numSamples; ++i)
    {
        if (cursor >= last)
        {
            sample = nullptr;
            break;
        }
        const auto index = static_cast<std::size_t>(cursor);
        const auto frac = static_cast<float>(cursor - static_cast<double>(index));
        out[0][i] += gain * (left[index] + frac * (left[index + 1] - left[index]));
        if (numChannels > 1)
            out[1][i] += gain * (right[index] + frac * (right[index + 1] - right[index]));
        cursor += increment;
    }
    playing.store(sample != nullptr);
    positionFrames.store(cursor);
}

} // namespace jls::engine
