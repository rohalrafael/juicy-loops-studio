#include "main_component.h"

#include <cmath>

namespace jls
{

MainComponent::MainComponent()
{
    playButton.onClick = [this] { transport.togglePause(); };
    stopButton.onClick = [this] { transport.stop(); };

    tempoSlider.setSliderStyle(juce::Slider::IncDecButtons);
    tempoSlider.setRange(engine::Transport::minTempo, engine::Transport::maxTempo, 0.001);
    tempoSlider.setNumDecimalPlacesToDisplay(3);
    tempoSlider.setValue(transport.tempo(), juce::dontSendNotification);
    tempoSlider.setTextValueSuffix(" BPM");
    tempoSlider.onValueChange = [this] { transport.setTempo(tempoSlider.getValue()); };

    positionLabel.setJustificationType(juce::Justification::centred);
    positionLabel.setFont(juce::FontOptions(20.0f));

    audioSettingsButton.onClick = [this] { showAudioSettings(); };

    for (auto* c : std::initializer_list<juce::Component*>{
             &playButton, &stopButton, &tempoSlider, &positionLabel, &audioSettingsButton})
        addAndMakeVisible(c);

    setWantsKeyboardFocus(true);
    setSize(800, 120);
    setAudioChannels(0, 2);
    startTimerHz(30);
}

MainComponent::~MainComponent() { shutdownAudio(); }

void MainComponent::prepareToPlay(int, double sampleRate) { currentSampleRate = sampleRate; }

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();
    transport.advance(bufferToFill.numSamples, currentSampleRate);
}

void MainComponent::releaseResources() {}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto bar = area.removeFromTop(40);

    playButton.setBounds(bar.removeFromLeft(80));
    bar.removeFromLeft(6);
    stopButton.setBounds(bar.removeFromLeft(80));
    bar.removeFromLeft(12);
    tempoSlider.setBounds(bar.removeFromLeft(180));
    bar.removeFromLeft(12);
    audioSettingsButton.setBounds(bar.removeFromRight(130));
    positionLabel.setBounds(bar);
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    // Space toggles play/stop, as in FL Studio.
    if (key == juce::KeyPress::spaceKey)
    {
        transport.togglePlay();
        return true;
    }
    return false;
}

void MainComponent::timerCallback()
{
    // Bar:beat display, 4/4 for now.
    const double beats = transport.positionInBeats();
    const int bar = static_cast<int>(std::floor(beats / 4.0)) + 1;
    const int beat = static_cast<int>(std::floor(beats)) % 4 + 1;
    positionLabel.setText(juce::String(bar) + ":" + juce::String(beat),
                          juce::dontSendNotification);
    playButton.setButtonText(transport.isPlaying() ? "Pause" : "Play");
}

void MainComponent::showAudioSettings()
{
    auto selector = std::make_unique<juce::AudioDeviceSelectorComponent>(
        deviceManager, 0, 2, 0, 2, true, true, true, false);
    selector->setSize(500, 450);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(selector.release());
    options.dialogTitle = "Audio settings";
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

} // namespace jls
