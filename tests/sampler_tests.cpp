// The Sampler's time-stretching and pitch: the stretching itself (offline, Signalsmith), the
// speed / length maths per mode, and how snapshots pick stretched samples.

#include "engine/engine.h"
#include "engine/time_stretch.h"
#include "model/project.h"
#include "model/project_file.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <numbers>

using namespace jls;
using Catch::Approx;

namespace
{

constexpr double rate = 44100.0;

std::shared_ptr<engine::SampleBuffer> sine(double frequency, double seconds)
{
    auto s = std::make_shared<engine::SampleBuffer>();
    s->sampleRate = rate;
    const auto n = static_cast<std::size_t>(seconds * rate);
    s->channels.assign(1, std::vector<float>(n));
    for (std::size_t i = 0; i < n; ++i)
        s->channels[0][i] =
            0.5f * static_cast<float>(std::sin(2.0 * std::numbers::pi * frequency * i / rate));
    return s;
}

// Frequency from upward zero crossings in the middle half (away from the edges).
double frequencyOf(const engine::SampleBuffer& s)
{
    const auto& x = s.channels[0];
    const std::size_t from = x.size() / 4, to = 3 * x.size() / 4;
    int crossings = 0;
    std::size_t first = 0, last = 0;
    for (std::size_t i = from + 1; i < to; ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
        {
            if (crossings == 0)
                first = i;
            last = i;
            ++crossings;
        }
    return crossings > 1 ? (crossings - 1) * s.sampleRate / static_cast<double>(last - first) : 0.0;
}

} // namespace

TEST_CASE("Stretching changes the length but not the pitch; transposing the reverse")
{
    const auto tone = sine(441.0, 1.0);
    const auto longer = engine::stretchSample(*tone, 2.0, 0.0);
    REQUIRE(longer != nullptr);
    CHECK(longer->numFrames() == 2 * tone->numFrames());
    CHECK(frequencyOf(*longer) == Approx(441.0).epsilon(0.02));

    const auto higher = engine::stretchSample(*tone, 1.0, 12.0);
    REQUIRE(higher != nullptr);
    CHECK(higher->numFrames() == tone->numFrames());
    CHECK(frequencyOf(*higher) == Approx(882.0).epsilon(0.02));

    CHECK(engine::stretchSample(*sine(441.0, 0.001), 2.0, 0.0) == nullptr); // too short
}

TEST_CASE("Sampler modes: speed and played length at the project tempo")
{
    model::Project project;
    project.tempo = 120.0;
    const auto id = project.addChannel("loop", sine(441.0, 2.0)); // 2 s, made at 100 BPM
    auto& channel = *project.findChannel(id);
    channel.sampler.originalTempo = 100.0;

    channel.sampler.mode = model::StretchMode::off;
    CHECK(project.playbackSpeed(channel) == 1.0);
    CHECK(project.playedSeconds(channel) == Approx(2.0));

    channel.sampler.mode = model::StretchMode::resample; // 20% faster song: 20% faster sample
    CHECK(project.playbackSpeed(channel) == Approx(1.2));
    CHECK(project.playedSeconds(channel) == Approx(2.0 / 1.2));

    channel.sampler.mode = model::StretchMode::stretch; // same length change, pitch kept
    CHECK(project.playbackSpeed(channel) == 1.0);
    CHECK(project.stretchFactor(channel) == Approx(100.0 / 120.0));
    CHECK(project.playedSeconds(channel) == Approx(2.0 / 1.2));

    channel.sampler.mode = model::StretchMode::off; // pitch alone changes speed in Off
    channel.sampler.pitch = 12.0;
    CHECK(project.playbackSpeed(channel) == Approx(2.0));
}

TEST_CASE("Snapshots use the stretched sample when it's ready, and resample until then")
{
    model::Project project;
    project.tempo = 120.0;
    const auto id = project.addChannel("loop", sine(441.0, 1.0));
    auto& channel = *project.findChannel(id);
    channel.sampler.mode = model::StretchMode::stretch;
    channel.sampler.originalTempo = 100.0;

    auto waiting =
        model::toEngineState(project, nullptr, [](const model::Channel&) { return nullptr; });
    CHECK(waiting->channels[0].sample == channel.sample);
    CHECK(waiting->channels[0].speed == Approx(1.2)); // in time, pitch moves for now

    const auto done = engine::stretchSample(*channel.sample, project.stretchFactor(channel), 0.0);
    auto ready =
        model::toEngineState(project, nullptr, [&](const model::Channel&) { return done; });
    CHECK(ready->channels[0].sample == done);
    CHECK(ready->channels[0].speed == 1.0);

    // At its original tempo nothing needs stretching.
    project.tempo = 100.0;
    auto same = model::toEngineState(project, nullptr, [&](const model::Channel&) { return done; });
    CHECK(same->channels[0].sample == channel.sample);
}

TEST_CASE("A Resample-mode audio clip plays faster at a faster tempo")
{
    // A ramp sample: the output value tells which frame is playing.
    auto ramp = std::make_shared<engine::SampleBuffer>();
    ramp->sampleRate = 48000.0;
    ramp->channels.assign(1, std::vector<float>(48000));
    for (int i = 0; i < 48000; ++i)
        ramp->channels[0][static_cast<std::size_t>(i)] = static_cast<float>(i) / 48000.0f;

    model::Project project;
    project.tempo = 120.0;
    const auto id = project.audioClipChannel("/s/r.wav", "r", ramp); // made at 120 BPM
    project.findChannel(id)->sampler.mode = model::StretchMode::resample;
    project.findChannel(id)->volume = 1.0f;
    project.tempo = 240.0; // twice as fast
    project.playlist.clips.push_back({0, 0, 0, 16, id});
    project.songMode = true;

    engine::Engine engine;
    engine.prepare(48000.0, 512);
    engine.transport().setTempo(240.0);
    engine.setState(model::toEngineState(project));
    engine.transport().play();
    std::vector<float> left(512), right(512);
    float* out[] = {left.data(), right.data()};
    engine.process(out, 2, 512);
    CHECK(left[100] == Approx(200.0f / 48000.0f).margin(1e-5)); // frame 200 after 100 samples
}

TEST_CASE("Sampler settings are saved")
{
    model::Project project;
    const auto id = project.addChannel("loop", nullptr);
    project.findChannel(id)->sampler = {model::StretchMode::stretch, 92.5, -3.0};
    const auto loaded = model::loadProject(model::saveProject(project, "/p/s.jlp"), "/p/s.jlp", {});
    REQUIRE(loaded.project);
    const auto& s = loaded.project->findChannel(id)->sampler;
    CHECK(s.mode == model::StretchMode::stretch);
    CHECK(s.originalTempo == Approx(92.5));
    CHECK(s.pitch == Approx(-3.0));
}
