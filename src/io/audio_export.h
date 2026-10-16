#pragma once

// Writing rendered audio to files (JUCE). The rendering itself is model::renderProject().

#include "model/render.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace jls::io
{

// Writes stereo audio as a WAV file (replacing any existing file). bitsPerSample: 16, 24 or 32.
juce::Result writeWav(const juce::File& file, const model::RenderedAudio& audio,
                      int bitsPerSample = 24);

} // namespace jls::io
