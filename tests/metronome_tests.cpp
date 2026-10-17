// Metronome: clicks on the exact sample of each beat, accents on bar downbeats, on/off.

#include "engine/metronome.h"
#include "engine/transport.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using jls::engine::Metronome;
using jls::engine::Transport;

namespace
{

constexpr double sampleRate = 48000.0;

// Runs transport + metronome for `totalSamples` in blocks of `blockSize`, mono.
std::vector<float> renderMono(Transport& transport, Metronome& metronome, int totalSamples,
                              int blockSize)
{
    std::vector<float> out(static_cast<size_t>(totalSamples), 0.0f);
    for (int start = 0; start < totalSamples; start += blockSize)
    {
        const int n = std::min(blockSize, totalSamples - start);
        float* channel = out.data() + start;
        const auto block = transport.advance(n, sampleRate);
        metronome.render(&channel, 1, n, block);
    }
    return out;
}

// Sample indices where a click starts: first non-silent sample after a long silence.
std::vector<int> onsets(const std::vector<float>& signal)
{
    std::vector<int> result;
    int silentRun = 1000;
    for (int i = 0; i < static_cast<int>(signal.size()); ++i)
    {
        if (std::abs(signal[static_cast<size_t>(i)]) > 1.0e-6f)
        {
            if (silentRun >= 1000)
                result.push_back(i);
            silentRun = 0;
        }
        else
        {
            ++silentRun;
        }
    }
    return result;
}

float peak(const std::vector<float>& signal, int from, int to)
{
    float p = 0.0f;
    for (int i = from; i < to; ++i)
        p = std::max(p, std::abs(signal[static_cast<size_t>(i)]));
    return p;
}

} // namespace

TEST_CASE("Metronome is silent when disabled or stopped")
{
    Transport transport;
    Metronome metronome;
    metronome.prepare(sampleRate);

    transport.play();
    CHECK(onsets(renderMono(transport, metronome, 96000, 512)).empty());

    metronome.setEnabled(true);
    transport.stop();
    CHECK(onsets(renderMono(transport, metronome, 96000, 512)).empty());
}

TEST_CASE("Metronome clicks on every beat, sample-accurately, across block sizes")
{
    for (int blockSize : {64, 441, 512, 4096})
    {
        Transport transport;
        transport.setTempo(120.0); // one beat = 24000 samples at 48 kHz
        Metronome metronome;
        metronome.prepare(sampleRate);
        metronome.setEnabled(true);
        transport.play();

        const auto clicks = onsets(renderMono(transport, metronome, 4 * 24000, blockSize));

        INFO("block size " << blockSize);
        REQUIRE(clicks.size() == 4);
        for (size_t i = 0; i < clicks.size(); ++i)
        {
            // sin() starts at 0, so the first audible sample is one after the beat.
            const int expected = static_cast<int>(i) * 24000;
            CHECK(std::abs(clicks[i] - expected) <= 1);
        }
    }
}

TEST_CASE("Downbeat of each bar is accented")
{
    Transport transport;
    transport.setTempo(120.0);
    transport.setBeatsPerBar(3);
    Metronome metronome;
    metronome.prepare(sampleRate);
    metronome.setEnabled(true);
    transport.play();

    const auto signal = renderMono(transport, metronome, 4 * 24000, 512);
    const float downbeat = peak(signal, 0, 2000);
    const float beat2 = peak(signal, 24000, 26000);
    const float nextDownbeat = peak(signal, 72000, 74000); // beat 3 in 3/4 = bar 2

    CHECK(downbeat > beat2 * 1.2f);
    CHECK(nextDownbeat > beat2 * 1.2f);
}

TEST_CASE("Resuming from pause clicks on the next beat, not immediately")
{
    Transport transport;
    transport.setTempo(120.0);
    Metronome metronome;
    metronome.prepare(sampleRate);
    metronome.setEnabled(true);

    transport.play();
    renderMono(transport, metronome, 36000, 512); // 1.5 beats
    transport.pause();
    renderMono(transport, metronome, 4800, 512); // let the tail die out
    transport.play();

    const auto clicks = onsets(renderMono(transport, metronome, 24000, 512));
    REQUIRE(clicks.size() == 1);
    // Resumed at beat 1.5 (approx., block-quantised pause), so beat 2 is ~12000 samples in.
    CHECK(std::abs(clicks[0] - 12000) <= 512);
}

TEST_CASE("Volume is clamped and scales the click")
{
    Metronome metronome;
    metronome.setVolume(2.0f);
    CHECK(metronome.volume() == 1.0f);
    metronome.setVolume(-1.0f);
    CHECK(metronome.volume() == 0.0f);

    auto clickPeak = [](float volume)
    {
        Transport transport;
        Metronome m;
        m.prepare(sampleRate);
        m.setEnabled(true);
        m.setVolume(volume);
        transport.play();
        return peak(renderMono(transport, m, 2000, 512), 0, 2000);
    };
    CHECK(clickPeak(0.25f) == Catch::Approx(clickPeak(1.0f) * 0.25f));
}
