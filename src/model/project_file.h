#pragma once

// Saving and loading projects as JSON (.jlp files; .jls before 2026-10-17, still opened). Plain
// C++: the caller does the file I/O and decodes audio files; this code only converts between
// Project and text. The format is described in docs/project-format.md.

#include "model/project.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace jls::model
{

inline constexpr int projectFormatVersion = 1;
inline constexpr const char* projectFileExtension = ".jlp";
inline constexpr const char* legacyProjectFileExtension = ".jls"; // still opened, never written

// Writes the whole project. Sample paths are stored both absolute and relative to
// `projectFile`'s folder, so a project folder can be moved or shared.
std::string saveProject(const Project& project, const std::filesystem::path& projectFile);

// Decodes the audio file at an absolute path; returns null if it can't be read.
using SampleFileLoader =
    std::function<std::shared_ptr<const engine::SampleBuffer>(const std::string& path)>;

struct LoadResult
{
    std::optional<Project> project;    // empty if the file couldn't be used at all
    std::string error;                 // why project is empty
    std::vector<std::string> warnings; // things that were skipped or couldn't be found
};

// Reads a project. Out-of-range values are clamped, unknown entries skipped (with a warning),
// and missing sample files reported but kept as references, so saving again doesn't lose them.
// Sample files are looked up relative to `projectFile`'s folder first, then by absolute path.
LoadResult loadProject(std::string_view json, const std::filesystem::path& projectFile,
                       const SampleFileLoader& loadSample);

} // namespace jls::model
