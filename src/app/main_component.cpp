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
    newPatternItem,
    clonePatternItem,
    renamePatternItem,
    deletePatternItem,
    playlistItem,
    pianoRollItem,
    channelRackItem,
    mixerItem,
    themeItem,
    audioSettingsItem,
    aboutItem,
};

constexpr int menuBarHeight = 24;
constexpr int transportHeight = 40;
constexpr int margin = 12;

// Keep keyboard focus on the main component so shortcuts always work.
void noFocus(juce::Component& c)
{
    c.setWantsKeyboardFocus(false);
    c.setMouseClickGrabsKeyboardFocus(false);
}

} // namespace

MainComponent::MainComponent(ui::ThemeManager& themeManager)
    : themes(themeManager), document(model::makeDefaultProject()),
      playlist(document, engine.transport()), pianoRoll(document, engine.transport()),
      mixer(document), effectPanel(document),
      channelRack(document, engine.transport(), sampleLoader)
{
    auto& transport = engine.transport();

    patButton.setTooltip("Pattern mode: loop the selected pattern (L)");
    songButton.setTooltip("Song mode: play the Playlist (L)");
    patButton.onClick = [this] { setSongMode(false); };
    songButton.onClick = [this] { setSongMode(true); };

    patternBox.setTooltip("Pattern being edited and placed");
    patternBox.onChange = [this]
    {
        const int index = patternBox.getSelectedItemIndex();
        const auto& patterns = document.project().patterns;
        if (index >= 0 && index < static_cast<int>(patterns.size()))
            selectPattern(patterns[static_cast<std::size_t>(index)].id);
    };
    newPatternButton.setTooltip("New pattern (F4)");
    newPatternButton.onClick = [this] { newPattern(); };

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
    timeSignatureBox.onChange = [this]
    {
        const int beats = timeSignatureBox.getSelectedId();
        if (beats > 0 && beats != document.project().beatsPerBar)
            document.apply([beats](model::Project& p) { p.beatsPerBar = beats; });
    };

    metronomeButton.setClickingTogglesState(true);
    metronomeButton.setTooltip("Metronome (Ctrl+M)");
    metronomeButton.onClick = [this]
    { engine.metronome().setEnabled(metronomeButton.getToggleState()); };

    positionLabel.setJustificationType(juce::Justification::centred);
    positionLabel.setFont(juce::FontOptions(20.0f));

    for (auto* c : std::initializer_list<juce::Component*>{
             &patButton, &songButton, &patternBox, &newPatternButton, &playButton, &stopButton,
             &timeSignatureBox, &metronomeButton})
        noFocus(*c);

    for (auto* c : std::initializer_list<juce::Component*>{
             &patButton, &songButton, &patternBox, &newPatternButton, &playButton, &stopButton,
             &tempoSlider, &timeSignatureBox, &metronomeButton, &positionLabel, &playlist})
        addAndMakeVisible(c);
    addChildComponent(pianoRoll);
    addChildComponent(mixer);
    addAndMakeVisible(channelRack); // after the main views, so the floating panels are on top
    addChildComponent(effectPanel);
    mixer.onOpenEffect = [this](model::EffectId id)
    {
        effectPanel.show(id);
        if (!effectPanelPlaced)
        {
            effectPanel.setCentrePosition(getLocalBounds().getCentre());
            effectPanelPlaced = true;
        }
    };
    channelRack.onOpenPianoRoll = [this](model::ChannelId) { showPianoRoll(); };

#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(this);
#else
    menuBar = std::make_unique<juce::MenuBarComponent>(this);
    addAndMakeVisible(*menuBar);
#endif

    document.onChange = [this] { documentChanged(); };
    documentChanged();

    setWantsKeyboardFocus(true);
    addMouseListener(this, true); // see mouseDown(): clicks anywhere restore shortcut focus
    setSize(1280, 800);
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
    const auto& project = document.project();
    engine.transport().setBeatsPerBar(project.beatsPerBar);
    publishState();
    channelRack.refresh();
    playlist.refresh();
    pianoRoll.refresh();
    mixer.refresh();
    if (effectPanel.isVisible())
        effectPanel.refresh();
    refreshTransportBar();
    menuItemsChanged();
}

