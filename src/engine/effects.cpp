// The built-in effects. Each is self-contained DSP with no allocation in process(); parameter
// values arrive per block in real units, in the order of the type's ParamInfo list.

#include "engine/effects.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace jls::engine
{
namespace
{

constexpr double pi = std::numbers::pi;

float dbToGain(float db)
{
    return std::pow(10.0f, db / 20.0f);
}

// Flushes tiny values to zero so feedback paths never fall into denormals.
inline float undenormal(float x)
{
    return std::abs(x) < 1.0e-15f ? 0.0f : x;
}

// ------------------------------------------------------------------------------- Filter
// Zavalishin's topology-preserving state-variable filter (stable under fast modulation).

class Filter final : public EffectProcessor
{
  public:
    void prepare(double rate, int) override
    {
        sampleRate = rate;
        reset();
    }

    void reset() override { state = {}; }

    void process(float* left, float* right, int n, std::span<const float> p) override
    {
        const double cutoff = std::clamp<double>(p[0], 20.0, 0.49 * sampleRate);
        const double g = std::tan(pi * cutoff / sampleRate);
        const double k = 2.0 - 1.95 * std::clamp<double>(p[1], 0.0, 1.0); // resonance
        const int mode = static_cast<int>(std::lround(p[2]));
        const double a1 = 1.0 / (1.0 + g * (g + k));
        const double a2 = g * a1;
        const double a3 = g * a2;

        float* io[2] = {left, right};
        for (int ch = 0; ch < 2; ++ch)
        {
            auto& [ic1, ic2] = state[static_cast<std::size_t>(ch)];
            for (int i = 0; i < n; ++i)
            {
                const double v0 = io[ch][i];
                const double v3 = v0 - ic2;
                const double v1 = a1 * ic1 + a2 * v3;
                const double v2 = ic2 + a2 * ic1 + a3 * v3;
                ic1 = undenormal(static_cast<float>(2.0 * v1 - ic1));
                ic2 = undenormal(static_cast<float>(2.0 * v2 - ic2));
                const double low = v2;
                const double band = v1;
                const double high = v0 - k * v1 - v2;
                io[ch][i] = static_cast<float>(mode == 0 ? low : (mode == 1 ? band : high));
            }
        }
    }

  private:
    double sampleRate = 44100.0;
    std::array<std::array<double, 2>, 2> state{};
};

// ------------------------------------------------------------------------------ 3-band EQ
// RBJ cookbook biquads: low shelf at 100 Hz, a peak at an adjustable frequency, high shelf
// at 8 kHz.

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    std::array<std::array<double, 2>, 2> z{}; // per channel

    float process(int ch, float x)
    {
        auto& s = z[static_cast<std::size_t>(ch)];
        const double y = b0 * x + s[0];
        s[0] = b1 * x - a1 * y + s[1];
        s[1] = b2 * x - a2 * y;
        return static_cast<float>(y);
    }

    void set(double nb0, double nb1, double nb2, double a0, double na1, double na2)
    {
        b0 = nb0 / a0;
        b1 = nb1 / a0;
        b2 = nb2 / a0;
        a1 = na1 / a0;
        a2 = na2 / a0;
    }

    void shelf(bool high, double rate, double freq, double gainDb)
    {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w = 2.0 * pi * freq / rate;
        const double cw = std::cos(w);
        const double alpha = std::sin(w) / 2.0 * std::sqrt(2.0); // shelf slope 1
        const double s = 2.0 * std::sqrt(A) * alpha;
        if (!high)
            set(A * ((A + 1) - (A - 1) * cw + s), 2 * A * ((A - 1) - (A + 1) * cw),
                A * ((A + 1) - (A - 1) * cw - s), (A + 1) + (A - 1) * cw + s,
                -2 * ((A - 1) + (A + 1) * cw), (A + 1) + (A - 1) * cw - s);
        else
            set(A * ((A + 1) + (A - 1) * cw + s), -2 * A * ((A - 1) + (A + 1) * cw),
                A * ((A + 1) + (A - 1) * cw - s), (A + 1) - (A - 1) * cw + s,
                2 * ((A - 1) - (A + 1) * cw), (A + 1) - (A - 1) * cw - s);
    }

    void peak(double rate, double freq, double gainDb, double q)
    {
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w = 2.0 * pi * freq / rate;
        const double alpha = std::sin(w) / (2.0 * q);
        const double cw = std::cos(w);
        set(1 + alpha * A, -2 * cw, 1 - alpha * A, 1 + alpha / A, -2 * cw, 1 - alpha / A);
    }
};

class Equalizer final : public EffectProcessor
{
  public:
    void prepare(double rate, int) override
    {
        sampleRate = rate;
        last.fill(std::nanf(""));
        reset();
    }

    void reset() override
    {
        for (auto* f : {&low, &mid, &high})
            f->z = {};
    }

    void process(float* left, float* right, int n, std::span<const float> p) override
    {
        const std::array<float, 4> now{p[0], p[1], p[2], p[3]};
        if (now != last)
        {
            low.shelf(false, sampleRate, 100.0, p[0]);
            mid.peak(sampleRate, std::clamp<double>(p[2], 20.0, 0.45 * sampleRate), p[1], 1.0);
            high.shelf(true, sampleRate, std::min(8000.0, 0.45 * sampleRate), p[3]);
            last = now;
        }
        float* io[2] = {left, right};
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                io[ch][i] = high.process(ch, mid.process(ch, low.process(ch, io[ch][i])));
    }

  private:
    double sampleRate = 44100.0;
    Biquad low, mid, high;
    std::array<float, 4> last{};
};

// ---------------------------------------------------------------------------------- Delay

class Delay final : public EffectProcessor
{
  public:
    static constexpr double maxSeconds = 2.0;

    void prepare(double rate, int) override
    {
        sampleRate = rate;
        for (auto& line : lines)
            line.assign(static_cast<std::size_t>(maxSeconds * rate) + 2, 0.0f);
        reset();
    }

    void reset() override
    {
        for (auto& line : lines)
            std::fill(line.begin(), line.end(), 0.0f);
        writePos = 0;
    }

    void process(float* left, float* right, int n, std::span<const float> p) override
    {
        const auto size = static_cast<int>(lines[0].size());
        if (size == 0)
            return;
        const int delay = std::clamp(static_cast<int>(p[0] / 1000.0 * sampleRate), 1, size - 1);
        const float feedback = std::clamp(p[1], 0.0f, 0.95f);
        const float mix = std::clamp(p[2], 0.0f, 1.0f);
        const bool pingPong = p[3] >= 0.5f;

        for (int i = 0; i < n; ++i)
        {
            const int readPos = (writePos - delay + size) % size;
            const float dl = lines[0][static_cast<std::size_t>(readPos)];
            const float dr = lines[1][static_cast<std::size_t>(readPos)];
            const float inL = left[i];
            const float inR = right[i];

            if (pingPong)
            {
                // Mono input enters the left line; each repeat crosses to the other side.
                lines[0][static_cast<std::size_t>(writePos)] =
                    undenormal(0.5f * (inL + inR) + dr * feedback);
                lines[1][static_cast<std::size_t>(writePos)] = undenormal(dl * feedback);
            }
            else
            {
                lines[0][static_cast<std::size_t>(writePos)] = undenormal(inL + dl * feedback);
                lines[1][static_cast<std::size_t>(writePos)] = undenormal(inR + dr * feedback);
            }

            left[i] = inL * (1.0f - mix) + dl * mix;
            right[i] = inR * (1.0f - mix) + dr * mix;
            writePos = (writePos + 1) % size;
        }
    }

  private:
    double sampleRate = 44100.0;
    std::array<std::vector<float>, 2> lines;
    int writePos = 0;
};

// --------------------------------------------------------------------------------- Reverb
// Freeverb (Jezar at Dreampoint, public domain): 8 parallel combs + 4 series allpasses per
// side, the right side's delays offset for stereo width.

class Reverb final : public EffectProcessor
{
  public:
    void prepare(double rate, int) override
    {
        static constexpr int combTunings[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
        static constexpr int allpassTunings[4] = {556, 441, 341, 225};
        static constexpr int stereoSpread = 23;
        const double scale = rate / 44100.0;

        for (int side = 0; side < 2; ++side)
        {
            for (int i = 0; i < 8; ++i)
                combs[side][i].buffer.assign(
                    static_cast<std::size_t>((combTunings[i] + side * stereoSpread) * scale), 0.0f);
            for (int i = 0; i < 4; ++i)
                allpasses[side][i].buffer.assign(
                    static_cast<std::size_t>((allpassTunings[i] + side * stereoSpread) * scale),
                    0.0f);
        }
        reset();
    }

    void reset() override
    {
        for (auto& side : combs)
            for (auto& c : side)
            {
                std::fill(c.buffer.begin(), c.buffer.end(), 0.0f);
                c.pos = 0;
                c.store = 0.0f;
            }
        for (auto& side : allpasses)
            for (auto& a : side)
            {
                std::fill(a.buffer.begin(), a.buffer.end(), 0.0f);
                a.pos = 0;
            }
    }

    void process(float* left, float* right, int n, std::span<const float> p) override
    {
        const float roomSize = std::clamp(p[0], 0.0f, 1.0f) * 0.28f + 0.7f;
        const float damping = std::clamp(p[1], 0.0f, 1.0f) * 0.4f;
        const float width = std::clamp(p[2], 0.0f, 1.0f);
        const float mix = std::clamp(p[3], 0.0f, 1.0f);
        const float wet1 = mix * 3.0f * (width / 2.0f + 0.5f);
        const float wet2 = mix * 3.0f * ((1.0f - width) / 2.0f);

        for (int i = 0; i < n; ++i)
        {
            const float input = (left[i] + right[i]) * 0.015f;
            float out[2] = {0.0f, 0.0f};
            for (int side = 0; side < 2; ++side)
            {
                for (auto& c : combs[side])
                    out[side] += c.process(input, roomSize, damping);
                for (auto& a : allpasses[side])
                    out[side] = a.process(out[side]);
            }
            left[i] = left[i] * (1.0f - mix) + out[0] * wet1 + out[1] * wet2;
            right[i] = right[i] * (1.0f - mix) + out[1] * wet1 + out[0] * wet2;
        }
    }

  private:
    struct Comb
    {
        std::vector<float> buffer;
        std::size_t pos = 0;
        float store = 0.0f;

        float process(float input, float feedback, float damp)
        {
            if (buffer.empty())
                return 0.0f;
            const float output = buffer[pos];
            store = undenormal(output * (1.0f - damp) + store * damp);
            buffer[pos] = undenormal(input + store * feedback);
            pos = (pos + 1) % buffer.size();
            return output;
        }
    };

    struct Allpass
    {
        std::vector<float> buffer;
        std::size_t pos = 0;

        float process(float input)
        {
            if (buffer.empty())
                return input;
            const float buffered = buffer[pos];
            buffer[pos] = undenormal(input + buffered * 0.5f);
            pos = (pos + 1) % buffer.size();
            return buffered - input;
        }
    };

    Comb combs[2][8];
    Allpass allpasses[2][4];
};

// ----------------------------------------------------------------------------- Compressor
// Feed-forward, stereo-linked peak compressor with a hard knee.

class Compressor final : public EffectProcessor
{
  public:
    void prepare(double rate, int) override
    {
        sampleRate = rate;
        reset();
    }

    void reset() override { reductionDb = 0.0f; }

    void process(float* left, float* right, int n, std::span<const float> p) override
    {
        const float threshold = p[0];
        const float ratio = std::max(1.0f, p[1]);
        const float attack =
            std::exp(-1.0f / static_cast<float>(std::max(1.0e-4, p[2] / 1000.0) * sampleRate));
        const float release =
            std::exp(-1.0f / static_cast<float>(std::max(1.0e-3, p[3] / 1000.0) * sampleRate));
        const float makeup = p[4];

        for (int i = 0; i < n; ++i)
        {
            const float level = std::max(std::abs(left[i]), std::abs(right[i]));
            const float levelDb = level > 1.0e-6f ? 20.0f * std::log10(level) : -120.0f;
            const float over = levelDb - threshold;
            const float target = over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
            const float coeff = target > reductionDb ? attack : release;
            reductionDb = target + coeff * (reductionDb - target);
            const float gain = dbToGain(makeup - reductionDb);
            left[i] *= gain;
            right[i] *= gain;
        }
    }

  private:
    double sampleRate = 44100.0;
    float reductionDb = 0.0f;
};

// --------------------------------------------------------------------------- Soft clipper
// Transparent below a threshold, then a tanh shoulder that approaches the ceiling and never
// goes past it. The shoulder starts with slope 1, so there's no corner where it takes over.
// (No oversampling yet: heavy drive on bright material can alias.)

class SoftClipper final : public EffectProcessor
{
  public:
    void prepare(double, int) override {}
    void reset() override {}

    void process(float* left, float* right, int n, std::span<const float> p) override
    {
        const float drive = dbToGain(std::max(0.0f, p[0]));
        const float ceiling = dbToGain(std::min(0.0f, p[1]));
        const float knee = std::clamp(p[2], 0.01f, 1.0f);
        const float threshold = ceiling * (1.0f - knee);
        const float range = ceiling - threshold;

        float* io[2] = {left, right};
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
            {
                const float x = io[ch][i] * drive;
                const float magnitude = std::abs(x);
                if (magnitude <= threshold)
                {
                    io[ch][i] = x;
                    continue;
                }
                const float shaped = threshold + range * std::tanh((magnitude - threshold) / range);
                io[ch][i] = std::copysign(shaped, x);
            }
    }
};

template <typename T> std::unique_ptr<EffectProcessor> make()
{
    return std::make_unique<T>();
}

} // namespace

std::vector<float> EffectType::defaultParams() const
{
    std::vector<float> values;
    for (const auto& p : params)
        values.push_back(p.defaultValue);
    return values;
}

const std::vector<EffectType>& effectTypes()
{
    static const std::vector<EffectType> types{
        {"filter",
         "Filter",
         {{"Cutoff", 20.0f, 20000.0f, 2000.0f, "Hz", true},
          {"Resonance", 0.0f, 1.0f, 0.2f},
          {"Mode", 0.0f, 2.0f, 0.0f, "", false, true, {"Low-pass", "Band-pass", "High-pass"}}},
         make<Filter>},
        {"eq",
         "EQ",
         {{"Low", -18.0f, 18.0f, 0.0f, "dB"},
          {"Mid", -18.0f, 18.0f, 0.0f, "dB"},
          {"Mid freq", 200.0f, 8000.0f, 1000.0f, "Hz", true},
          {"High", -18.0f, 18.0f, 0.0f, "dB"}},
         make<Equalizer>},
        {"delay",
         "Delay",
         {{"Time", 10.0f, 2000.0f, 375.0f, "ms", true},
          {"Feedback", 0.0f, 0.95f, 0.4f},
          {"Mix", 0.0f, 1.0f, 0.3f},
          {"Ping-pong", 0.0f, 1.0f, 0.0f, "", false, true, {"Off", "On"}}},
         make<Delay>},
        {"reverb",
         "Reverb",
         {{"Size", 0.0f, 1.0f, 0.6f},
          {"Damping", 0.0f, 1.0f, 0.4f},
          {"Width", 0.0f, 1.0f, 1.0f},
          {"Mix", 0.0f, 1.0f, 0.25f}},
         make<Reverb>},
        {"compressor",
         "Compressor",
         {{"Threshold", -60.0f, 0.0f, -18.0f, "dB"},
          {"Ratio", 1.0f, 20.0f, 4.0f, ":1", true},
          {"Attack", 0.1f, 100.0f, 10.0f, "ms", true},
          {"Release", 10.0f, 1000.0f, 120.0f, "ms", true},
          {"Makeup", 0.0f, 24.0f, 0.0f, "dB"}},
         make<Compressor>},
        {"softclip",
         "Soft Clipper",
         {{"Drive", 0.0f, 24.0f, 0.0f, "dB"},
          {"Ceiling", -12.0f, 0.0f, -0.3f, "dB"},
          {"Knee", 0.0f, 1.0f, 0.1f}},
         make<SoftClipper>},
    };
    return types;
}

const EffectType* findEffectType(std::string_view id)
{
    for (const auto& type : effectTypes())
        if (id == type.id)
            return &type;
    return nullptr;
}

} // namespace jls::engine
