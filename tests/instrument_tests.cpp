// Plugin instruments without plugins: a fake InstrumentProcessor records the note events the
// engine sends it, so timing, note-offs and the "no hanging notes" rules can be checked exactly.

#include "engine/effect_pool.h"
#include "engine/engine.h"
#include "model/project.h"
#include "model/project_file.h"
#include "model/render.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using namespace jls;
using engine::NoteEvent;

namespace
{

constexpr double rate = 48000.0;
constexpr int block = 512;
// At 120 BPM and 48 kHz: 24000 samples per beat, 6000 per step, 250 per tick.
constexpr int samplesPerStep = 6000;
constexpr int samplesPerTick = 250;

struct Recorded
{
    NoteEvent::Kind kind;
    long long sample; // since the instrument was created
    int pitch;
    float value = 0.0f; // velocity / controller value / bend; not compared
    bool operator==(const Recorded& o) const
    {
        return kind == o.kind && sample == o.sample && pitch == o.pitch;
    }
};

using Log = std::vector<Recorded>;

// Records events and outputs 0.25 while any key is down.
class FakeInstrument final : public engine::InstrumentProcessor
{
  public:
    explicit FakeInstrument(std::shared_ptr<Log> l) : log(std::move(l)) {}
    void prepare(double, int) override {}
    void reset() override {}
    void process(std::span<const NoteEvent> events, float* left, float* right,
                 int numSamples) override
    {
        std::size_t next = 0;
        for (int i = 0; i < numSamples; ++i)
        {
            for (; next < events.size() && events[next].sampleOffset == i; ++next)
            {
                const auto& e = events[next];
                log->push_back({e.kind, position + i, e.pitch, e.velocity});
                if (e.kind == NoteEvent::Kind::noteOn)
                    ++down;
                else if (e.kind == NoteEvent::Kind::noteOff)
                    down = std::max(0, down - 1);
                else
                    down = 0;
            }
            left[i] = right[i] = down > 0 ? 0.25f : 0.0f;
        }
        REQUIRE(next == events.size()); // events arrive sorted and inside the block
        position += numSamples;
    }
    std::string saveState() const override { return "fake-state"; }

  private:
    std::shared_ptr<Log> log;
    long long position = 0;
    int down = 0;
};

engine::EffectPool::InstrumentFactory fakeFactory(std::shared_ptr<Log> log)
{
    return [log](const std::string& id, const std::string&, double,
                 int) -> std::unique_ptr<engine::InstrumentProcessor>
    {
        if (id == "missing")
            return nullptr;
        return std::make_unique<FakeInstrument>(log);
    };
}

struct Rig
{
    std::shared_ptr<Log> log = std::make_shared<Log>();
    engine::EffectPool pool;
    engine::Engine engine;
    model::Project project;
    model::ChannelId synth = 0;

    Rig()
    {
        pool.setFormat(rate, block);
        pool.setInstrumentFactory(fakeFactory(log));
        engine.prepare(rate, block);
        engine.transport().setTempo(120.0);
        synth = project.addInstrumentChannel("fake:synth", "Synth");
    }

    void addNote(int startTick, int lengthTicks, int pitch = 60)
    {
        project.current().notes[synth].push_back({startTick, lengthTicks, pitch, 0.8f});
    }
    void publish() { engine.setState(model::toEngineState(project, &pool)); }

