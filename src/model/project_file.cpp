#include "model/project_file.h"

#include "engine/effects.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>

namespace jls::model
{
namespace
{

using json = nlohmann::ordered_json;
namespace fs = std::filesystem;

constexpr const char* formatTag = "juicy-loops-studio-project";

// Floats written as 6-decimal doubles: 0.8f is saved as 0.8, not 0.800000011920929, and reads
// back as exactly the same float.
double tidy(float value)
{
    return std::round(static_cast<double>(value) * 1.0e6) / 1.0e6;
}

json sampleToJson(const Channel& channel, const fs::path& projectDir)
{
    if (!channel.builtinSample.empty())
        return {{"builtin", channel.builtinSample}};
    if (channel.samplePath.empty())
        return nullptr;

    json sample = {{"path", channel.samplePath}};
    std::error_code ec;
    const auto relative = fs::path(channel.samplePath).lexically_relative(projectDir);
    if (!ec && !relative.empty() && !projectDir.empty())
        sample["relativePath"] = relative.generic_string();
    return sample;
}

// Reads values with defaults, clamping and warnings, so a damaged or hand-edited file loads
// as far as possible.
class Reader
{
  public:
    explicit Reader(std::vector<std::string>& w) : warnings(w) {}

    template <typename T> T get(const json& obj, const char* key, T fallback)
    {
        const auto it = obj.find(key);
        if (it == obj.end() || it->is_null())
            return fallback;
        try
        {
            return it->get<T>();
        }
        catch (const json::exception&)
        {
            warn(std::string("\"") + key + "\" has the wrong type; using the default");
            return fallback;
        }
    }

    template <typename T> T clamped(const json& obj, const char* key, T fallback, T low, T high)
    {
        return std::clamp(get<T>(obj, key, fallback), low, high);
    }

    const json& array(const json& obj, const char* key)
    {
        static const json empty = json::array();
        const auto it = obj.find(key);
        if (it == obj.end())
            return empty;
        if (!it->is_array())
        {
            warn(std::string("\"") + key + "\" should be a list; ignored");
            return empty;
        }
        return *it;
    }

    void warn(std::string message) { warnings.push_back(std::move(message)); }

