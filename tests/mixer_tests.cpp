// Mixer and effects: every built-in effect's behaviour and stability, the EffectPool, routing
// through inserts to the Master, plugin effects through the factory, and sample previews.

#include "engine/effect_pool.h"
#include "engine/effects.h"
#include "engine/engine.h"
#include "model/project.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

using namespace jls;
using engine::EffectPool;
using engine::findEffectType;

namespace
{

constexpr double rate = 48000.0;

struct Stereo
{
    std::vector<float> left, right;
    explicit Stereo(int n, float value = 0.0f) : left(n, value), right(n, value) {}
    int size() const { return static_cast<int>(left.size()); }
};

Stereo sine(double frequency, int n, float amplitude = 0.5f)
{
    Stereo s(n);
    for (int i = 0; i < n; ++i)
        s.left[i] = s.right[i] =
            amplitude * static_cast<float>(std::sin(2.0 * std::numbers::pi * frequency * i / rate));
    return s;
}

float rms(const std::vector<float>& x, int from = 0)
{
    double sum = 0.0;
    for (std::size_t i = static_cast<std::size_t>(from); i < x.size(); ++i)
        sum += static_cast<double>(x[i]) * x[i];
    return static_cast<float>(std::sqrt(sum / static_cast<double>(x.size() - from)));
}

// Runs an effect over a whole signal in 512-sample blocks.
Stereo run(const char* type, Stereo signal, std::vector<float> params)
{
    auto fx = findEffectType(type)->create();
    fx->prepare(rate, 512);
    for (int start = 0; start < signal.size(); start += 512)
    {
        const int n = std::min(512, signal.size() - start);
        fx->process(signal.left.data() + start, signal.right.data() + start, n, params);
    }
    return signal;
}

std::vector<float> defaults(const char* type)
{
    return findEffectType(type)->defaultParams();
}

} // namespace

TEST_CASE("Every effect is silent on silence and stays finite at extreme settings")
{
    for (const auto& type : engine::effectTypes())
    {
        INFO(type.name);
        CHECK(rms(run(type.id, Stereo(4800), type.defaultParams()).left) == 0.0f);

        for (bool useMax : {false, true})
        {
            std::vector<float> params;
            for (const auto& p : type.params)
                params.push_back(useMax ? p.max : p.min);
            Stereo noise(48000);
            std::uint32_t seed = 1;
            for (int i = 0; i < noise.size(); ++i)
            {
                seed = seed * 1664525u + 1013904223u;
                noise.left[i] = noise.right[i] = static_cast<float>(seed >> 8) / 8388608.0f - 1.0f;
            }
            const auto out = run(type.id, noise, params);
            for (float s : out.left)
            {
                REQUIRE(std::isfinite(s));
                REQUIRE(std::abs(s) < 100.0f);
            }
        }
    }
}

TEST_CASE("Filter: low-pass keeps lows and cuts highs, high-pass the reverse")
{
    const auto low = sine(60.0, 24000);
    const auto high = sine(6000.0, 24000);
    const std::vector<float> lowPass{300.0f, 0.0f, 0.0f};
    const std::vector<float> highPass{3000.0f, 0.0f, 2.0f};

    CHECK(rms(run("filter", low, lowPass).left, 4800) > 0.8f * rms(low.left, 4800));
    CHECK(rms(run("filter", high, lowPass).left, 4800) < 0.05f * rms(high.left, 4800));
    CHECK(rms(run("filter", high, highPass).left, 4800) > 0.8f * rms(high.left, 4800));
    CHECK(rms(run("filter", low, highPass).left, 4800) < 0.05f * rms(low.left, 4800));
}

TEST_CASE("EQ: flat settings are transparent, a low boost raises the lows")
{
    const auto bass = sine(40.0, 48000);
    CHECK(rms(run("eq", bass, defaults("eq")).left, 9600) ==
          Catch::Approx(rms(bass.left, 9600)).epsilon(0.01));
    auto boosted = defaults("eq");
    boosted[0] = 12.0f;
    CHECK(rms(run("eq", bass, boosted).left, 9600) > 3.0f * rms(bass.left, 9600));
}

