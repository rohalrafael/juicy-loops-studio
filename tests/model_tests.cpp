// The project model: channels, patterns, steps and notes, undo, the engine snapshot, looping
// channels, scales, slicing, copy/paste and time formatting.

#include "model/clipboard.h"
#include "model/document.h"
#include "model/project.h"
#include "model/project_file.h"

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
    const auto& kickNotes = state->patterns[0].notes[0];
    REQUIRE(kickNotes.size() == 1);
    CHECK(kickNotes[0].startTick == 4 * ticksPerStep);
    CHECK(kickNotes[0].pitch == Note::defaultPitch);
    CHECK(state->patterns[0].lengthTicks == 16 * ticksPerStep);
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
    REQUIRE(state->clips.size() == 2);
    CHECK(state->clips[0].patternIndex == 1);
    CHECK(state->clips[1].patternIndex == 0);
    CHECK(state->clips[1].startTick == 16 * ticksPerStep);
    CHECK(state->clips[1].lengthTicks == 32 * ticksPerStep);
    CHECK(state->songLengthTicks == 48 * ticksPerStep);
    CHECK(state->patterns[0].notes.size() == project.channels.size());
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

TEST_CASE("Steps are notes at the default pitch")
{
    Pattern pattern;
    pattern.set(1, 4, true);
    REQUIRE(pattern.notesFor(1).size() == 1);
    const auto& note = pattern.notesFor(1)[0];
    CHECK(note.startTick == 4 * ticksPerStep);
    CHECK(note.lengthTicks == ticksPerStep);
    CHECK(note.isStep());
    CHECK(pattern.isOn(1, 4));

    pattern.set(1, 4, true); // already on: no duplicate
    CHECK(pattern.notesFor(1).size() == 1);
    pattern.set(1, 4, false);
    CHECK(pattern.notesFor(1).empty());
}

TEST_CASE("A channel with melodic notes is no longer a step row")
{
    Pattern pattern;
    pattern.set(1, 0, true);
    CHECK(pattern.hasOnlySteps(1));
    pattern.notes[1].push_back({ticksPerStep * 2, ticksPerStep * 4, 67, 0.9f});
    CHECK_FALSE(pattern.hasOnlySteps(1));
    CHECK_FALSE(pattern.isOn(1, 2)); // a long G5 isn't a step
    CHECK(pattern.hasOnlySteps(2));  // an empty channel is
}

TEST_CASE("Pattern length grows in whole bars to fit its notes")
{
    Pattern pattern;
    CHECK(pattern.lengthInSteps(16) == 16);
    pattern.notes[1].push_back({15 * ticksPerStep, ticksPerStep, 60, 0.8f}); // ends at step 16
    CHECK(pattern.lengthInSteps(16) == 16);
    pattern.notes[1].push_back({16 * ticksPerStep, 1, 60, 0.8f}); // one tick into bar 2
    CHECK(pattern.lengthInSteps(16) == 32);
    CHECK(pattern.lengthInSteps(12) == 24); // 3/4 bars
}

TEST_CASE("Removing the selected channel selects another")
{
    auto project = makeDefaultProject();
    const auto kick = project.channels[0].id;
    CHECK(project.selectedChannel == kick);
    project.removeChannel(kick);
    CHECK(project.selectedChannel == project.channels[0].id);
}

TEST_CASE("Times are shown as minutes, seconds and milliseconds")
{
    CHECK(formatTime(0.0) == "0:00.000");
    CHECK(formatTime(1.2345) == "0:01.234");
    CHECK(formatTime(65.5) == "1:05.500");
    CHECK(formatTime(4503.25) == "75:03.250");
    CHECK(formatTime(-3.0) == "0:00.000");
    // 16 steps at 120 BPM = 4 beats = 2 seconds.
    Project project;
    project.tempo = 120.0;
    CHECK(formatTime(16 * project.secondsPerStep()) == "0:02.000");
}

TEST_CASE("Channels shorter than the pattern loop; their repeats are played")
{
    Project project;
    const auto drums = project.addChannel("Drums", nullptr);
    const auto bass = project.addChannel("Bass", nullptr);
    auto& pattern = project.current();
    pattern.set(drums, 0, true);
    pattern.set(drums, 6, true);                                          // drums: 1 bar of content
    pattern.notes[bass].push_back({0, 64 * ticksPerStep - 24, 48, 0.8f}); // bass: 4 bars
    REQUIRE(project.lengthInSteps(pattern) == 64);

    CHECK(project.naturalLoopInSteps(pattern, drums) == 16);
    CHECK(project.loopLengthInSteps(pattern, drums) == 16);
    CHECK(project.loopLengthInSteps(pattern, bass) == 0); // fills the pattern already
    pattern.set(drums, 20, true);                         // now 2 bars of content
    CHECK(project.loopLengthInSteps(pattern, drums) == 32);
    pattern.set(drums, 40, true); // 3 bars -> rounds up to 4 = the pattern: no repeats
    CHECK(project.loopLengthInSteps(pattern, drums) == 0);
    pattern.set(drums, 20, false);
    pattern.set(drums, 40, false);

    // The snapshot holds the repeats.
    auto state = toEngineState(project);
    CHECK(state->patterns[0].notes[0].size() == 8); // 2 steps x 4 bars
    CHECK(state->patterns[0].notes[1].size() == 1);

    // Off per channel, or for the whole rack: played once.
    project.findChannel(drums)->loop = false;
    CHECK(project.loopLengthInSteps(pattern, drums) == 0);
    CHECK(toEngineState(project)->patterns[0].notes[0].size() == 2);
    project.findChannel(drums)->loop = true;
    project.loopChannels = false;
    CHECK(toEngineState(project)->patterns[0].notes[0].size() == 2);
}

