#include "main_component.h"

#include "ui/theme_dialog.h"

#include <array>
#include <cmath>

namespace jls
{
namespace
{

enum MenuItem
{
    quitItem = 1,
    undoItem,
    redoItem,
    themeItem,
    audioSettingsItem,
    aboutItem,
};

constexpr int menuBarHeight = 24;

} // namespace

MainComponent::MainComponent(ui::ThemeManager& themeManager)
    : themes(themeManager), document(model::makeDefaultProject()),
      channelRack(document, engine.transport(), sampleLoader)
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

#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(this);
#else
    menuBar = std::make_unique<juce::MenuBarComponent>(this);
    addAndMakeVisible(*menuBar);
#endif

    document.onChange = [this] { documentChanged(); };
    engine.setState(model::toEngineState(document.project()));

    setWantsKeyboardFocus(true);
    setSize(std::max(960, channelRack.preferredWidth() + 24),
            (menuBar ? menuBarHeight : 0) + 12 + 40 + 12 + channelRack.preferredHeight() + 12);
    setAudioChannels(0, 2);
    startTimerHz(30);
}

MainComponent::~MainComponent()
{
#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(nullptr);
#endif
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
    auto bounds = getLocalBounds();
    if (menuBar)
        menuBar->setBounds(bounds.removeFromTop(menuBarHeight));

    auto area = bounds.reduced(12);
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

juce::StringArray MainComponent::getMenuBarNames()
{
    return {"File", "Edit", "Options", "Help"};
}

juce::PopupMenu MainComponent::getMenuForIndex(int index, const juce::String&)
{
    juce::PopupMenu menu;
    switch (index)
    {
    case 0:
        menu.addItem(juce::PopupMenu::Item("Quit").setID(quitItem));
        break;
    case 1:
        menu.addItem(juce::PopupMenu::Item("Undo").setID(undoItem).setEnabled(document.canUndo()));
        menu.addItem(juce::PopupMenu::Item("Redo").setID(redoItem).setEnabled(document.canRedo()));
        break;
    case 2:
        menu.addItem(juce::PopupMenu::Item("Theme...").setID(themeItem));
        menu.addItem(juce::PopupMenu::Item("Audio settings...").setID(audioSettingsItem));
        break;
    case 3:
        menu.addItem(juce::PopupMenu::Item("About Juicy Loops Studio").setID(aboutItem));
        break;
    default:
        break;
    }
    return menu;
}

void MainComponent::menuItemSelected(int itemId, int)
{
    switch (itemId)
    {
    case quitItem:
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
        break;
    case undoItem:
        document.undo();
        break;
    case redoItem:
        document.redo();
        break;
    case themeItem:
        ui::ThemeDialog::show(themes);
        break;
    case audioSettingsItem:
        showAudioSettings();
        break;
    case aboutItem:
        showAbout();
        break;
    default:
        break;
    }
}

void MainComponent::showAbout()
{
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions::makeOptionsOk(
            juce::MessageBoxIconType::NoIcon, "Juicy Loops Studio",
            "Version " + juce::JUCEApplication::getInstance()->getApplicationVersion() +
                "\n\nA free, open-source DAW. Licensed under the GNU AGPLv3.\n\n"
                "Bundled fonts (IBM Plex Sans, Bricolage Grotesque, Sora, Outfit) are under the "
                "SIL Open Font License.\n\nNot affiliated with Image-Line."),
        nullptr);
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