TEST_CASE("Delay: echoes at the set time, each repeat scaled by the feedback")
{
    Stereo impulse(48000);
    impulse.left[0] = impulse.right[0] = 1.0f;
    const auto out = run("delay", impulse, {250.0f, 0.5f, 0.4f, 0.0f}); // 250 ms = 12000
    CHECK(out.left[0] == Catch::Approx(0.6f));                          // dry part
    CHECK(out.left[12000] == Catch::Approx(0.4f));                      // first echo, wet
    CHECK(out.left[24000] == Catch::Approx(0.2f));                      // second echo, * feedback
    CHECK(out.left[6000] == 0.0f);
}

TEST_CASE("Delay: ping-pong alternates sides")
{
    Stereo impulse(48000);
    impulse.left[0] = impulse.right[0] = 1.0f;
    const auto out = run("delay", impulse, {250.0f, 0.5f, 1.0f, 1.0f});
    CHECK(out.left[12000] > 0.0f);
    CHECK(out.right[12000] == 0.0f);
    CHECK(out.right[24000] > 0.0f);
    CHECK(out.left[24000] == 0.0f);
}

TEST_CASE("Reverb: an impulse leaves a decaying tail")
{
    Stereo impulse(96000);
    impulse.left[0] = impulse.right[0] = 1.0f;
    const auto out = run("reverb", impulse, {0.7f, 0.3f, 1.0f, 1.0f});
    std::vector<float> early(out.left.begin() + 2400, out.left.begin() + 24000);
    std::vector<float> late(out.left.begin() + 72000, out.left.end());
    CHECK(rms(early) > 0.0005f);
    CHECK(rms(late) < rms(early));
}

TEST_CASE("Compressor: reduces level above the threshold by the ratio")
{
    // -6 dBFS in, threshold -18, ratio 4: 12 dB over becomes 3 dB over, so -15 dBFS out.
    const auto loud = sine(1000.0, 48000, 0.5012f);
    const auto out = run("compressor", loud, {-18.0f, 4.0f, 1.0f, 50.0f, 0.0f});
    const float peak = *std::max_element(out.left.begin() + 24000, out.left.end());
    CHECK(20.0f * std::log10(peak) == Catch::Approx(-15.0f).margin(1.0f));
}

TEST_CASE("Soft clipper: transparent below the knee, never above the ceiling")
{
    const std::vector<float> params{0.0f, -6.0f, 0.5f}; // ceiling 0.501, knee from 0.25
    const auto quiet = sine(200.0, 4800, 0.2f);
    const auto quietOut = run("softclip", quiet, params);
    for (int i = 0; i < quiet.size(); ++i)
        REQUIRE(quietOut.left[static_cast<std::size_t>(i)] ==
                quiet.left[static_cast<std::size_t>(i)]);

    const float ceiling = std::pow(10.0f, -6.0f / 20.0f);
    const auto loud = run("softclip", sine(200.0, 4800, 4.0f), params);
    const float peak = *std::max_element(loud.left.begin(), loud.left.end());
    CHECK(peak <= ceiling); // reaches it only when driven very hard (tanh rounds to 1)
    CHECK(peak > 0.95f * ceiling);

    // The curve is continuous with slope 1 where it starts: no corner.
    Stereo ramp(1000);
    for (int i = 0; i < 1000; ++i)
        ramp.left[static_cast<std::size_t>(i)] = ramp.right[static_cast<std::size_t>(i)] =
            i / 1000.0f;
    const auto shaped = run("softclip", ramp, params);
    for (int i = 1; i < 1000; ++i)
    {
        const float slope = (shaped.left[static_cast<std::size_t>(i)] -
                             shaped.left[static_cast<std::size_t>(i - 1)]) *
                            1000.0f;
        REQUIRE(slope <= 1.0001f);
        REQUIRE(slope >= 0.0f);
    }
}

