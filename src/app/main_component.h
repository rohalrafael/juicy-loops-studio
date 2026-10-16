#pragma once

#include "engine/metronome.h"
#include "engine/transport.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace jls
{

// Temporary top-level component: transport bar only, until the real
// Channel Rack / Playlist / Mixer views exist.
class MainComponent final : public juce::AudioAppComponent, private juce::Timer
{
  public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

  private:
    void timerCallback() override;
    void showAudioSettings();

    engine::Transport transport;
    engine::Metronome metronome;
    double currentSampleRate = 0.0;

    juce::TextButton playButton{"Play"};
    juce::TextButton stopButton{"Stop"};
    juce::Slider tempoSlider;
    juce::ComboBox timeSignatureBox;
    juce::TextButton metronomeButton{"Metronome"};
    juce::Label positionLabel;
    juce::TextButton audioSettingsButton{"Audio settings"};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace jls
