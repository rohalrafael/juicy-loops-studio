#pragma once

// Waveform overviews: the lowest and highest sample value per bucket, computed once per sample
// so long files still draw quickly (Browser preview, audio clips in the Playlist).

#include "engine/sample_buffer.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace jls::ui
{

using Peaks = std::vector<std::pair<float, float>>; // (min, max) per bucket, across channels

inline Peaks computePeaks(const engine::SampleBuffer& sample, int buckets = 1024)
{
    Peaks peaks;
    const long long frames = sample.numFrames();
    if (frames <= 0 || buckets <= 0)
        return peaks;
    peaks.resize(static_cast<std::size_t>(buckets));
    for (int b = 0; b < buckets; ++b)
    {
        const long long from = frames * b / buckets;
        const long long to = std::max(from + 1, frames * (b + 1) / buckets);
        float low = 0.0f, high = 0.0f;
        for (const auto& channel : sample.channels)
            for (long long i = from; i < to && i < frames; ++i)
            {
                low = std::min(low, channel[static_cast<std::size_t>(i)]);
                high = std::max(high, channel[static_cast<std::size_t>(i)]);
            }
        peaks[static_cast<std::size_t>(b)] = {low, high};
    }
    return peaks;
}

} // namespace jls::ui