TEST_CASE("Soft clipper: drive raises the level")
{
    const auto in = sine(200.0, 4800, 0.1f);
    const auto out = run("softclip", in, {12.0f, 0.0f, 0.5f}); // +12 dB: 0.4, below the knee
    CHECK(rms(out.left) ==
          Catch::Approx(std::pow(10.0f, 12.0f / 20.0f) * rms(in.left)).epsilon(0.001));
}

TEST_CASE("New projects get a soft clipper on the Master")
{
    const auto project = model::makeDefaultProject();
    REQUIRE(project.mixer.tracks[0].effects.size() == 1);
    CHECK(project.mixer.tracks[0].effects[0].type == "softclip");
}

TEST_CASE("EffectPool keeps one processor per effect id")
{
    EffectPool pool;
    auto a = pool.get(1, "reverb");
    REQUIRE(a);
    CHECK(pool.get(1, "reverb") == a);
    CHECK(pool.get(1, "delay") != a); // type changed: new processor
    CHECK(pool.get(2, "nonsense") == nullptr);

    pool.collectUnused(); // both were used since the last collect (2 as a remembered failure)
    CHECK(pool.size() == 2);
    pool.collectUnused(); // not used since
    CHECK(pool.size() == 0);

    auto b = pool.get(3, "eq");
    pool.setFormat(96000.0, 256);
    CHECK(pool.get(3, "eq") != b); // re-created for the new format
}

namespace
{

// One channel, one hit at the start, routed through the mixer as configured.
float renderPeak(model::Project& project, engine::Engine& engine, EffectPool* pool = nullptr)
{
    auto sample = std::make_shared<engine::SampleBuffer>();
    sample->sampleRate = rate;
    sample->channels.assign(1, std::vector<float>(1000, 1.0f));
    project.channels[0].sample = sample;
    project.channels[0].volume = 1.0f;
    project.current().set(project.channels[0].id, 0, true);

    engine.setState(model::toEngineState(project, pool));
    engine.transport().play();
    std::vector<float> left(512, 0.0f), right(512, 0.0f);
    float* out[] = {left.data(), right.data()};
    engine.process(out, 2, 512);
    return *std::max_element(left.begin(), left.end());
}

model::Project oneChannelProject()
{
    model::Project project;
    project.addChannel("Test", nullptr);
    return project;
}

} // namespace

TEST_CASE("Channels play through their insert and the Master")
{
    auto project = oneChannelProject();
    REQUIRE(project.channels[0].mixerTrack == 1);

    SECTION("unity")
    {
        engine::Engine engine;
        engine.prepare(rate, 512);
        CHECK(renderPeak(project, engine) == Catch::Approx(1.0f));
        CHECK(engine.takePeak(1, 0) == Catch::Approx(1.0f));
        CHECK(engine.takePeak(0, 0) == Catch::Approx(1.0f));
        CHECK(engine.takePeak(1, 0) == 0.0f); // reading resets the meter
        CHECK(engine.takePeak(2, 0) == 0.0f); // nothing on insert 2
    }
    SECTION("insert and master faders multiply")
    {
        project.mixer.tracks[1].volumeDb = -6.0206f;
        project.mixer.tracks[0].volumeDb = -6.0206f;
        engine::Engine engine;
        engine.prepare(rate, 512);
        CHECK(renderPeak(project, engine) == Catch::Approx(0.25f).epsilon(0.001));
    }
    SECTION("muting the insert silences it")
    {
        project.mixer.tracks[1].muted = true;
        engine::Engine engine;
        engine.prepare(rate, 512);
        CHECK(renderPeak(project, engine) == 0.0f);
    }
    SECTION("soloing another insert silences it")
    {
        project.mixer.tracks[3].solo = true;
        engine::Engine engine;
        engine.prepare(rate, 512);
        CHECK(renderPeak(project, engine) == 0.0f);
    }
    SECTION("soloing its own insert keeps it")
    {
        project.mixer.tracks[1].solo = true;
        engine::Engine engine;
        engine.prepare(rate, 512);
        CHECK(renderPeak(project, engine) == Catch::Approx(1.0f));
    }
    SECTION("an effect on the insert is applied")
    {
        // Compressor with ratio 1 is a clean gain: +6 dB makeup doubles the level.
        const auto id = project.addEffect(1, "compressor");
        project.findEffect(id)->params = {0.0f, 1.0f, 1.0f, 10.0f, 6.0206f};
        EffectPool pool;
        pool.setFormat(rate, 512);
        engine::Engine engine;
        engine.prepare(rate, 512);
        CHECK(renderPeak(project, engine, &pool) == Catch::Approx(2.0f).epsilon(0.001));
    }
}

