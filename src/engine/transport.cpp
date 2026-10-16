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
        position.store(0.0);

    BlockPosition block;
    block.startBeat = position.load();
    block.endBeat = block.startBeat;
    block.beatsPerBar = barLength.load();
    block.playing = playing.load() && numSamples > 0 && sampleRate > 0.0;

    if (!block.playing)
        return block;

    block.endBeat += numSamples / sampleRate * (bpm.load() / 60.0);
    position.store(block.endBeat);
    return block;
}

} // namespace jls::engine