void MainComponent::publishState()
{
    engine.setState(model::toEngineState(document.project(), &effectPool));
}

void MainComponent::refreshTransportBar()
{
    const auto& project = document.project();

    patButton.setToggleState(!project.songMode, juce::dontSendNotification);
    songButton.setToggleState(project.songMode, juce::dontSendNotification);

    patternBox.clear(juce::dontSendNotification);
    for (std::size_t i = 0; i < project.patterns.size(); ++i)
    {
        patternBox.addItem(juce::String::fromUTF8(project.patterns[i].name.c_str()),
                           static_cast<int>(i) + 1);
        if (project.patterns[i].id == project.current().id)
            patternBox.setSelectedId(static_cast<int>(i) + 1, juce::dontSendNotification);
    }

    timeSignatureBox.setSelectedId(project.beatsPerBar, juce::dontSendNotification);
}

void MainComponent::setSongMode(bool song)
{
    if (document.project().songMode != song)
        document.setViewState([song](model::Project& p) { p.songMode = song; });
}

void MainComponent::selectPattern(model::PatternId id)
{
    if (document.project().currentPattern != id)
        document.setViewState([id](model::Project& p) { p.currentPattern = id; });
}

void MainComponent::newPattern()
{
    model::PatternId added = 0;
    document.apply([&added](model::Project& p) { added = p.addPattern(); });
    selectPattern(added);
}

void MainComponent::clonePattern()
{
    model::PatternId copy = 0;
    const auto source = document.project().current().id;
    document.apply([&copy, source](model::Project& p) { copy = p.clonePattern(source); });
    selectPattern(copy);
}

void MainComponent::deletePattern()
{
    if (document.project().patterns.size() <= 1)
        return;
    const auto id = document.project().current().id;
    document.apply([id](model::Project& p) { p.removePattern(id); });
}

void MainComponent::renamePattern()
{
    const auto id = document.project().current().id;
    auto* window = new juce::AlertWindow("Rename pattern", {}, juce::MessageBoxIconType::NoIcon);
    window->addTextEditor("name",
                          juce::String::fromUTF8(document.project().current().name.c_str()));
    window->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    window->enterModalState(true,
                            juce::ModalCallbackFunction::create(
                                [this, id, window](int result)
                                {
                                    const auto name = window->getTextEditorContents("name").trim();
                                    if (result == 1 && name.isNotEmpty())
                                        document.apply(
                                            [id, name](model::Project& p)
                                            {
                                                if (auto* pattern = p.findPattern(id))
                                                    pattern->name = name.toStdString();
                                            });
                                }),
                            true);
}

void MainComponent::toggleChannelRack()
{
    channelRack.setVisible(!channelRack.isVisible());
    if (channelRack.isVisible())
        channelRack.toFront(false);
    menuItemsChanged();
}

void MainComponent::showPlaylist()
{
    pianoRoll.setVisible(false);
    playlist.setVisible(true);
    menuItemsChanged();
}

void MainComponent::showPianoRoll()
{
    playlist.setVisible(false);
    pianoRoll.setVisible(true);
    pianoRoll.refresh();
    menuItemsChanged();
}

