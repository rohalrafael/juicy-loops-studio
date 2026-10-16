#pragma once

#include <atomic>

namespace jls::engine
{

// The span of musical time one audio block covers: beats in [startBeat, endBeat).
struct BlockPosition
{
    double startBeat = 0.0;
    double endBeat = 0.0;
    int beatsPerBar = 4;
    bool playing = false;
};

// Song transport: play state, tempo and playhead position.
//
// Threading: play()/stop()/setTempo() are called from the UI thread.
// advance() is called only from the audio thread. Nothing here locks or allocates.
class Transport
{
  public:
    static constexpr double minTempo = 10.0;
    static constexpr double maxTempo = 999.0;
    static constexpr double defaultTempo = 130.0;
    static constexpr int minBeatsPerBar = 1;
    static constexpr int maxBeatsPerBar = 16;

    void play();
    // Stops playback but keeps the playhead where it is.
    void pause();
    // Stops playback and rewinds to the start, like FL's stop button.
    void stop();
    // Play <-> stop (rewinds), like FL's spacebar.
    void togglePlay();
    // Play <-> pause, like FL's play button.
    void togglePause();
    bool isPlaying() const;

    void setTempo(double bpm);
    double tempo() const;

    // Time signature numerator; the beat is always a quarter note for now.
    void setBeatsPerBar(int beats);
    int beatsPerBar() const;

    double positionInBeats() const;

    // Audio thread: move the playhead forward by one block and return the span it covered.
    BlockPosition advance(int numSamples, double sampleRate);

  private:
    std::atomic<bool> playing{false};
    std::atomic<bool> rewindRequested{false};
    std::atomic<double> bpm{defaultTempo};
    std::atomic<int> barLength{4};
    std::atomic<double> position{0.0}; // written only by the audio thread

    // Audio thread only. The playhead is computed from a whole sample count since the last
    // tempo/sample-rate change instead of summing per-block increments, so it doesn't drift.
    double anchorBeat = 0.0;
    long long samplesSinceAnchor = 0;
    double anchorBpm = 0.0;
    double anchorSampleRate = 0.0;
};

} // namespace jls::engine
