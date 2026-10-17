// The transport: play / pause / stop, tempo, a long-run drift check, and seeking.

#include "engine/transport.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using jls::engine::Transport;

TEST_CASE("Transport starts stopped at the default tempo")
{
    Transport t;
    CHECK_FALSE(t.isPlaying());
    CHECK(t.tempo() == Transport::defaultTempo);
    CHECK(t.positionInBeats() == 0.0);
}

TEST_CASE("Playhead advances by tempo while playing")
{
    Transport t;
    t.setTempo(120.0);
    t.advance(48000, 48000.0); // not playing: no movement
    CHECK(t.positionInBeats() == 0.0);

    t.play();
    t.advance(48000, 48000.0); // one second at 120 BPM = 2 beats
    CHECK(t.positionInBeats() == Catch::Approx(2.0));
}

TEST_CASE("Pause keeps the playhead, stop rewinds it")
{
    Transport t;
    t.setTempo(60.0);
    t.play();
    t.advance(44100, 44100.0);

    t.togglePause();
    CHECK_FALSE(t.isPlaying());
    t.advance(44100, 44100.0);
    CHECK(t.positionInBeats() == Catch::Approx(1.0));

    t.togglePause();
    CHECK(t.isPlaying());

    t.stop();
    t.advance(512, 44100.0); // rewind is applied on the audio thread
    CHECK(t.positionInBeats() == 0.0);
}

TEST_CASE("togglePlay goes play -> stop with rewind")
{
    Transport t;
    t.togglePlay();
    t.advance(44100, 44100.0);
    CHECK(t.positionInBeats() > 0.0);

    t.togglePlay();
    t.advance(1, 44100.0);
    CHECK_FALSE(t.isPlaying());
    CHECK(t.positionInBeats() == 0.0);
}

TEST_CASE("Tempo is clamped to the supported range")
{
    Transport t;
    t.setTempo(1.0);
    CHECK(t.tempo() == Transport::minTempo);
    t.setTempo(5000.0);
    CHECK(t.tempo() == Transport::maxTempo);
}

TEST_CASE("advance reports the beat span it covered")
{
    Transport t;
    t.setTempo(120.0);
    t.setBeatsPerBar(3);

    auto stopped = t.advance(48000, 48000.0);
    CHECK_FALSE(stopped.playing);
    CHECK(stopped.startBeat == stopped.endBeat);

    t.play();
    auto first = t.advance(24000, 48000.0);
    auto second = t.advance(24000, 48000.0);
    CHECK(first.playing);
    CHECK(first.beatsPerBar == 3);
    CHECK(first.startBeat == 0.0);
    CHECK(first.endBeat == Catch::Approx(1.0));
    CHECK(second.startBeat == first.endBeat); // contiguous, no gaps or overlaps
}

TEST_CASE("Beats per bar is clamped")
{
    Transport t;
    t.setBeatsPerBar(0);
    CHECK(t.beatsPerBar() == Transport::minBeatsPerBar);
    t.setBeatsPerBar(99);
    CHECK(t.beatsPerBar() == Transport::maxBeatsPerBar);
}

TEST_CASE("Seeking moves the playhead, playing or not")
{
    Transport transport;
    transport.seek(6.5);
    auto block = transport.advance(512, 48000.0); // stopped: the seek still applies
    CHECK_FALSE(block.playing);
    CHECK(transport.positionInBeats() == 6.5);

    transport.play();
    block = transport.advance(24000, 48000.0); // 130 BPM: 24000 samples = 1.0833 beats
    CHECK(block.startBeat == 6.5);
    transport.seek(2.0);
    block = transport.advance(512, 48000.0);
    CHECK(block.startBeat == 2.0);
    CHECK(transport.isPlaying());

    // Stop after a seek still rewinds to the start.
    transport.seek(10.0);
    transport.stop();
    transport.advance(512, 48000.0);
    CHECK(transport.positionInBeats() == 0.0);
}
