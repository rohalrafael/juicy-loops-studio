// For every note, its occurrences k * patternLength + startTick that fall inside the block
// are found arithmetically (no per-tick loop). Song mode first splits the block at the song's loop
// point, then asks each clip under the playhead for its pattern's notes in clip-local time.

#include "engine/step_sequencer.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace jls::engine
{
namespace
{

class Collector
{
  public:
    Collector(const EngineState& s, double blockStartTick, double blockEndTick, int samples,
              std::span<Trigger> o)
        : state(s), startTick(blockStartTick), span(blockEndTick - blockStartTick),
          numSamples(samples), out(o)
    {
    }

    // Emits every note of `pattern` whose (looping) pattern-time lies in [from, to).
    // A note at pattern time x happens at block time x + offset. Notes end by the end of their
    // pattern repeat and by `limit` (block time), e.g. the end of their clip.
    void emit(const PatternState& pattern, double from, double to, double offset,
              double limit = std::numeric_limits<double>::infinity())
    {
        const double length = pattern.lengthTicks;
        if (length <= 0.0)
            return;

        const auto numChannels = std::min(state.channels.size(), pattern.notes.size());
        for (std::size_t ch = 0; ch < numChannels; ++ch)
        {
            if (state.channels[ch].muted)
                continue;
            for (const auto& note : pattern.notes[ch])
            {
                // Occurrences note.startTick + k * length that fall in [from, to).
                for (double k = std::ceil((from - note.startTick) / length);; k += 1.0)
                {
                    const double x = note.startTick + k * length;
                    if (x >= to)
                        break;
                    const double end =
                        std::min({x + note.lengthTicks, (k + 1.0) * length, limit - offset});
                    add(static_cast<int>(ch), x + offset, end + offset, note);
                }
            }
        }
    }

    // An audio clip starting at `tick` (block time), lasting `length` ticks.
    void addClip(int channel, double tick, double length)
    {
        if (count == static_cast<int>(out.size()))
            return;
        const double fraction = (tick - startTick) / span;
        Trigger trigger;
        trigger.channelIndex = channel;
        trigger.sampleOffset =
            std::clamp(static_cast<int>(std::lround(fraction * numSamples)), 0, numSamples - 1);
        trigger.velocity = 100.0f / 128.0f; // unity
        trigger.endTick = tick + length;
        trigger.audioClip = true;
        trigger.lengthTicks = length;
        out[static_cast<std::size_t>(count++)] = trigger;
    }

    int finish()
    {
        std::sort(out.begin(), out.begin() + count, [](const Trigger& a, const Trigger& b)
                  { return a.sampleOffset < b.sampleOffset; });
        return count;
    }

  private:
    void add(int channel, double tick, double endTick, const NoteState& note)
    {
        if (count == static_cast<int>(out.size()))
            return;
        const double fraction = (tick - startTick) / span;
        const int offset =
            std::clamp(static_cast<int>(std::lround(fraction * numSamples)), 0, numSamples - 1);
        out[static_cast<std::size_t>(count++)] = {channel, offset, note.pitch, note.velocity,
                                                  endTick};
    }

    const EngineState& state;
    double startTick;
    double span;
    int numSamples;
    std::span<Trigger> out;
    int count = 0;
};

} // namespace

int collectTriggers(const EngineState& state, const BlockPosition& block, int numSamples,
                    std::span<Trigger> out)
{
    if (!block.playing || numSamples <= 0 || block.endBeat <= block.startBeat)
        return 0;

    const double t0 = block.startBeat * ticksPerBeat;
    const double t1 = block.endBeat * ticksPerBeat;
    Collector collector(state, t0, t1, numSamples, out);

    if (!state.songMode)
    {
        if (state.currentPattern >= 0 &&
            state.currentPattern < static_cast<int>(state.patterns.size()))
            collector.emit(state.patterns[static_cast<std::size_t>(state.currentPattern)], t0, t1,
                           0.0);
        return collector.finish();
    }

    const double songLength = state.songLengthTicks;
    if (songLength <= 0.0)
        return 0;

    // Split the block where the song loops, then play the clips in each piece.
    for (double from = t0; from < t1;)
    {
        const double loopStart = std::floor(from / songLength) * songLength;
        const double to = std::min(t1, loopStart + songLength);
        for (const auto& clip : state.clips)
        {
            if (clip.audioChannel >= 0)
            {
                const double clipStart = loopStart + clip.startTick;
                if (clipStart >= from && clipStart < to &&
                    clip.audioChannel < static_cast<int>(state.channels.size()) &&
                    !state.channels[static_cast<std::size_t>(clip.audioChannel)].muted)
                    collector.addClip(clip.audioChannel, clipStart, clip.lengthTicks);
                continue;
            }
            if (clip.patternIndex < 0 ||
                clip.patternIndex >= static_cast<int>(state.patterns.size()))
                continue;
            const double clipStart = loopStart + clip.startTick;
            const double a = std::max(from, clipStart);
            const double b = std::min(to, clipStart + clip.lengthTicks);
            if (a < b)
                collector.emit(state.patterns[static_cast<std::size_t>(clip.patternIndex)],
                               a - clipStart, b - clipStart, clipStart,
                               clipStart + clip.lengthTicks);
        }
        from = to;
    }
    return collector.finish();
}

int collectClipsUnderPlayhead(const EngineState& state, double tick, std::span<Trigger> out)
{
    const double songLength = state.songLengthTicks;
    if (!state.songMode || songLength <= 0.0)
        return 0;
    const double inSong = std::fmod(std::max(0.0, tick), songLength);
    int count = 0;
    for (const auto& clip : state.clips)
    {
        if (clip.audioChannel < 0 || clip.audioChannel >= static_cast<int>(state.channels.size()) ||
            state.channels[static_cast<std::size_t>(clip.audioChannel)].muted)
            continue;
        const double offset = inSong - clip.startTick;
        if (offset <= 0.0 || offset >= clip.lengthTicks || count == static_cast<int>(out.size()))
            continue;
        Trigger trigger;
        trigger.channelIndex = clip.audioChannel;
        trigger.velocity = 100.0f / 128.0f;
        trigger.audioClip = true;
        trigger.clipOffsetTicks = offset;
        trigger.lengthTicks = clip.lengthTicks - offset;
        trigger.endTick = tick + trigger.lengthTicks;
        out[static_cast<std::size_t>(count++)] = trigger;
    }
    return count;
}

} // namespace jls::engine
