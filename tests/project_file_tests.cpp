// Project files: save/load round trips, damaged and foreign files, finding moved samples,
// export rendering, and tracking unsaved changes.

#include "model/document.h"
#include "model/project_file.h"
#include "model/render.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

using namespace jls;
using namespace jls::model;
namespace fs = std::filesystem;

namespace
{

// A stand-in for decoding audio files: any path loads as a short constant sample and is recorded.
struct FakeLoader
{
    std::vector<std::string> requested;
    SampleFileLoader function()
    {
        return [this](const std::string& path)
        {
            requested.push_back(path);
            auto buffer = std::make_shared<engine::SampleBuffer>();
            buffer->channels.assign(1, std::vector<float>(100, 0.5f));
            return buffer;
        };
    }
};

Project richProject()
{
    auto project = makeDefaultProject();
    project.tempo = 142.5;
    project.beatsPerBar = 3;
    const auto kick = project.channels[0].id;
    const auto hat = project.channels[2].id;
    project.channels[1].volume = 0.5f;
    project.channels[1].pan = -0.25f;
    project.channels[1].muted = true;
    project.channels[3].mixerTrack = 7;
    project.channels[3].builtinSample.clear();
    project.channels[3].samplePath = "/samples/snare.wav";

    project.current().set(kick, 0, true);
    project.current().notes[hat].push_back({30, 50, 67, 0.4f});
    const auto second = project.addPattern();
    project.findPattern(second)->gridSteps = 32;
    project.playlist.clips = {{project.current().id, 0, 0, 16}, {second, 3, 24, 40}};

    project.mixer.tracks[2].volumeDb = -3.5f;
    project.mixer.tracks[2].solo = true;
    project.mixer.tracks[0].pan = 0.3f;
    project.mixer.tracks[3].width = 1.5f;
    const auto reverb = project.addEffect(2, "reverb");
    project.findEffect(reverb)->params[0] = 0.9f;
    project.findEffect(reverb)->enabled = false;
    project.addEffect(0, "compressor");
    const auto plugin = project.addPluginEffect(5, "VST3-Room-abc123", "Room Reverb");
    project.findEffect(plugin)->pluginState = "QUJDRA==";

    project.currentPattern = second;
    project.selectedChannel = hat;
    project.selectedMixerTrack = 2;
    project.songMode = true;
    return project;
}

} // namespace

TEST_CASE("Projects survive a save and load round trip")
{
    const auto original = richProject();
    FakeLoader loader;
    const auto text = saveProject(original, "/projects/song.jlp");
    const auto result = loadProject(text, "/projects/song.jlp", loader.function());
    REQUIRE(result.project);
    const auto& p = *result.project;

    CHECK(p.tempo == 142.5);
    CHECK(p.beatsPerBar == 3);
    REQUIRE(p.channels.size() == original.channels.size());
    for (std::size_t i = 0; i < p.channels.size(); ++i)
    {
        const auto& a = original.channels[i];
        const auto& b = p.channels[i];
        CHECK(b.id == a.id);
        CHECK(b.name == a.name);
        CHECK(b.volume == a.volume);
        CHECK(b.pan == a.pan);
        CHECK(b.muted == a.muted);
        CHECK(b.mixerTrack == a.mixerTrack);
        CHECK(b.builtinSample == a.builtinSample);
        CHECK(b.samplePath == a.samplePath);
    }
    CHECK(p.channels[0].sample != nullptr); // built-in sounds are re-synthesized

    REQUIRE(p.patterns.size() == 2);
    for (std::size_t i = 0; i < 2; ++i)
    {
        CHECK(p.patterns[i].id == original.patterns[i].id);
        CHECK(p.patterns[i].name == original.patterns[i].name);
        CHECK(p.patterns[i].gridSteps == original.patterns[i].gridSteps);
        CHECK(p.patterns[i].notes == original.patterns[i].notes);
    }
    CHECK(p.playlist.clips == original.playlist.clips);
    CHECK(p.playlist.numTracks == original.playlist.numTracks);

    REQUIRE(p.mixer.tracks.size() == original.mixer.tracks.size());
    for (std::size_t i = 0; i < p.mixer.tracks.size(); ++i)
    {
        const auto& a = original.mixer.tracks[i];
        const auto& b = p.mixer.tracks[i];
        CHECK(b.name == a.name);
        CHECK(b.volumeDb == a.volumeDb);
        CHECK(b.pan == a.pan);
        CHECK(b.width == a.width);
        CHECK(b.muted == a.muted);
        CHECK(b.solo == a.solo);
        CHECK(b.effects == a.effects);
    }

    CHECK(p.currentPattern == original.currentPattern);
    CHECK(p.selectedChannel == original.selectedChannel);
    CHECK(p.selectedMixerTrack == 2);
    CHECK(p.songMode);
    CHECK(p.nextChannelId == original.nextChannelId);
    CHECK(p.nextPatternId == original.nextPatternId);
    CHECK(p.nextEffectId == original.nextEffectId);
}

