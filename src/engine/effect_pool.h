#pragma once

// UI-thread cache of effect processors, keyed by effect id. It is how effect state (delay
// lines, reverb tails) outlives the immutable engine snapshots: each snapshot shares the same
// processor objects through shared_ptr.

#include "engine/effects.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace jls::engine
{

// Keeps one live EffectProcessor per effect id across engine snapshots, so an effect's
// internal state (delay lines, reverb tails) survives edits. UI thread only: processors are
// created and prepared here, never on the audio thread.
class EffectPool
{
  public:
    // Format used for new processors. Changing it drops all cached processors, so the next
    // snapshot gets fresh ones prepared for the new format.
    void setFormat(double sampleRate, int maxBlockSize);

    // Creates plugin effects: (plugin id, saved state, sample rate, block size) -> processor,
    // or null if the plugin isn't available. Set by the app, which knows how to host plugins.
    using PluginFactory = std::function<std::unique_ptr<EffectProcessor>(
        const std::string& pluginId, const std::string& state, double sampleRate, int blockSize)>;
    void setPluginFactory(PluginFactory factory) { pluginFactory = std::move(factory); }

    // The processor for `id`, created (and prepared) on first use. Null for unknown types and
    // unavailable plugins. For plugins, `pluginId` says which one and `state` restores it.
    std::shared_ptr<EffectProcessor> get(std::uint32_t id, std::string_view type,
                                         const std::string& pluginId = {},
                                         const std::string& state = {});

    // The live processor for `id` if one exists (e.g. to read a plugin's state). UI thread.
    std::shared_ptr<EffectProcessor> find(std::uint32_t id) const;

    // Call after building a snapshot: forgets processors whose ids weren't requested since
    // the previous call. (Snapshots still holding them keep them alive until retired.)
    void collectUnused();

    std::size_t size() const { return processors.size(); }

  private:
    struct Entry
    {
        std::shared_ptr<EffectProcessor> processor;
        std::string type;
        std::string pluginId;
        bool used = false;
    };

    std::map<std::uint32_t, Entry> processors;
    double sampleRate = 44100.0;
    int maxBlockSize = 512;
    PluginFactory pluginFactory;
};

} // namespace jls::engine
