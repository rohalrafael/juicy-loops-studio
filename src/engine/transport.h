#pragma once

#include <atomic>

namespace jls::engine
{

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

    double positionInBeats() const;

    // Audio thread: move the playhead forward by one block.
    void advance(int numSamples, double sampleRate);

private:
    std::atomic<bool> playing{false};
    std::atomic<bool> rewindRequested{false};
    std::atomic<double> bpm{defaultTempo};
    std::atomic<double> position{0.0}; // written only by the audio thread
};

} // namespace jls::engine
