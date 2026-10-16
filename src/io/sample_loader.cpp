#include "io/sample_loader.h"

namespace jls::io
{

SampleLoader::SampleLoader()
{
    formats.registerBasicFormats();
}

bool SampleLoader::canLoad(const juce::File& file) const
{
    return file.existsAsFile() &&
           formats.findFormatForFileExtension(file.getFileExtension()) != nullptr;
}

std::shared_ptr<const engine::SampleBuffer> SampleLoader::load(const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
        return nullptr;

    const auto frames = static_cast<int>(std::min<juce::int64>(
        reader->lengthInSamples, static_cast<juce::int64>(maxSeconds * reader->sampleRate)));
    const int channels = std::min(2, static_cast<int>(reader->numChannels));

    juce::AudioBuffer<float> audio(channels, frames);
    if (!reader->read(&audio, 0, frames, 0, true, channels > 1))
        return nullptr;

    auto buffer = std::make_shared<engine::SampleBuffer>();
    buffer->sampleRate = reader->sampleRate;
    for (int ch = 0; ch < channels; ++ch)
        buffer->channels.emplace_back(audio.getReadPointer(ch), audio.getReadPointer(ch) + frames);
    return buffer;
}

} // namespace jls::io
