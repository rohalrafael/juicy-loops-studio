#pragma once

#include "engine/sample_buffer.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <memory>

namespace jls::io
{

// Decodes audio files (WAV, AIFF, FLAC, Ogg Vorbis, MP3 where supported) into memory.
// UI thread; loads synchronously, which is fine for one-shot samples.
class SampleLoader
{
  public:
    static constexpr double maxSeconds = 120.0;

    SampleLoader();

    bool canLoad(const juce::File& file) const;
    // Returns nullptr if the file can't be read. Keeps at most the first two channels.
    std::shared_ptr<const engine::SampleBuffer> load(const juce::File& file);

  private:
    juce::AudioFormatManager formats;
};

} // namespace jls::io
