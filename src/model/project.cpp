#include "model/project.h"

#include "engine/drum_synth.h"

#include <algorithm>

namespace jls::model
{

bool Pattern::isOn(ChannelId channel, int step) const
{
    const auto it = steps.find(channel);
    if (it == steps.end() || step < 0 || step >= static_cast<int>(it->second.size()))
        return false;
    return it->second[static_cast<std::size_t>(step)];
}

void Pattern::set(ChannelId channel, int step, bool on)
{
    if (step < 0 || step >= maxLength)
        return;
    auto& row = steps[channel];
    if (step >= static_cast<int>(row.size()))
        row.resize(static_cast<std::size_t>(step) + 1, false);
    row[static_cast<std::size_t>(step)] = on;
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
    pattern.steps.erase(id);
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
    return project;
}

std::unique_ptr<engine::EngineState> toEngineState(const Project& project)
{
    auto state = std::make_unique<engine::EngineState>();
    state->patternLengthInSteps = project.pattern.lengthInSteps;
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

        cs.steps.assign(static_cast<std::size_t>(project.pattern.lengthInSteps), 0);
        for (int s = 0; s < project.pattern.lengthInSteps; ++s)
            cs.steps[static_cast<std::size_t>(s)] = project.pattern.isOn(channel.id, s) ? 1 : 0;

        state->channels.push_back(std::move(cs));
    }
    return state;
}

} // namespace jls::model
