#pragma once

// Decoded audio held in memory, shared (as shared_ptr<const>) by the model, undo history and
// engine snapshots.

#include <vector>

namespace jls::engine
{

// Decoded audio held in memory: one vector per channel (mono or stereo).
struct SampleBuffer
{
    std::vector<std::vector<float>> channels;
    double sampleRate = 44100.0;

    int numChannels() const { return static_cast<int>(channels.size()); }
    int numFrames() const { return channels.empty() ? 0 : static_cast<int>(channels[0].size()); }
};

} // namespace jls::engine
