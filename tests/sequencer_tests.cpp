// The step sequencer: notes, pattern loops and Playlist clips turned into triggers on the exact
// sample, independent of block size.

#include "engine/engine.h"
#include "engine/sampler.h"
#include "engine/spsc_queue.h"
#include "engine/step_sequencer.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <vector>

using namespace jls::engine;

namespace
{

constexpr double sampleRate = 48000.0;
constexpr int samplesPerStep = 6000; // 120 BPM: a beat is 24000 samples, a 16th is 6000

std::shared_ptr<const SampleBuffer> constantSample(int frames, double rate = sampleRate)
{
    auto buffer = std::make_shared<SampleBuffer>();
    buffer->sampleRate = rate;
    buffer->channels.assign(1, std::vector<float>(static_cast<std::size_t>(frames), 1.0f));
    return buffer;
}

constexpr int tps = ticksPerStep;

PatternState patternWithSteps(std::vector<int> onSteps, int length = 16)
{
    PatternState pattern;
    pattern.lengthTicks = length * tps;
    pattern.notes.resize(1);
    for (int s : onSteps)
        pattern.notes[0].push_back({s * tps, tps, 60, 100.0f / 128.0f});
    return pattern;
}

// One channel, one pattern, pattern mode.
EngineState stateWithSteps(std::vector<int> onSteps, int length = 16)
{
    EngineState state;
    ChannelState channel;
    channel.id = 1;
    channel.sample = constantSample(100);
    state.channels.push_back(channel);
    state.patterns.push_back(patternWithSteps(std::move(onSteps), length));
    return state;
}

// Every trigger over `total` samples, in blocks of `blockSize`, with absolute offsets.
std::vector<Trigger> collectAll(const EngineState& state, int total, int blockSize)
{
    Transport transport;
    transport.setTempo(120.0);
    transport.play();
    std::array<Trigger, 64> out{};
    std::vector<Trigger> all;
    for (int start = 0; start < total; start += blockSize)
    {
        const int n = std::min(blockSize, total - start);
        const auto block = transport.advance(n, sampleRate);
        const int count = collectTriggers(state, block, n, out);
        for (int i = 0; i < count; ++i)
        {
            auto t = out[static_cast<std::size_t>(i)];
            t.sampleOffset += start;
            all.push_back(t);
        }
    }
    return all;
}

// Absolute sample positions of every trigger over `total` samples, in blocks of `blockSize`.
std::vector<int> triggerPositions(const EngineState& state, int total, int blockSize)
{
    Transport transport;
    transport.setTempo(120.0);
    transport.play();
    std::array<Trigger, 64> out{};
    std::vector<int> positions;
    for (int start = 0; start < total; start += blockSize)
    {
        const int n = std::min(blockSize, total - start);
        const auto block = transport.advance(n, sampleRate);
        const int count = collectTriggers(state, block, n, out);
        for (int i = 0; i < count; ++i)
            positions.push_back(start + out[static_cast<std::size_t>(i)].sampleOffset);
    }
    return positions;
}

} // namespace

TEST_CASE("SpscQueue is FIFO and reports when full")
{
    SpscQueue<int, 3> queue;
    CHECK(queue.hasSpace());
    CHECK(queue.push(1));
    CHECK(queue.push(2));
    CHECK(queue.push(3));
    CHECK_FALSE(queue.hasSpace());
    CHECK_FALSE(queue.push(4));
    CHECK(queue.pop() == 1);
    CHECK(queue.push(4));
    CHECK(queue.pop() == 2);
    CHECK(queue.pop() == 3);
    CHECK(queue.pop() == 4);
    CHECK_FALSE(queue.pop().has_value());
}

TEST_CASE("Steps trigger on the exact sample regardless of block size")
{
    const auto state = stateWithSteps({0, 4, 6});
    for (int blockSize : {32, 441, 512, 8192})
    {
        INFO("block size " << blockSize);
        const auto positions = triggerPositions(state, 16 * samplesPerStep, blockSize);
        REQUIRE(positions.size() == 3);
        CHECK(positions[0] == 0);
        CHECK(positions[1] == 4 * samplesPerStep);
        CHECK(positions[2] == 6 * samplesPerStep);
    }
}

TEST_CASE("Playhead doesn't drift over a long run")
{
    // 10 minutes of 64-sample blocks: step 0 of bar 300 must still land on its exact sample.
    const auto state = stateWithSteps({0});
    const int total = 300 * 16 * samplesPerStep + 1;
    const auto positions = triggerPositions(state, total, 64);
    REQUIRE(positions.size() == 301);
    CHECK(positions.back() == 300 * 16 * samplesPerStep);
}

