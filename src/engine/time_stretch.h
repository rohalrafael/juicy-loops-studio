#pragma once

// Offline time-stretching and pitch-shifting of whole samples (Signalsmith Stretch), for the
// Sampler's Stretch mode. Slow: runs on a background thread, never on the audio thread.

#include "engine/sample_buffer.h"

#include <memory>

namespace jls::engine
{

// A copy of `sample` that lasts `timeFactor` times as long (2 = twice as long, half the speed)
// and is transposed by `semitones`, without the one changing the other. Returns null if the
// sample is too short to stretch (a few milliseconds) or the factor isn't positive.
std::shared_ptr<const SampleBuffer> stretchSample(const SampleBuffer& sample, double timeFactor,
                                                  double semitones);

} // namespace jls::engine
