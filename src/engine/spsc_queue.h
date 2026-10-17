#pragma once

// A fixed-size, lock-free single-producer / single-consumer queue: how the UI thread and the
// audio thread hand things to each other without locks or allocation.

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>

namespace jls::engine
{

// Wait-free single-producer / single-consumer ring buffer.
// push() and hasSpace() from the producer thread only; pop() from the consumer thread only.
template <typename T, std::size_t Capacity> class SpscQueue
{
  public:
    bool push(T value)
    {
        const auto tail = writeIndex.load(std::memory_order_relaxed);
        const auto next = increment(tail);
        if (next == readIndex.load(std::memory_order_acquire))
            return false;

        slots[tail] = std::move(value);
        writeIndex.store(next, std::memory_order_release);
        return true;
    }

    bool hasSpace() const
    {
        const auto tail = writeIndex.load(std::memory_order_relaxed);
        return increment(tail) != readIndex.load(std::memory_order_acquire);
    }

    std::optional<T> pop()
    {
        const auto head = readIndex.load(std::memory_order_relaxed);
        if (head == writeIndex.load(std::memory_order_acquire))
            return std::nullopt;

        T value = std::move(slots[head]);
        readIndex.store(increment(head), std::memory_order_release);
        return value;
    }

  private:
    static constexpr std::size_t increment(std::size_t i) { return (i + 1) % (Capacity + 1); }

    std::array<T, Capacity + 1> slots{};
    alignas(64) std::atomic<std::size_t> writeIndex{0};
    alignas(64) std::atomic<std::size_t> readIndex{0};
};

} // namespace jls::engine