TEST_CASE("Pattern loops after its length")
{
    const auto state = stateWithSteps({1}, 8);
    const auto positions = triggerPositions(state, 3 * 8 * samplesPerStep, 512);
    REQUIRE(positions.size() == 3);
    CHECK(positions[1] == (8 + 1) * samplesPerStep);
    CHECK(positions[2] == (16 + 1) * samplesPerStep);
}

TEST_CASE("Pattern mode plays the current pattern only")
{
    auto state = stateWithSteps({0});
    state.patterns.push_back(patternWithSteps({8}));
    state.currentPattern = 1;
    const auto positions = triggerPositions(state, 16 * samplesPerStep, 512);
    REQUIRE(positions.size() == 1);
    CHECK(positions[0] == 8 * samplesPerStep);
}

TEST_CASE("Song mode plays clips at their positions and loops the song")
{
    EngineState state = stateWithSteps({0});         // pattern 0: hit on step 0
    state.patterns.push_back(patternWithSteps({4})); // pattern 1: hit on step 4
    state.songMode = true;
    state.clips = {{0, 0, 16 * tps}, {1, 16 * tps, 16 * tps}, {0, 48 * tps, 16 * tps}};
    state.songLengthTicks = 64 * tps; // bar 1, bar 2, bar 4 (bar 3 empty)

    const auto positions = triggerPositions(state, 2 * 64 * samplesPerStep, 512);
    const std::vector<int> oneLoop{0, (16 + 4) * samplesPerStep, 48 * samplesPerStep};
    REQUIRE(positions.size() == 6);
    for (std::size_t i = 0; i < 3; ++i)
    {
        CHECK(positions[i] == oneLoop[i]);
        CHECK(positions[i + 3] == oneLoop[i] + 64 * samplesPerStep);
    }
}

TEST_CASE("A clip longer than its pattern repeats it; a shorter one cuts it")
{
    EngineState state = stateWithSteps({0, 6}, 8);
    state.songMode = true;
    state.clips = {{0, 0, 20 * tps}}; // 2.5 repeats of the 8-step pattern
    state.songLengthTicks = 32 * tps;
    const auto positions = triggerPositions(state, 32 * samplesPerStep, 512);
    const std::vector<int> expected{0, 6, 8, 14, 16}; // the hit at 22 is past the clip end
    REQUIRE(positions.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i)
        CHECK(positions[i] == expected[i] * samplesPerStep);
}

TEST_CASE("Overlapping clips on different tracks both play")
{
    EngineState state = stateWithSteps({0});
    state.songMode = true;
    state.clips = {{0, 0, 16 * tps}, {0, 0, 16 * tps}};
    state.songLengthTicks = 16 * tps;
    CHECK(triggerPositions(state, 16 * samplesPerStep, 512).size() == 2);
}

TEST_CASE("Song mode with an empty playlist is silent")
{
    EngineState state = stateWithSteps({0, 4, 8, 12});
    state.songMode = true;
    state.songLengthTicks = 0;
    CHECK(triggerPositions(state, 16 * samplesPerStep, 512).empty());
}

TEST_CASE("Notes between steps trigger on their exact sample")
{
    EngineState state = stateWithSteps({});
    state.patterns[0].notes[0] = {{12, tps, 60, 0.5f},
                                  {tps * 3 + 8, tps, 64, 1.0f}}; // half a step in, ...
    const auto triggers = collectAll(state, 16 * samplesPerStep, 441);
    REQUIRE(triggers.size() == 2);
    CHECK(triggers[0].sampleOffset == samplesPerStep / 2); // 12 ticks = half a 16th
    CHECK(triggers[0].velocity == 0.5f);
    CHECK(triggers[1].sampleOffset == 3 * samplesPerStep + samplesPerStep / 3);
    CHECK(triggers[1].pitch == 64);
}

TEST_CASE("Triggers from several clips come out in time order")
{
    EngineState state = stateWithSteps({1});
    state.patterns.push_back(patternWithSteps({0}));
    state.songMode = true;
    state.clips = {{0, 0, 16 * tps}, {1, 0, 16 * tps}}; // pattern 0 hits step 1, pattern 1 step 0
    state.songLengthTicks = 16 * tps;
    const auto positions = triggerPositions(state, 16 * samplesPerStep, 16 * samplesPerStep);
    REQUIRE(positions.size() == 2);
    CHECK(positions[0] == 0);
    CHECK(positions[1] == samplesPerStep);
}

