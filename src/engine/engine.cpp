// Audio-thread processing order per block (see docs/architecture.md, "Audio callback"):
//   1. adopt the newest snapshot (old one goes back to the UI thread for deletion)
//   2. advance the transport, collect note triggers for the block
//   3. render sampler voices into their mixer-track buses, split at trigger offsets; turn
//      triggers on instrument channels (and the ends of held notes) into note events
//   4. render instruments with their events, add them into their mixer-track buses
//   5. run each insert's effects + fader/pan + meter, sum into the Master
//   6. Master effects + fader/pan + meter, add to the output; metronome and Browser preview
//      on top

#include "engine/engine.h"

#include <algorithm>
#include <cmath>

namespace jls::engine
{

Engine::~Engine()
{
    collectGarbage();
    while (auto state = incoming.pop())
        delete *state;
    delete current;
}

void Engine::setState(std::unique_ptr<EngineState> state)
{
    pending = std::move(state);
    collectGarbage();
}

void Engine::collectGarbage()
{
    while (auto state = retired.pop())
        delete *state;

    if (pending && incoming.push(pending.get()))
        pending.release();
}

void Engine::prepare(double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;
    maxBlock = std::max(64, maxBlockSize);
    sampler.prepare(newSampleRate);
    metronomeState.prepare(newSampleRate);
    previewPlayer.prepare(newSampleRate);

    busMemory.assign(static_cast<std::size_t>(maxMixerTracks * 2 * maxBlock), 0.0f);
    for (std::size_t i = 0; i < buses.size(); ++i)
        buses[i] = busMemory.data() + i * static_cast<std::size_t>(maxBlock);
    instrumentMemory.assign(static_cast<std::size_t>(2 * maxBlock), 0.0f);
    heldCount = 0;
    liveCount = 0;
    sustain = {};
    wasPlaying = false;

    if (current != nullptr)
    {
        for (auto& track : current->tracks)
            for (auto& effect : track.effects)
                if (effect.processor)
                    effect.processor->prepare(newSampleRate, maxBlock);
        for (auto& channel : current->channels)
            if (channel.instrument)
                channel.instrument->prepare(newSampleRate, maxBlock);
    }

    preparedRate.store(newSampleRate);
    preparedBlock.store(maxBlock);
}

float Engine::takePeak(int track, int side)
{
    if (track < 0 || track >= maxMixerTracks || side < 0 || side > 1)
        return 0.0f;
    return peaks[static_cast<std::size_t>(track * 2 + side)].exchange(0.0f);
}

void Engine::adoptNewestState()
{
    // Only take a new state if the old one can be handed back for deletion.
    while (retired.hasSpace())
    {
        auto next = incoming.pop();
        if (!next)
            return;

        // Stop voices before the old state (which may own their samples) leaves this thread.
        sampler.stateChanged(**next);
        if (current != nullptr)
            retired.push(current);
        current = *next;

        // Held notes follow their channel to its new index. A removed channel or replaced
        // instrument takes its notes with it (the old instrument is no longer played).
        heldCount = remapHeld(heldNotes.data(), heldCount);
        liveCount = remapHeld(liveNotes.data(), liveCount);
        if (sustain.channelId != 0 && remapHeld(&sustain, 1) == 0)
            sustain = {};
    }
}

int Engine::remapHeld(HeldNote* notes, int count) const
{
    int kept = 0;
    for (int i = 0; i < count; ++i)
    {
        auto note = notes[i];
        const int index = indexOfChannel(note.channelId);
        if (index < 0 ||
            current->channels[static_cast<std::size_t>(index)].instrument.get() != note.instrument)
            continue;
        note.channelIndex = index;
        notes[kept++] = note;
    }
    return kept;
}

int Engine::indexOfChannel(std::uint32_t id) const
{
    if (current == nullptr)
        return -1;
    for (std::size_t i = 0; i < current->channels.size(); ++i)
        if (current->channels[i].id == id)
            return static_cast<int>(i);
    return -1;
}

bool Engine::sendLive(const LiveEvent& event)
{
    const std::scoped_lock lock(liveSenders);
    return liveEvents.push(event);
}

void Engine::releaseLiveNotes()
{
    for (int i = 0; i < liveCount; ++i)
    {
        const auto& note = liveNotes[static_cast<std::size_t>(i)];
        addNoteEvent(note.channelIndex, {NoteEvent::Kind::noteOff, 0, note.pitch});
    }
    liveCount = 0;
    if (sustain.channelId != 0)
        addNoteEvent(sustain.channelIndex, {NoteEvent::Kind::controller, 0, 64, 0.0f});
    sustain = {};
}

void Engine::applyLiveEvents()
{
    while (auto next = liveEvents.pop())
    {
        const auto& e = *next;
        switch (e.kind)
        {
        case LiveEvent::Kind::releaseAll:
            releaseLiveNotes();
            sampler.releaseLive(-1);
            break;

        case LiveEvent::Kind::noteOff:
            sampler.releaseLive(e.number);
            for (int i = 0; i < liveCount; ++i)
            {
                const auto& note = liveNotes[static_cast<std::size_t>(i)];
                if (note.pitch == e.number)
                {
                    addNoteEvent(note.channelIndex, {NoteEvent::Kind::noteOff, 0, note.pitch});
                    liveNotes[static_cast<std::size_t>(i)] =
                        liveNotes[static_cast<std::size_t>(--liveCount)];
                    break;
                }
            }
            break;

        case LiveEvent::Kind::noteOn:
        {
            const int index = indexOfChannel(e.channelId);
            if (index < 0)
                break;
            const int pitch = std::clamp(e.number, 0, 127);
            const auto& channel = current->channels[static_cast<std::size_t>(index)];
            if (!channel.instrument)
            {
                // A sample: its envelope (if on) releases at the key's note-off.
                sampler.trigger(*current, index, pitch, std::clamp(e.value, 0.0f, 1.0f), -1.0,
                                true);
                break;
            }
            // Same key again while held (e.g. a missed note-off): release it first.
            for (int i = 0; i < liveCount; ++i)
                if (liveNotes[static_cast<std::size_t>(i)].pitch == pitch)
                {
                    const auto old = liveNotes[static_cast<std::size_t>(i)];
                    addNoteEvent(old.channelIndex, {NoteEvent::Kind::noteOff, 0, pitch});
                    liveNotes[static_cast<std::size_t>(i)] =
                        liveNotes[static_cast<std::size_t>(--liveCount)];
                    break;
                }
            if (liveCount == maxLiveNotes)
                break; // can't remember it, so don't start it
            addNoteEvent(index,
                         {NoteEvent::Kind::noteOn, 0, pitch, std::clamp(e.value, 0.0f, 1.0f)});
            liveNotes[static_cast<std::size_t>(liveCount++)] = {
                channel.id, index, channel.instrument.get(), pitch, 0.0};
            break;
        }

        case LiveEvent::Kind::controller:
        case LiveEvent::Kind::pitchBend:
        {
            int index = indexOfChannel(e.channelId);
            const bool isSustain = e.kind == LiveEvent::Kind::controller && e.number == 64;
            if (isSustain && e.value < 0.5f)
            {
                // Lift the pedal where it went down, whatever is selected now.
                if (sustain.channelId == 0)
                    break;
                index = sustain.channelIndex;
                sustain = {};
            }
            if (!isInstrument(index))
                break;
            if (isSustain && e.value >= 0.5f)
            {
                if (sustain.channelId != 0 && sustain.channelIndex != index)
                    addNoteEvent(sustain.channelIndex, {NoteEvent::Kind::controller, 0, 64, 0.0f});
                const auto& channel = current->channels[static_cast<std::size_t>(index)];
                sustain = {channel.id, index, channel.instrument.get(), 64, 0.0};
            }
            if (e.kind == LiveEvent::Kind::pitchBend)
                addNoteEvent(index,
                             {NoteEvent::Kind::pitchBend, 0, 0, std::clamp(e.value, -1.0f, 1.0f)});
            else
                addNoteEvent(index, {NoteEvent::Kind::controller, 0, std::clamp(e.number, 0, 127),
                                     std::clamp(e.value, 0.0f, 1.0f)});
            break;
        }
        }
    }
}

bool Engine::isInstrument(int channelIndex) const
{
    return current != nullptr && channelIndex >= 0 &&
           channelIndex < static_cast<int>(current->channels.size()) &&
           current->channels[static_cast<std::size_t>(channelIndex)].instrument != nullptr;
}

void Engine::addNoteEvent(int channelIndex, NoteEvent event)
{
    if (noteEventCount < maxNoteEvents)
    {
        noteEvents[static_cast<std::size_t>(noteEventCount)] = {channelIndex, noteEventCount,
                                                                event};
        ++noteEventCount;
    }
}

void Engine::releaseAllNotes()
{
    for (int i = 0; i < heldCount; ++i)
    {
        const auto& note = heldNotes[static_cast<std::size_t>(i)];
        addNoteEvent(note.channelIndex, {NoteEvent::Kind::noteOff, 0, note.pitch});
    }
    heldCount = 0;
    // The all-notes-off below silences live notes (and lifts the pedal) too.
    liveCount = 0;
    sustain = {};
    if (current != nullptr)
        for (int ch = 0; ch < static_cast<int>(current->channels.size()); ++ch)
            if (isInstrument(ch))
                addNoteEvent(ch, {NoteEvent::Kind::allNotesOff, 0});
}

void Engine::startInstrumentNote(const Trigger& trigger, int offsetOfEnd)
{
    const auto& channel = current->channels[static_cast<std::size_t>(trigger.channelIndex)];

    // The same key again while it's still held: end the old note first (MIDI can't stack them).
    for (int i = 0; i < heldCount; ++i)
    {
        const auto& note = heldNotes[static_cast<std::size_t>(i)];
        if (note.channelIndex == trigger.channelIndex && note.pitch == trigger.pitch)
        {
            addNoteEvent(trigger.channelIndex,
                         {NoteEvent::Kind::noteOff, trigger.sampleOffset, trigger.pitch});
            heldNotes[static_cast<std::size_t>(i)] =
                heldNotes[static_cast<std::size_t>(--heldCount)];
            break;
        }
    }
    // A note-off already queued for a later point in this block belongs to the old note; it
    // must not cut the new one, so move it to the new note's start.
    for (int i = 0; i < noteEventCount; ++i)
    {
        auto& e = noteEvents[static_cast<std::size_t>(i)];
        if (e.channelIndex == trigger.channelIndex && e.event.kind == NoteEvent::Kind::noteOff &&
            e.event.pitch == trigger.pitch && e.event.sampleOffset > trigger.sampleOffset)
            e.event.sampleOffset = trigger.sampleOffset;
    }

    addNoteEvent(trigger.channelIndex,
                 {NoteEvent::Kind::noteOn, trigger.sampleOffset, trigger.pitch, trigger.velocity});

    if (offsetOfEnd >= 0) // ends within this block
    {
        addNoteEvent(
            trigger.channelIndex,
            {NoteEvent::Kind::noteOff, std::max(offsetOfEnd, trigger.sampleOffset), trigger.pitch});
    }
    else if (heldCount < maxHeldNotes)
    {
        heldNotes[static_cast<std::size_t>(heldCount++)] = {channel.id, trigger.channelIndex,
                                                            channel.instrument.get(), trigger.pitch,
                                                            trigger.endTick};
    }
    else // no room to remember it: end it right away rather than risk a hanging note
    {
        addNoteEvent(trigger.channelIndex,
                     {NoteEvent::Kind::noteOff, trigger.sampleOffset, trigger.pitch});
    }
}

void Engine::renderInstruments(int numTracks, int numSamples)
{
    // Group the events by channel, in time order (emission order breaks ties: a note-off that
    // was queued before a note-on at the same offset stays before it).
    std::sort(noteEvents.begin(), noteEvents.begin() + noteEventCount,
              [](const ChannelEvent& a, const ChannelEvent& b)
              {
                  if (a.channelIndex != b.channelIndex)
                      return a.channelIndex < b.channelIndex;
                  if (a.event.sampleOffset != b.event.sampleOffset)
                      return a.event.sampleOffset < b.event.sampleOffset;
                  return a.order < b.order;
              });

    // Walk the sorted array, copying each channel's events into one contiguous list.
    std::array<NoteEvent, 256> channelEvents{};
    float* left = instrumentMemory.data();
    float* right = left + maxBlock;
    int e = 0;
    for (int ch = 0; ch < static_cast<int>(current->channels.size()); ++ch)
    {
        while (e < noteEventCount && noteEvents[static_cast<std::size_t>(e)].channelIndex < ch)
            ++e;
        std::size_t count = 0;
        for (; e < noteEventCount && noteEvents[static_cast<std::size_t>(e)].channelIndex == ch;
             ++e)
            if (count < channelEvents.size())
                channelEvents[count++] = noteEvents[static_cast<std::size_t>(e)].event;

        const auto& channel = current->channels[static_cast<std::size_t>(ch)];
        if (!channel.instrument)
            continue;
        // Instruments run every block, with or without events, so releases and tails continue.
        channel.instrument->setPlayPosition(playPosition);
        channel.instrument->process({channelEvents.data(), count}, left, right, numSamples);

        const float gainLeft = channel.muted ? 0.0f : channel.gainLeft;
        const float gainRight = channel.muted ? 0.0f : channel.gainRight;
        const int track = channel.mixerTrack < numTracks ? channel.mixerTrack : 0;
        float* outLeft = buses[static_cast<std::size_t>(2 * track)];
        float* outRight = buses[static_cast<std::size_t>(2 * track + 1)];
        for (int i = 0; i < numSamples; ++i)
        {
            outLeft[i] += left[i] * gainLeft;
            outRight[i] += right[i] * gainRight;
        }
    }
    noteEventCount = 0;
}

void Engine::process(float* const* channels, int numChannels, int numSamples)
{
    adoptNewestState();
    if (maxBlock == 0)
        return; // not prepared

    // The mixer buses hold maxBlock samples: split larger blocks.
    for (int offset = 0; offset < numSamples; offset += maxBlock)
        renderChunk(channels, numChannels, offset, std::min(maxBlock, numSamples - offset));
}

void Engine::renderChunk(float* const* channels, int numChannels, int offset, int numSamples)
{
    const auto block = transportState.advance(numSamples, sampleRate);
    updatePlayPosition(block);
    const int numTracks = current == nullptr ? 1
                                             : std::clamp(static_cast<int>(current->tracks.size()),
                                                          1, maxMixerTracks);

    for (int i = 0; i < numTracks * 2; ++i)
        std::fill_n(buses[static_cast<std::size_t>(i)], numSamples, 0.0f);

    if (current != nullptr)
    {
        // Stopping, pausing or jumping (e.g. Stop + Play within one block) ends held notes.
        noteEventCount = 0;
        const bool jumped = wasPlaying && block.startBeat != lastEndBeat;
        const bool starting = block.playing && (!wasPlaying || jumped);
        if (wasPlaying && (!block.playing || jumped))
        {
            releaseAllNotes();
            sampler.stopClips();
        }
        wasPlaying = block.playing;
        lastEndBeat = block.endBeat;
        applyLiveEvents();

        const double t0 = block.startBeat * ticksPerBeat;
        const double t1 = block.endBeat * ticksPerBeat;
        const auto offsetOf = [&](double tick)
        {
            const double fraction = (tick - t0) / (t1 - t0);
            return std::clamp(static_cast<int>(std::lround(fraction * numSamples)), 0,
                              numSamples - 1);
        };

        // Held notes that end in this block.
        if (block.playing)
        {
            for (int i = 0; i < heldCount;)
            {
                const auto& note = heldNotes[static_cast<std::size_t>(i)];
                if (note.endTick < t1)
                {
                    addNoteEvent(note.channelIndex,
                                 {NoteEvent::Kind::noteOff, offsetOf(note.endTick), note.pitch});
                    heldNotes[static_cast<std::size_t>(i)] =
                        heldNotes[static_cast<std::size_t>(--heldCount)];
                }
                else
                {
                    ++i;
                }
            }
        }

        const int count = collectTriggers(*current, block, numSamples, triggers);

        // Audio clips are timed in ticks; their sample plays in seconds at the current tempo.
        const double secondsPerTick = 60.0 / (transportState.tempo() * ticksPerBeat);
        // Play (or a jump) landing inside audio clips starts them part-way in.
        if (starting)
        {
            std::array<Trigger, 64> resumed{};
            const int n = collectClipsUnderPlayhead(*current, t0, resumed);
            for (int i = 0; i < n; ++i)
            {
                const auto& clip = resumed[static_cast<std::size_t>(i)];
                sampler.triggerClip(*current, clip.channelIndex,
                                    clip.clipOffsetTicks * secondsPerTick,
                                    clip.lengthTicks * secondsPerTick);
            }
        }

        // Render in segments so each trigger starts on its exact sample.
        int cursor = 0;
        for (int i = 0; i < count; ++i)
        {
            const auto& trigger = triggers[static_cast<std::size_t>(i)];
            if (trigger.audioClip)
            {
                sampler.render(*current, buses.data(), numTracks * 2, cursor, trigger.sampleOffset);
                sampler.triggerClip(*current, trigger.channelIndex,
                                    trigger.clipOffsetTicks * secondsPerTick,
                                    trigger.lengthTicks * secondsPerTick);
                cursor = trigger.sampleOffset;
                continue;
            }
            if (isInstrument(trigger.channelIndex))
            {
                startInstrumentNote(trigger, trigger.endTick < t1 ? offsetOf(trigger.endTick) : -1);
                continue;
            }
            sampler.render(*current, buses.data(), numTracks * 2, cursor, trigger.sampleOffset);
            // The note's length matters to the Sampler's envelope (it releases at the end).
            const double startTick = t0 + (t1 - t0) * trigger.sampleOffset / numSamples;
            sampler.trigger(*current, trigger.channelIndex, trigger.pitch, trigger.velocity,
                            std::max(0.0, trigger.endTick - startTick) * secondsPerTick);
            cursor = trigger.sampleOffset;
        }
        sampler.render(*current, buses.data(), numTracks * 2, cursor, numSamples);
        renderInstruments(numTracks, numSamples);
    }

    // Inserts feed the Master.
    float* masterLeft = buses[0];
    float* masterRight = buses[1];
    for (int t = 1; t < numTracks; ++t)
    {
        processTrack(t, numSamples);
        const float* left = buses[static_cast<std::size_t>(2 * t)];
        const float* right = buses[static_cast<std::size_t>(2 * t + 1)];
        for (int i = 0; i < numSamples; ++i)
        {
            masterLeft[i] += left[i];
            masterRight[i] += right[i];
        }
    }
    processTrack(0, numSamples);

    // Master to the output (mono outputs get the left side).
    for (int i = 0; i < numSamples; ++i)
    {
        channels[0][offset + i] += masterLeft[i];
        if (numChannels > 1)
            channels[1][offset + i] += masterRight[i];
    }

    std::array<float*, 2> out{channels[0] + offset,
                              numChannels > 1 ? channels[1] + offset : nullptr};
    metronomeState.render(out.data(), std::min(numChannels, 2), numSamples, block);
    previewPlayer.render(out.data(), std::min(numChannels, 2), numSamples);
    scopeBuffer.write(out[0], out[1], numSamples); // everything that's heard
}

void Engine::updatePlayPosition(const BlockPosition& block)
{
    // Plugins see the position within what's looping (the pattern in PAT mode, the song in
    // SONG mode), like the position display, plus the loop itself.
    int loopTicks = 0;
    if (current != nullptr)
    {
        if (current->songMode)
            loopTicks = current->songLengthTicks;
        else if (current->currentPattern >= 0 &&
                 current->currentPattern < static_cast<int>(current->patterns.size()))
            loopTicks =
                current->patterns[static_cast<std::size_t>(current->currentPattern)].lengthTicks;
    }
    auto& p = playPosition;
    p.playing = block.playing;
    p.bpm = transportState.tempo();
    p.beatsPerBar = block.beatsPerBar;
    p.loopLengthBeats = static_cast<double>(loopTicks) / ticksPerBeat;
    p.beat =
        p.loopLengthBeats > 0.0 ? std::fmod(block.startBeat, p.loopLengthBeats) : block.startBeat;
    p.seconds = p.bpm > 0.0 ? p.beat * 60.0 / p.bpm : 0.0;
    p.sample = std::llround(p.seconds * sampleRate);
}

void Engine::processTrack(int track, int numSamples)
{
    float* left = buses[static_cast<std::size_t>(2 * track)];
    float* right = buses[static_cast<std::size_t>(2 * track + 1)];

    float gainLeft = 1.0f;
    float gainRight = 1.0f;
    if (current != nullptr && track < static_cast<int>(current->tracks.size()))
    {
        const auto& state = current->tracks[static_cast<std::size_t>(track)];
        for (const auto& effect : state.effects)
            if (effect.enabled && effect.processor != nullptr)
            {
                effect.processor->setPlayPosition(playPosition);
                effect.processor->process(left, right, numSamples, effect.params);
            }
        gainLeft = state.gainLeft;
        gainRight = state.gainRight;
    }

    float peakLeft = 0.0f;
    float peakRight = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        left[i] *= gainLeft;
        right[i] *= gainRight;
        peakLeft = std::max(peakLeft, std::abs(left[i]));
        peakRight = std::max(peakRight, std::abs(right[i]));
    }

    // Keep the highest level until the UI reads it.
    auto& pl = peaks[static_cast<std::size_t>(2 * track)];
    auto& pr = peaks[static_cast<std::size_t>(2 * track + 1)];
    if (peakLeft > pl.load(std::memory_order_relaxed))
        pl.store(peakLeft, std::memory_order_relaxed);
    if (peakRight > pr.load(std::memory_order_relaxed))
        pr.store(peakRight, std::memory_order_relaxed);
}

} // namespace jls::engine
