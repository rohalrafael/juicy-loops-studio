#include "app/stretch_cache.h"

#include "engine/time_stretch.h"

#include <juce_events/juce_events.h>

#include <cmath>

namespace jls
{

StretchCache::StretchCache() : alive(std::make_shared<bool>(true)) {}

StretchCache::~StretchCache()
{
    *alive = false;
    pool.removeAllJobs(true, 10000);
}

StretchCache::Key StretchCache::keyFor(const model::Project& project, const model::Channel& channel)
{
    return {channel.sample.get(), std::llround(project.stretchFactor(channel) * 1e6),
            std::llround(channel.sampler.pitch * 1e3)};
}

std::shared_ptr<const engine::SampleBuffer> StretchCache::get(const model::Project& project,
                                                              const model::Channel& channel)
{
    if (channel.sample == nullptr)
        return nullptr;
    const auto key = keyFor(project, channel);
    auto it = entries.find(key);
    if (it != entries.end())
    {
        it->second.used = true;
        return it->second.result;
    }

    auto& entry = entries[key];
    entry.source = channel.sample;
    const double factor = project.stretchFactor(channel);
    const double pitch = channel.sampler.pitch;
    auto source = channel.sample;
    auto wanted = entry.wanted;
    auto flag = alive;
    pool.addJob(
        [this, key, source, factor, pitch, wanted, flag]
        {
            // Skip a tempo that's already gone (e.g. while the tempo is being dragged).
            if (!wanted->load())
                return juce::ThreadPoolJob::jobHasFinished;
            auto result = engine::stretchSample(*source, factor, pitch);
            juce::MessageManager::callAsync(
                [this, key, result, flag]
                {
                    if (!*flag)
                        return;
                    auto found = entries.find(key);
                    if (found == entries.end())
                        return; // no longer wanted
                    // A sample too short to stretch plays as it is.
                    found->second.result = result != nullptr ? result : found->second.source;
                    if (onReady)
                        onReady();
                });
            return juce::ThreadPoolJob::jobHasFinished;
        });
    return nullptr;
}

std::shared_ptr<const engine::SampleBuffer> StretchCache::getNow(const model::Project& project,
                                                                 const model::Channel& channel)
{
    if (auto ready = get(project, channel))
        return ready;
    auto result = engine::stretchSample(*channel.sample, project.stretchFactor(channel),
                                        channel.sampler.pitch);
    auto& entry = entries[keyFor(project, channel)];
    entry.result = result != nullptr ? result : channel.sample;
    return entry.result;
}

void StretchCache::beginSnapshot()
{
    for (auto& [key, entry] : entries)
        entry.used = false;
}

void StretchCache::endSnapshot()
{
    for (auto& [key, entry] : entries)
        if (!entry.used)
            entry.wanted->store(false);
    std::erase_if(entries, [](const auto& e) { return !e.second.used; });
}

} // namespace jls
