#include "engine/scope.h"

#include <algorithm>

namespace jls::engine
{

void ScopeBuffer::write(const float* left, const float* right, int numSamples) noexcept
{
    const auto start = written.load(std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i)
    {
        const float value = right != nullptr ? 0.5f * (left[i] + right[i]) : left[i];
        samples[(start + static_cast<std::uint32_t>(i)) % size].store(value,
                                                                      std::memory_order_relaxed);
    }
    written.store(start + static_cast<std::uint32_t>(numSamples), std::memory_order_release);
}

void ScopeBuffer::read(float* destination, int count) const noexcept
{
    count = std::clamp(count, 0, size);
    const auto end = written.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i)
    {
        const auto index = end - static_cast<std::uint32_t>(count) + static_cast<std::uint32_t>(i);
        destination[i] = samples[index % size].load(std::memory_order_relaxed);
    }
}

int findScopeTrigger(const float* samples, int count, int window) noexcept
{
    const int latest = std::max(0, count - window);
    // Search backwards from the newest possible start for negative (or zero) going positive.
    for (int i = latest; i > 0; --i)
        if (samples[i - 1] <= 0.0f && samples[i] > 0.0f)
            return i;
    return latest;
}

} // namespace jls::engine