TEST_CASE("Files that aren't projects are rejected with a reason")
{
    FakeLoader loader;
    CHECK(loadProject("not json", "x.jlp", loader.function()).error ==
          "This isn't a valid project file.");
    CHECK(loadProject(R"({"name": "Juicy"})", "x.jlp", loader.function()).error ==
          "This isn't a Juicy Loops Studio project.");
    CHECK_FALSE(loadProject("[]", "x.jlp", loader.function()).project);
}

TEST_CASE("Damaged projects load as far as possible, with warnings")
{
    FakeLoader loader;
    const auto result = loadProject(R"({
        "format": "juicy-loops-studio-project", "version": 99,
        "tempo": 5000, "beatsPerBar": "four",
        "channels": [
            {"id": 3, "name": "Ok", "volume": 7, "mixerTrack": 400, "sample": {"builtin": "kick"}},
            {"id": 3, "name": "Duplicate"},
            {"id": 4, "name": "Odd", "sample": {"builtin": "cowbell"}}
        ],
        "patterns": [
            {"id": 9, "name": "P", "notes": [
                {"channel": 3, "notes": [[0, 24, 60, 0.8], [5], [-10, 0, 300, 2]]},
                {"channel": 77, "notes": [[0, 24, 60, 0.8]]}
            ]}
        ],
        "playlist": {"clips": [{"pattern": 9, "track": 2, "start": 0, "length": 16},
                               {"pattern": 1234}]},
        "mixer": {"tracks": [{"name": "Master", "solo": true,
                              "effects": [{"type": "flanger"},
                                          {"id": 5, "type": "delay", "params": {"Time": 99999}}]}]},
        "view": {"currentPattern": 555}
    })",
                                    "x.jlp", loader.function());
    REQUIRE(result.project);
    const auto& p = *result.project;
    CHECK(result.warnings.size() >= 7);

    CHECK(p.tempo == 999.0);
    CHECK(p.beatsPerBar == 4);
    REQUIRE(p.channels.size() == 2);
    CHECK(p.channels[0].volume == 1.0f);
    CHECK(p.channels[0].mixerTrack == Mixer::numInserts);
    CHECK(p.channels[1].sample == nullptr); // unknown built-in

    const auto& notes = p.patterns[0].notesFor(3);
    REQUIRE(notes.size() == 2);
    CHECK(notes[1].startTick == 0);
    CHECK(notes[1].lengthTicks == 1);
    CHECK(notes[1].pitch == 127);
    CHECK(notes[1].velocity == 1.0f);
    CHECK(p.patterns[0].notesFor(77).empty());

    REQUIRE(p.playlist.clips.size() == 1);
    CHECK_FALSE(p.mixer.tracks[0].solo); // the Master can't be soloed
    REQUIRE(p.mixer.tracks[0].effects.size() == 1);
    CHECK(p.mixer.tracks[0].effects[0].params[0] == 2000.0f); // clamped to the max
    CHECK(p.mixer.tracks[0].effects[0].params[1] ==
          engine::findEffectType("delay")->params[1].defaultValue); // missing: default
    CHECK(p.currentPattern == 9);
    CHECK(p.nextEffectId > 5);
}

TEST_CASE("Samples are found next to a moved project, else by absolute path")
{
    const auto root = fs::temp_directory_path() / "jls-project-test";
    fs::remove_all(root);
    fs::create_directories(root / "old" / "samples");
    fs::create_directories(root / "new" / "samples");
    std::ofstream(root / "old" / "samples" / "a.wav") << "x";
    std::ofstream(root / "new" / "samples" / "a.wav") << "x";

    auto project = makeDefaultProject();
    project.channels[0].builtinSample.clear();
    project.channels[0].samplePath = (root / "old" / "samples" / "a.wav").string();
    const auto text = saveProject(project, root / "old" / "song.jlp");

    SECTION("moved folder: the copy next to the project wins")
    {
        FakeLoader loader;
        const auto result = loadProject(text, root / "new" / "song.jlp", loader.function());
        REQUIRE(loader.requested.size() == 1);
        CHECK(fs::path(loader.requested[0]) == root / "new" / "samples" / "a.wav");
        CHECK(result.warnings.empty());
    }
    SECTION("nothing next to it: the absolute path is used")
    {
        FakeLoader loader;
        loadProject(text, root / "elsewhere" / "song.jlp", loader.function());
        REQUIRE(loader.requested.size() == 1);
        CHECK(fs::path(loader.requested[0]) == root / "old" / "samples" / "a.wav");
    }
    SECTION("missing everywhere: a warning, but the reference is kept")
    {
        fs::remove_all(root / "old");
        FakeLoader loader;
        const auto result = loadProject(text, root / "elsewhere" / "song.jlp", loader.function());
        REQUIRE(result.project);
        CHECK(result.project->channels[0].sample == nullptr);
        CHECK(result.project->channels[0].samplePath == project.channels[0].samplePath);
        REQUIRE(result.warnings.size() == 1);
        CHECK(result.warnings[0].find("Missing sample") != std::string::npos);
    }
    fs::remove_all(root);
}

