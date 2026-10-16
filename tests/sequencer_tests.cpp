#include "engine/engine.h"
#include "engine/sampler.h"
#include "engine/spsc_queue.h"
#include "engine/step_sequencer.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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

EngineState stateWithSteps(std::vector<int> onSteps, int length = 16)
{
    EngineState state;
    state.patternLengthInSteps = length;
    ChannelState channel;
    channel.id = 1;
    channel.sample = constantSample(100);
    channel.steps.assign(static_cast<std::size_t>(length), 0);
    for (int s : onSteps)
        channel.steps[static_cast<std::size_t>(s)] = 1;
    state.channels.push_back(channel);
    return state;
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
    engine.prepare(sampleRate);
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
