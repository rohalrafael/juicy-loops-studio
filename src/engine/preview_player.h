#pragma once

// Plays one sample straight to the output, outside the mixer and the song: the Browser's
// previews. The UI thread sends commands through a lock-free queue; the audio thread renders.

#include "engine/sample_buffer.h"
#include "engine/spsc_queue.h"

#include <atomic>
#include <cstdint>

namespace jls::engine
{

class PreviewPlayer
{
  public:
    static constexpr float gain = 0.8f; // a little under unity, previews shouldn't clip

    // UI thread. Plays frames [startFrame, endFrame) of `sample` (raw pointer: the caller keeps
    // it alive until consumed() reaches the returned number). Returns the command's number, or
    // 0 if the queue was full.
    std::uint64_t play(const SampleBuffer* sample, double startFrame, double endFrame);
    std::uint64_t stop();

    // Any thread. The audio thread has applied every command up to this number, so samples
    // replaced by those commands are no longer used and may be freed.
    std::uint64_t consumed() const { return consumedCommand.load(); }
    bool isPlaying() const { return playing.load(); }
    double position() const { return positionFrames.load(); } // in the sample's frames

    // Audio thread.
    void prepare(double sampleRate);
    // Adds the preview into `out` (mono outputs get the left side).
    void render(float* const* out, int numChannels, int numSamples);

  private:
    struct Command
    {
        const SampleBuffer* sample = nullptr; // null = stop
        double start = 0.0;
        double end = 0.0;
        std::uint64_t number = 0;
    };

    SpscQueue<Command, 64> commands;
    std::uint64_t sent = 0; // UI thread
    std::atomic<std::uint64_t> consumedCommand{0};
    std::atomic<bool> playing{false};
    std::atomic<double> positionFrames{0.0};

    // Audio thread only.
    const SampleBuffer* sample = nullptr;
    double cursor = 0.0; // playing position, in the sample's frames
    double end = 0.0;
    double deviceRate = 44100.0;
};

} // namespace jls::engine
