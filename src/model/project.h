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
using PatternId = std::uint32_t;

inline constexpr int stepsPerBeat = 4;                    // a step is a 16th note
inline constexpr int ticksPerBeat = engine::ticksPerBeat; // note positions are in ticks
inline constexpr int ticksPerStep = ticksPerBeat / stepsPerBeat;

struct Channel
{
    ChannelId id = 0;
    std::string name;
    std::shared_ptr<const engine::SampleBuffer> sample;
    float volume = 0.8f; // linear gain 0..1
    float pan = 0.0f;    // -1 (left) .. +1 (right)
    bool muted = false;
};

struct Note
{
    static constexpr int defaultPitch = 60; // "C5" in FL's naming; plays the sample unchanged
    static constexpr float defaultVelocity = 100.0f / 128.0f; // FL's default, plays at unity

    int startTick = 0;
    int lengthTicks = ticksPerStep;
    int pitch = defaultPitch;         // MIDI note number, 0..127
    float velocity = defaultVelocity; // 0..1

    int endTick() const { return startTick + lengthTicks; }
    // A plain Channel Rack step: default pitch, one step long, on a step boundary.
    bool isStep() const;
    bool operator==(const Note&) const = default;
};

// A pattern holds notes for every channel, as in FL (each pattern has its own notes).
// Channel Rack steps are just notes at the default pitch (see Note::isStep()).
struct Pattern
{
    static constexpr int defaultLength = 16; // steps
    static constexpr int maxSteps = 4096;

    PatternId id = 0;
    std::string name;
    int minLengthInSteps = defaultLength; // grows past this (in whole bars) to fit its notes
    std::map<ChannelId, std::vector<Note>> notes;

    const std::vector<Note>& notesFor(ChannelId channel) const;
    // Length in steps: the minimum, or the last note's end rounded up to a whole bar.
    int lengthInSteps(int stepsPerBar) const;
    bool isEmpty() const;

    // Step view, used by the Channel Rack.
    bool hasOnlySteps(ChannelId channel) const; // false once a channel has melodic notes
    bool isOn(ChannelId channel, int step) const;
    void set(ChannelId channel, int step, bool on);
};

// A pattern placed in the Playlist. Positions are in steps (16ths) from the song start.
// A clip longer than its pattern repeats the pattern.
struct Clip
{
    PatternId pattern = 0;
    int track = 0;
    int startStep = 0;
    int lengthInSteps = Pattern::defaultLength;

    int endStep() const { return startStep + lengthInSteps; }
    bool operator==(const Clip&) const = default;
};

struct Playlist
{
    static constexpr int defaultTracks = 16;

    int numTracks = defaultTracks;
    std::vector<Clip> clips;
};

struct Project
{
    std::vector<Channel> channels; // in Channel Rack order
    std::vector<Pattern> patterns; // in pattern-selector order; never empty
    Playlist playlist;
    int beatsPerBar = 4; // time signature numerator (n/4)

    // View state: kept across undo/redo (see Document).
    PatternId currentPattern = 0;
    ChannelId selectedChannel = 0; // 0 = none; the Piano Roll (F7) edits this channel
    bool songMode = false;

    ChannelId nextChannelId = 1;
    PatternId nextPatternId = 1;

    Project(); // starts with one empty pattern, "Pattern 1"

    ChannelId addChannel(std::string name, std::shared_ptr<const engine::SampleBuffer> sample);
    void removeChannel(ChannelId id); // also removes its notes from every pattern
    Channel* findChannel(ChannelId id);
    const Channel* findChannel(ChannelId id) const;

    // New empty pattern named "Pattern N" after the existing ones. Doesn't select it.
    PatternId addPattern();
    // Copy of a pattern (notes and length) right after it, named "<name> (copy)".
    PatternId clonePattern(PatternId id);
    // Removes the pattern and its clips. The last remaining pattern can't be removed.
    void removePattern(PatternId id);
    Pattern* findPattern(PatternId id);
    const Pattern* findPattern(PatternId id) const;
    Pattern& current();
    const Pattern& current() const;

    int stepsPerBar() const { return beatsPerBar * stepsPerBeat; }
    int lengthInSteps(const Pattern& pattern) const { return pattern.lengthInSteps(stepsPerBar()); }
    // Length of the arrangement: end of the last clip, rounded up to a whole bar (0 if empty).
    int songLengthInSteps() const;
};

// Kick, Clap, Hat, Snare with built-in synthesized sounds, like FL's empty template.
// The Kick is selected.
Project makeDefaultProject(double sampleRate = 44100.0);

// Snapshot for the audio thread (allocates; UI thread only).
std::unique_ptr<engine::EngineState> toEngineState(const Project& project);

} // namespace jls::model