namespace
{

float peak(const std::vector<float>& x, std::size_t from, std::size_t to)
{
    float p = 0.0f;
    for (std::size_t i = from; i < std::min(to, x.size()); ++i)
        p = std::max(p, std::abs(x[i]));
    return p;
}

} // namespace

TEST_CASE("Rendering plays the song once, then the tail")
{
    auto project = makeDefaultProject(48000.0);
    project.tempo = 120.0; // a 4/4 bar is 2 s
    const auto kick = project.channels[0].id;
    project.current().set(kick, 0, true);

    SECTION("empty Playlist: the current pattern, once")
    {
        CHECK(renderLengthSeconds(project) == Catch::Approx(2.0));
        const auto audio = renderProject(project, 48000.0, 1.0);
        CHECK(audio.left.size() == 3 * 48000);
        CHECK(peak(audio.left, 0, 4800) > 0.1f);        // the kick at the start
        CHECK(peak(audio.left, 96000, 144000) == 0.0f); // no loop back to the start
    }
    SECTION("song mode: clips in order, length rounded to bars")
    {
        project.playlist.clips = {{project.current().id, 0, 32, 16}}; // bar 3
        CHECK(renderLengthSeconds(project) == Catch::Approx(6.0));
        const auto audio = renderProject(project, 48000.0, 0.0);
        CHECK(peak(audio.left, 0, 4 * 48000) == 0.0f); // bars 1-2 empty
        CHECK(peak(audio.left, 4 * 48000, 4 * 48000 + 4800) > 0.1f);
    }
    SECTION("the mixer applies: a muted insert renders silence")
    {
        project.mixer.tracks[1].muted = true;
        const auto audio = renderProject(project, 48000.0, 0.5);
        CHECK(peak(audio.left, 0, audio.left.size()) == 0.0f);
    }
    SECTION("effects keep ringing into the tail")
    {
        project.addEffect(1, "reverb");
        const auto audio = renderProject(project, 48000.0, 2.0);
        CHECK(peak(audio.left, 2 * 48000 + 4800, 3 * 48000) > 0.00002f); // quiet, but there
    }
}

TEST_CASE("New projects don't clip, even with every drum on the same step")
{
    auto project = makeDefaultProject(48000.0);
    for (const auto& channel : project.channels)
        project.current().set(channel.id, 0, true);
    const auto audio = renderProject(project, 48000.0, 0.0);
    const float ceiling = std::pow(10.0f, -0.3f / 20.0f); // the Master soft clipper's default
    CHECK(peak(audio.left, 0, audio.left.size()) <= ceiling);
    CHECK(peak(audio.right, 0, audio.right.size()) <= ceiling);

    // Even without it the built-in drums leave headroom: they used to go over full scale and
    // drive the clipper, which sounded loud and gritty.
    project.mixer.tracks[0].effects.clear();
    const auto raw = renderProject(project, 48000.0, 0.0);
    CHECK(peak(raw.left, 0, raw.left.size()) < 1.0f);
}

TEST_CASE("Unsaved changes are tracked through undo and redo")
{
    Document doc(makeDefaultProject());
    CHECK_FALSE(doc.isModified());
    doc.apply([](Project& p) { p.tempo = 100.0; });
    CHECK(doc.isModified());
    doc.undo();
    CHECK_FALSE(doc.isModified()); // back at the saved state
    doc.redo();
    CHECK(doc.isModified());
    doc.markSaved();
    CHECK_FALSE(doc.isModified());
    doc.setViewState([](Project& p) { p.songMode = true; });
    CHECK_FALSE(doc.isModified()); // view state isn't a change to the song
    doc.undo();
    CHECK(doc.isModified());

    doc.reset(makeDefaultProject());
    CHECK_FALSE(doc.isModified());
    CHECK_FALSE(doc.canUndo());
}
