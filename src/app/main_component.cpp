#include "main_component.h"

#include <array>
#include <cmath>

namespace jls
{

MainComponent::MainComponent()
    : document(model::makeDefaultProject()), channelRack(document, engine.transport(), sampleLoader)
{
    auto& transport = engine.transport();

    playButton.onClick = [&transport] { transport.togglePause(); };
    stopButton.onClick = [&transport] { transport.stop(); };

    tempoSlider.setSliderStyle(juce::Slider::IncDecButtons);
    tempoSlider.setRange(engine::Transport::minTempo, engine::Transport::maxTempo, 0.001);
    tempoSlider.setNumDecimalPlacesToDisplay(3);
    tempoSlider.setValue(transport.tempo(), juce::dontSendNotification);
    tempoSlider.setTextValueSuffix(" BPM");
    tempoSlider.onValueChange = [this, &transport] { transport.setTempo(tempoSlider.getValue()); };

    for (int beats = engine::Transport::minBeatsPerBar; beats <= engine::Transport::maxBeatsPerBar;
         ++beats)
        timeSignatureBox.addItem(juce::String(beats) + "/4", beats);
    timeSignatureBox.setSelectedId(transport.beatsPerBar(), juce::dontSendNotification);
    timeSignatureBox.onChange = [this, &transport]
    { transport.setBeatsPerBar(timeSignatureBox.getSelectedId()); };

    metronomeButton.setClickingTogglesState(true);
    metronomeButton.setTooltip("Metronome (Ctrl+M)");
    metronomeButton.onClick = [this]
    { engine.metronome().setEnabled(metronomeButton.getToggleState()); };

    positionLabel.setJustificationType(juce::Justification::centred);
    positionLabel.setFont(juce::FontOptions(20.0f));

    audioSettingsButton.onClick = [this] { showAudioSettings(); };

    // Keep keyboard focus here so shortcuts (Space, Ctrl+M, Ctrl+Z) always work.
    for (auto* button : {&playButton, &stopButton, &metronomeButton, &audioSettingsButton})
    {
        button->setWantsKeyboardFocus(false);
        button->setMouseClickGrabsKeyboardFocus(false);
    }
    timeSignatureBox.setWantsKeyboardFocus(false);

    for (auto* c : std::initializer_list<juce::Component*>{
             &playButton, &stopButton, &tempoSlider, &timeSignatureBox, &metronomeButton,
             &positionLabel, &audioSettingsButton, &channelRack})
        addAndMakeVisible(c);

    document.onChange = [this] { documentChanged(); };
    engine.setState(model::toEngineState(document.project()));

    setWantsKeyboardFocus(true);
    setSize(std::max(960, channelRack.preferredWidth() + 24),
            12 + 40 + 12 + channelRack.preferredHeight() + 12);
    setAudioChannels(0, 2);
    startTimerHz(30);
}

MainComponent::~MainComponent()
{
    document.onChange = nullptr;
    shutdownAudio();
}

void MainComponent::documentChanged()
{
    channelRack.refresh();
    engine.setState(model::toEngineState(document.project()));
}

void MainComponent::prepareToPlay(int, double sampleRate)
{
    engine.prepare(sampleRate);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    // Channel pointers offset to the active region; fixed-size so nothing allocates.
    std::array<float*, 32> channels{};
    const int numChannels =
        std::min(bufferToFill.buffer->getNumChannels(), static_cast<int>(channels.size()));
    for (int ch = 0; ch < numChannels; ++ch)
        channels[static_cast<size_t>(ch)] =
            bufferToFill.buffer->getWritePointer(ch, bufferToFill.startSample);

    engine.process(channels.data(), numChannels, bufferToFill.numSamples);
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
    timeSignatureBox.setBounds(bar.removeFromLeft(70));
    bar.removeFromLeft(6);
    metronomeButton.setBounds(bar.removeFromLeft(100));
    bar.removeFromLeft(12);
    audioSettingsButton.setBounds(bar.removeFromRight(130));
    positionLabel.setBounds(bar);

    area.removeFromTop(12);
    channelRack.setBounds(area.removeFromTop(channelRack.preferredHeight())
                              .withWidth(std::min(area.getWidth(), channelRack.preferredWidth())));
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    using juce::KeyPress;
    using juce::ModifierKeys;
    const auto cmd = ModifierKeys::commandModifier;

    // Space toggles play/stop, as in FL Studio.
    if (key == KeyPress::spaceKey)
    {
        engine.transport().togglePlay();
        return true;
    }
    // Ctrl+M (Cmd+M on macOS) toggles the metronome, as in FL Studio.
    if (key == KeyPress('m', cmd, 0))
    {
        engine.metronome().toggleEnabled();
        return true;
    }
    if (key == KeyPress('z', cmd, 0))
        return document.undo(), true;
    if (key == KeyPress('y', cmd, 0) || key == KeyPress('z', cmd | ModifierKeys::shiftModifier, 0))
        return document.redo(), true;
    return false;
}

void MainComponent::timerCallback()
{
    engine.collectGarbage();

    const auto& transport = engine.transport();
    const double beats = transport.positionInBeats();
    const int beatsPerBar = transport.beatsPerBar();
    const int bar = static_cast<int>(std::floor(beats / beatsPerBar)) + 1;
    const int beat = static_cast<int>(std::floor(beats)) % beatsPerBar + 1;
    positionLabel.setText(juce::String(bar) + ":" + juce::String(beat), juce::dontSendNotification);
    playButton.setButtonText(transport.isPlaying() ? "Pause" : "Play");
    metronomeButton.setToggleState(engine.metronome().isEnabled(), juce::dontSendNotification);
}

void MainComponent::showAudioSettings()
{
    auto selector = std::make_unique<juce::AudioDeviceSelectorComponent>(deviceManager, 0, 2, 0, 2,
                                                                         true, true, true, false);
    selector->setSize(500, 450);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(selector.release());
    options.dialogTitle = "Audio settings";
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

} // namespace jls