TEST_CASE("Filling a loop copies its notes so the channel sounds the same")
{
    Project project;
    const auto drums = project.addChannel("Drums", nullptr);
    const auto bass = project.addChannel("Bass", nullptr);
    auto& pattern = project.current();
    pattern.set(drums, 0, true);
    pattern.set(drums, 6, true);
    pattern.notes[bass].push_back({0, 64 * ticksPerStep, 48, 0.8f}); // makes the pattern 4 bars
    const auto before = toEngineState(project)->patterns[0].notes[0].size();
    REQUIRE(before == 8);

    pattern.fillLoop(drums, 16, 32);
    CHECK(pattern.isOn(drums, 16));
    CHECK(pattern.isOn(drums, 22));
    CHECK_FALSE(pattern.isOn(drums, 32));
    CHECK(project.loopLengthInSteps(pattern, drums) == 32);
    CHECK(toEngineState(project)->patterns[0].notes[0].size() == before); // same 8 hits
}

TEST_CASE("The rack's grid length doesn't change what plays; notes do")
{
    Project project;
    const auto drums = project.addChannel("Drums", nullptr);
    auto& pattern = project.current();
    pattern.set(drums, 0, true);
    pattern.gridSteps = 32; // the user picked 32 in the rack
    CHECK(project.lengthInSteps(pattern) == 16);
    CHECK(project.gridLengthInSteps(pattern) == 32);
    CHECK(toEngineState(project)->patterns[0].lengthTicks == 16 * ticksPerStep);

    pattern.set(drums, 20, true); // a note in the second bar: now both bars play
    CHECK(project.lengthInSteps(pattern) == 32);
    // Notes past the grid widen it.
    pattern.notes[drums].push_back({60 * ticksPerStep, ticksPerStep, 60, 0.8f});
    CHECK(project.gridLengthInSteps(pattern) == 64);
}

TEST_CASE("Loop switches are saved")
{
    Project project;
    const auto id = project.addChannel("Drums", nullptr);
    project.findChannel(id)->loop = false;
    project.loopChannels = false;
    const auto loaded = loadProject(saveProject(project, "/p/x.jlp"), "/p/x.jlp", {});
    REQUIRE(loaded.project);
    CHECK_FALSE(loaded.project->loopChannels);
    CHECK_FALSE(loaded.project->findChannel(id)->loop);
}

TEST_CASE("Scales: major and minor degrees from any root")
{
    // C major: the white keys.
    for (int pitch = 60; pitch < 72; ++pitch)
    {
        const bool white = pitch % 12 == 0 || pitch % 12 == 2 || pitch % 12 == 4 ||
                           pitch % 12 == 5 || pitch % 12 == 7 || pitch % 12 == 9 ||
                           pitch % 12 == 11;
        CHECK(isInScale(pitch, 0, Scale::major) == white);
    }
    // A minor = C major's notes.
    for (int pitch = 0; pitch < 128; ++pitch)
        CHECK(isInScale(pitch, 9, Scale::minor) == isInScale(pitch, 0, Scale::major));
    // D minor has B flat (A#), not B.
    CHECK(isInScale(70, 2, Scale::minor));
    CHECK_FALSE(isInScale(71, 2, Scale::minor));
    CHECK(isInScale(61, 0, Scale::none));

    // Kept through undo, and saved.
    Document doc(Project{});
    doc.apply([](Project& p) { p.addChannel("A", nullptr); });
    doc.setViewState(
        [](Project& p)
        {
            p.scale = Scale::minor;
            p.scaleRoot = 9;
        });
    doc.undo();
    CHECK(doc.project().scale == Scale::minor);
    const auto loaded = loadProject(saveProject(doc.project(), "/p/s.jlp"), "/p/s.jlp", {});
    CHECK(loaded.project->scale == Scale::minor);
    CHECK(loaded.project->scaleRoot == 9);
}