  private:
    std::vector<std::string>& warnings;
};

} // namespace

std::string saveProject(const Project& project, const fs::path& projectFile)
{
    const auto projectDir = projectFile.parent_path();

    json channels = json::array();
    for (const auto& c : project.channels)
    {
        json channel = {{"id", c.id},         {"name", c.name},   {"volume", tidy(c.volume)},
                        {"pan", tidy(c.pan)}, {"muted", c.muted}, {"mixerTrack", c.mixerTrack},
                        {"loop", c.loop}};
        if (c.audioClip)
            channel["audioClip"] = true;
        if (c.sampler != SamplerSettings{})
            channel["sampler"] = {
                {"stretch", c.sampler.mode == StretchMode::resample  ? "resample"
                            : c.sampler.mode == StretchMode::stretch ? "stretch"
                                                                     : "off"},
                {"originalTempo", tidy(static_cast<float>(c.sampler.originalTempo))},
                {"pitch", tidy(static_cast<float>(c.sampler.pitch))},
                {"cutItself", c.sampler.cutItself},
                {"cut", c.sampler.cutGroup},
                {"cutBy", c.sampler.cutBy},
                {"envelope",
                 {{"on", c.sampler.envelope.on},
                  {"delay", tidy(static_cast<float>(c.sampler.envelope.delay))},
                  {"attack", tidy(static_cast<float>(c.sampler.envelope.attack))},
                  {"hold", tidy(static_cast<float>(c.sampler.envelope.hold))},
                  {"decay", tidy(static_cast<float>(c.sampler.envelope.decay))},
                  {"sustain", tidy(static_cast<float>(c.sampler.envelope.sustain))},
                  {"release", tidy(static_cast<float>(c.sampler.envelope.release))}}}};
        if (c.isInstrument())
            channel["instrument"] = {
                {"id", c.pluginId}, {"name", c.pluginName}, {"state", c.pluginState}};
        else
            channel["sample"] = sampleToJson(c, projectDir);
        channels.push_back(std::move(channel));
    }

    json patterns = json::array();
    for (const auto& p : project.patterns)
    {
        json notes = json::array();
        for (const auto& [channel, list] : p.notes)
        {
            if (list.empty())
                continue;
            json compact = json::array();
            for (const auto& n : list) // [start, length, pitch, velocity] keeps files small
                compact.push_back({n.startTick, n.lengthTicks, n.pitch, tidy(n.velocity)});
            notes.push_back({{"channel", channel}, {"notes", compact}});
        }
        patterns.push_back(
            {{"id", p.id}, {"name", p.name}, {"gridLength", p.gridSteps}, {"notes", notes}});
        if (p.minLengthSteps > 0)
            patterns.back()["minPlayLength"] = p.minLengthSteps;
    }

    json clips = json::array();
    for (const auto& c : project.playlist.clips)
    {
        json clip = {{"track", c.track}, {"start", c.startStep}, {"length", c.lengthInSteps}};
        if (c.isAudio())
        {
            clip["audio"] = c.audio; // an audio clip: the channel whose sample it plays
            if (c.offsetSteps > 0)
                clip["offset"] = c.offsetSteps;
        }
        else
            clip["pattern"] = c.pattern;
        clips.push_back(std::move(clip));
    }

    json tracks = json::array();
    for (const auto& t : project.mixer.tracks)
    {
        json effects = json::array();
        for (const auto& slot : t.effects)
        {
            json params = json::object();
            if (const auto* info = engine::findEffectType(slot.type))
                for (std::size_t i = 0; i < info->params.size() && i < slot.params.size(); ++i)
                    params[info->params[i].name] =
                        tidy(slot.params[i]); // by name: survives reordering
            json effect = {{"id", slot.id},
                           {"type", slot.type},
                           {"enabled", slot.enabled},
                           {"params", params}};
            if (slot.isPlugin())
                effect["plugin"] = {
                    {"id", slot.pluginId}, {"name", slot.pluginName}, {"state", slot.pluginState}};
            effects.push_back(std::move(effect));
        }
        tracks.push_back({{"name", t.name},
                          {"volumeDb", tidy(t.volumeDb)},
                          {"pan", tidy(t.pan)},
                          {"width", tidy(t.width)},
                          {"muted", t.muted},
                          {"solo", t.solo},
                          {"effects", effects}});
    }

    const json root = {
        {"format", formatTag},
        {"version", projectFormatVersion},
        {"tempo", project.tempo},
        {"beatsPerBar", project.beatsPerBar},
        {"loopChannels", project.loopChannels},
        {"channels", channels},
        {"patterns", patterns},
        {"playlist", {{"tracks", project.playlist.numTracks}, {"clips", clips}}},
        {"mixer", {{"tracks", tracks}}},
        {"view",
         {{"currentPattern", project.currentPattern},
          {"selectedChannel", project.selectedChannel},
          {"selectedMixerTrack", project.selectedMixerTrack},
          {"songMode", project.songMode},
          {"scale", project.scale == Scale::major   ? "major"
                    : project.scale == Scale::minor ? "minor"
                                                    : "none"},
          {"scaleRoot", project.scaleRoot},
          {"showAudioClips", project.showAudioClips}}},
        {"nextIds",
         {{"channel", project.nextChannelId},
          {"pattern", project.nextPatternId},
          {"effect", project.nextEffectId}}},
    };
    return root.dump(2) + "\n";
}

LoadResult loadProject(std::string_view text, const fs::path& projectFile,
                       const SampleFileLoader& loadSample)
{
    LoadResult result;
    const auto root = json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object())
    {
        result.error = "This isn't a valid project file.";
        return result;
    }
    if (root.value("format", "") != formatTag)
    {
        result.error = "This isn't a Juicy Loops Studio project.";
        return result;
    }

    Reader read(result.warnings);
    if (read.get<int>(root, "version", 1) > projectFormatVersion)
        read.warn("This project was saved by a newer version of Juicy Loops Studio; some parts "
                  "may be missing.");

    Project project;
    project.patterns.clear();
    project.tempo = read.clamped<double>(root, "tempo", 130.0, 10.0, 999.0);
    project.beatsPerBar = read.clamped<int>(root, "beatsPerBar", 4, 1, 16);
    project.loopChannels = read.get<bool>(root, "loopChannels", true);

