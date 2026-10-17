#pragma once

// Stretched versions of Stretch-mode samples for the current tempo and pitch, made on a
// background thread (Signalsmith is far too slow for the audio thread) and kept while used.

#include "engine/sample_buffer.h"
#include "model/project.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <tuple>

namespace jls
{

class StretchCache
{
  public:
    StretchCache();
    ~StretchCache();

    // UI thread: a stretched sample finished; build a new snapshot so it gets used.
    std::function<void()> onReady;

    // The stretched sample for `channel` at the project's tempo, or null while it's being made
    // (a job is started the first time it's asked for).
    std::shared_ptr<const engine::SampleBuffer> get(const model::Project& project,
                                                    const model::Channel& channel);
    // The same, made right away if needed (export).
    std::shared_ptr<const engine::SampleBuffer> getNow(const model::Project& project,
                                                       const model::Channel& channel);

    // Around building a snapshot: results not asked for in between are dropped (an old tempo),
    // and their unfinished jobs are skipped.
    void beginSnapshot();
    void endSnapshot();

  private:
    struct Key
    {
        const engine::SampleBuffer* sample;
        long long factor; // stretch factor * 1e6
        long long pitch;  // semitones * 1e3
        bool operator<(const Key& o) const
        {
            return std::tie(sample, factor, pitch) < std::tie(o.sample, o.factor, o.pitch);
        }
    };
    struct Entry
    {
        std::shared_ptr<const engine::SampleBuffer> source; // kept alive for the job
        std::shared_ptr<const engine::SampleBuffer> result;
        // Shared with the job: cleared when the entry is dropped, so the job can skip the work.
        std::shared_ptr<std::atomic<bool>> wanted = std::make_shared<std::atomic<bool>>(true);
        bool used = true;
    };

    static Key keyFor(const model::Project& project, const model::Channel& channel);

    std::map<Key, Entry> entries;
    std::shared_ptr<bool> alive; // lets finished jobs know whether the cache still exists
    juce::ThreadPool pool{1};
};

} // namespace jls
