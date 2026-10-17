#pragma once

// The real-time core: owns the transport, sampler, metronome and mixer buses, plays plugin
// instruments, and receives immutable EngineState snapshots from the UI thread through a
// lock-free queue. This is the only engine object the app talks to.

#include "engine/engine_state.h"
#include "engine/metronome.h"
#include "engine/sampler.h"
#include "engine/spsc_queue.h"
#include "engine/step_sequencer.h"
#include "engine/transport.h"

#include <array>
#include <atomic>
#include <memory>
#include <vector>

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

    // Before audio starts (may allocate). Also prepares the effects in the current state.
    void prepare(double sampleRate, int maxBlockSize);
    // Audio thread: renders one block into (pre-cleared or not) output channels, additively.
    void process(float* const* channels, int numChannels, int numSamples);

    // Format from the last prepare(), for creating effect processors on the UI thread.
    double preparedSampleRate() const { return preparedRate.load(); }
    int preparedBlockSize() const { return preparedBlock.load(); }

    // UI thread: highest absolute level on a mixer track since the last call, after its fader.
    float takePeak(int track, int side);

    // Notes currently held on instruments (for tests and debugging). Audio thread / offline.
    int heldNoteCount() const { return heldCount; }

  private:
    void adoptNewestState();
    void renderChunk(float* const* channels, int numChannels, int offset, int numSamples);
    void processTrack(int track, int numSamples);

    // Instruments. Every started note is held until its Trigger::endTick, so it always gets a
    // note-off, also when the pattern changes under it; stopping or jumping the transport
    // releases everything (plus an all-notes-off), so nothing can hang.
    struct HeldNote
    {
        std::uint32_t channelId = 0;
        int channelIndex = 0;
        const InstrumentProcessor* instrument = nullptr; // to notice a replaced instrument
        int pitch = 60;
        double endTick = 0.0;
    };
    struct ChannelEvent
    {
        int channelIndex = 0;
        int order = 0; // emission order, keeps same-offset events in sequence when sorting
        NoteEvent event;
    };
    static constexpr int maxHeldNotes = 512;
    static constexpr int maxNoteEvents = 2048;

    bool isInstrument(int channelIndex) const;
    void addNoteEvent(int channelIndex, NoteEvent event);
    void releaseAllNotes();
    void startInstrumentNote(const Trigger& trigger, int offsetOfEnd);
    void renderInstruments(int numTracks, int numSamples);

    Transport transportState;
    Metronome metronomeState;
    Sampler sampler;

    SpscQueue<EngineState*, 128> incoming; // UI -> audio
    SpscQueue<EngineState*, 128> retired;  // audio -> UI
    std::unique_ptr<EngineState> pending;  // UI thread only

    // Audio thread only.
    EngineState* current = nullptr;
    double sampleRate = 44100.0;
    int maxBlock = 0;
    std::array<Trigger, 1024> triggers{};
    std::vector<float> busMemory;                   // maxMixerTracks * 2 * maxBlock
    std::array<float*, maxMixerTracks * 2> buses{}; // into busMemory
    std::vector<float> instrumentMemory;            // 2 * maxBlock: one instrument's output
    std::array<HeldNote, maxHeldNotes> heldNotes{};
    int heldCount = 0;
    std::array<ChannelEvent, maxNoteEvents> noteEvents{};
    int noteEventCount = 0;
    bool wasPlaying = false;
    double lastEndBeat = 0.0;

    std::atomic<double> preparedRate{0.0};
    std::atomic<int> preparedBlock{0};
    std::array<std::atomic<float>, maxMixerTracks * 2> peaks{};
};

} // namespace jls::engine
