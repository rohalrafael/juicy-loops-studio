#pragma once

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
