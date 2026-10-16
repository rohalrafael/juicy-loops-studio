#include "io/audio_export.h"

namespace jls::io
{

juce::Result writeWav(const juce::File& file, const model::RenderedAudio& audio, int bitsPerSample)
{
    if (file.exists() && !file.deleteFile())
        return juce::Result::fail("Couldn't replace " + file.getFullPathName());

    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    if (stream == nullptr)
        return juce::Result::fail("Couldn't write to " + file.getFullPathName());

    juce::WavAudioFormat wav;
    auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions{}
                                                  .withSampleRate(audio.sampleRate)
                                                  .withNumChannels(2)
                                                  .withBitsPerSample(bitsPerSample));
    if (writer == nullptr)
        return juce::Result::fail("Couldn't create a WAV file with these settings.");

    const float* channels[] = {audio.left.data(), audio.right.data()};
    if (!writer->writeFromFloatArrays(channels, 2, static_cast<int>(audio.left.size())))
        return juce::Result::fail("Writing " + file.getFullPathName() + " failed.");
    return juce::Result::ok();
}

} // namespace jls::io
