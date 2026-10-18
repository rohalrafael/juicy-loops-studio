#pragma once

// Sample playback for every sample channel and audio clip: preallocated voices, pitch and speed
// (cubic interpolation), the volume envelope, cutting (cut itself, cut groups), clip voices that
// start part-way in and fade out. Audio thread.

#include "engine/engine_state.h"

#include <array>
#include <cstdint>

namespace jls::engine
{

// Polyphonic one-shot sample playback for all channels. Audio thread only; never allocates.
class Sampler
{
  public:
    static constexpr int maxVoices = 64;

    void prepare(double sampleRate);

    // Call whenever the engine switches to a new state, before rendering with it.
    // Stops voices whose channel was removed or whose sample was replaced, so no voice
    // ever points at a sample the current state doesn't keep alive.
    void stateChanged(const EngineState& state);

    // Starts the channel's sample. `pitch` 60 plays it unchanged (each semitone resamples by
    // 2^(1/12)); `velocity` scales the level, with FL's default 100/128 playing at unity.
    // `noteSeconds`: when the note ends (its envelope then releases); -1 = not known yet (a live
    // note: releaseLive() ends it). Without an envelope the sample plays to its end anyway.
    // Cuts the voices the channel's cut settings say to (see ChannelState).
    void trigger(const EngineState& state, int channelIndex, int pitch = 60,
                 float velocity = 100.0f / 128.0f, double noteSeconds = -1.0, bool live = false);
    // Releases live voices playing `pitch` (all live voices for -1).
    void releaseLive(int pitch);
    // An audio clip: plays the channel's sample from `startSeconds` into it, for at most
    // `lengthSeconds` (then fades out quickly), at unity gain and original pitch.
    void triggerClip(const EngineState& state, int channelIndex, double startSeconds,
                     double lengthSeconds);
    // Fades out every audio-clip voice (transport stopped or jumped). Samples ring on.
    void stopClips();

    // Adds all sounding voices into their channel's mixer-track bus, [from, to).
    // buses[2 * track] is a track's left, buses[2 * track + 1] its right; channels routed to a
    // track beyond numBuses go to track 0. With numBuses == 1 only track 0's left is written.
    void render(const EngineState& state, float* const* buses, int numBuses, int from, int to);

    int activeVoiceCount() const;

  private:
    struct Voice
    {
        bool active = false;
        std::uint32_t channelId = 0;
        const SampleBuffer* sample = nullptr;
        double position = 0.0;
        double increment = 1.0;
        float gain = 1.0f; // from velocity
        std::uint64_t startedAt = 0;
        long long remaining = -1; // output samples left (audio clips); -1 = until the end
        int pitch = 60;
        bool live = false;
        long long cutFade = -1;  // samples left of a cut's fade-out; -1 = not cut
        long long noteLeft = -1; // samples until the note ends (envelope release); -1 = never
        SampleEnvelope envelope; // copied at the start
        enum class Stage
        {
            delay,
            attack,
            hold,
            decay,
            sustain,
            release,
            done
        } stage = Stage::delay;
        long long stageLeft = 0; // samples left in the stage
        float level = 0.0f;      // envelope output
        float releaseStep = 0.0f;
    };

    static constexpr long long clipFadeSamples = 128; // a clip cut fades over this many samples
    static constexpr long long cutFadeSamples = 64;   // a cut note fades over this many samples

    Voice& allocateVoice();
    float nextEnvelopeLevel(Voice& voice) const; // advances one sample
    void enterStage(Voice& voice, Voice::Stage stage) const;
    void cutFor(const EngineState& state, const ChannelState& channel);

    std::array<Voice, maxVoices> voices{};
    double sampleRate = 44100.0;
    std::uint64_t triggerCounter = 0;
};

} // namespace jls::engine
