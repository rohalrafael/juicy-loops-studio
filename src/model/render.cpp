#include "model/render.h"

#include "engine/effect_pool.h"
#include "engine/engine.h"

#include <cmath>

namespace jls::model
{
namespace
{

constexpr int defaultBlockSize = 512;

void resetProcessors(const engine::EngineState& state)
{
    for (const auto& track : state.tracks)
        for (const auto& effect : track.effects)
            if (effect.processor)
                effect.processor->reset();
    for (const auto& channel : state.channels)
        if (channel.instrument)
            channel.instrument->reset();
}

bool rendersSong(const Project& project)
{
    return !project.playlist.clips.empty();
}

} // namespace

double renderLengthSeconds(const Project& project)
{
    const int steps = rendersSong(project) ? project.songLengthInSteps()
                                           : project.lengthInSteps(project.current());
    const double beats = static_cast<double>(steps) / stepsPerBeat;
    return beats * 60.0 / project.tempo;
}

RenderedAudio renderProject(const Project& project, double sampleRate, double tailSeconds,
                            const engine::EffectPool::PluginFactory& plugins,
                            const engine::EffectPool::InstrumentFactory& instruments)
{
    engine::EffectPool pool;
    pool.setFormat(sampleRate, defaultBlockSize);
    pool.setPluginFactory(plugins);
    pool.setInstrumentFactory(instruments);
    return renderProject(project, sampleRate, tailSeconds, pool, defaultBlockSize);
}

RenderedAudio renderProject(const Project& project, double sampleRate, double tailSeconds,
                            engine::EffectPool& pool, int blockSize)
{
    RenderedAudio audio;
    audio.sampleRate = sampleRate;
    const auto total = static_cast<std::size_t>(
        std::ceil((renderLengthSeconds(project) + std::max(0.0, tailSeconds)) * sampleRate));
    audio.left.assign(total, 0.0f);
    audio.right.assign(total, 0.0f);

    // A private engine: same code path as live playback, driven block by block.
    Project copy = project;
    copy.songMode = rendersSong(project);

    auto engine = std::make_unique<engine::Engine>();
    engine->prepare(sampleRate, blockSize);
    engine->transport().setTempo(copy.tempo);
    engine->transport().setBeatsPerBar(copy.beatsPerBar);
    auto state = toEngineState(copy, &pool);
    resetProcessors(*state);
    const auto keep = *state; // to reset the processors again at the end
    engine->setState(std::move(state));

    // Play only the song/pattern once: stop the transport at the end, then let tails ring.
    const auto playSamples =
        static_cast<std::size_t>(std::llround(renderLengthSeconds(project) * sampleRate));
    const auto block = static_cast<std::size_t>(blockSize);
    engine->transport().play();
    for (std::size_t start = 0; start < total; start += block)
    {
        if (start >= playSamples && engine->transport().isPlaying())
            engine->transport().pause();
        const auto n = std::min<std::size_t>(block, total - start);
        // Don't let a block run past the end of the song (it would trigger the loop start).
        const auto chunk = start < playSamples ? std::min(n, playSamples - start) : n;
        float* out[] = {audio.left.data() + start, audio.right.data() + start};
        engine->process(out, 2, static_cast<int>(chunk));
        if (chunk < n)
        {
            engine->transport().pause();
            float* rest[] = {out[0] + chunk, out[1] + chunk};
            engine->process(rest, 2, static_cast<int>(n - chunk));
        }
    }
    resetProcessors(keep);
    return audio;
}

} // namespace jls::model
