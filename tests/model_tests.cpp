#include "model/document.h"
#include "model/project.h"

#include "engine/drum_synth.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace jls::model;

TEST_CASE("Default project has FL's starting channels with sounds")
{
    const auto project = makeDefaultProject();
    REQUIRE(project.channels.size() == 4);
    CHECK(project.channels[0].name == "Kick");
    CHECK(project.channels[1].name == "Clap");
    CHECK(project.channels[2].name == "Hat");
    CHECK(project.channels[3].name == "Snare");
    for (const auto& channel : project.channels)
    {
        REQUIRE(channel.sample);
        CHECK(channel.sample->numFrames() > 0);
    }
}

TEST_CASE("Synthesized drums are audible, finite and don't clip")
{
    using jls::engine::DrumSound;
    for (auto sound : {DrumSound::kick, DrumSound::clap, DrumSound::hat, DrumSound::snare})
    {
        const auto buffer = jls::engine::synthesizeDrum(sound, 48000.0);
        float peak = 0.0f;
        for (float s : buffer.channels[0])
        {
            REQUIRE(std::isfinite(s));
            peak = std::max(peak, std::abs(s));
        }
        CHECK(peak > 0.1f);
        CHECK(peak <= 1.0f);
    }
}

TEST_CASE("Pattern steps are per channel and removed with the channel")
{
    auto project = makeDefaultProject();
    const auto kick = project.channels[0].id;
    const auto hat = project.channels[2].id;

    project.pattern.set(kick, 0, true);
    project.pattern.set(hat, 2, true);
    CHECK(project.pattern.isOn(kick, 0));
    CHECK_FALSE(project.pattern.isOn(kick, 2));
    CHECK(project.pattern.isOn(hat, 2));
    CHECK_FALSE(project.pattern.isOn(hat, 99));

    project.removeChannel(kick);
    CHECK(project.channels.size() == 3);
    CHECK(project.findChannel(kick) == nullptr);
    CHECK_FALSE(project.pattern.isOn(kick, 0));
}

TEST_CASE("Channel ids are never reused")
{
    Project project;
    const auto a = project.addChannel("A", nullptr);
    project.removeChannel(a);
    const auto b = project.addChannel("B", nullptr);
    CHECK(a != b);
}

TEST_CASE("Engine snapshot mirrors steps, mute, volume and pan")
{
    auto project = makeDefaultProject();
    auto& kick = project.channels[0];
    kick.volume = 0.5f;
    kick.pan = -1.0f; // hard left
    project.channels[1].muted = true;
    project.pattern.set(kick.id, 4, true);

    const auto state = toEngineState(project);
    REQUIRE(state->channels.size() == 4);
    const auto& k = state->channels[0];
    CHECK(k.steps.size() == 16);
    CHECK(k.steps[4] == 1);
    CHECK(k.steps[0] == 0);
    CHECK(k.gainLeft == Catch::Approx(0.5f));
    CHECK(k.gainRight == Catch::Approx(0.0f));
    CHECK(state->channels[1].muted);
    // Centre pan keeps full volume on both sides.
    CHECK(state->channels[2].gainLeft == Catch::Approx(state->channels[2].gainRight));
}

TEST_CASE("Document undo and redo")
{
    Document doc(makeDefaultProject());
    int changes = 0;
    doc.onChange = [&] { ++changes; };
    const auto kick = doc.project().channels[0].id;

    CHECK_FALSE(doc.canUndo());
    doc.apply([&](Project& p) { p.pattern.set(kick, 0, true); });
    doc.apply([&](Project& p) { p.pattern.set(kick, 4, true); });
    CHECK(changes == 2);

    CHECK(doc.undo());
    CHECK(doc.project().pattern.isOn(kick, 0));
    CHECK_FALSE(doc.project().pattern.isOn(kick, 4));
    CHECK(doc.redo());
    CHECK(doc.project().pattern.isOn(kick, 4));

    // A new edit after undo discards the redo history.
    doc.undo();
    doc.apply([&](Project& p) { p.pattern.set(kick, 8, true); });
    CHECK_FALSE(doc.canRedo());
    CHECK(changes == 6);
}

TEST_CASE("Edits within one gesture undo together")
{
    Document doc(makeDefaultProject());
    const auto kick = doc.project().channels[0].id;

    const int gesture = doc.newGestureId();
    for (int step = 0; step < 4; ++step)
        doc.apply([&](Project& p) { p.pattern.set(kick, step, true); }, gesture);

    doc.undo();
    for (int step = 0; step < 4; ++step)
        CHECK_FALSE(doc.project().pattern.isOn(kick, step));
    CHECK_FALSE(doc.canUndo());
}

TEST_CASE("Undo history is bounded")
{
    Document doc;
    for (std::size_t i = 0; i < Document::maxUndoSteps + 50; ++i)
        doc.apply([](Project& p) { p.addChannel("x", nullptr); });

    std::size_t undos = 0;
    while (doc.undo())
        ++undos;
    CHECK(undos == Document::maxUndoSteps);
}
