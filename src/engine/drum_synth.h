#pragma once

// Synthesized built-in drum sounds (kick, clap, hat, snare), rendered into SampleBuffers so the
// app ships no sample files.

#include "engine/sample_buffer.h"

namespace jls::engine
{

// Procedurally generated default drum sounds, so the app ships without sample assets.
enum class DrumSound
{
    kick,
    clap,
    hat,
    snare,
};

SampleBuffer synthesizeDrum(DrumSound sound, double sampleRate);

} // namespace jls::engine
