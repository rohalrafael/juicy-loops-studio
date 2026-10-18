#pragma once

// The real-time core: owns the transport, sampler, metronome and mixer buses, plays plugin
// instruments, and receives immutable EngineState snapshots from the UI thread through a
// lock-free queue. This is the only engine object the app talks to.

#include "engine/engine_state.h"
#include "engine/metronome.h"
#include "engine/preview_player.h"
#include "engine/sampler.h"
#include "engine/scope.h"
#include "engine/spsc_queue.h"
#include "engine/step_sequencer.h"
#include "engine/transport.h"

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace jls::engine
{

// A note or controller played live (computer keyboard, MIDI input). See Engine::sendLive().
struct LiveEvent
{
    enum class Kind : std::uint8_t
    {
        noteOn,     // starts `number` on channel `channelId` at velocity `value`
        noteOff,    // releases `number`, on whichever channel it was started
        controller, // MIDI controller `number` = `value` (0..1) on `channelId`
        pitchBend,  // `value` -1..1 on `channelId`
        releaseAll, // ends every live note (e.g. the window lost keyboard focus)
    };

    Kind kind = Kind::noteOn;
    std::uint32_t channelId = 0; // target for noteOn / controller / pitchBend
    int number = 60;             // pitch or controller number
    float value = 0.0f;          // velocity, controller value or bend
};

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
    PreviewPlayer& preview() { return previewPlayer; }       // the Browser's sample previews
    const ScopeBuffer& scope() const { return scopeBuffer; } // the output, for the oscilloscope

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
    int liveNoteCount() const { return liveCount; }
    // What effects and instruments were last told about the playback position.
    const PlayPosition& lastPlayPosition() const { return playPosition; }

    // Any thread but the audio thread (UI, MIDI input): queues a live event; the audio thread
    // applies it at the start of its next block. Senders are serialized with a mutex the audio
    // thread never takes. Returns false if the queue is full (the event is dropped).
    // Live notes play instruments (held until their note-off) and trigger samples. A held
    // sustain pedal is lifted on the channel it went down on, so changing the selected channel
    // can't leave notes ringing.
    bool sendLive(const LiveEvent& event);

  private:
    void adoptNewestState();
    void renderChunk(float* const* channels, int numChannels, int offset, int numSamples);
    void updatePlayPosition(const BlockPosition& block);
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
    int indexOfChannel(std::uint32_t id) const;
    void applyLiveEvents();
    void releaseLiveNotes();
    // After a snapshot change: moves held notes to their channel's new index and drops those
    // whose channel or instrument is gone. Returns the new count.
    int remapHeld(HeldNote* notes, int count) const;
    void addNoteEvent(int channelIndex, NoteEvent event);
    void releaseAllNotes();
    void startInstrumentNote(const Trigger& trigger, int offsetOfEnd);
    void renderInstruments(int numTracks, int numSamples);

    Transport transportState;
    Metronome metronomeState;
    PreviewPlayer previewPlayer;
    ScopeBuffer scopeBuffer;
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
    PlayPosition playPosition; // this chunk's, handed to every effect and instrument

    // Live input: queue (UI/MIDI threads -> audio) and what is held (audio thread only).
    static constexpr int maxLiveNotes = 128;
    std::mutex liveSenders;
    SpscQueue<LiveEvent, 1024> liveEvents;
    std::array<HeldNote, maxLiveNotes> liveNotes{};
    int liveCount = 0;
    HeldNote sustain{}; // channel whose sustain pedal is down; channelId 0 = none

    std::atomic<double> preparedRate{0.0};
    std::atomic<int> preparedBlock{0};
    std::array<std::atomic<float>, maxMixerTracks * 2> peaks{};
};

} // namespace jls::engine
