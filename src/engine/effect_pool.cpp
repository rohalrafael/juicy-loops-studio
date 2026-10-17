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
    instruments.clear();
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

std::shared_ptr<InstrumentProcessor> EffectPool::getInstrument(std::uint32_t channelId,
                                                               const std::string& pluginId,
                                                               const std::string& state)
{
    auto it = instruments.find(channelId);
    if (it == instruments.end() || it->second.pluginId != pluginId)
    {
        // Failures are remembered like for effects (null instrument).
        std::unique_ptr<InstrumentProcessor> instrument;
        if (instrumentFactory)
            instrument = instrumentFactory(pluginId, state, sampleRate, maxBlockSize);
        if (instrument != nullptr)
            instrument->prepare(sampleRate, maxBlockSize);
        InstrumentEntry entry;
        entry.instrument = std::move(instrument);
        entry.pluginId = pluginId;
        it = instruments.insert_or_assign(channelId, std::move(entry)).first;
    }
    it->second.used = true;
    return it->second.instrument;
}

std::shared_ptr<InstrumentProcessor> EffectPool::findInstrument(std::uint32_t channelId) const
{
    const auto it = instruments.find(channelId);
    return it == instruments.end() ? nullptr : it->second.instrument;
}

void EffectPool::collectUnused()
{
    std::erase_if(processors, [](const auto& entry) { return !entry.second.used; });
    for (auto& [id, entry] : processors)
        entry.used = false;
    std::erase_if(instruments, [](const auto& entry) { return !entry.second.used; });
    for (auto& [id, entry] : instruments)
        entry.used = false;
}

} // namespace jls::engine
