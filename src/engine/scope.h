#pragma once

// The output for the UI's oscilloscope. The audio thread writes what it outputs (left and right
// mixed to mono) into a ring of atomics; the UI copies the newest samples whenever it repaints.
// Lock-free and allocation-free. A reader may get a few samples from the next block mixed in,
// which a scope can live with.

#include <array>
#include <atomic>
#include <cstdint>

namespace jls::engine
{

class ScopeBuffer
{
  public:
    static constexpr int size = 4096; // samples kept (~85 ms at 48 kHz)

    // Audio thread. `right` may be null (mono output).
    void write(const float* left, const float* right, int numSamples) noexcept;
    // Any thread: the newest `count` samples (at most `size`), oldest first.
    void read(float* destination, int count) const noexcept;

  private:
    std::array<std::atomic<float>, size> samples{};
    std::atomic<std::uint32_t> written{0}; // total samples written; wraps harmlessly
};

// Where to start drawing `window` samples of `samples[0, count)` so a steady wave stands still:
// the last rising zero crossing that still leaves `window` samples after it, or the newest
// window when there's none (silence, noise).
int findScopeTrigger(const float* samples, int count, int window) noexcept;

} // namespace jls::engine