TEST_CASE("Slicing a note into rolls keeps pitch, velocity and the overall span")
{
    const Note note{48, 4 * ticksPerStep, 66, 0.5f};     // a beat long, starting on step 2
    const auto quarters = sliceNote(note, ticksPerStep); // 1/16 notes
    REQUIRE(quarters.size() == 4);
    for (std::size_t i = 0; i < quarters.size(); ++i)
    {
        CHECK(quarters[i].startTick == 48 + static_cast<int>(i) * ticksPerStep);
        CHECK(quarters[i].lengthTicks == ticksPerStep);
        CHECK(quarters[i].pitch == 66);
        CHECK(quarters[i].velocity == 0.5f);
    }
    // Triplets that don't divide evenly: the last piece is shorter, the end stays put.
    const auto triplets = sliceNote({0, 100, 60, 0.8f}, 32);
    REQUIRE(triplets.size() == 4);
    CHECK(triplets.back().startTick == 96);
    CHECK(triplets.back().endTick() == 100);
    // Pieces as long as the note (or longer): unchanged.
    CHECK(sliceNote(note, 4 * ticksPerStep).size() == 1);
    CHECK(sliceNote(note, 0).size() == 1);
}

TEST_CASE("Solo mutes the other channels, and again unmutes them all")
{
    auto project = makeDefaultProject();
    const auto kick = project.channels[0].id;
    project.channels[2].muted = true; // a channel muted before stays part of the toggle
    project.soloChannel(kick);
    for (const auto& c : project.channels)
        CHECK(c.muted == (c.id != kick));
    project.soloChannel(kick); // already solo: everything plays again
    for (const auto& c : project.channels)
        CHECK_FALSE(c.muted);
    // Soloing a muted channel unmutes it.
    project.channels[1].muted = true;
    project.soloChannel(project.channels[1].id);
    CHECK_FALSE(project.channels[1].muted);
    CHECK(project.channels[0].muted);
}

TEST_CASE("Fill each N steps replaces a channel's steps")
{
    Pattern pattern;
    pattern.set(7, 3, true);
    pattern.fillSteps(7, 2, 16);
    REQUIRE(pattern.notesFor(7).size() == 8);
    for (int step = 0; step < 16; ++step)
        CHECK(pattern.isOn(7, step) == (step % 2 == 0));
    pattern.fillSteps(7, 4, 32);
    CHECK(pattern.notesFor(7).size() == 8);
    CHECK(pattern.isOn(7, 28));
    CHECK_FALSE(pattern.isOn(7, 2));
}

TEST_CASE("Moving notes keeps the group together at the edges")
{
    std::vector<Note> notes = {{ticksPerStep, ticksPerStep, 60, 0.8f},
                               {4 * ticksPerStep, ticksPerStep, 126, 0.8f},
                               {0, ticksPerStep, 50, 0.8f}};
    // Only the selected ones move.
    CHECK(moveNotes(notes, {0, 1}, ticksPerStep, 1));
    CHECK(notes[0].startTick == 2 * ticksPerStep);
    CHECK(notes[0].pitch == 61);
    CHECK(notes[1].pitch == 127);
    CHECK(notes[2].startTick == 0);
    // At the top pitch the group can't go higher; at the start it can't go earlier.
    CHECK_FALSE(moveNotes(notes, {0, 1}, 0, 1));
    CHECK(notes[0].pitch == 61);
    CHECK(moveNotes(notes, {0, 1}, -10 * ticksPerStep, 0));
    CHECK(notes[0].startTick == 0);
    CHECK(notes[1].startTick == 3 * ticksPerStep); // 5 steps, moved 2 earlier
    CHECK_FALSE(moveNotes(notes, {}, ticksPerStep, 0));
}

TEST_CASE("Copied notes paste right after their block, again and again")
{
    // A 1-bar beat whose last hit ends at step 14: the block is the whole bar (rounded to beats).
    std::vector<Note> notes = {{0, ticksPerStep, 60, 0.8f},
                               {6 * ticksPerStep, ticksPerStep, 60, 0.8f},
                               {13 * ticksPerStep, ticksPerStep, 62, 0.5f}};
    auto clipboard = copyNotes(notes, {0, 1, 2}, ticksPerBeat);
    CHECK(clipboard.start == 0);
    CHECK(clipboard.length == 16 * ticksPerStep);
    auto first = nextPaste(clipboard);
    REQUIRE(first.size() == 3);
    CHECK(first[0].startTick == 16 * ticksPerStep);
    CHECK(first[2].pitch == 62);
    CHECK(first[2].velocity == 0.5f);
    CHECK(nextPaste(clipboard)[0].startTick == 32 * ticksPerStep);

    // A short roll in the middle of a bar: block = the beat it's in.
    auto roll = copyNotes(notes, {1}, ticksPerBeat);
    CHECK(roll.start == ticksPerBeat);
    CHECK(roll.length == ticksPerBeat);
    CHECK(nextPaste(roll)[0].startTick == 6 * ticksPerStep + ticksPerBeat);

    CHECK(copyNotes(notes, {}, ticksPerBeat).empty());
}

TEST_CASE("Copied clips paste a whole number of bars later, on their own tracks")
{
    std::vector<Clip> clips = {{1, 0, 0, 16}, {2, 3, 8, 16}}; // ends at step 24
    auto clipboard = copyClips(clips, {0, 1}, 16);
    CHECK(clipboard.length == 32);
    const auto pasted = nextPaste(clipboard);
    CHECK(pasted[0].startStep == 32);
    CHECK(pasted[1].startStep == 40);
    CHECK(pasted[1].track == 3);
}