    // Channels and their samples.
    const auto projectDir = projectFile.parent_path();
    for (const auto& c : read.array(root, "channels"))
    {
        Channel channel;
        channel.id = read.get<ChannelId>(c, "id", 0);
        if (channel.id == 0 || project.findChannel(channel.id) != nullptr)
        {
            read.warn("A channel with a missing or duplicate id was skipped.");
            continue;
        }
        channel.name = read.get<std::string>(c, "name", "Channel");
        channel.volume = read.clamped<float>(c, "volume", 0.8f, 0.0f, 1.0f);
        channel.pan = read.clamped<float>(c, "pan", 0.0f, -1.0f, 1.0f);
        channel.muted = read.get<bool>(c, "muted", false);
        channel.mixerTrack = read.clamped<int>(c, "mixerTrack", 0, 0, Mixer::numInserts);
        channel.loop = read.get<bool>(c, "loop", true);
        channel.audioClip = read.get<bool>(c, "audioClip", false);
        if (const auto sampler = c.find("sampler"); sampler != c.end() && sampler->is_object())
        {
            const auto mode = read.get<std::string>(*sampler, "stretch", "off");
            channel.sampler.mode = mode == "resample"  ? StretchMode::resample
                                   : mode == "stretch" ? StretchMode::stretch
                                                       : StretchMode::off;
            channel.sampler.originalTempo =
                read.clamped<double>(*sampler, "originalTempo", 0.0, 0.0, 999.0);
            channel.sampler.pitch = read.clamped<double>(*sampler, "pitch", 0.0, -24.0, 24.0);
            channel.sampler.cutItself = read.get<bool>(*sampler, "cutItself", false);
            channel.sampler.cutGroup =
                read.clamped<int>(*sampler, "cut", 0, 0, SamplerSettings::maxCutGroup);
            channel.sampler.cutBy =
                read.clamped<int>(*sampler, "cutBy", 0, 0, SamplerSettings::maxCutGroup);
            if (const auto env = sampler->find("envelope");
                env != sampler->end() && env->is_object())
            {
                auto& e = channel.sampler.envelope;
                const Envelope defaults;
                e.on = read.get<bool>(*env, "on", false);
                e.delay = read.clamped<double>(*env, "delay", defaults.delay, 0.0, 10.0);
                e.attack = read.clamped<double>(*env, "attack", defaults.attack, 0.0, 10.0);
                e.hold = read.clamped<double>(*env, "hold", defaults.hold, 0.0, 10.0);
                e.decay = read.clamped<double>(*env, "decay", defaults.decay, 0.0, 10.0);
                e.sustain = read.clamped<double>(*env, "sustain", defaults.sustain, 0.0, 1.0);
                e.release = read.clamped<double>(*env, "release", defaults.release, 0.0, 10.0);
            }
        }

        // A plugin instrument channel. Kept even if the plugin isn't installed here (it stays
        // silent), so the project survives a round trip through another machine.
        const auto instrument = c.find("instrument");
        if (instrument != c.end() && instrument->is_object())
        {
            channel.pluginId = read.get<std::string>(*instrument, "id", "");
            channel.pluginName = read.get<std::string>(*instrument, "name", channel.name);
            channel.pluginState = read.get<std::string>(*instrument, "state", "");
            if (channel.pluginId.empty())
                read.warn("The instrument of \"" + channel.name + "\" has no plugin id.");
        }

        const auto sample = c.find("sample");
        if (!channel.isInstrument() && sample != c.end() && sample->is_object())
        {
            channel.builtinSample = read.get<std::string>(*sample, "builtin", "");
            channel.samplePath = read.get<std::string>(*sample, "path", "");
            const auto relative = read.get<std::string>(*sample, "relativePath", "");

            if (!channel.builtinSample.empty())
            {
                channel.sample = builtinSample(channel.builtinSample);
                if (channel.sample == nullptr)
                    read.warn("\"" + channel.name + "\" uses an unknown built-in sound \"" +
                              channel.builtinSample + "\".");
            }
            else if (!channel.samplePath.empty())
            {
                // Next to the project first (the folder may have moved), then where it was.
                std::vector<fs::path> candidates;
                if (!relative.empty() && !projectDir.empty())
                    candidates.push_back(projectDir / fs::path(relative));
                candidates.emplace_back(channel.samplePath);
                for (const auto& path : candidates)
                {
                    std::error_code ec;
                    if (fs::is_regular_file(path, ec) && loadSample)
                        if ((channel.sample = loadSample(path.string())) != nullptr)
                            break;
                }
                if (channel.sample == nullptr)
                    read.warn("Missing sample for \"" + channel.name + "\": " + channel.samplePath);
            }
        }
        project.channels.push_back(std::move(channel));
    }

