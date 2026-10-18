#pragma once

// The snapshot types the UI thread builds (model::toEngineState) and hands to the audio thread.
// Everything here is read-only once published; positions are in ticks (ticksPerBeat).

#include "engine/effects.h"
#include "engine/instrument.h"
#include "engine/sample_buffer.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace jls::engine
{

// Musical time resolution: note and clip positions are in ticks.
inline constexpr int ticksPerBeat = 96;

// Mixer tracks the engine can handle: Master (0) plus inserts.
inline constexpr int maxMixerTracks = 64;

// Immutable snapshot of everything the audio thread needs to play the project.
// Built on the UI thread, handed to the audio thread through Engine::setState().
// The Sampler's volume envelope (FL's DAHDSR), times in seconds, sustain 0..1. Off: the sample
// plays to its end at full level and note length doesn't matter (a one-shot).
struct SampleEnvelope
{
    bool on = false;
    float delay = 0.0f;
    float attack = 0.005f;
    float hold = 0.0f;
    float decay = 0.3f;
    float sustain = 1.0f;
    float release = 0.1f;
};

struct ChannelState
{
    std::uint32_t id = 0;
    std::shared_ptr<const SampleBuffer> sample;
    // A plugin instrument plays the channel instead of the sample when set. Shared like effect
    // processors (see EffectState): the audio thread never copies or releases it.
    std::shared_ptr<InstrumentProcessor> instrument;
    float gainLeft = 1.0f;
    float gainRight = 1.0f;
    bool muted = false;
    int mixerTrack = 0; // index into EngineState::tracks; 0 = Master
    // Playback speed of the sample (1 = as recorded): the Sampler's pitch, and its Resample mode
    // following the tempo. A Stretch-mode sample arrives already stretched, at speed 1.
    double speed = 1.0;
    // Cutting (FL's Cut itself / Cut / Cut by): a new note stops this channel's sounding notes
    // when cutItself; it also stops every channel whose cutBy equals this one's cutGroup.
    // 0 = no group.
    bool cutItself = false;
    int cutGroup = 0;
    int cutBy = 0;
    SampleEnvelope envelope;
};

struct EffectState
{
    // Shared with later snapshots so the effect's internal state carries over; the audio thread
    // only ever uses the raw pointer, never copies or releases it.
    std::shared_ptr<EffectProcessor> processor;
    std::vector<float> params;
    bool enabled = true;
};

struct TrackState
{
    float gainLeft = 1.0f; // fader and pan combined; 0 when muted or soloed out
    float gainRight = 1.0f;
    std::vector<EffectState> effects;
};

struct NoteState
{
    int startTick = 0;                  // within the pattern
    int lengthTicks = ticksPerBeat / 4; // only instruments hear it; samples play to their end
    int pitch = 60;                     // 60 plays the sample at its original pitch
    float velocity = 100.0f / 128.0f;
};

struct PatternState
{
    int lengthTicks = 16 * ticksPerBeat / 4;
    // notes[channelIndex], indexed like EngineState::channels.
    std::vector<std::vector<NoteState>> notes;
};

struct ClipState
{
    int patternIndex = 0; // into EngineState::patterns (pattern clips)
    int startTick = 0;    // position in the song
    int lengthTicks = 16 * ticksPerBeat / 4;
    int audioChannel = -1; // audio clips: index into EngineState::channels, whose sample plays
    int offsetTicks = 0;   // audio clips: where in the sample the clip starts (left trim)
};

struct EngineState
{
    std::vector<ChannelState> channels;
    std::vector<PatternState> patterns;
    std::vector<ClipState> clips;
    std::vector<TrackState> tracks; // [0] = Master; empty = everything straight to the output

    bool songMode = false;   // false: loop patterns[currentPattern]; true: play clips
    int currentPattern = 0;  // index into patterns
    int songLengthTicks = 0; // song mode loops after this; 0 = nothing to play
};

} // namespace jls::engine