TEST_CASE("A track's width narrows to mono or widens the stereo image")
{
    // A channel panned hard left: all left, nothing right, through insert 1.
    auto project = oneChannelProject();
    project.channels[0].pan = -1.0f;
    const auto render = [&project](float width)
    {
        project.mixer.tracks[1].width = width;
        engine::Engine engine;
        engine.prepare(rate, 512);
        renderPeak(project, engine);
        // renderPeak played the first block; render one more and read a sample from the middle.
        std::vector<float> left(512, 0.0f), right(512, 0.0f);
        float* out[] = {left.data(), right.data()};
        engine.process(out, 2, 512);
        return std::pair{left[100], right[100]};
    };
    const auto [unityLeft, unityRight] = render(1.0f);
    CHECK(unityLeft == Catch::Approx(1.0f));
    CHECK(unityRight == Catch::Approx(0.0f).margin(1e-6));
    const auto [monoLeft, monoRight] = render(0.0f);
    CHECK(monoLeft == Catch::Approx(0.5f)); // both sides get the mid
    CHECK(monoRight == Catch::Approx(0.5f));
    const auto [wideLeft, wideRight] = render(2.0f);
    CHECK(wideLeft == Catch::Approx(1.5f)); // the side doubled
    CHECK(wideRight == Catch::Approx(-0.5f));
}

TEST_CASE("Mixer model: routing, effect slots and snapshot gains")
{
    auto project = model::makeDefaultProject();
    for (int i = 0; i < 4; ++i)
        CHECK(project.channels[static_cast<std::size_t>(i)].mixerTrack == i + 1);
    project.addChannel("Fifth", nullptr);
    CHECK(project.channels.back().mixerTrack == 5);
    CHECK(project.mixer.tracks.size() == model::Mixer::numInserts + 1);
    CHECK(project.mixer.tracks[0].name == "Master");

    for (int i = 0; i < model::MixerTrack::maxEffects; ++i)
        CHECK(project.addEffect(2, "delay") != 0);
    CHECK(project.addEffect(2, "delay") == 0); // full
    CHECK(project.addEffect(3, "nonsense") == 0);

    const auto id = project.mixer.tracks[2].effects[3].id;
    int track = -1;
    REQUIRE(project.findEffect(id, &track) != nullptr);
    CHECK(track == 2);
    project.removeEffect(id);
    CHECK(project.findEffect(id) == nullptr);
    CHECK(project.mixer.tracks[2].effects.size() == 7);

    project.mixer.tracks[4].pan = -1.0f;
    project.mixer.tracks[5].volumeDb = model::MixerTrack::minVolumeDb;
    const auto state = model::toEngineState(project);
    CHECK(state->tracks[4].gainRight == 0.0f);
    CHECK(state->tracks[4].gainLeft == Catch::Approx(1.0f));
    CHECK(state->tracks[5].gainLeft == 0.0f); // bottom of the fader is silence
    CHECK(state->channels[2].mixerTrack == 3);
}

TEST_CASE("Effect processors carry over between snapshots")
{
    auto project = oneChannelProject();
    const auto id = project.addEffect(1, "reverb");
    EffectPool pool;
    const auto first = model::toEngineState(project, &pool);
    project.findEffect(id)->params[3] = 0.9f; // turn a knob
    const auto second = model::toEngineState(project, &pool);
    REQUIRE(first->tracks[1].effects.size() == 1);
    CHECK(first->tracks[1].effects[0].processor == second->tracks[1].effects[0].processor);
    CHECK(second->tracks[1].effects[0].params[3] == 0.9f);

    project.removeEffect(id);
    const auto third = model::toEngineState(project, &pool);
    CHECK(third->tracks[1].effects.empty());
    CHECK(pool.size() == 0);
}