void MainComponent::toggleMixer()
{
    mixer.setVisible(!mixer.isVisible());
    resized();
    menuItemsChanged();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    engine.prepare(sampleRate, samplesPerBlockExpected);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    juce::ScopedNoDenormals noDenormals; // effects' feedback paths must never go denormal
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

    auto area = bounds.reduced(margin);
    auto bar = area.removeFromTop(transportHeight);

    // Fits from ~890 px wide; below that the least important controls drop out.
    patButton.setBounds(bar.removeFromLeft(50));
    songButton.setBounds(bar.removeFromLeft(58));
    bar.removeFromLeft(6);
    patternBox.setBounds(bar.removeFromLeft(130).reduced(0, 4));
    newPatternButton.setBounds(bar.removeFromLeft(32).reduced(2, 4));
    bar.removeFromLeft(10);
    playButton.setBounds(bar.removeFromLeft(64));
    bar.removeFromLeft(4);
    stopButton.setBounds(bar.removeFromLeft(64));
    bar.removeFromLeft(10);
    tempoSlider.setBounds(bar.removeFromLeft(170));
    bar.removeFromLeft(6);
    positionLabel.setBounds(bar.removeFromLeft(92));
    bar.removeFromLeft(6);
    const bool roomForRest = bar.getWidth() >= 64 + 4 + 96;
    timeSignatureBox.setVisible(roomForRest);
    metronomeButton.setVisible(roomForRest);
    timeSignatureBox.setBounds(bar.removeFromLeft(64).reduced(0, 4));
    bar.removeFromLeft(4);
    metronomeButton.setBounds(bar.removeFromLeft(96));

    area.removeFromTop(margin);
    if (mixer.isVisible())
    {
        const int height = std::min(ui::MixerView::preferredHeight, area.getHeight() / 2);
        mixer.setBounds(area.removeFromBottom(height));
        area.removeFromBottom(margin / 2);
    }
    playlist.setBounds(area);
    pianoRoll.setBounds(area);

    // The rack floats: place it once, then keep wherever the user dragged it (sizes follow
    // the channel count).
    const int width = channelRack.preferredWidth();
    const int height = channelRack.preferredHeight();
    if (!channelRackPlaced && !area.isEmpty())
    {
        // Below the first tracks, so the Playlist's "click to place" hint stays visible.
        channelRack.setBounds(area.getX() + 140,
                              area.getY() + ui::PlaylistView::headerHeight +
                                  ui::PlaylistView::rulerHeight + 3 * ui::PlaylistView::trackHeight,
                              width, height);
        channelRackPlaced = true;
    }
    else
    {
        channelRack.setSize(width, height);
    }
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    using juce::KeyPress;
    using juce::ModifierKeys;
    const auto cmd = ModifierKeys::commandModifier;

    // Shortcuts follow FL Studio where it has one.
    if (key == KeyPress::spaceKey)
        return engine.transport().togglePlay(), true;
    if (key == KeyPress('m', cmd, 0))
        return engine.metronome().toggleEnabled(), true;
    if (key == KeyPress('l'))
        return setSongMode(!document.project().songMode), true;
    if (key == KeyPress(KeyPress::F4Key))
        return newPattern(), true;
    if (key == KeyPress(KeyPress::F5Key))
        return showPlaylist(), true;
    if (key == KeyPress(KeyPress::F6Key))
        return toggleChannelRack(), true;
    if (key == KeyPress(KeyPress::F7Key))
        return showPianoRoll(), true;
    if (key == KeyPress(KeyPress::F9Key))
        return toggleMixer(), true;
    if (key == KeyPress('z', cmd, 0))
        return document.undo(), true;
    if (key == KeyPress('y', cmd, 0) || key == KeyPress('z', cmd | ModifierKeys::shiftModifier, 0))
        return document.redo(), true;
    return false;
}

juce::StringArray MainComponent::getMenuBarNames()
{
    return {"File", "Edit", "Patterns", "View", "Options", "Help"};
}

