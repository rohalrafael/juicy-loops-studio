// Tests for Split by channel (Project::splitByChannel): one pattern per channel, all as long as
// the original, on their own Playlist tracks, sounding the same.

#include "model/project.h"
#include "model/project_file.h"
#include "model/render.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

using namespace jls::model;

namespace
{

// Kick and clap loop a 1-bar beat; the snare has one hit in bar 4, so the pattern is 4 bars.
Project beatProject()
{
    auto project = makeDefaultProject(48000.0);
    auto& pattern = project.current();
    const auto kick = project.channels[0].id;
    const auto clap = project.channels[1].id;
    const auto snare = project.channels[3].id;
    for (int s : {0, 4, 8, 12})
        pattern.set(kick, s, true);
    for (int s : {4, 12})
        pattern.set(clap, s, true);
    pattern.set(snare, 60, true);
    return project;
}

const Pattern* byName(const Project& project, const std::string& name)
{
    for (const auto& p : project.patterns)
        if (p.name == name)
            return &p;
    return nullptr;
}

} // namespace

TEST_CASE("Split by channel: one pattern per channel, each as long as the original")
{
    auto project = beatProject();
    const auto original = project.currentPattern;
    REQUIRE(project.lengthInSteps(project.current()) == 64);

    const auto parts = project.splitByChannel(original);
    REQUIRE(parts.size() == 3);                        // the hat has no notes
    REQUIRE(project.findPattern(original) != nullptr); // kept
    const auto* kick = byName(project, "Pattern 1 - Kick");
    const auto* clap = byName(project, "Pattern 1 - Clap");
    const auto* snare = byName(project, "Pattern 1 - Snare");
    REQUIRE(kick != nullptr);
    REQUIRE(clap != nullptr);
    REQUIRE(snare != nullptr);
    for (const auto* part : {kick, clap, snare})
        CHECK(project.lengthInSteps(*part) == 64);
    // The looping kick's repeats are written out: 4 bars of 4 hits.
    CHECK(kick->notesFor(project.channels[0].id).size() == 16);
    // The snare didn't loop: still one hit, the length is fixed instead.
    CHECK(snare->notesFor(project.channels[3].id).size() == 1);
    // Right after the original, in Channel Rack order.
    CHECK(project.patterns[1].id == kick->id);
    CHECK(project.patterns[3].id == snare->id);

    // Their clips are labelled with just the channel; other patterns keep their names.
    CHECK(project.clipLabel(*kick) == "Kick");
    CHECK(project.clipLabel(*snare) == "Snare");
    CHECK(project.clipLabel(*project.findPattern(original)) == "Pattern 1");

    // A pattern with one channel isn't split.
    const auto single = project.addPattern();
    project.findPattern(single)->set(project.channels[0].id, 0, true);
    CHECK(project.splitByChannel(single).empty());
    CHECK(project.clipLabel(*project.findPattern(single)) == "Pattern 2"); // not a part
}

TEST_CASE("Split by channel puts each part on its own free track")
{
    auto project = beatProject();
    const auto original = project.currentPattern;
    const auto other = project.addPattern();
    project.findPattern(other)->set(project.channels[2].id, 0, true);
    // The beat on track 2 twice; something else in the way on track 3 during the first.
    project.playlist.clips.push_back({original, 2, 0, 64});
    project.playlist.clips.push_back({original, 2, 128, 64});
    project.playlist.clips.push_back({other, 3, 32, 16});

    const auto parts = project.splitByChannel(original);
    REQUIRE(parts.size() == 3);
    std::vector<int> tracks;
    for (const auto part : parts)
    {
        std::vector<Clip> clips;
        for (const auto& c : project.playlist.clips)
            if (c.pattern == part)
                clips.push_back(c);
        REQUIRE(clips.size() == 2);              // both copies of the beat
        CHECK(clips[0].track == clips[1].track); // the same track for both
        CHECK(clips[0].lengthInSteps == 64);
        tracks.push_back(clips[0].track);
    }
    CHECK(tracks == std::vector<int>{2, 4, 5}); // track 3 is taken during the first copy
    CHECK(std::none_of(project.playlist.clips.begin(), project.playlist.clips.end(),
                       [original](const Clip& c) { return c.pattern == original; }));

    // Near the bottom, tracks are added rather than stacking clips.
    auto low = beatProject();
    low.playlist.clips.push_back({low.currentPattern, low.playlist.numTracks - 1, 0, 64});
    low.splitByChannel(low.currentPattern);
    CHECK(low.playlist.numTracks == Playlist::defaultTracks + 2);
}

TEST_CASE("A song sounds the same after Split by channel")
{
    auto project = beatProject();
    project.songMode = true;
    project.playlist.clips.push_back({project.currentPattern, 0, 0, 64});
    project.playlist.clips.push_back({project.currentPattern, 0, 64, 128}); // longer: repeats
    const auto before = renderProject(project, 48000.0, 0.5);
    REQUIRE(project.splitByChannel(project.currentPattern).size() == 3);
    const auto after = renderProject(project, 48000.0, 0.5);
    REQUIRE(after.left.size() == before.left.size());
    float worst = 0.0f;
    for (std::size_t i = 0; i < before.left.size(); ++i)
        worst = std::max(worst, std::abs(after.left[i] - before.left[i]));
    CHECK(worst < 1.0e-5f);
}

TEST_CASE("A pattern's fixed length is saved")
{
    auto project = beatProject();
    const auto parts = project.splitByChannel(project.currentPattern);
    const auto loaded = loadProject(saveProject(project, "/p/s.jlp"), "/p/s.jlp", {});
    REQUIRE(loaded.project);
    const auto* snare = loaded.project->findPattern(parts[2]);
    REQUIRE(snare != nullptr);
    CHECK(snare->minLengthSteps == 64);
    CHECK(loaded.project->lengthInSteps(*snare) == 64);
}
