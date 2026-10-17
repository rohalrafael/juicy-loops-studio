#pragma once

// UI-thread cache of effect processors (keyed by effect id) and plugin instruments (keyed by
// channel id). It is how their state (delay lines, reverb tails, a synth's held voices) outlives
// the immutable engine snapshots: each snapshot shares the same objects through shared_ptr.

#include "engine/effects.h"
#include "engine/instrument.h"

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

    // Creates plugin instruments, like PluginFactory for effects.
    using InstrumentFactory = std::function<std::unique_ptr<InstrumentProcessor>(
        const std::string& pluginId, const std::string& state, double sampleRate, int blockSize)>;
    void setInstrumentFactory(InstrumentFactory factory) { instrumentFactory = std::move(factory); }

    // The instrument of channel `channelId`, created (and prepared) on first use or when the
    // plugin changes. Null if the plugin isn't available.
    std::shared_ptr<InstrumentProcessor> getInstrument(std::uint32_t channelId,
                                                       const std::string& pluginId,
                                                       const std::string& state = {});
    std::shared_ptr<InstrumentProcessor> findInstrument(std::uint32_t channelId) const;

    // Call after building a snapshot: forgets processors and instruments whose ids weren't
    // requested since the previous call. (Snapshots holding them keep them alive until retired.)
    void collectUnused();

    std::size_t size() const { return processors.size(); }
    std::size_t instrumentCount() const { return instruments.size(); }

  private:
    struct Entry
    {
        std::shared_ptr<EffectProcessor> processor;
        std::string type;
        std::string pluginId;
        bool used = false;
    };

    struct InstrumentEntry
    {
        std::shared_ptr<InstrumentProcessor> instrument;
        std::string pluginId;
        bool used = false;
    };

    std::map<std::uint32_t, Entry> processors;
    std::map<std::uint32_t, InstrumentEntry> instruments;
    double sampleRate = 44100.0;
    int maxBlockSize = 512;
    PluginFactory pluginFactory;
    InstrumentFactory instrumentFactory;
};

} // namespace jls::engine
