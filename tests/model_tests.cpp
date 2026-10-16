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

    project.current().set(kick, 0, true);
    project.current().set(hat, 2, true);
    CHECK(project.current().isOn(kick, 0));
    CHECK_FALSE(project.current().isOn(kick, 2));
    CHECK(project.current().isOn(hat, 2));
    CHECK_FALSE(project.current().isOn(hat, 99));

    project.removeChannel(kick);
    CHECK(project.channels.size() == 3);
    CHECK(project.findChannel(kick) == nullptr);
    CHECK_FALSE(project.current().isOn(kick, 0));
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
    project.current().set(kick.id, 4, true);

    const auto state = toEngineState(project);
    REQUIRE(state->channels.size() == 4);
    REQUIRE(state->patterns.size() == 1);
    const auto& kickSteps = state->patterns[0].steps[0];
    CHECK(kickSteps.size() == 16);
    CHECK(kickSteps[4] == 1);
    CHECK(kickSteps[0] == 0);
    const auto& k = state->channels[0];
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
    doc.apply([&](Project& p) { p.current().set(kick, 0, true); });
    doc.apply([&](Project& p) { p.current().set(kick, 4, true); });
    CHECK(changes == 2);

    CHECK(doc.undo());
    CHECK(doc.project().current().isOn(kick, 0));
    CHECK_FALSE(doc.project().current().isOn(kick, 4));
    CHECK(doc.redo());
    CHECK(doc.project().current().isOn(kick, 4));

    // A new edit after undo discards the redo history.
    doc.undo();
    doc.apply([&](Project& p) { p.current().set(kick, 8, true); });
    CHECK_FALSE(doc.canRedo());
    CHECK(changes == 6);
}

TEST_CASE("Edits within one gesture undo together")
{
    Document doc(makeDefaultProject());
    const auto kick = doc.project().channels[0].id;

    const int gesture = doc.newGestureId();
    for (int step = 0; step < 4; ++step)
        doc.apply([&](Project& p) { p.current().set(kick, step, true); }, gesture);

    doc.undo();
    for (int step = 0; step < 4; ++step)
        CHECK_FALSE(doc.project().current().isOn(kick, step));
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

TEST_CASE("A new project has one empty pattern, selected")
{
    const Project project;
    REQUIRE(project.patterns.size() == 1);
    CHECK(project.patterns[0].name == "Pattern 1");
    CHECK(project.current().id == project.patterns[0].id);
    CHECK(project.current().isEmpty());
    CHECK_FALSE(project.songMode);
}

TEST_CASE("Patterns are numbered, cloned and removed with their clips")
{
    Project project;
    const auto first = project.current().id;
    const auto second = project.addPattern();
    CHECK(project.findPattern(second)->name == "Pattern 2");

    project.findPattern(first)->set(7, 3, true);
    const auto copy = project.clonePattern(first);
    REQUIRE(project.patterns.size() == 3);
    CHECK(project.patterns[1].id == copy); // inserted right after the original
    CHECK(project.findPattern(copy)->name == "Pattern 1 (copy)");
    CHECK(project.findPattern(copy)->isOn(7, 3));

    project.playlist.clips = {{first, 0, 0, 16}, {second, 1, 0, 16}, {first, 2, 16, 16}};
    project.currentPattern = first;
    project.removePattern(first);
    CHECK(project.findPattern(first) == nullptr);
    REQUIRE(project.playlist.clips.size() == 1);
    CHECK(project.playlist.clips[0].pattern == second);
    CHECK(project.currentPattern == copy); // selection moves to the neighbour

    // New names continue after the highest number, even after deletions.
    CHECK(project.findPattern(project.addPattern())->name == "Pattern 3");
}

TEST_CASE("The last pattern can't be removed")
{
    Project project;
    project.removePattern(project.current().id);
    CHECK(project.patterns.size() == 1);
}

TEST_CASE("Removing a channel removes its steps from every pattern")
{
    auto project = makeDefaultProject();
    const auto kick = project.channels[0].id;
    const auto p2 = project.addPattern();
    project.current().set(kick, 0, true);
    project.findPattern(p2)->set(kick, 4, true);
    project.removeChannel(kick);
    CHECK(project.current().isEmpty());
    CHECK(project.findPattern(p2)->isEmpty());
}

TEST_CASE("Song length rounds the last clip end up to a bar")
{
    Project project;
    CHECK(project.songLengthInSteps() == 0);
    const auto id = project.current().id;
    project.playlist.clips = {{id, 0, 0, 16}, {id, 3, 20, 8}}; // ends at step 28
    CHECK(project.songLengthInSteps() == 32);
    project.beatsPerBar = 3; // 12-step bars
    CHECK(project.songLengthInSteps() == 36);
}

TEST_CASE("Engine snapshot maps clips and the current pattern to indices")
{
    auto project = makeDefaultProject();
    const auto p1 = project.current().id;
    const auto p2 = project.addPattern();
    project.currentPattern = p2;
    project.songMode = true;
    project.playlist.clips = {{p2, 0, 0, 16}, {p1, 1, 16, 32}};

    const auto state = toEngineState(project);
    CHECK(state->songMode);
    CHECK(state->currentPattern == 1);
    CHECK(state->songLengthInSteps == 48);
    REQUIRE(state->clips.size() == 2);
    CHECK(state->clips[0].patternIndex == 1);
    CHECK(state->clips[1].patternIndex == 0);
    CHECK(state->clips[1].startStep == 16);
    CHECK(state->clips[1].lengthInSteps == 32);
    CHECK(state->patterns[0].steps.size() == project.channels.size());
}

TEST_CASE("Undo restores content but keeps the selected pattern and mode")
{
    Document doc(makeDefaultProject());
    PatternId added = 0;
    doc.apply([&](Project& p) { added = p.addPattern(); });
    doc.setViewState(
        [&](Project& p)
        {
            p.currentPattern = added;
            p.songMode = true;
        });
    doc.apply([&](Project& p) { p.current().set(1, 0, true); });

    CHECK(doc.undo()); // removes the step, keeps selection + mode
    CHECK(doc.project().currentPattern == added);
    CHECK(doc.project().songMode);

    CHECK(doc.undo()); // removes the added pattern: selection falls back
    CHECK(doc.project().findPattern(added) == nullptr);
    CHECK(doc.project().songMode);
    CHECK(doc.project().current().id == doc.project().patterns[0].id);
}

TEST_CASE("View state changes aren't undo steps")
{
    Document doc(makeDefaultProject());
    doc.setViewState([](Project& p) { p.songMode = true; });
    CHECK_FALSE(doc.canUndo());
    CHECK(doc.project().songMode);
}
