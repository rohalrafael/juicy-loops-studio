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

std::shared_ptr<EffectProcessor> EffectPool::get(std::uint32_t id, std::string_view type)
{
    auto it = processors.find(id);
    if (it == processors.end() || it->second.type != type)
    {
        const auto* info = findEffectType(type);
        if (info == nullptr)
            return nullptr;
        Entry entry;
        entry.processor = info->create();
        entry.processor->prepare(sampleRate, maxBlockSize);
        entry.type = std::string(type);
        it = processors.insert_or_assign(id, std::move(entry)).first;
    }
    it->second.used = true;
    return it->second.processor;
}

void EffectPool::collectUnused()
{
    std::erase_if(processors, [](const auto& entry) { return !entry.second.used; });
    for (auto& [id, entry] : processors)
        entry.used = false;
}

} // namespace jls::engine