    // Patterns and notes.
    for (const auto& p : read.array(root, "patterns"))
    {
        Pattern pattern;
        pattern.id = read.get<PatternId>(p, "id", 0);
        if (pattern.id == 0 || project.findPattern(pattern.id) != nullptr)
        {
            read.warn("A pattern with a missing or duplicate id was skipped.");
            continue;
        }
        pattern.name = read.get<std::string>(p, "name", "Pattern");
        // "minLength" in files from before 2026-10-17 (it used to set the playing length).
        pattern.gridSteps =
            read.clamped<int>(p, p.contains("gridLength") ? "gridLength" : "minLength",
                              Pattern::defaultLength, 4, Pattern::maxSteps);
        pattern.minLengthSteps = read.clamped<int>(p, "minPlayLength", 0, 0, Pattern::maxSteps);
        for (const auto& entry : read.array(p, "notes"))
        {
            const auto channel = read.get<ChannelId>(entry, "channel", 0);
            if (project.findChannel(channel) == nullptr)
            {
                read.warn("Notes for a channel that doesn't exist were skipped.");
                continue;
            }
            auto& list = pattern.notes[channel];
            for (const auto& n : read.array(entry, "notes"))
            {
                if (!n.is_array() || n.size() < 4 || !n[0].is_number() || !n[1].is_number() ||
                    !n[2].is_number() || !n[3].is_number())
                {
                    read.warn("A malformed note was skipped.");
                    continue;
                }
                Note note;
                note.startTick = std::max(0, n[0].get<int>());
                note.lengthTicks = std::max(1, n[1].get<int>());
                note.pitch = std::clamp(n[2].get<int>(), 0, 127);
                note.velocity = std::clamp(n[3].get<float>(), 0.0f, 1.0f);
                list.push_back(note);
            }
        }
        project.patterns.push_back(std::move(pattern));
    }
    if (project.patterns.empty())
        project.addPattern(); // a project always has at least one pattern

    // Playlist.
    const auto playlist = root.find("playlist");
    if (playlist != root.end() && playlist->is_object())
    {
        project.playlist.numTracks =
            read.clamped<int>(*playlist, "tracks", Playlist::defaultTracks, 1, 500);
        for (const auto& c : read.array(*playlist, "clips"))
        {
            Clip clip;
            clip.audio = read.get<ChannelId>(c, "audio", 0);
            clip.pattern = read.get<PatternId>(c, "pattern", 0);
            if (clip.isAudio() ? project.findChannel(clip.audio) == nullptr
                               : project.findPattern(clip.pattern) == nullptr)
            {
                read.warn(clip.isAudio()
                              ? "An audio clip for a channel that doesn't exist was skipped."
                              : "A Playlist clip for a pattern that doesn't exist was skipped.");
                continue;
            }
            clip.track = read.clamped<int>(c, "track", 0, 0, project.playlist.numTracks - 1);
            clip.startStep = std::max(0, read.get<int>(c, "start", 0));
            clip.lengthInSteps = std::max(1, read.get<int>(c, "length", Pattern::defaultLength));
            clip.offsetSteps = clip.isAudio() ? std::max(0, read.get<int>(c, "offset", 0)) : 0;
            project.playlist.clips.push_back(clip);
        }
    }

