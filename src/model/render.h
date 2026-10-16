#pragma once

// Offline rendering ("export"): plays a project through its own Engine as fast as possible and
// collects the output. Plain C++, independent of the live audio device.

#include "model/project.h"

#include <vector>

namespace jls::model
{

struct RenderedAudio
{
    std::vector<float> left;
    std::vector<float> right;
    double sampleRate = 44100.0;
};

// Renders the song (song mode), or the current pattern once if the Playlist is empty, followed
// by `tailSeconds` so reverb and delay tails can ring out. The metronome is never included.
RenderedAudio renderProject(const Project& project, double sampleRate, double tailSeconds = 2.0);

// Length of what renderProject() plays before the tail, in seconds.
double renderLengthSeconds(const Project& project);

} // namespace jls::model
