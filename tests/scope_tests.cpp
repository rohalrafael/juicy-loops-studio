// Tests for the oscilloscope's ring buffer and trigger (engine/scope).

#include "engine/scope.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <vector>

using namespace jls::engine;

TEST_CASE("The scope buffer returns the newest samples, oldest first, across the wrap")
{
    auto buffer = std::make_unique<ScopeBuffer>();
    std::vector<float> left(1000), right(1000);
    float next = 0.0f;
    // Write more than the buffer holds, in uneven blocks, so it wraps.
    for (int block = 0; block < 6; ++block)
    {
        for (std::size_t i = 0; i < left.size(); ++i, next += 1.0f)
        {
            left[i] = next;
            right[i] = next + 2.0f; // the mono mix is next + 1
        }
        buffer->write(left.data(), right.data(), 1000);
    }
    std::vector<float> out(10);
    buffer->read(out.data(), 10);
    for (int i = 0; i < 10; ++i)
        CHECK(out[static_cast<std::size_t>(i)] == next - 10.0f + static_cast<float>(i) + 1.0f);

    // Mono output: the left side as it is.
    const float mono[] = {0.25f, -0.5f};
    buffer->write(mono, nullptr, 2);
    buffer->read(out.data(), 2);
    CHECK(out[0] == 0.25f);
    CHECK(out[1] == -0.5f);
}

TEST_CASE("The scope trigger starts on a rising zero crossing so a steady wave stands still")
{
    // A sine with a 100-sample period; the window must fit after the trigger.
    std::vector<float> wave(2048);
    for (std::size_t i = 0; i < wave.size(); ++i)
        wave[i] = static_cast<float>(
            std::sin(2.0 * std::numbers::pi * (static_cast<double>(i) + 37.0) / 100.0));
    const int start = findScopeTrigger(wave.data(), 2048, 1024);
    REQUIRE(start > 0);
    CHECK(start <= 2048 - 1024);
    CHECK(wave[static_cast<std::size_t>(start - 1)] <= 0.0f);
    CHECK(wave[static_cast<std::size_t>(start)] > 0.0f);
    CHECK((start + 37) % 100 <= 1); // the same phase every time (just after the zero)

    // Silence has no crossing: the newest window.
    std::vector<float> silence(2048, 0.0f);
    CHECK(findScopeTrigger(silence.data(), 2048, 1024) == 1024);
}