    // Mixer. Tracks are stored in order: [0] = Master.
    const auto mixer = root.find("mixer");
    if (mixer != root.end() && mixer->is_object())
    {
        const auto& tracks = read.array(*mixer, "tracks");
        if (tracks.size() > project.mixer.tracks.size())
            read.warn("The project has more mixer tracks than this version supports; the extra "
                      "ones were ignored.");
        for (std::size_t i = 0; i < tracks.size() && i < project.mixer.tracks.size(); ++i)
        {
            const auto& t = tracks[i];
            auto& track = project.mixer.tracks[i];
            track.name = read.get<std::string>(t, "name", track.name);
            track.volumeDb = read.clamped<float>(t, "volumeDb", 0.0f, MixerTrack::minVolumeDb,
                                                 MixerTrack::maxVolumeDb);
            track.pan = read.clamped<float>(t, "pan", 0.0f, -1.0f, 1.0f);
            track.width = read.clamped<float>(t, "width", 1.0f, 0.0f, MixerTrack::maxWidth);
            track.muted = read.get<bool>(t, "muted", false);
            track.solo = i > 0 && read.get<bool>(t, "solo", false);
            for (const auto& e : read.array(t, "effects"))
            {
                const auto type = read.get<std::string>(e, "type", "");
                const auto* info = engine::findEffectType(type);
                if (type == engine::pluginEffectType)
                {
                    // Kept even if the plugin isn't installed here: it stays silent (passes audio
                    // through) and is saved back unchanged.
                    if (static_cast<int>(track.effects.size()) >= MixerTrack::maxEffects)
                        continue;
                    EffectSlot slot;
                    slot.id = read.get<EffectId>(e, "id", 0);
                    slot.type = type;
                    slot.enabled = read.get<bool>(e, "enabled", true);
                    const auto plugin = e.find("plugin");
                    if (plugin != e.end() && plugin->is_object())
                    {
                        slot.pluginId = read.get<std::string>(*plugin, "id", "");
                        slot.pluginName = read.get<std::string>(*plugin, "name", "Plugin");
                        slot.pluginState = read.get<std::string>(*plugin, "state", "");
                    }
                    if (slot.pluginId.empty())
                    {
                        read.warn("A plugin effect on " + track.name +
                                  " has no plugin id; skipped.");
                        continue;
                    }
                    if (slot.id == 0 || project.findEffect(slot.id) != nullptr)
                        slot.id = 0;
                    track.effects.push_back(std::move(slot));
                    continue;
                }
                if (info == nullptr)
                {
                    read.warn("Unknown effect \"" + type + "\" on " + track.name + " was skipped.");
                    continue;
                }
                if (static_cast<int>(track.effects.size()) >= MixerTrack::maxEffects)
                {
                    read.warn("Too many effects on " + track.name +
                              "; the extra ones were skipped.");
                    break;
                }
                EffectSlot slot;
                slot.id = read.get<EffectId>(e, "id", 0);
                slot.type = type;
                slot.enabled = read.get<bool>(e, "enabled", true);
                slot.params = info->defaultParams();
                const auto params = e.find("params");
                if (params != e.end() && params->is_object())
                    for (std::size_t p = 0; p < info->params.size(); ++p)
                    {
                        const auto& param = info->params[p];
                        slot.params[p] =
                            std::clamp(read.get<float>(*params, param.name, param.defaultValue),
                                       param.min, param.max);
                    }
                if (slot.id == 0 || project.findEffect(slot.id) != nullptr)
                    slot.id = 0; // fixed below, once nextEffectId is known
                track.effects.push_back(std::move(slot));
            }
        }
    }

    // Id counters: never below what's in use, even if the file says otherwise.
    const auto ids = root.find("nextIds");
    const json noIds = json::object();
    const auto& idObj = ids != root.end() && ids->is_object() ? *ids : noIds;
    ChannelId maxChannel = 0;
    for (const auto& c : project.channels)
        maxChannel = std::max(maxChannel, c.id);
    PatternId maxPattern = 0;
    for (const auto& p : project.patterns)
        maxPattern = std::max(maxPattern, p.id);
    EffectId maxEffect = 0;
    for (const auto& t : project.mixer.tracks)
        for (const auto& e : t.effects)
            maxEffect = std::max(maxEffect, e.id);
    project.nextChannelId = std::max(read.get<ChannelId>(idObj, "channel", 1), maxChannel + 1);
    project.nextPatternId = std::max(read.get<PatternId>(idObj, "pattern", 1), maxPattern + 1);
    project.nextEffectId = std::max(read.get<EffectId>(idObj, "effect", 1), maxEffect + 1);
    for (auto& t : project.mixer.tracks)
        for (auto& e : t.effects)
            if (e.id == 0)
                e.id = project.nextEffectId++;

    // View state, falling back to something valid.
    const auto view = root.find("view");
    const auto& viewObj = view != root.end() && view->is_object() ? *view : noIds;
    project.currentPattern = read.get<PatternId>(viewObj, "currentPattern", 0);
    if (project.findPattern(project.currentPattern) == nullptr)
        project.currentPattern = project.patterns.front().id;
    project.selectedChannel = read.get<ChannelId>(viewObj, "selectedChannel", 0);
    if (project.findChannel(project.selectedChannel) == nullptr)
        project.selectedChannel = project.channels.empty() ? 0 : project.channels.front().id;
    project.selectedMixerTrack =
        read.clamped<int>(viewObj, "selectedMixerTrack", 1, 0, Mixer::numInserts);
    project.songMode = read.get<bool>(viewObj, "songMode", false);
    const auto scale = read.get<std::string>(viewObj, "scale", "none");
    project.scale = scale == "major" ? Scale::major : scale == "minor" ? Scale::minor : Scale::none;
    project.scaleRoot = read.clamped<int>(viewObj, "scaleRoot", 0, 0, 11);
    project.showAudioClips = read.get<bool>(viewObj, "showAudioClips", false);

    result.project = std::move(project);
    return result;
}

} // namespace jls::model
