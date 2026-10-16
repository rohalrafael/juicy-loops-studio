#pragma once

#include "engine/engine.h"
#include "io/sample_loader.h"
#include "model/document.h"
#include "ui/channel_rack.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace jls
{

// Top-level component: transport bar + Channel Rack. Owns the document and the audio engine.
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
    void documentChanged();
    void showAudioSettings();

    engine::Engine engine;
    model::Document document;
    io::SampleLoader sampleLoader;

    juce::TextButton playButton{"Play"};
    juce::TextButton stopButton{"Stop"};
    juce::Slider tempoSlider;
    juce::ComboBox timeSignatureBox;
    juce::TextButton metronomeButton{"Metronome"};
    juce::Label positionLabel;
    juce::TextButton audioSettingsButton{"Audio settings"};
    ui::ChannelRack channelRack;
    juce::TooltipWindow tooltips{this, 600};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace jls
