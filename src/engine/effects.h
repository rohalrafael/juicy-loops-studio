#pragma once

#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace jls::engine
{

// One adjustable setting of an effect, in real units (Hz, dB, ms, 0..1, ...).
struct ParamInfo
{
    const char* name;
    float min;
    float max;
    float defaultValue;
    const char* unit = "";
    bool logarithmic = false;                // knobs move evenly in log space (frequencies, times)
    bool stepped = false;                    // whole numbers only (modes, switches)
    std::vector<const char*> stepNames = {}; // labels for stepped params, if any
};

// Audio processing for one effect instance. prepare() may allocate; process() and reset()
// must not, and run on the audio thread only.
class EffectProcessor
{
  public:
    virtual ~EffectProcessor() = default;
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;
    // Processes stereo audio in place. `params` holds one value per ParamInfo, in order.
    virtual void process(float* left, float* right, int numSamples,
                         std::span<const float> params) = 0;
};

struct EffectType
{
    const char* id;   // stored in projects; never change
    const char* name; // shown to the user
    std::vector<ParamInfo> params;
    std::unique_ptr<EffectProcessor> (*create)();

    std::vector<float> defaultParams() const;
};

// The built-in effects, in menu order: Filter, EQ, Delay, Reverb, Compressor, Soft Clipper.
const std::vector<EffectType>& effectTypes();
const EffectType* findEffectType(std::string_view id);

} // namespace jls::engine
