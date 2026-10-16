#pragma once

#include "engine/engine_state.h"
#include "engine/sample_buffer.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace jls::model
{

using ChannelId = std::uint32_t;

struct Channel
{
    ChannelId id = 0;
    std::string name;
    std::shared_ptr<const engine::SampleBuffer> sample;
    float volume = 0.8f; // linear gain 0..1
    float pan = 0.0f;    // -1 (left) .. +1 (right)
    bool muted = false;
};

// Step data lives in the pattern, per channel, as in FL (each pattern has its own steps).
struct Pattern
{
    static constexpr int defaultLength = 16;
    static constexpr int maxLength = 64;

    int lengthInSteps = defaultLength;
    std::map<ChannelId, std::vector<bool>> steps;

    bool isOn(ChannelId channel, int step) const;
    void set(ChannelId channel, int step, bool on);
};

struct Project
{
    std::vector<Channel> channels; // in Channel Rack order
    Pattern pattern;
    ChannelId nextChannelId = 1;

    ChannelId addChannel(std::string name, std::shared_ptr<const engine::SampleBuffer> sample);
    void removeChannel(ChannelId id);
    Channel* findChannel(ChannelId id);
    const Channel* findChannel(ChannelId id) const;
};

// Kick, Clap, Hat, Snare with built-in synthesized sounds, like FL's empty template.
Project makeDefaultProject(double sampleRate = 44100.0);

// Snapshot for the audio thread (allocates; UI thread only).
std::unique_ptr<engine::EngineState> toEngineState(const Project& project);

} // namespace jls::model
