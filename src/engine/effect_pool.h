#pragma once

#include "engine/effects.h"

#include <cstdint>
#include <map>
#include <memory>
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

    // The processor for `id`, created (and prepared) on first use. Null for unknown types.
    std::shared_ptr<EffectProcessor> get(std::uint32_t id, std::string_view type);

    // Call after building a snapshot: forgets processors whose ids weren't requested since
    // the previous call. (Snapshots still holding them keep them alive until retired.)
    void collectUnused();

    std::size_t size() const { return processors.size(); }

  private:
    struct Entry
    {
        std::shared_ptr<EffectProcessor> processor;
        std::string type;
        bool used = false;
    };

    std::map<std::uint32_t, Entry> processors;
    double sampleRate = 44100.0;
    int maxBlockSize = 512;
};

} // namespace jls::engine
