#pragma once

// The project data model: channels, patterns (notes per channel), the Playlist arrangement and
// the Mixer. Plain C++ value types, cheap to copy (samples are shared), so the whole project can
// be snapshotted for undo (see Document) and converted into an engine snapshot.

#include "engine/effect_pool.h"
#include "engine/engine_state.h"
#include "engine/sample_buffer.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace jls::model
{

using ChannelId = std::uint32_t;
using PatternId = std::uint32_t;
using EffectId = std::uint32_t;

inline constexpr int stepsPerBeat = 4;                    // a step is a 16th note
inline constexpr int ticksPerBeat = engine::ticksPerBeat; // note positions are in ticks
inline constexpr int ticksPerStep = ticksPerBeat / stepsPerBeat;

// How a sample follows the project tempo (the Sampler's time-stretching modes).
enum class StretchMode
{
    off,      // plays at its own speed whatever the tempo
    resample, // speed follows the tempo; pitch moves with it (like a record)
    stretch,  // length follows the tempo; pitch stays (time-stretched in the background)
};

// The Sampler's settings for a sample channel.
struct SamplerSettings
{
    StretchMode mode = StretchMode::off;
    double originalTempo = 0.0; // BPM the sample is at; 0 = not set (taken as the project's)
    double pitch = 0.0;         // semitones, -24..24

    bool operator==(const SamplerSettings&) const = default;
};

struct Channel
{
    ChannelId id = 0;
    std::string name;
    std::shared_ptr<const engine::SampleBuffer> sample;
    // Where the sample came from, so projects can be saved: a built-in sound ("kick", see
    // engine::DrumSound) or an audio file path. Both empty = no sample.
    std::string builtinSample;
    std::string samplePath;
    float volume = 0.8f; // linear gain 0..1
    float pan = 0.0f;    // -1 (left) .. +1 (right)
    bool muted = false;
    int mixerTrack = 0; // 0 = Master, 1.. = inserts
    // Repeat this channel's notes when the pattern is longer than they are (see
    // Project::loopLengthInSteps()). Applies only while Project::loopChannels is on.
    bool loop = true;
    // The source of audio clips in the Playlist (Clip::audio): its sample plays in song time.
    // Hidden from the Channel Rack unless Project::showAudioClips is on.
    bool audioClip = false;
    SamplerSettings sampler; // time-stretching and pitch (sample channels)

    // A hosted plugin instrument plays the channel instead of a sample (see setInstrument()).
    std::string pluginId;    // the host's identifier string; empty = sampler channel
    std::string pluginName;  // shown in the UI, also when the plugin isn't installed
    std::string pluginState; // base64 of the plugin's own state, as of the last save

    bool isInstrument() const { return !pluginId.empty(); }
    // Makes this a plugin instrument channel (drops the sample) ...
    void setInstrument(std::string id, std::string name);
    // ... or a sampler channel again (drops the plugin).
    void setSample(std::shared_ptr<const engine::SampleBuffer> buffer, std::string path,
                   std::string builtin = {});
};

// An effect in a mixer track's slot. `type` is an engine::EffectType id, or
// engine::pluginEffectType for a hosted plugin (then the plugin fields are used, not params).
struct EffectSlot
{
    EffectId id = 0;
    std::string type;
    std::vector<float> params; // one per engine::ParamInfo, in real units
    bool enabled = true;

    std::string pluginId;    // which plugin (the host's identifier string)
    std::string pluginName;  // shown in the UI, also when the plugin isn't installed
    std::string pluginState; // base64 of the plugin's own state, as of the last save

    bool isPlugin() const { return type == engine::pluginEffectType; }

    bool operator==(const EffectSlot&) const = default;
};

struct MixerTrack
{
    static constexpr float minVolumeDb = -60.0f; // the bottom of the fader is silence
    static constexpr float maxVolumeDb = 6.0f;
    static constexpr int maxEffects = 8;

    std::string name;
    float volumeDb = 0.0f;
    float pan = 0.0f;
    bool muted = false;
    bool solo = false;
    std::vector<EffectSlot> effects; // in processing order, at most maxEffects

    float gain() const; // linear; 0 at minVolumeDb
};

// FL-style mixer: tracks[0] is the Master, the rest are inserts that all feed it.
struct Mixer
{
    static constexpr int numInserts = 16;

    std::vector<MixerTrack> tracks; // Master + numInserts

    Mixer();
    bool anySolo() const;
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
    // Steps the Channel Rack shows (16/32/64 or dragged). Doesn't change what plays: the
    // pattern plays only as far as its notes (see lengthInSteps()).
    int gridSteps = defaultLength;
    std::map<ChannelId, std::vector<Note>> notes;

    const std::vector<Note>& notesFor(ChannelId channel) const;
    // Where a channel's notes end, in steps (0 if it has none).
    int contentEndInSteps(ChannelId channel) const;
    // Makes a looping channel's repeats real: copies its notes in [0, fromSteps) over and over
    // until toSteps, so it sounds the same and can then be edited beyond its old loop.
    void fillLoop(ChannelId channel, int fromSteps, int toSteps);
    // Length in steps, i.e. what plays and loops: the last note's end rounded up to a whole bar,
    // at least one bar.
    int lengthInSteps(int stepsPerBar) const;
    bool isEmpty() const;

    // Step view, used by the Channel Rack.
    bool hasOnlySteps(ChannelId channel) const; // false once a channel has melodic notes
    bool isOn(ChannelId channel, int step) const;
    void set(ChannelId channel, int step, bool on);
};

// A pattern placed in the Playlist. Positions are in steps (16ths) from the song start.
// A clip longer than its pattern repeats the pattern.
// A pattern, or an audio clip (`audio` set: the audio-clip channel whose sample it plays), placed
// in the Playlist. Positions are in steps (16ths) from the song start. A pattern clip longer than
// its pattern repeats it; an audio clip longer than its sample is silent after it.
struct Clip
{
    PatternId pattern = 0;
    int track = 0;
    int startStep = 0;
    int lengthInSteps = Pattern::defaultLength;
    ChannelId audio = 0; // audio clips: the Channel (Channel::audioClip) to play
    int offsetSteps = 0; // audio clips: how far into the sample the clip starts (left-edge trim)

    bool isAudio() const { return audio != 0; }
    int endStep() const { return startStep + lengthInSteps; }
    bool operator==(const Clip&) const = default;
};

struct Playlist
{
    static constexpr int defaultTracks = 16;

    int numTracks = defaultTracks;
    std::vector<Clip> clips;
};

// Cuts a note into consecutive notes `pieceTicks` long (the last one shorter if it doesn't
// divide evenly), with the same pitch and velocity: the Piano Roll's slicer, for rolls.
std::vector<Note> sliceNote(const Note& note, int pieceTicks);

// Scales the Piano Roll can highlight.
enum class Scale
{
    none, // just white/black keys
    major,
    minor, // natural minor
};

// True if `pitch` belongs to `scale` built on `root` (0 = C ... 11 = B). Every pitch does for
// Scale::none.
bool isInScale(int pitch, int root, Scale scale);

struct Project
{
    std::vector<Channel> channels; // in Channel Rack order
    std::vector<Pattern> patterns; // in pattern-selector order; never empty
    Playlist playlist;
    Mixer mixer;
    double tempo = 130.0; // BPM
    int beatsPerBar = 4;  // time signature numerator (n/4)
    // The Channel Rack's loop switch: channels whose notes are shorter than the pattern repeat
    // them (each channel can opt out with Channel::loop).
    bool loopChannels = true;

    // View state: kept across undo/redo (see Document).
    PatternId currentPattern = 0;
    ChannelId selectedChannel = 0; // 0 = none; the Piano Roll (F7) edits this channel
    int selectedMixerTrack = 1;    // the mixer track whose effect slots are shown
    bool showAudioClips = false;   // Channel Rack also lists audio-clip channels
    Scale scale = Scale::none;     // Piano Roll scale highlight
    int scaleRoot = 0;             // 0 = C ... 11 = B
    bool songMode = false;

    ChannelId nextChannelId = 1;
    PatternId nextPatternId = 1;
    EffectId nextEffectId = 1;

    Project(); // starts with one empty pattern, "Pattern 1"

    // New channels go to the first insert no other channel uses (Master if all are taken).
    ChannelId addChannel(std::string name, std::shared_ptr<const engine::SampleBuffer> sample);
    // Sampler maths for a sample channel at the project tempo:
    // how much faster than recorded it plays (Off / Resample; 1 for Stretch, whose sample is
    // stretched instead), and how much longer the Stretch result is than the sample.
    double playbackSpeed(const Channel& channel) const;
    double stretchFactor(const Channel& channel) const;
    // How long the sample lasts when played, in seconds (after speed or stretching).
    double playedSeconds(const Channel& channel) const;

    // The audio-clip channel for a sample file: the existing one for that path, or a new one
    // (named after the file, routed like a new channel, hidden in the Channel Rack).
    ChannelId audioClipChannel(const std::string& path, const std::string& name,
                               std::shared_ptr<const engine::SampleBuffer> sample);
    // A channel played by a hosted plugin instrument, named after it.
    ChannelId addInstrumentChannel(const std::string& pluginId, const std::string& pluginName);
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
    double secondsPerStep() const { return 60.0 / tempo / stepsPerBeat; }
    int lengthInSteps(const Pattern& pattern) const { return pattern.lengthInSteps(stepsPerBar()); }
    // Steps the Channel Rack shows: its grid setting, or more when the notes reach further.
    int gridLengthInSteps(const Pattern& pattern) const
    {
        return std::max(pattern.gridSteps, lengthInSteps(pattern));
    }
    // Length of the arrangement: end of the last clip, rounded up to a whole bar (0 if empty).
    int songLengthInSteps() const;

    // A channel's notes rounded up to 1, 2, 4, 8... bars: the length it repeats at.
    int naturalLoopInSteps(const Pattern& pattern, ChannelId channel) const;
    // The length a channel actually repeats at in `pattern`, or 0 if it doesn't (looping off,
    // no notes, or the notes already fill the pattern).
    int loopLengthInSteps(const Pattern& pattern, ChannelId channel) const;

    // Adds an effect with default settings to a mixer track's chain. Returns 0 if the track
    // is full or the type unknown.
    EffectId addEffect(int track, const std::string& type);
    // Adds a hosted plugin effect. Returns 0 if the track is full.
    EffectId addPluginEffect(int track, const std::string& pluginId, const std::string& name);
    EffectSlot* findEffect(EffectId id, int* trackOut = nullptr);
    const EffectSlot* findEffect(EffectId id, int* trackOut = nullptr) const;
    void removeEffect(EffectId id);
};

// A duration as a clock, "M:SS.mmm" (minutes grow past 59: "75:03.250"). Negative = 0.
std::string formatTime(double seconds);

// A built-in synthesized sound by name ("kick", "clap", "hat", "snare"); null if unknown.
std::shared_ptr<const engine::SampleBuffer> builtinSample(const std::string& name,
                                                          double sampleRate = 44100.0);

// Kick, Clap, Hat, Snare with built-in synthesized sounds, like FL's empty template, and a Soft
// Clipper on the Master. The Kick is selected.
Project makeDefaultProject(double sampleRate = 44100.0);

// Snapshot for the audio thread (allocates; UI thread only). Effect processors and plugin
// instruments come from `effects`, which keeps their state between snapshots; without a pool
// they are left out.
//
// `stretched` supplies Stretch-mode samples already stretched for the current tempo (or null
// while they're being made: the channel then plays resampled, so it stays in time).
using StretchedSamples =
    std::function<std::shared_ptr<const engine::SampleBuffer>(const Channel& channel)>;
std::unique_ptr<engine::EngineState> toEngineState(const Project& project,
                                                   engine::EffectPool* effects = nullptr,
                                                   const StretchedSamples& stretched = {});

} // namespace jls::model
