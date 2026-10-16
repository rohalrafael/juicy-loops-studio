#include "model/project.h"

#include "engine/drum_synth.h"

#include <algorithm>
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

int Pattern::lengthInSteps(int stepsPerBar) const
{
    int lastTick = 0;
    for (const auto& [channel, list] : notes)
        for (const auto& note : list)
            lastTick = std::max(lastTick, note.endTick());

    const int steps = (lastTick + ticksPerStep - 1) / ticksPerStep;
    const int bar = std::max(1, stepsPerBar);
    return std::min(maxSteps, std::max(minLengthInSteps, (steps + bar - 1) / bar * bar));
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
    channels.push_back(std::move(channel));
    return channels.back().id;
}

void Project::removeChannel(ChannelId id)
{
    std::erase_if(channels, [id](const Channel& c) { return c.id == id; });
    for (auto& pattern : patterns)
        pattern.notes.erase(id);
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

Project makeDefaultProject(double sampleRate)
{
    using engine::DrumSound;
    auto make = [sampleRate](DrumSound sound)
    {
        return std::make_shared<const engine::SampleBuffer>(
            engine::synthesizeDrum(sound, sampleRate));
    };

    Project project;
    project.addChannel("Kick", make(DrumSound::kick));
    project.addChannel("Clap", make(DrumSound::clap));
    project.addChannel("Hat", make(DrumSound::hat));
    project.addChannel("Snare", make(DrumSound::snare));
    project.selectedChannel = project.channels.front().id;
    return project;
}

std::unique_ptr<engine::EngineState> toEngineState(const Project& project)
{
    auto state = std::make_unique<engine::EngineState>();
    state->channels.reserve(project.channels.size());

    for (const auto& channel : project.channels)
    {
        engine::ChannelState cs;
        cs.id = channel.id;
        cs.sample = channel.sample;
        cs.muted = channel.muted;
        // Balance pan: centre = full volume on both sides.
        const float pan = std::clamp(channel.pan, -1.0f, 1.0f);
        cs.gainLeft = channel.volume * std::min(1.0f, 1.0f - pan);
        cs.gainRight = channel.volume * std::min(1.0f, 1.0f + pan);
        state->channels.push_back(std::move(cs));
    }

    for (std::size_t i = 0; i < project.patterns.size(); ++i)
    {
        const auto& pattern = project.patterns[i];
        engine::PatternState ps;
        ps.lengthTicks = project.lengthInSteps(pattern) * ticksPerStep;
        ps.notes.resize(project.channels.size());
        for (std::size_t ch = 0; ch < project.channels.size(); ++ch)
            for (const auto& note : pattern.notesFor(project.channels[ch].id))
                ps.notes[ch].push_back({note.startTick, note.pitch, note.velocity});
        state->patterns.push_back(std::move(ps));

        if (pattern.id == project.current().id)
            state->currentPattern = static_cast<int>(i);
    }

    for (const auto& clip : project.playlist.clips)
    {
        const auto it = std::find_if(project.patterns.begin(), project.patterns.end(),
                                     [&](const Pattern& p) { return p.id == clip.pattern; });
        if (it == project.patterns.end())
            continue;
        state->clips.push_back({static_cast<int>(it - project.patterns.begin()),
                                clip.startStep * ticksPerStep, clip.lengthInSteps * ticksPerStep});
    }

    state->songMode = project.songMode;
    state->songLengthTicks = project.songLengthInSteps() * ticksPerStep;
    return state;
}

} // namespace jls::model