    // Renders `samples` in blocks; returns the left output.
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

TEST_CASE("Instrument notes get a note-on and a note-off on their exact samples")
{
    Rig rig;
    rig.addNote(24, 48, 64); // step 1, two steps long
    rig.publish();
    rig.engine.transport().play();
    const auto out = rig.run(4 * 16 * samplesPerStep / 4 + 2 * samplesPerStep);

    REQUIRE(rig.log->size() >= 3);
    CHECK((*rig.log)[0] == Recorded{NoteEvent::Kind::noteOn, samplesPerStep, 64});
    CHECK((*rig.log)[1] == Recorded{NoteEvent::Kind::noteOff, 3 * samplesPerStep, 64});
    // The pattern (16 steps) loops and plays the note again.
    CHECK((*rig.log)[2] == Recorded{NoteEvent::Kind::noteOn, 17 * samplesPerStep, 64});

    // The instrument's output reaches the master while the key is down, and only then.
    CHECK(out[static_cast<std::size_t>(samplesPerStep - 1)] == 0.0f);
    CHECK(out[static_cast<std::size_t>(2 * samplesPerStep)] != 0.0f);
    CHECK(out[static_cast<std::size_t>(4 * samplesPerStep)] == 0.0f);
}

TEST_CASE("Ticks between steps are honoured for note starts and ends")
{
    Rig rig;
    rig.addNote(5, 7); // 5 ticks in, 7 ticks long
    rig.publish();
    rig.engine.transport().play();
    rig.run(samplesPerStep);
    REQUIRE(rig.log->size() == 2);
    CHECK((*rig.log)[0].sample == 5 * samplesPerTick);
    CHECK((*rig.log)[1].sample == 12 * samplesPerTick);
}

TEST_CASE("A clip that ends before its note ends the note")
{
    Rig rig;
    rig.addNote(0, 8 * 24);                                                    // eight steps
    rig.project.playlist.clips.push_back({rig.project.current().id, 0, 0, 4}); // four steps
    rig.project.songMode = true;
    rig.publish();
    rig.engine.transport().play();
    rig.run(8 * samplesPerStep);
    REQUIRE(rig.log->size() == 2);
    CHECK((*rig.log)[1] == Recorded{NoteEvent::Kind::noteOff, 4 * samplesPerStep, 60});
}

TEST_CASE("Stopping the transport releases held notes")
{
    Rig rig;
    rig.addNote(0, 16 * 24); // the whole pattern
    rig.publish();
    rig.engine.transport().play();
    rig.run(10 * block);
    CHECK(rig.engine.heldNoteCount() == 1);

    rig.engine.transport().stop();
    rig.run(block);
    CHECK(rig.engine.heldNoteCount() == 0);
    const auto& log = *rig.log;
    REQUIRE(log.size() == 3);
    CHECK(log[1] == Recorded{NoteEvent::Kind::noteOff, 10 * block, 60});
    CHECK(log[2].kind == NoteEvent::Kind::allNotesOff);

    // Playing again starts from the top with a fresh note-on.
    rig.engine.transport().play();
    rig.run(block);
    REQUIRE(log.size() == 4);
    CHECK(log[3] == Recorded{NoteEvent::Kind::noteOn, 11 * block, 60});
}

TEST_CASE("Retriggering a held key ends the old note first")
{
    Rig rig;
    rig.addNote(0, 8 * 24);  // long note
    rig.addNote(2 * 24, 24); // same key, starts while the first is still down
    rig.publish();
    rig.engine.transport().play();
    rig.run(12 * samplesPerStep);

    const Log expected = {{NoteEvent::Kind::noteOn, 0, 60},
                          {NoteEvent::Kind::noteOff, 2 * samplesPerStep, 60},
                          {NoteEvent::Kind::noteOn, 2 * samplesPerStep, 60},
                          {NoteEvent::Kind::noteOff, 3 * samplesPerStep, 60}};
    CHECK(*rig.log == expected); // and no second note-off when the first note would have ended
}

TEST_CASE("Removing an instrument channel or replacing its plugin forgets its held notes")
{
    Rig rig;
    rig.addNote(0, 16 * 24);
    rig.publish();
    rig.engine.transport().play();
    rig.run(4 * block);
    CHECK(rig.engine.heldNoteCount() == 1);

    // Another plugin on the same channel: a new instrument, the old one isn't played any more.
    rig.project.findChannel(rig.synth)->setInstrument("fake:other", "Other");
    rig.publish();
    rig.run(block);
    CHECK(rig.engine.heldNoteCount() == 0);

    rig.engine.transport().stop();
    rig.run(block);
    rig.engine.transport().play();
    rig.run(4 * block);
    CHECK(rig.engine.heldNoteCount() == 1);
    rig.project.removeChannel(rig.synth);
    rig.publish();
    rig.run(block);
    CHECK(rig.engine.heldNoteCount() == 0);
}

TEST_CASE("Held notes survive edits that keep the instrument")
{
    Rig rig;
    rig.addNote(0, 4 * 24);
    rig.publish();
    rig.engine.transport().play();
    rig.run(block);

    // A channel added in front shifts the synth's index; the note still ends on time.
    rig.project.channels.insert(rig.project.channels.begin(), model::Channel{});
    rig.project.channels.front().id = 999;
    rig.publish();
    rig.run(5 * samplesPerStep);
    REQUIRE(rig.log->size() >= 2);
    CHECK((*rig.log)[1] == Recorded{NoteEvent::Kind::noteOff, 4 * samplesPerStep, 60});
}

TEST_CASE("The pool keeps one instrument per channel and drops unused ones")
{
    auto log = std::make_shared<Log>();
    engine::EffectPool pool;
    pool.setInstrumentFactory(fakeFactory(log));
    model::Project project;
    const auto a = project.addInstrumentChannel("fake:a", "A");
    const auto missing = project.addInstrumentChannel("missing", "Gone");

    auto first = model::toEngineState(project, &pool);
    REQUIRE(first->channels[0].instrument != nullptr);
    CHECK(first->channels[0].sample == nullptr);
    CHECK(first->channels[1].instrument == nullptr); // not available: silent channel
    auto second = model::toEngineState(project, &pool);
    CHECK(second->channels[0].instrument == first->channels[0].instrument);
    CHECK(pool.findInstrument(a) == first->channels[0].instrument);

    project.findChannel(a)->setInstrument("fake:b", "B");
    auto third = model::toEngineState(project, &pool);
    CHECK(third->channels[0].instrument != first->channels[0].instrument);

    project.removeChannel(a);
    project.removeChannel(missing);
    model::toEngineState(project, &pool);
    CHECK(pool.instrumentCount() == 0);
}

TEST_CASE("Instrument channels are saved and loaded; a sample turns them back into samplers")
{
    model::Project project;
    const auto id = project.addInstrumentChannel("VST3-Synth-1234", "Synth");
    project.findChannel(id)->pluginState = "c3RhdGU=";
    const auto text = model::saveProject(project, "/p/song.jls");
    const auto result = model::loadProject(text, "/p/song.jls", {});
    REQUIRE(result.project);
    CHECK(result.warnings.empty());
    const auto* loaded = result.project->findChannel(id);
    REQUIRE(loaded != nullptr);
    CHECK(loaded->isInstrument());
    CHECK(loaded->pluginId == "VST3-Synth-1234");
    CHECK(loaded->pluginName == "Synth");
    CHECK(loaded->pluginState == "c3RhdGU=");
    CHECK(loaded->samplePath.empty());

    auto channel = *loaded;
    channel.setSample(model::builtinSample("kick"), "", "kick");
    CHECK_FALSE(channel.isInstrument());
    CHECK(channel.pluginState.empty());
}

TEST_CASE("Export renders plugin instruments and releases them at the end")
{
    model::Project project;
    const auto id = project.addInstrumentChannel("fake:synth", "Synth");
    project.current().notes[id].push_back({0, 16 * 24, 60, 0.8f}); // the whole pattern
    auto log = std::make_shared<Log>();
    const auto audio = model::renderProject(project, rate, 0.5, {}, fakeFactory(log));
    CHECK(audio.left[1000] != 0.0f);
    CHECK(audio.left.back() == 0.0f); // released when the song ended
    REQUIRE(log->size() >= 2);
    CHECK(log->front().kind == NoteEvent::Kind::noteOn);
    CHECK(std::any_of(log->begin(), log->end(),
                      [](const Recorded& r) { return r.kind == NoteEvent::Kind::noteOff; }));
}

TEST_CASE("Export can reuse live instruments instead of loading them again")
{
    model::Project project;
    const auto id = project.addInstrumentChannel("fake:synth", "Synth");
    project.current().notes[id].push_back({0, 4 * 24, 60, 0.8f});

    auto log = std::make_shared<Log>();
    int created = 0;
    engine::EffectPool pool;
    pool.setFormat(rate, 256);
    pool.setInstrumentFactory(
        [&created, log](const std::string&, const std::string&, double, int)
        {
            ++created;
            return std::make_unique<FakeInstrument>(log);
        });
    const auto live = model::toEngineState(project, &pool); // what the live engine plays
    REQUIRE(created == 1);

    const auto audio = model::renderProject(project, rate, 0.1, pool, 256);
    CHECK(created == 1); // the same instance rendered the export
    CHECK(audio.left[1000] != 0.0f);
    CHECK(pool.findInstrument(id) == live->channels[0].instrument);
}

// ------------------------------------------------------------------------------- live input

namespace
{

engine::LiveEvent liveOn(model::ChannelId channel, int pitch, float velocity = 0.8f)
{
    return {engine::LiveEvent::Kind::noteOn, channel, pitch, velocity};
}

engine::LiveEvent liveOff(int pitch)
{
    return {engine::LiveEvent::Kind::noteOff, 0, pitch, 0.0f};
}

} // namespace

TEST_CASE("Live notes play instruments at the next block, also while stopped")
{
    Rig rig;
    rig.publish();
    rig.run(block); // transport stopped
    REQUIRE(rig.engine.sendLive(liveOn(rig.synth, 64)));
    const auto out = rig.run(block);
    CHECK(rig.engine.liveNoteCount() == 1);
    CHECK(out[10] != 0.0f);
    rig.engine.sendLive(liveOff(64));
    rig.run(block);
    CHECK(rig.engine.liveNoteCount() == 0);

    const Log expected = {{NoteEvent::Kind::noteOn, block, 64},
                          {NoteEvent::Kind::noteOff, 2 * block, 64}};
    CHECK(*rig.log == expected);
}

TEST_CASE("A live note ends on its channel even after the selection changed")
{
    Rig rig;
    auto otherLog = std::make_shared<Log>();
    const auto other = rig.project.addInstrumentChannel("fake:other", "Other");
    rig.pool.setInstrumentFactory(
        [&rig, otherLog](const std::string& id, const std::string&, double,
                         int) -> std::unique_ptr<engine::InstrumentProcessor>
        { return std::make_unique<FakeInstrument>(id == "fake:other" ? otherLog : rig.log); });
    rig.publish();
    rig.engine.sendLive(liveOn(rig.synth, 60));
    rig.run(block);
    rig.engine.sendLive(liveOn(other, 62)); // the user selected another channel
    rig.engine.sendLive(liveOff(60));       // releases the key pressed on the first
    rig.run(block);

    REQUIRE(rig.log->size() == 2);
    CHECK(rig.log->back().kind == NoteEvent::Kind::noteOff);
    REQUIRE(otherLog->size() == 1);
    CHECK(otherLog->front().kind == NoteEvent::Kind::noteOn);
}

TEST_CASE("Live notes trigger samples on sampler channels")
{
    Rig rig;
    const auto kick = rig.project.addChannel("Kick", model::builtinSample("kick", rate));
    rig.publish();
    rig.engine.sendLive(liveOn(kick, 60));
    const auto out = rig.run(block);
    CHECK(std::any_of(out.begin(), out.end(), [](float x) { return x != 0.0f; }));
    CHECK(rig.engine.liveNoteCount() == 0); // one-shots aren't held
}

TEST_CASE("The sustain pedal is lifted where it went down")
{
    Rig rig;
    auto otherLog = std::make_shared<Log>();
    const auto other = rig.project.addInstrumentChannel("fake:other", "Other");
    rig.pool.setInstrumentFactory(
        [&rig, otherLog](const std::string& id, const std::string&, double,
                         int) -> std::unique_ptr<engine::InstrumentProcessor>
        { return std::make_unique<FakeInstrument>(id == "fake:other" ? otherLog : rig.log); });
    rig.publish();
    using Kind = engine::LiveEvent::Kind;
    rig.engine.sendLive({Kind::controller, rig.synth, 64, 1.0f}); // pedal down on the synth
    rig.engine.sendLive({Kind::pitchBend, rig.synth, 0, 0.5f});
    rig.run(block);
    rig.engine.sendLive({Kind::controller, other, 64, 0.0f}); // up, with another selected
    rig.run(block);

    REQUIRE(rig.log->size() == 3);
    CHECK((*rig.log)[0].kind == NoteEvent::Kind::controller);
    CHECK((*rig.log)[0].value == 1.0f);
    CHECK((*rig.log)[1].kind == NoteEvent::Kind::pitchBend);
    CHECK((*rig.log)[1].value == 0.5f);
    CHECK((*rig.log)[2] == Recorded{NoteEvent::Kind::controller, block, 64});
    CHECK((*rig.log)[2].value == 0.0f);
    CHECK(otherLog->empty());
}

TEST_CASE("Releasing all live notes, and removing their channel, leaves nothing held")
{
    Rig rig;
    rig.publish();
    rig.engine.sendLive(liveOn(rig.synth, 60));
    rig.engine.sendLive(liveOn(rig.synth, 64));
    rig.run(block);
    CHECK(rig.engine.liveNoteCount() == 2);
    rig.engine.sendLive({engine::LiveEvent::Kind::releaseAll, 0, 0, 0.0f});
    rig.run(block);
    CHECK(rig.engine.liveNoteCount() == 0);
    CHECK(std::count_if(rig.log->begin(), rig.log->end(),
                        [](const Recorded& r) { return r.kind == NoteEvent::Kind::noteOff; }) == 2);

    rig.engine.sendLive(liveOn(rig.synth, 67));
    rig.run(block);
    rig.project.removeChannel(rig.synth);
    rig.publish();
    rig.run(block);
    CHECK(rig.engine.liveNoteCount() == 0);
    rig.engine.sendLive(liveOn(rig.synth, 67)); // a channel that's gone: ignored
    rig.run(block);
    CHECK(rig.engine.liveNoteCount() == 0);
}

TEST_CASE("Pressing a held key again releases it first")
{
    Rig rig;
    rig.publish();
    rig.engine.sendLive(liveOn(rig.synth, 60));
    rig.engine.sendLive(liveOn(rig.synth, 60));
    rig.run(block);
    CHECK(rig.engine.liveNoteCount() == 1);
    const Log expected = {{NoteEvent::Kind::noteOn, 0, 60},
                          {NoteEvent::Kind::noteOff, 0, 60},
                          {NoteEvent::Kind::noteOn, 0, 60}};
    CHECK(*rig.log == expected);
}

// ---------------------------------------------------------------------------- play position

namespace
{

// Remembers every position it was told about (one per block).
class PositionRecorder final : public engine::InstrumentProcessor
{
  public:
    explicit PositionRecorder(std::shared_ptr<std::vector<engine::PlayPosition>> l)
        : log(std::move(l))
    {
    }
    void prepare(double, int) override {}
    void reset() override {}
    void setPlayPosition(const engine::PlayPosition& p) override { log->push_back(p); }
    void process(std::span<const NoteEvent>, float* left, float* right, int n) override
    {
        std::fill_n(left, n, 0.0f);
        std::fill_n(right, n, 0.0f);
    }