juce::PopupMenu MainComponent::getMenuForIndex(int index, const juce::String&)
{
    using Item = juce::PopupMenu::Item;
    juce::PopupMenu menu;
    switch (index)
    {
    case 0:
        menu.addItem(Item("Quit").setID(quitItem));
        break;
    case 1:
        menu.addItem(Item("Undo").setID(undoItem).setEnabled(document.canUndo()));
        menu.addItem(Item("Redo").setID(redoItem).setEnabled(document.canRedo()));
        break;
    case 2:
    {
        menu.addItem(Item("New pattern").setID(newPatternItem));
        menu.addItem(Item("Clone pattern").setID(clonePatternItem));
        menu.addItem(Item("Rename pattern...").setID(renamePatternItem));
        menu.addItem(Item("Delete pattern")
                         .setID(deletePatternItem)
                         .setEnabled(document.project().patterns.size() > 1));
        break;
    }
    case 3:
    {
        auto withKey = [](Item item, const char* key)
        {
            item.shortcutKeyDescription = key;
            return item;
        };
        menu.addItem(
            withKey(Item("Playlist").setID(playlistItem).setTicked(playlist.isVisible()), "F5"));
        menu.addItem(withKey(
            Item("Channel rack").setID(channelRackItem).setTicked(channelRack.isVisible()), "F6"));
        menu.addItem(withKey(
            Item("Piano roll").setID(pianoRollItem).setTicked(pianoRoll.isVisible()), "F7"));
        menu.addItem(withKey(Item("Mixer").setID(mixerItem).setTicked(mixer.isVisible()), "F9"));
        break;
    }
    case 4:
        menu.addItem(Item("Theme...").setID(themeItem));
        menu.addItem(Item("Audio settings...").setID(audioSettingsItem));
        break;
    case 5:
        menu.addItem(Item("About Juicy Loops Studio").setID(aboutItem));
        break;
    default:
        break;
    }
    return menu;
}

void MainComponent::mouseDown(const juce::MouseEvent&)
{
    // Keyboard shortcuts are handled here, but focus can end up nowhere (e.g. after a menu or
    // dialog closes). Any click takes it back, unless the click went into a text field.
    if (dynamic_cast<juce::TextEditor*>(getCurrentlyFocusedComponent()) == nullptr)
        grabKeyboardFocus();
}

void MainComponent::menuBarActivated(bool isActive)
{
    // The menu held keyboard focus; hand it back once it has closed (by selection or Escape).
    if (isActive)
        return;
    juce::Component::SafePointer<MainComponent> self(this);
    juce::MessageManager::callAsync(
        [self]
        {
            if (self != nullptr && self->isShowing())
                self->grabKeyboardFocus();
        });
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
    case newPatternItem:
        newPattern();
        break;
    case clonePatternItem:
        clonePattern();
        break;
    case renamePatternItem:
        renamePattern();
        break;
    case deletePatternItem:
        deletePattern();
        break;
    case playlistItem:
        showPlaylist();
        break;
    case pianoRollItem:
        showPianoRoll();
        break;
    case channelRackItem:
        toggleChannelRack();
        break;
    case mixerItem:
        toggleMixer();
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

    // The audio device (re)started with a new format: rebuild effect processors for it.
    const double rate = engine.preparedSampleRate();
    const int block = engine.preparedBlockSize();
    if (rate > 0.0 && (std::abs(rate - formatRate) > 0.5 || block != formatBlock))
    {
        formatRate = rate;
        formatBlock = block;
        effectPool.setFormat(rate, block);
        publishState();
    }

    if (mixer.isVisible())
        mixer.updateMeters([this](int track, int side) { return engine.takePeak(track, side); });

    // Safety net for shortcuts: if focus ended up nowhere (e.g. a dialog or popup closed)
    // while our window is the active one, take it back. Never steals focus from other apps.
    if (getCurrentlyFocusedComponent() == nullptr && isShowing())
        if (auto* peer = getPeer(); peer != nullptr && peer->isFocused())
            grabKeyboardFocus();

    // Position within what's looping: the pattern in PAT mode, the song in SONG mode.
    const auto& project = document.project();
    const auto& transport = engine.transport();
    const int loopSteps =
        project.songMode ? project.songLengthInSteps() : project.lengthInSteps(project.current());
    double steps = transport.positionInBeats() * model::stepsPerBeat;
    if (loopSteps > 0)
        steps = std::fmod(steps, static_cast<double>(loopSteps));
    const int stepsPerBar = project.stepsPerBar();
    const int bar = static_cast<int>(steps) / stepsPerBar + 1;
    const int beat = static_cast<int>(steps) % stepsPerBar / model::stepsPerBeat + 1;
    const int step = static_cast<int>(steps) % model::stepsPerBeat + 1;
    positionLabel.setText(juce::String(bar) + ":" + juce::String(beat) + ":" + juce::String(step),
                          juce::dontSendNotification);

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
