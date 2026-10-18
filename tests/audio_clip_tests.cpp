// Audio clips: a channel's sample placed in the Playlist and played in song time. A ramp sample
// (value rises with the frame number) shows exactly which part of the sample is heard.

#include "engine/engine.h"
#include "model/project.h"
#include "model/project_file.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>
#include <vector>

using namespace jls;
using Catch::Approx;

namespace
{

constexpr double rate = 48000.0;
constexpr int block = 512;
constexpr int samplesPerStep = 6000; // 120 BPM at 48 kHz
constexpr float channelGain = 0.8f;  // a new channel's volume

// 1 s at 48 kHz: frame i has the value 0.5 + i / 96000.
std::shared_ptr<const engine::SampleBuffer> ramp()
{
    auto sample = std::make_shared<engine::SampleBuffer>();
    sample->sampleRate = rate;
    sample->channels.assign(1, std::vector<float>(48000));
    for (int i = 0; i < 48000; ++i)
        sample->channels[0][static_cast<std::size_t>(i)] = 0.5f + static_cast<float>(i) / 96000.0f;
    return sample;
}

float rampAt(double frame)
{
    return channelGain * (0.5f + static_cast<float>(frame) / 96000.0f);
}

struct Song
{
    engine::Engine engine;
    model::Project project;
    model::ChannelId audio = 0;

    Song(int startStep, int lengthSteps)
    {
        engine.prepare(rate, block);
        engine.transport().setTempo(120.0);
        project.tempo = 120.0;
        audio = project.audioClipChannel("/samples/ramp.wav", "ramp", ramp());
        project.playlist.clips.push_back({0, 0, startStep, lengthSteps, audio});
        project.songMode = true;
        engine.setState(model::toEngineState(project));
    }

    std::vector<float> run(int samples)
    {
        std::vector<float> left(static_cast<std::size_t>(samples)), right(left.size());
        for (int start = 0; start < samples; start += block)
        {
            float* out[] = {left.data() + start, right.data() + start};
            engine.process(out, 2, std::min(block, samples - start));
        }
        return left;
    }
};

} // namespace

TEST_CASE("An audio clip plays its sample from the clip's start, on the exact sample")
{
    Song song(4, 8); // one second long, like the sample
    song.engine.transport().play();
    const auto out = song.run(16 * samplesPerStep);
    const auto at = [&](int i) { return out[static_cast<std::size_t>(i)]; };

    CHECK(at(4 * samplesPerStep - 1) == 0.0f);
    CHECK(at(4 * samplesPerStep) == Approx(rampAt(0)).margin(1e-4));
    CHECK(at(4 * samplesPerStep + 1000) == Approx(rampAt(1000)).margin(1e-4));
    CHECK(at(12 * samplesPerStep + 200) == 0.0f); // the sample (and the clip) are over
}

TEST_CASE("A clip shorter than its sample cuts it off (with a short fade)")
{
    Song song(0, 2); // a quarter of the sample
    song.engine.transport().play();
    const auto out = song.run(4 * samplesPerStep);
    CHECK(out[2 * samplesPerStep - 300] == Approx(rampAt(2 * samplesPerStep - 300)).margin(1e-4));
    CHECK(out[2 * samplesPerStep + 10] == 0.0f);
}

TEST_CASE("Starting playback in the middle of an audio clip plays from that point")
{
    Song song(0, 8);
    song.engine.transport().seek(1.0); // 1 beat = 4 steps = half a second into the clip
    song.engine.transport().play();
    const auto out = song.run(block);
    CHECK(out[0] == Approx(rampAt(4 * samplesPerStep)).margin(1e-4));
    CHECK(out[100] == Approx(rampAt(4 * samplesPerStep + 100)).margin(1e-4));
}

TEST_CASE("Stopping or jumping fades audio clips out")
{
    Song song(0, 8);
    song.engine.transport().play();
    song.run(10 * block);
    song.engine.transport().stop();
    const auto out = song.run(block);
    CHECK(out[0] != 0.0f);   // fading...
    CHECK(out[200] == 0.0f); // ...and gone well within a block
}

TEST_CASE("Audio clips don't play in pattern mode, nor when their channel is muted")
{
    Song song(0, 8);
    song.project.songMode = false;
    song.engine.setState(model::toEngineState(song.project));
    song.engine.transport().play();
    auto out = song.run(4 * block);
    CHECK(std::all_of(out.begin(), out.end(), [](float x) { return x == 0.0f; }));

    Song muted(0, 8);
    muted.project.findChannel(muted.audio)->muted = true;
    muted.engine.setState(model::toEngineState(muted.project));
    muted.engine.transport().play();
    out = muted.run(4 * block);
    CHECK(std::all_of(out.begin(), out.end(), [](float x) { return x == 0.0f; }));
}

TEST_CASE("Audio clip channels: one per file, removed with their clips, saved and loaded")
{
    model::Project project;
    const auto a = project.audioClipChannel("/s/vox.wav", "vox", ramp());
    CHECK(project.audioClipChannel("/s/vox.wav", "vox", ramp()) == a); // same file, same source
    CHECK(project.findChannel(a)->audioClip);
    project.playlist.clips.push_back({0, 2, 16, 8, a});

    const auto text = model::saveProject(project, "/s/song.jlp");
    const auto loaded =
        model::loadProject(text, "/s/song.jlp", [](const std::string&) { return ramp(); });
    REQUIRE(loaded.project);
    // (The only warning: /s/vox.wav doesn't exist on this machine. The clip is kept.)
    CHECK(loaded.warnings.size() == 1);
    const auto& clip = loaded.project->playlist.clips.at(0);
    CHECK(clip.audio == a);
    CHECK(clip.track == 2);
    CHECK(loaded.project->findChannel(a)->audioClip);

    project.removeChannel(a);
    CHECK(project.playlist.clips.empty());
}

TEST_CASE("A trimmed audio clip starts later in its sample")
{
    Song song(0, 4);
    song.project.playlist.clips[0].offsetSteps = 4; // left edge dragged 4 steps in
    song.engine.setState(model::toEngineState(song.project));
    song.engine.transport().play();
    auto out = song.run(block);
    CHECK(out[0] == Approx(rampAt(4 * samplesPerStep)).margin(1e-4));

    // Starting in the middle of a trimmed clip adds both offsets.
    Song mid(0, 4);
    mid.project.playlist.clips[0].offsetSteps = 2;
    mid.engine.setState(model::toEngineState(mid.project));
    mid.engine.transport().seek(0.25); // 1 step in
    mid.engine.transport().play();
    out = mid.run(block);
    CHECK(out[0] == Approx(rampAt(3 * samplesPerStep)).margin(1e-4));

    // Saved with the project.
    const auto loaded = model::loadProject(model::saveProject(mid.project, "/s/t.jlp"), "/s/t.jlp",
                                           [](const std::string&) { return ramp(); });
    CHECK(loaded.project->playlist.clips.at(0).offsetSteps == 2);
}
