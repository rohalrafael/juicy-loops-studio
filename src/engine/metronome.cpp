#include "engine/metronome.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace jls::engine
{

void Metronome::prepare(double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    samplesLeft = 0;
}

void Metronome::setEnabled(bool shouldBeEnabled)
{
    enabled.store(shouldBeEnabled);
}

bool Metronome::isEnabled() const
{
    return enabled.load();
}

void Metronome::toggleEnabled()
{
    enabled.store(!enabled.load());
}

void Metronome::setVolume(float newGain)
{
    gain.store(std::clamp(newGain, 0.0f, 1.0f));
}

float Metronome::volume() const
{
    return gain.load();
}

void Metronome::render(float* const* channels, int numChannels, int numSamples,
                       const BlockPosition& block)
{
    if (numSamples <= 0)
        return;

    int cursor = 0;

    if (block.playing && enabled.load() && block.endBeat > block.startBeat)
    {
        const double span = block.endBeat - block.startBeat;

        // Every whole beat n with startBeat <= n < endBeat starts a click in this block.
        for (auto beat = static_cast<long long>(std::ceil(block.startBeat));
             static_cast<double>(beat) < block.endBeat; ++beat)
        {
            const double fraction = (static_cast<double>(beat) - block.startBeat) / span;
            const int offset =
                std::clamp(static_cast<int>(fraction * numSamples), 0, numSamples - 1);

            renderClick(channels, numChannels, cursor, offset);
            startClick(block.beatsPerBar > 0 && beat % block.beatsPerBar == 0);
            cursor = offset;
        }
    }

    // Finish whatever click is sounding, even after playback stops.
    renderClick(channels, numChannels, cursor, numSamples);
}

void Metronome::startClick(bool accent)
{
    const double frequency = accent ? accentFrequency : beatFrequency;
    const int length = static_cast<int>(clickSeconds * sampleRate);

    phase = 0.0;
    phaseIncrement = 2.0 * std::numbers::pi * frequency / sampleRate;
    envelope = gain.load() * (accent ? 1.0f : 0.7f);
    // Exponential decay to about -60 dB over the click.
    decay = static_cast<float>(std::pow(0.001, 1.0 / std::max(length, 1)));
    samplesLeft = length;
}

void Metronome::renderClick(float* const* channels, int numChannels, int from, int to)
{
    for (int i = from; i < to && samplesLeft > 0; ++i, --samplesLeft)
    {
        const float sample = envelope * static_cast<float>(std::sin(phase));
        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][i] += sample;

        phase += phaseIncrement;
        envelope *= decay;
    }
}

} // namespace jls::engine
