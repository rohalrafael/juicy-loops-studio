#pragma once

#include "engine/engine_state.h"
#include "engine/metronome.h"
#include "engine/sampler.h"
#include "engine/spsc_queue.h"
#include "engine/step_sequencer.h"
#include "engine/transport.h"

#include <array>
#include <memory>

namespace jls::engine
{

// Owns everything that runs on the audio thread and the hand-off of state to it.
//
// UI thread:    setState(), collectGarbage(), transport()/metronome() controls.
// Audio thread: prepare(), process().
// New states travel to the audio thread through a lock-free queue; replaced states travel
// back and are deleted in collectGarbage(), so the audio thread never allocates or frees.
class Engine
{
  public:
    Engine() = default;
    ~Engine(); // call only after audio has stopped
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    Transport& transport() { return transportState; }
    Metronome& metronome() { return metronomeState; }

    // UI thread. Publishes a new snapshot; if the queue is full it's retried by collectGarbage().
    void setState(std::unique_ptr<EngineState> state);
    // UI thread, call regularly (e.g. from a timer): frees retired states, retries pending ones.
    void collectGarbage();

    void prepare(double sampleRate);
    // Audio thread: renders one block into (pre-cleared or not) output channels, additively.
    void process(float* const* channels, int numChannels, int numSamples);

  private:
    void adoptNewestState();

    Transport transportState;
    Metronome metronomeState;
    Sampler sampler;

    SpscQueue<EngineState*, 128> incoming; // UI -> audio
    SpscQueue<EngineState*, 128> retired;  // audio -> UI
    std::unique_ptr<EngineState> pending;  // UI thread only

    // Audio thread only.
    EngineState* current = nullptr;
    double sampleRate = 44100.0;
    std::array<Trigger, 1024> triggers{};
};

} // namespace jls::engine