TEST_CASE("Muted channels don't trigger")
{
    auto state = stateWithSteps({0, 4, 8, 12});
    state.channels[0].muted = true;
    CHECK(triggerPositions(state, 16 * samplesPerStep, 512).empty());
}

TEST_CASE("Sampler plays a sample once at channel gain, then frees the voice")
{
    auto state = stateWithSteps({});
    state.channels[0].gainLeft = 0.5f;
    state.channels[0].gainRight = 0.25f;

    Sampler sampler;
    sampler.prepare(sampleRate);
    sampler.trigger(state, 0);
    CHECK(sampler.activeVoiceCount() == 1);

    std::vector<float> left(256, 0.0f), right(256, 0.0f);
    float* channels[] = {left.data(), right.data()};
    sampler.render(state, channels, 2, 0, 256);

    CHECK(left[0] == Catch::Approx(0.5f));
    CHECK(right[0] == Catch::Approx(0.25f));
    CHECK(left[98] == Catch::Approx(0.5f));
    CHECK(left[150] == 0.0f); // 100-frame sample has ended
    CHECK(sampler.activeVoiceCount() == 0);
}

TEST_CASE("Sampler pitches by resampling and scales by velocity")
{
    auto state = stateWithSteps({});
    state.channels[0].sample = constantSample(1200);

    auto sounding = [&](int pitch, float velocity, float& level)
    {
        Sampler sampler;
        sampler.prepare(sampleRate);
        sampler.trigger(state, 0, pitch, velocity);
        std::vector<float> out(4000, 0.0f);
        float* channels[] = {out.data()};
        sampler.render(state, channels, 1, 0, 4000);
        level = out[0];
        return std::count_if(out.begin(), out.end(), [](float s) { return s > 0.0f; });
    };

    float level = 0.0f;
    CHECK(std::abs(sounding(60, 100.0f / 128.0f, level) - 1199) <= 1);
    CHECK(level == Catch::Approx(1.0f));                               // default velocity = unity
    CHECK(std::abs(sounding(72, 100.0f / 128.0f, level) - 600) <= 2);  // octave up: half as long
    CHECK(std::abs(sounding(48, 100.0f / 128.0f, level) - 2398) <= 3); // octave down: twice
    sounding(60, 50.0f / 128.0f, level);
    CHECK(level == Catch::Approx(0.5f));
}

TEST_CASE("Sampler converts sample rate")
{
    auto state = stateWithSteps({});
    state.channels[0].sample = constantSample(1000, sampleRate / 2.0);

    Sampler sampler;
    sampler.prepare(sampleRate);
    sampler.trigger(state, 0);

    std::vector<float> out(4000, 0.0f);
    float* channels[] = {out.data()};
    sampler.render(state, channels, 1, 0, 4000);

    // Half-rate sample plays twice as long: ~2000 output frames.
    const auto sounding = std::count_if(out.begin(), out.end(), [](float s) { return s > 0.0f; });
    CHECK(std::abs(sounding - 2000) <= 2);
}

TEST_CASE("Sampler stops voices whose channel is removed or whose sample is replaced")
{
    auto state = stateWithSteps({});
    Sampler sampler;
    sampler.prepare(sampleRate);

    sampler.trigger(state, 0);
    auto replaced = state;
    replaced.channels[0].sample = constantSample(100);
    sampler.stateChanged(replaced);
    CHECK(sampler.activeVoiceCount() == 0);

    sampler.trigger(state, 0);
    EngineState removed;
    sampler.stateChanged(removed);
    CHECK(sampler.activeVoiceCount() == 0);

    sampler.trigger(state, 0);
    sampler.stateChanged(state); // unchanged: keeps playing
    CHECK(sampler.activeVoiceCount() == 1);
}

TEST_CASE("Engine plays published state and frees retired states on the UI thread")
{
    std::weak_ptr<const SampleBuffer> watched;
    Engine engine;
    engine.prepare(sampleRate, 512);
    engine.transport().setTempo(120.0);

    {
        auto state = std::make_unique<EngineState>(stateWithSteps({0}));
        watched = state->channels[0].sample;
        engine.setState(std::move(state));
    }

    std::vector<float> out(512, 0.0f);
    float* channels[] = {out.data()};

    engine.transport().play();
    engine.process(channels, 1, 512);
    CHECK(out[0] == Catch::Approx(1.0f)); // step 0 fires at sample 0

    // Replace with an empty project: the old state (and its sample) must come back to be freed.
    engine.setState(std::make_unique<EngineState>());
    engine.process(channels, 1, 512);
    CHECK_FALSE(watched.expired()); // still in the retire queue
    engine.collectGarbage();
    CHECK(watched.expired());
}
