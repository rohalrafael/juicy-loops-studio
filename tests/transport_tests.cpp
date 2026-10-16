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
