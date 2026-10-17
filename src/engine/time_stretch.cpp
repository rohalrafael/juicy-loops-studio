#include "engine/time_stretch.h"

#include <signalsmith-stretch/signalsmith-stretch.h>

#include <cmath>

namespace jls::engine
{

std::shared_ptr<const SampleBuffer> stretchSample(const SampleBuffer& sample, double timeFactor,
                                                  double semitones)
{
    const int channels = sample.numChannels();
    const int inputFrames = sample.numFrames();
    if (channels == 0 || inputFrames == 0 || !(timeFactor > 0.0))
        return nullptr;
    const int outputFrames = static_cast<int>(std::lround(inputFrames * timeFactor));
    if (outputFrames <= 0)
        return nullptr;

    signalsmith::stretch::SignalsmithStretch<float> stretch;
    stretch.presetDefault(channels, static_cast<float>(sample.sampleRate));
    stretch.setTransposeSemitones(static_cast<float>(semitones));

    auto result = std::make_shared<SampleBuffer>();
    result->sampleRate = sample.sampleRate;
    result->channels.assign(static_cast<std::size_t>(channels),
                            std::vector<float>(static_cast<std::size_t>(outputFrames)));

    std::vector<const float*> inputs;
    std::vector<float*> outputs;
    for (int c = 0; c < channels; ++c)
    {
        inputs.push_back(sample.channels[static_cast<std::size_t>(c)].data());
        outputs.push_back(result->channels[static_cast<std::size_t>(c)].data());
    }
    if (!stretch.exact(inputs.data(), inputFrames, outputs.data(), outputFrames))
        return nullptr; // too short to stretch
    return result;
}

} // namespace jls::engine