namespace
{

// Stands in for a hosted plugin: doubles the signal and reports a state string.
class FakePlugin final : public engine::EffectProcessor
{
  public:
    explicit FakePlugin(std::string s) : state(std::move(s)) {}
    void prepare(double, int) override {}
    void reset() override {}
    void process(float* l, float* r, int n, std::span<const float>) override
    {
        for (int i = 0; i < n; ++i)
        {
            l[i] *= 2.0f;
            r[i] *= 2.0f;
        }
    }
    std::string saveState() const override { return state + "+live"; }
    std::string state;
};

} // namespace

TEST_CASE("Plugin effects are created by the pool's plugin factory")
{
    EffectPool pool;
    int created = 0;
    pool.setPluginFactory(
        [&created](const std::string& id, const std::string& state, double,
                   int) -> std::unique_ptr<engine::EffectProcessor>
        {
            ++created;
            if (id != "known")
                return nullptr;
            return std::make_unique<FakePlugin>(state);
        });

    auto a = pool.get(1, engine::pluginEffectType, "known", "saved");
    REQUIRE(a);
    CHECK(a->saveState() == "saved+live");
    CHECK(pool.get(1, engine::pluginEffectType, "known", "saved") == a);
    CHECK(pool.find(1) == a);

    CHECK(pool.get(2, engine::pluginEffectType, "missing") == nullptr);
    CHECK(pool.get(2, engine::pluginEffectType, "missing") == nullptr);
    CHECK(created == 2); // the failure is remembered, not retried on every snapshot
}

TEST_CASE("A plugin effect on an insert processes audio; a missing one passes it through")
{
    auto project = oneChannelProject();
    project.addPluginEffect(1, "known", "Doubler");
    EffectPool pool;
    pool.setFormat(rate, 512);
    pool.setPluginFactory(
        [](const std::string& id, const std::string& state, double,
           int) -> std::unique_ptr<engine::EffectProcessor>
        { return id == "known" ? std::make_unique<FakePlugin>(state) : nullptr; });
    {
        engine::Engine engine;
        engine.prepare(rate, 512);
        CHECK(renderPeak(project, engine, &pool) == Catch::Approx(2.0f));
    }
    project.mixer.tracks[1].effects[0].pluginId = "not-installed";
    {
        engine::Engine engine;
        engine.prepare(rate, 512);
        CHECK(renderPeak(project, engine, &pool) == Catch::Approx(1.0f));
    }
}

TEST_CASE("Sample previews play a range of a sample to the output, then stop")
{
    engine::SampleBuffer sample;
    sample.sampleRate = rate;
    sample.channels.assign(2, std::vector<float>(1000, 0.5f));

    engine::PreviewPlayer preview;
    preview.prepare(rate);
    std::vector<float> left(512, 0.0f), right(512, 0.0f);
    float* out[] = {left.data(), right.data()};

    const auto first = preview.play(&sample, 100.0, 400.0);
    REQUIRE(first == 1);
    preview.render(out, 2, 512);
    CHECK(preview.consumed() == 1);
    CHECK(left[0] == Catch::Approx(0.5f * engine::PreviewPlayer::gain));
    CHECK(left[299] != 0.0f);
    CHECK(left[300] == 0.0f); // the range ended
    CHECK_FALSE(preview.isPlaying());

    std::fill(left.begin(), left.end(), 0.0f);
    preview.play(&sample, 0.0, 1000.0);
    preview.render(out, 2, 100);
    CHECK(preview.isPlaying());
    CHECK(preview.position() == Catch::Approx(100.0));
    const auto stopped = preview.stop();
    preview.render(out, 2, 100);
    CHECK(preview.consumed() == stopped); // the sample may be freed now
    CHECK_FALSE(preview.isPlaying());
}
