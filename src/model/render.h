#pragma once

// Offline rendering ("export"): plays a project through its own Engine as fast as possible and
// collects the output. Plain C++, independent of the live audio device.

#include "engine/effect_pool.h"
#include "model/project.h"

#include <vector>

namespace jls::model
{

struct RenderedAudio
{
    std::vector<float> left;
    std::vector<float> right;
    double sampleRate = 44100.0;
};

// Renders the song (song mode), or the current pattern once if the Playlist is empty, followed
// by `tailSeconds` so reverb and delay tails can ring out. The metronome is never included.
// Plugin effects are created with `plugins` (without it they pass audio through unchanged),
// plugin instruments with `instruments` (without it they are silent).
RenderedAudio renderProject(const Project& project, double sampleRate, double tailSeconds = 2.0,
                            const engine::EffectPool::PluginFactory& plugins = {},
                            const engine::EffectPool::InstrumentFactory& instruments = {});

// The same with existing processors: effects and instruments come from `pool` (e.g. the live
// one, so plugins don't have to be loaded again; some restore their state asynchronously).
// Rendering runs in blocks of `blockSize`, which must not exceed what the pool's processors
// were prepared for. All of the project's processors are reset before and after, so no tail or
// held voice leaks in or out. Nothing else may use them meanwhile (e.g. hold the audio lock).
RenderedAudio renderProject(const Project& project, double sampleRate, double tailSeconds,
                            engine::EffectPool& pool, int blockSize,
                            const StretchedSamples& stretched = {});

// Length of what renderProject() plays before the tail, in seconds.
double renderLengthSeconds(const Project& project);

} // namespace jls::model
