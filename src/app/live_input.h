#pragma once

// Live playing: the computer keyboard as a piano (FL's "typing keyboard to piano") and MIDI
// input devices. Both play the selected channel through Engine::sendLive().

#include "engine/engine.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <set>

namespace jls
{

class LiveInput final : private juce::MidiInputCallback
{
  public:
    LiveInput(engine::Engine& engine, juce::AudioDeviceManager& devices);
    ~LiveInput() override;

    // Channel that live notes play (the selected channel); 0 = none. Any thread.
    void setTargetChannel(std::uint32_t id) { target.store(id); }

    // Typing keyboard: Z S X D C V G B H N J M play C5..B5, Q 2 W 3 E R 5 T 6 Y 7 U I 9 O 0 P
    // play C6..E7 (FL's layout, minus keys used for shortcuts). UI thread.
    void setTypingEnabled(bool enabled);
    bool isTypingEnabled() const { return typing; }
    // True for keys that play notes, so the owner's keyPressed() can swallow them.
    bool isNoteKey(const juce::KeyPress& key) const;
    // Call from keyStateChanged(): starts notes for keys that went down, ends released ones.
    // Returns true if a note key changed.
    bool updateKeys();
    // Ends all typing-keyboard notes (call when keyboard focus is lost).
    void releaseKeys();
    bool anyKeyDown() const;

  private:
    struct Key
    {
        char character;
        int pitch;
    };
    static const std::array<Key, 29>& keys();

    void enableNewMidiDevices();
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message) override;

    engine::Engine& engine;
    juce::AudioDeviceManager& devices;
    std::atomic<std::uint32_t> target{0};
    bool typing = true;
    std::array<int, 29> keyPitch{}; // pitch a held key started (0 = up)
    std::set<juce::String> seenMidiDevices;
    juce::MidiDeviceListConnection deviceListConnection;
};

} // namespace jls
