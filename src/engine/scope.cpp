#include "engine/scope.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

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

void spectrumDb(const float* samples, int n, float* work, float* magnitudesDb) noexcept
{
    // Interleaved complex input (re, im), Hann-windowed.
    for (int i = 0; i < n; ++i)
    {
        const double hann = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * i / n);
        work[2 * i] = static_cast<float>(samples[i] * hann);
        work[2 * i + 1] = 0.0f;
    }
    // Iterative radix-2 FFT: bit-reversal, then butterflies.
    for (int i = 1, j = 0; i < n; ++i)
    {
        int bit = n >> 1;
        for (; (j & bit) != 0; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
        {
            std::swap(work[2 * i], work[2 * j]);
            std::swap(work[2 * i + 1], work[2 * j + 1]);
        }
    }
    for (int length = 2; length <= n; length <<= 1)
    {
        const double angle = -2.0 * std::numbers::pi / length;
        const double stepRe = std::cos(angle);
        const double stepIm = std::sin(angle);
        for (int start = 0; start < n; start += length)
        {
            double wRe = 1.0;
            double wIm = 0.0;
            for (int k = 0; k < length / 2; ++k)
            {
                const int a = 2 * (start + k);
                const int b = 2 * (start + k + length / 2);
                const double tRe = work[b] * wRe - work[b + 1] * wIm;
                const double tIm = work[b] * wIm + work[b + 1] * wRe;
                work[b] = static_cast<float>(work[a] - tRe);
                work[b + 1] = static_cast<float>(work[a + 1] - tIm);
                work[a] = static_cast<float>(work[a] + tRe);
                work[a + 1] = static_cast<float>(work[a + 1] + tIm);
                const double nextRe = wRe * stepRe - wIm * stepIm;
                wIm = wRe * stepIm + wIm * stepRe;
                wRe = nextRe;
            }
        }
    }
    // A full-scale sine through a Hann window peaks at n / 4.
    const float scale = 4.0f / static_cast<float>(n);
    for (int k = 0; k < n / 2; ++k)
    {
        const float magnitude = std::hypot(work[2 * k], work[2 * k + 1]) * scale;
        magnitudesDb[k] = 20.0f * std::log10(std::max(magnitude, 1.0e-6f));
    }
}

} // namespace jls::engine
