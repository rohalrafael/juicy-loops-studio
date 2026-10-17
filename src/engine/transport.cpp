// The playhead is anchorBeat + samplesSinceAnchor * bpm / (60 * sampleRate), re-anchored only when
// the tempo or sample rate changes. Summing per-block increments instead drifts (see the
// "Playhead doesn't drift" test).

#include "engine/transport.h"

#include <algorithm>

namespace jls::engine
{

void Transport::play()
{
    playing.store(true);
}

void Transport::pause()
{
    playing.store(false);
}

void Transport::seek(double beat)
{
    rewindRequested.store(false);
    seekTarget.store(std::max(0.0, beat));
}

void Transport::stop()
{
    playing.store(false);
    rewindRequested.store(true);
}

void Transport::togglePlay()
{
    if (isPlaying())
        stop();
    else
        play();
}

void Transport::togglePause()
{
    if (isPlaying())
        pause();
    else
        play();
}

bool Transport::isPlaying() const
{
    return playing.load();
}

void Transport::setTempo(double newBpm)
{
    bpm.store(std::clamp(newBpm, minTempo, maxTempo));
}

double Transport::tempo() const
{
    return bpm.load();
}

void Transport::setBeatsPerBar(int beats)
{
    barLength.store(std::clamp(beats, minBeatsPerBar, maxBeatsPerBar));
}

int Transport::beatsPerBar() const
{
    return barLength.load();
}

double Transport::positionInBeats() const
{
    return position.load();
}

BlockPosition Transport::advance(int numSamples, double sampleRate)
{
    if (rewindRequested.exchange(false))
    {
        seekTarget.store(-1.0);
        anchorBeat = 0.0;
        samplesSinceAnchor = 0;
        position.store(0.0);
    }
    if (const double target = seekTarget.exchange(-1.0); target >= 0.0)
    {
        anchorBeat = target;
        samplesSinceAnchor = 0;
        position.store(target);
    }

    BlockPosition block;
    block.startBeat = position.load();
    block.endBeat = block.startBeat;
    block.beatsPerBar = barLength.load();
    block.playing = playing.load() && numSamples > 0 && sampleRate > 0.0;

    if (!block.playing)
        return block;

    const double currentBpm = bpm.load();
    if (currentBpm != anchorBpm || sampleRate != anchorSampleRate)
    {
        anchorBeat = block.startBeat;
        samplesSinceAnchor = 0;
        anchorBpm = currentBpm;
        anchorSampleRate = sampleRate;
    }

    samplesSinceAnchor += numSamples;
    // Multiply before dividing: for typical rates/tempos this is exact at beat boundaries.
    block.endBeat =
        anchorBeat + static_cast<double>(samplesSinceAnchor) * currentBpm / (60.0 * sampleRate);
    position.store(block.endBeat);
    return block;
}

} // namespace jls::engine
