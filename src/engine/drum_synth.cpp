#include "engine/drum_synth.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace jls::engine
{
namespace
{

constexpr double twoPi = 2.0 * std::numbers::pi;

// Deterministic white noise in [-1, 1], so every run sounds identical.
class Noise
{
  public:
    float next()
    {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<float>(static_cast<double>(state >> 11) * 0x1.0p-53 * 2.0 - 1.0);
    }

  private:
    std::uint64_t state = 0x9E3779B97F4A7C15ull;
};

SampleBuffer makeBuffer(double sampleRate, double seconds)
{
    SampleBuffer buffer;
    buffer.sampleRate = sampleRate;
    buffer.channels.assign(1, std::vector<float>(static_cast<std::size_t>(seconds * sampleRate)));
    return buffer;
}

// The sounds' peaks, scaled down so a busy default beat (several drums at once, channels at 80%)
// stays below full scale instead of driving the Master's clipper: at full level it peaked at
// +4.6 dBFS and sounded loud and gritty.
constexpr float headroom = 0.45f; // about -7 dB

void normalise(SampleBuffer& buffer, float peakTarget)
{
    float peak = 0.0f;
    for (float s : buffer.channels[0])
        peak = std::max(peak, std::abs(s));
    if (peak > 0.0f)
        for (float& s : buffer.channels[0])
            s *= peakTarget / peak;
}

SampleBuffer kick(double sr)
{
    auto buffer = makeBuffer(sr, 0.45);
    double phase = 0.0;
    for (std::size_t i = 0; i < buffer.channels[0].size(); ++i)
    {
        const double t = static_cast<double>(i) / sr;
        const double frequency = 45.0 + 110.0 * std::exp(-t * 30.0); // pitch sweep down
        phase += twoPi * frequency / sr;
        const double amp = std::exp(-t * 7.0) * std::min(1.0, t * 2000.0); // tiny attack ramp
        buffer.channels[0][i] = static_cast<float>(std::tanh(1.5 * std::sin(phase)) * amp);
    }
    normalise(buffer, 0.9f * headroom);
    return buffer;
}

SampleBuffer snare(double sr)
{
    auto buffer = makeBuffer(sr, 0.25);
    Noise noise;
    float highpassed = 0.0f, previous = 0.0f;
    for (std::size_t i = 0; i < buffer.channels[0].size(); ++i)
    {
        const double t = static_cast<double>(i) / sr;
        const float n = noise.next();
        highpassed = 0.85f * (highpassed + n - previous);
        previous = n;
        const double tone = std::sin(twoPi * 185.0 * t) * std::exp(-t * 30.0);
        const double rattle = highpassed * std::exp(-t * 18.0);
        buffer.channels[0][i] = static_cast<float>(0.6 * tone + 0.8 * rattle);
    }
    normalise(buffer, 0.8f * headroom);
    return buffer;
}

SampleBuffer clap(double sr)
{
    auto buffer = makeBuffer(sr, 0.3);
    Noise noise;
    float band = 0.0f, low = 0.0f;
    const double burstGap = 0.011; // three quick bursts, then the tail
    for (std::size_t i = 0; i < buffer.channels[0].size(); ++i)
    {
        const double t = static_cast<double>(i) / sr;
        // Crude band-pass around ~1.2 kHz (state-variable style).
        const float n = noise.next();
        low += 0.15f * band;
        band += 0.15f * (n - low - 0.5f * band);

        double envelope;
        if (t < 3 * burstGap)
            envelope = std::exp(-std::fmod(t, burstGap) * 300.0);
        else
            envelope = std::exp(-(t - 3 * burstGap) * 14.0);
        buffer.channels[0][i] = static_cast<float>(band * envelope);
    }
    normalise(buffer, 0.75f * headroom);
    return buffer;
}

SampleBuffer hat(double sr)
{
    auto buffer = makeBuffer(sr, 0.09);
    Noise noise;
    float highpassed = 0.0f, previous = 0.0f;
    for (std::size_t i = 0; i < buffer.channels[0].size(); ++i)
    {
        const double t = static_cast<double>(i) / sr;
        const float n = noise.next();
        highpassed = 0.6f * (highpassed + n - previous);
        previous = n;
        buffer.channels[0][i] = static_cast<float>(highpassed * std::exp(-t * 45.0));
    }
    normalise(buffer, 0.5f * headroom);
    return buffer;
}

} // namespace

SampleBuffer synthesizeDrum(DrumSound sound, double sampleRate)
{
    switch (sound)
    {
    case DrumSound::kick:
        return kick(sampleRate);
    case DrumSound::clap:
        return clap(sampleRate);
    case DrumSound::hat:
        return hat(sampleRate);
    case DrumSound::snare:
        return snare(sampleRate);
    }
    return {};
}

} // namespace jls::engine