  private:
    std::shared_ptr<std::vector<engine::PlayPosition>> log;
};

} // namespace

TEST_CASE("Instruments are told the tempo and the position within the loop")
{
    Rig rig;
    auto positions = std::make_shared<std::vector<engine::PlayPosition>>();
    rig.pool.setInstrumentFactory([positions](const std::string&, const std::string&, double, int)
                                  { return std::make_unique<PositionRecorder>(positions); });
    rig.publish();

    rig.run(block);
    REQUIRE(positions->size() == 1);
    CHECK_FALSE(positions->back().playing);
    CHECK(positions->back().bpm == 120.0);
    CHECK(positions->back().loopLengthBeats == 4.0); // one 16-step pattern

    rig.engine.transport().play();
    // 4 beats (one loop) + 1.5 beats, in whole blocks: 5.5 * 24000 = 132000 samples.
    const int blocks = 132000 / block; // 257 blocks = 131584 samples
    rig.run(blocks * block);
    const auto& p = positions->back(); // the start of the last block
    const double expectedBeat = std::fmod((blocks - 1) * block / 24000.0, 4.0);
    CHECK(p.playing);
    CHECK(std::abs(p.beat - expectedBeat) < 1e-9);
    CHECK(std::abs(p.seconds - expectedBeat * 0.5) < 1e-9); // 120 BPM: half a second per beat
    CHECK(p.sample == std::llround(expectedBeat * 24000.0));
    CHECK(p.barStartBeat() == 0.0);
}

TEST_CASE("In song mode the song is the loop; bars follow the time signature")
{
    Rig rig;
    auto positions = std::make_shared<std::vector<engine::PlayPosition>>();
    rig.pool.setInstrumentFactory([positions](const std::string&, const std::string&, double, int)
                                  { return std::make_unique<PositionRecorder>(positions); });
    rig.project.beatsPerBar = 3;
    rig.engine.transport().setBeatsPerBar(3);
    rig.project.playlist.clips.push_back({rig.project.current().id, 0, 0, 24}); // 6 beats
    rig.project.songMode = true;
    rig.publish();
    rig.engine.transport().play();
    rig.run(4 * 24000 + block); // past beat 4

    const auto& p = positions->back();
    CHECK(p.loopLengthBeats == 6.0); // 24 steps = 2 bars of 3/4
    CHECK(p.beatsPerBar == 3);
    CHECK(p.beat >= 4.0);
    CHECK(p.barStartBeat() == 3.0);
}
