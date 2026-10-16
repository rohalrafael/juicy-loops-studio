#include "engine/effect_pool.h"

namespace jls::engine
{

void EffectPool::setFormat(double newSampleRate, int newMaxBlockSize)
{
    if (newSampleRate == sampleRate && newMaxBlockSize == maxBlockSize)
        return;
    sampleRate = newSampleRate;
    maxBlockSize = newMaxBlockSize;
    processors.clear();
}

std::shared_ptr<EffectProcessor> EffectPool::get(std::uint32_t id, std::string_view type,
                                                 const std::string& pluginId,
                                                 const std::string& state)
{
    auto it = processors.find(id);
    if (it == processors.end() || it->second.type != type || it->second.pluginId != pluginId)
    {
        std::unique_ptr<EffectProcessor> processor;
        if (type == pluginEffectType)
        {
            if (pluginFactory)
                processor = pluginFactory(pluginId, state, sampleRate, maxBlockSize);
        }
        else if (const auto* info = findEffectType(type))
        {
            processor = info->create();
        }
        // A failed creation is remembered too (null processor), so an unavailable plugin isn't
        // retried on every edit; it's retried when the format or the plugin id changes.
        if (processor != nullptr)
            processor->prepare(sampleRate, maxBlockSize);
        Entry entry;
        entry.processor = std::move(processor);
        entry.type = std::string(type);
        entry.pluginId = pluginId;
        it = processors.insert_or_assign(id, std::move(entry)).first;
    }
    it->second.used = true;
    return it->second.processor;
}

std::shared_ptr<EffectProcessor> EffectPool::find(std::uint32_t id) const
{
    const auto it = processors.find(id);
    return it == processors.end() ? nullptr : it->second.processor;
}

void EffectPool::collectUnused()
{
    std::erase_if(processors, [](const auto& entry) { return !entry.second.used; });
    for (auto& [id, entry] : processors)
        entry.used = false;
}

} // namespace jls::engine
