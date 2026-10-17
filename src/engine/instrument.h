#pragma once

// Instruments that play a channel's notes and produce audio (hosted plugin synths). The engine
// sends them note events with exact sample offsets; plain C++ so the engine stays JUCE-free.

#include <cstdint>
#include <span>
#include <string>

namespace jls::engine
{

// One note event within an audio block.
struct NoteEvent
{
    enum class Kind : std::uint8_t
    {
        noteOff,     // releases `pitch`
        noteOn,      // starts `pitch` at `velocity`
        allNotesOff, // panic: sent when the transport stops or jumps
    };

    Kind kind = Kind::noteOn;
    int sampleOffset = 0;  // within the block
    int pitch = 60;        // MIDI note number
    float velocity = 0.0f; // 0..1 (note-ons)
};

// Audio generation for one channel. prepare() may allocate; process() and reset() must not,
// and run on the audio thread only.
class InstrumentProcessor
{
  public:
    virtual ~InstrumentProcessor() = default;
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;
    // Writes (not adds) numSamples of stereo output, applying `events` (sorted by offset) at
    // their sample offsets.
    virtual void process(std::span<const NoteEvent> events, float* left, float* right,
                         int numSamples) = 0;

    // Opaque state to store in the project (a plugin's settings). UI thread.
    virtual std::string saveState() const { return {}; }
};

} // namespace jls::engine
