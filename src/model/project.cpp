// Project operations and toEngineState(), which flattens the model into the audio thread's
// snapshot: ids become indices, steps become ticks, mute/solo/fader/pan become per-track gains.

#include "model/project.h"

#include "engine/drum_synth.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace jls::model
{

bool Note::isStep() const
{
    return pitch == defaultPitch && lengthTicks == ticksPerStep && startTick % ticksPerStep == 0;
}

const std::vector<Note>& Pattern::notesFor(ChannelId channel) const
{
    static const std::vector<Note> none;
    const auto it = notes.find(channel);
    return it == notes.end() ? none : it->second;
}

int Pattern::contentEndInSteps(ChannelId channel) const
{
    int lastTick = 0;
    for (const auto& note : notesFor(channel))
        lastTick = std::max(lastTick, note.endTick());
    return (lastTick + ticksPerStep - 1) / ticksPerStep;
}

void Pattern::fillLoop(ChannelId channel, int fromSteps, int toSteps)
{
    if (fromSteps <= 0 || toSteps <= fromSteps)
        return;
    auto& list = notes[channel];
    const int loopTicks = fromSteps * ticksPerStep;
    std::vector<Note> original;
    for (const auto& note : list)
        if (note.startTick < loopTicks)
            original.push_back(note);
    std::erase_if(list, [loopTicks](const Note& n) { return n.startTick >= loopTicks; });
    for (int offset = loopTicks; offset < toSteps * ticksPerStep; offset += loopTicks)
        for (auto note : original)
        {
            note.startTick += offset;
            if (note.startTick < toSteps * ticksPerStep)
                list.push_back(note);
        }
}

int Pattern::lengthInSteps(int stepsPerBar) const
{
    int lastTick = 0;
    for (const auto& [channel, list] : notes)
        for (const auto& note : list)
            lastTick = std::max(lastTick, note.endTick());

    const int steps = (lastTick + ticksPerStep - 1) / ticksPerStep;
    const int bar = std::max(1, stepsPerBar);
    return std::min(maxSteps, std::max(bar, (steps + bar - 1) / bar * bar));
}

bool Pattern::isEmpty() const
{
    return std::all_of(notes.begin(), notes.end(),
                       [](const auto& entry) { return entry.second.empty(); });
}

bool Pattern::hasOnlySteps(ChannelId channel) const
{
    const auto& list = notesFor(channel);
    return std::all_of(list.begin(), list.end(), [](const Note& n) { return n.isStep(); });
}

bool Pattern::isOn(ChannelId channel, int step) const
{
    const auto& list = notesFor(channel);
    return std::any_of(list.begin(), list.end(), [step](const Note& n)
                       { return n.isStep() && n.startTick == step * ticksPerStep; });
}

void Pattern::set(ChannelId channel, int step, bool on)
{
    if (step < 0 || step >= maxSteps || isOn(channel, step) == on)
        return;
    auto& list = notes[channel];
    if (on)
    {
        Note note;
        note.startTick = step * ticksPerStep;
        list.push_back(note);
    }
    else
    {
        std::erase_if(list, [step](const Note& n)
                      { return n.isStep() && n.startTick == step * ticksPerStep; });
    }
}

void Pattern::fillSteps(ChannelId channel, int every, int lengthInSteps)
{
    auto& list = notes[channel];
    list.clear();
    every = std::max(1, every);
    for (int step = 0; step < std::min(lengthInSteps, maxSteps); step += every)
    {
        Note note;
        note.startTick = step * ticksPerStep;
        list.push_back(note);
    }
}

float MixerTrack::gain() const
{
    return volumeDb <= minVolumeDb ? 0.0f : std::pow(10.0f, volumeDb / 20.0f);
}

Mixer::Mixer()
{
    tracks.resize(numInserts + 1);
    tracks[0].name = "Master";
    for (int i = 1; i <= numInserts; ++i)
        tracks[static_cast<std::size_t>(i)].name = "Insert " + std::to_string(i);
}

bool Mixer::anySolo() const
{
    return std::any_of(tracks.begin() + 1, tracks.end(),
                       [](const MixerTrack& t) { return t.solo; });
}

Project::Project()
{
    currentPattern = addPattern();
}

ChannelId Project::addChannel(std::string name, std::shared_ptr<const engine::SampleBuffer> sample)
{
    Channel channel;
    channel.id = nextChannelId++;
    channel.name = std::move(name);
    channel.sample = std::move(sample);
    for (int track = 1; track <= Mixer::numInserts; ++track)
        if (std::none_of(channels.begin(), channels.end(),
                         [track](const Channel& c) { return c.mixerTrack == track; }))
        {
            channel.mixerTrack = track;
            break;
        }
    channels.push_back(std::move(channel));
    return channels.back().id;
}

double Project::playbackSpeed(const Channel& channel) const
{
    const double pitch = std::exp2(std::clamp(channel.sampler.pitch, -24.0, 24.0) / 12.0);
    const double original =
        channel.sampler.originalTempo > 0.0 ? channel.sampler.originalTempo : tempo;
    switch (channel.sampler.mode)
    {
    case StretchMode::off:
        return pitch;
    case StretchMode::resample:
        return pitch * tempo / original;
    case StretchMode::stretch:
        return 1.0;
    }
    return 1.0;
}

double Project::stretchFactor(const Channel& channel) const
{
    if (channel.sampler.mode != StretchMode::stretch || channel.sampler.originalTempo <= 0.0)
        return 1.0;
    return channel.sampler.originalTempo / tempo; // faster song = shorter sample
}

double Project::playedSeconds(const Channel& channel) const
{
    if (channel.sample == nullptr || channel.sample->numFrames() == 0)
        return 0.0;
    const double seconds = channel.sample->numFrames() / channel.sample->sampleRate;
    return seconds * stretchFactor(channel) / playbackSpeed(channel);
}

ChannelId Project::audioClipChannel(const std::string& path, const std::string& name,
                                    std::shared_ptr<const engine::SampleBuffer> sample)
{
    for (const auto& c : channels)
        if (c.audioClip && c.samplePath == path)
            return c.id;
    const auto id = addChannel(name, std::move(sample));
    auto* channel = findChannel(id);
    channel->samplePath = path;
    channel->audioClip = true;
    // Audio clips keep in time with the song: stretched relative to the tempo they came in at.
    channel->sampler.mode = StretchMode::stretch;
    channel->sampler.originalTempo = tempo;
    return id;
}

ChannelId Project::addInstrumentChannel(const std::string& pluginId, const std::string& pluginName)
{
    const auto id = addChannel(pluginName, nullptr);
    findChannel(id)->setInstrument(pluginId, pluginName);
    return id;
}

void Channel::setInstrument(std::string newPluginId, std::string newPluginName)
{
    pluginId = std::move(newPluginId);
    pluginName = std::move(newPluginName);
    pluginState.clear();
    sample = nullptr;
    samplePath.clear();
    builtinSample.clear();
}

void Channel::setSample(std::shared_ptr<const engine::SampleBuffer> buffer, std::string path,
                        std::string builtin)
{
    sample = std::move(buffer);
    samplePath = std::move(path);
    builtinSample = std::move(builtin);
    pluginId.clear();
    pluginName.clear();
    pluginState.clear();
}

void Project::soloChannel(ChannelId id)
{
    const bool alreadySolo = std::all_of(channels.begin(), channels.end(), [id](const Channel& c)
                                         { return c.audioClip || (c.id == id) != c.muted; });
    for (auto& c : channels)
        if (!c.audioClip)
            c.muted = alreadySolo ? false : c.id != id;
}

void Project::removeChannel(ChannelId id)
{
    std::erase_if(channels, [id](const Channel& c) { return c.id == id; });
    for (auto& pattern : patterns)
        pattern.notes.erase(id);
    std::erase_if(playlist.clips, [id](const Clip& c) { return c.audio == id; });
    if (selectedChannel == id)
        selectedChannel = channels.empty() ? 0 : channels.front().id;
}

Channel* Project::findChannel(ChannelId id)
{
    const auto it = std::find_if(channels.begin(), channels.end(),
                                 [id](const Channel& c) { return c.id == id; });
    return it == channels.end() ? nullptr : &*it;
}

const Channel* Project::findChannel(ChannelId id) const
{
    return const_cast<Project*>(this)->findChannel(id);
}

PatternId Project::addPattern()
{
    // Name after the highest "Pattern N" so names stay unique after deletions.
    int highest = 0;
    for (const auto& p : patterns)
        if (p.name.rfind("Pattern ", 0) == 0)
            highest = std::max(highest, std::atoi(p.name.c_str() + 8));

    Pattern pattern;
    pattern.id = nextPatternId++;
    pattern.name = "Pattern " + std::to_string(highest + 1);
    patterns.push_back(std::move(pattern));
    return patterns.back().id;
}

PatternId Project::clonePattern(PatternId id)
{
    const auto it = std::find_if(patterns.begin(), patterns.end(),
                                 [id](const Pattern& p) { return p.id == id; });
    if (it == patterns.end())
        return 0;

    Pattern copy = *it;
    copy.id = nextPatternId++;
    copy.name += " (copy)";
    return patterns.insert(it + 1, std::move(copy))->id;
}

void Project::removePattern(PatternId id)
{
    if (patterns.size() <= 1)
        return;

    const auto it = std::find_if(patterns.begin(), patterns.end(),
                                 [id](const Pattern& p) { return p.id == id; });
    if (it == patterns.end())
        return;

    const auto index = static_cast<std::size_t>(it - patterns.begin());
    patterns.erase(it);
    std::erase_if(playlist.clips, [id](const Clip& c) { return c.pattern == id; });

    if (currentPattern == id)
        currentPattern = patterns[std::min(index, patterns.size() - 1)].id;
}

Pattern* Project::findPattern(PatternId id)
{
    const auto it = std::find_if(patterns.begin(), patterns.end(),
                                 [id](const Pattern& p) { return p.id == id; });
    return it == patterns.end() ? nullptr : &*it;
}

const Pattern* Project::findPattern(PatternId id) const
{
    return const_cast<Project*>(this)->findPattern(id);
}

Pattern& Project::current()
{
    if (auto* p = findPattern(currentPattern))
        return *p;
    return patterns.front();
}

const Pattern& Project::current() const
{
    return const_cast<Project*>(this)->current();
}

int Project::songLengthInSteps() const
{
    int end = 0;
    for (const auto& clip : playlist.clips)
        end = std::max(end, clip.endStep());
    const int bar = stepsPerBar();
    return bar <= 0 ? end : (end + bar - 1) / bar * bar;
}

EffectId Project::addEffect(int track, const std::string& type)
{
    const auto* info = engine::findEffectType(type);
    if (info == nullptr || track < 0 || track >= static_cast<int>(mixer.tracks.size()))
        return 0;
    auto& effects = mixer.tracks[static_cast<std::size_t>(track)].effects;
    if (static_cast<int>(effects.size()) >= MixerTrack::maxEffects)
        return 0;
    EffectSlot slot;
    slot.id = nextEffectId++;
    slot.type = type;
    slot.params = info->defaultParams();
    effects.push_back(std::move(slot));
    return effects.back().id;
}

EffectId Project::addPluginEffect(int track, const std::string& pluginId, const std::string& name)
{
    if (track < 0 || track >= static_cast<int>(mixer.tracks.size()))
        return 0;
    auto& effects = mixer.tracks[static_cast<std::size_t>(track)].effects;
    if (static_cast<int>(effects.size()) >= MixerTrack::maxEffects)
        return 0;
    EffectSlot slot;
    slot.id = nextEffectId++;
    slot.type = engine::pluginEffectType;
    slot.pluginId = pluginId;
    slot.pluginName = name;
    effects.push_back(std::move(slot));
    return effects.back().id;
}

EffectSlot* Project::findEffect(EffectId id, int* trackOut)
{
    for (std::size_t t = 0; t < mixer.tracks.size(); ++t)
        for (auto& slot : mixer.tracks[t].effects)
            if (slot.id == id)
            {
                if (trackOut != nullptr)
                    *trackOut = static_cast<int>(t);
                return &slot;
            }
    return nullptr;
}

const EffectSlot* Project::findEffect(EffectId id, int* trackOut) const
{
    return const_cast<Project*>(this)->findEffect(id, trackOut);
}

void Project::removeEffect(EffectId id)
{
    for (auto& track : mixer.tracks)
        std::erase_if(track.effects, [id](const EffectSlot& s) { return s.id == id; });
}

std::shared_ptr<const engine::SampleBuffer> builtinSample(const std::string& name,
                                                          double sampleRate)
{
    using engine::DrumSound;
    DrumSound sound;
    if (name == "kick")
        sound = DrumSound::kick;
    else if (name == "clap")
        sound = DrumSound::clap;
    else if (name == "hat")
        sound = DrumSound::hat;
    else if (name == "snare")
        sound = DrumSound::snare;
    else
        return nullptr;
    return std::make_shared<const engine::SampleBuffer>(engine::synthesizeDrum(sound, sampleRate));
}

int Project::naturalLoopInSteps(const Pattern& pattern, ChannelId channel) const
{
    const int end = pattern.contentEndInSteps(channel);
    if (end <= 0)
        return 0;
    int loop = std::max(1, stepsPerBar());
    while (loop < end && loop < Pattern::maxSteps)
        loop *= 2;
    return loop;
}

int Project::loopLengthInSteps(const Pattern& pattern, ChannelId channel) const
{
    const auto* c = findChannel(channel);
    if (!loopChannels || c == nullptr || !c->loop)
        return 0;
    const int loop = naturalLoopInSteps(pattern, channel);
    return loop < lengthInSteps(pattern) ? loop : 0;
}

std::vector<Note> sliceNote(const Note& note, int pieceTicks)
{
    if (pieceTicks <= 0 || pieceTicks >= note.lengthTicks)
        return {note};
    std::vector<Note> pieces;
    for (int start = note.startTick; start < note.endTick(); start += pieceTicks)
    {
        Note piece = note;
        piece.startTick = start;
        piece.lengthTicks = std::min(pieceTicks, note.endTick() - start);
        pieces.push_back(piece);
    }
    return pieces;
}

bool moveNotes(std::vector<Note>& notes, const std::vector<int>& indices, int ticks, int semitones)
{
    int dt = ticks;
    int dp = semitones;
    bool any = false;
    for (int i : indices)
    {
        if (i < 0 || i >= static_cast<int>(notes.size()))
            continue;
        const auto& n = notes[static_cast<std::size_t>(i)];
        dt = std::max(dt, -n.startTick);
        dp = std::clamp(dp, -n.pitch, 127 - n.pitch);
        any = true;
    }
    if (!any || (dt == 0 && dp == 0))
        return false;
    for (int i : indices)
        if (i >= 0 && i < static_cast<int>(notes.size()))
        {
            notes[static_cast<std::size_t>(i)].startTick += dt;
            notes[static_cast<std::size_t>(i)].pitch += dp;
        }
    return true;
}

bool isInScale(int pitch, int root, Scale scale)
{
    // Semitones above the root that are in the scale, as bit masks.
    constexpr unsigned major = 0b101010110101; // 0 2 4 5 7 9 11
    constexpr unsigned minor = 0b010110101101; // 0 2 3 5 7 8 10
    if (scale == Scale::none)
        return true;
    const int degree = ((pitch - root) % 12 + 12) % 12;
    return ((scale == Scale::major ? major : minor) >> degree) & 1u;
}

std::string formatTime(double seconds)
{
    const auto ms = static_cast<long long>(std::floor(std::max(0.0, seconds) * 1000.0 + 1e-6));
    char text[32];
    std::snprintf(text, sizeof(text), "%lld:%02lld.%03lld", ms / 60000, ms / 1000 % 60, ms % 1000);
    return text;
}

Project makeDefaultProject(double sampleRate)
{
    Project project;
    for (const auto& [name, builtin] : {std::pair{"Kick", "kick"}, std::pair{"Clap", "clap"},
                                        std::pair{"Hat", "hat"}, std::pair{"Snare", "snare"}})
    {
        const auto id = project.addChannel(name, builtinSample(builtin, sampleRate));
        project.findChannel(id)->builtinSample = builtin;
    }
    project.selectedChannel = project.channels.front().id;
    // Like the limiter in FL's template: keeps the summed mix from hard-clipping.
    project.addEffect(0, "softclip");
    return project;
}

std::unique_ptr<engine::EngineState> toEngineState(const Project& project,
                                                   engine::EffectPool* effects,
                                                   const StretchedSamples& stretched)
{
    auto state = std::make_unique<engine::EngineState>();
    state->channels.reserve(project.channels.size());

    for (const auto& channel : project.channels)
    {
        engine::ChannelState cs;
        cs.id = channel.id;
        cs.muted = channel.muted;
        if (channel.isInstrument())
        {
            if (effects != nullptr)
                cs.instrument =
                    effects->getInstrument(channel.id, channel.pluginId, channel.pluginState);
        }
        else
        {
            cs.sample = channel.sample;
            cs.speed = project.playbackSpeed(channel);
            const double factor = project.stretchFactor(channel);
            const bool needsStretch =
                channel.sampler.mode == StretchMode::stretch &&
                (std::abs(factor - 1.0) > 1e-6 || std::abs(channel.sampler.pitch) > 1e-6);
            if (needsStretch)
            {
                if (auto done = stretched ? stretched(channel) : nullptr)
                    cs.sample = std::move(done);
                else // not ready yet: resample so it stays in time meanwhile
                    cs.speed = std::exp2(channel.sampler.pitch / 12.0) / factor;
            }
        }
        // Balance pan: centre = full volume on both sides.
        const float pan = std::clamp(channel.pan, -1.0f, 1.0f);
        cs.gainLeft = channel.volume * std::min(1.0f, 1.0f - pan);
        cs.gainRight = channel.volume * std::min(1.0f, 1.0f + pan);
        cs.mixerTrack =
            std::clamp(channel.mixerTrack, 0, static_cast<int>(project.mixer.tracks.size()) - 1);
        state->channels.push_back(std::move(cs));
    }

    for (std::size_t i = 0; i < project.patterns.size(); ++i)
    {
        const auto& pattern = project.patterns[i];
        engine::PatternState ps;
        ps.lengthTicks = project.lengthInSteps(pattern) * ticksPerStep;
        ps.notes.resize(project.channels.size());
        for (std::size_t ch = 0; ch < project.channels.size(); ++ch)
        {
            // A looping channel's notes are repeated here, so the engine just plays them.
            const auto id = project.channels[ch].id;
            const int loopTicks = project.loopLengthInSteps(pattern, id) * ticksPerStep;
            const int repeatTicks = loopTicks > 0 ? loopTicks : ps.lengthTicks;
            for (int offset = 0; offset < ps.lengthTicks; offset += repeatTicks)
                for (const auto& note : pattern.notesFor(id))
                    if (offset == 0 || note.startTick + offset < ps.lengthTicks)
                        ps.notes[ch].push_back(
                            {note.startTick + offset, note.lengthTicks, note.pitch, note.velocity});
        }
        state->patterns.push_back(std::move(ps));

        if (pattern.id == project.current().id)
            state->currentPattern = static_cast<int>(i);
    }

    for (const auto& clip : project.playlist.clips)
    {
        if (clip.isAudio())
        {
            const auto channel = std::find_if(project.channels.begin(), project.channels.end(),
                                              [&](const Channel& c) { return c.id == clip.audio; });
            if (channel != project.channels.end())
                state->clips.push_back({-1, clip.startStep * ticksPerStep,
                                        clip.lengthInSteps * ticksPerStep,
                                        static_cast<int>(channel - project.channels.begin()),
                                        clip.offsetSteps * ticksPerStep});
            continue;
        }
        const auto it = std::find_if(project.patterns.begin(), project.patterns.end(),
                                     [&](const Pattern& p) { return p.id == clip.pattern; });
        if (it == project.patterns.end())
            continue;
        state->clips.push_back({static_cast<int>(it - project.patterns.begin()),
                                clip.startStep * ticksPerStep, clip.lengthInSteps * ticksPerStep});
    }

    // Mixer: fader, pan, mute and solo resolve to two gains per track.
    const bool anySolo = project.mixer.anySolo();
    for (std::size_t t = 0; t < project.mixer.tracks.size(); ++t)
    {
        const auto& track = project.mixer.tracks[t];
        engine::TrackState ts;
        const bool audible = !track.muted && (t == 0 || !anySolo || track.solo);
        const float gain = audible ? track.gain() : 0.0f;
        const float pan = std::clamp(track.pan, -1.0f, 1.0f);
        ts.gainLeft = gain * std::min(1.0f, 1.0f - pan);
        ts.gainRight = gain * std::min(1.0f, 1.0f + pan);
        if (effects != nullptr)
            for (const auto& slot : track.effects)
                if (auto processor =
                        effects->get(slot.id, slot.type, slot.pluginId, slot.pluginState))
                    ts.effects.push_back({std::move(processor), slot.params, slot.enabled});
        state->tracks.push_back(std::move(ts));
    }
    if (effects != nullptr)
        effects->collectUnused();

    state->songMode = project.songMode;
    state->songLengthTicks = project.songLengthInSteps() * ticksPerStep;
    return state;
}

} // namespace jls::model
