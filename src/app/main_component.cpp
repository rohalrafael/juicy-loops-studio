// See main_component.h. Keyboard shortcuts follow FL Studio where it has one (Space, L, F4-F9,
// Ctrl+M); they live here, so keyboard focus is steered back to this component after menus,
// dialogs and clicks (see mouseDown, menuBarActivated, timerCallback).

#include "main_component.h"

#include "io/audio_export.h"
#include "model/project_file.h"
#include "model/render.h"
#include "ui/theme_dialog.h"

#include <array>
#include <cmath>

namespace jls
{
namespace
{

enum MenuItem
{
    newItem = 1,
    openItem,
    saveItem,
    saveAsItem,
    exportItem,
    quitItem,
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
    pluginsItem,
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
    : themes(themeManager), pluginHost(themeManager.themesFolder().getParentDirectory()),
      document(model::makeDefaultProject()), playlist(document, engine.transport()),
      pianoRoll(document, engine.transport()), mixer(document), effectPanel(document),
      projectFolder(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)),
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
    tempoSlider.setTextValueSuffix(" BPM");
    tempoSlider.onValueChange = [this]
    {
        const double bpm = tempoSlider.getValue();
        if (std::abs(bpm - document.project().tempo) > 1.0e-6)
            document.apply([bpm](model::Project& p) { p.tempo = bpm; });
    };

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
    mixer.onOpenEffect = [this](model::EffectId id) { openEffectEditor(id); };
    mixer.listPlugins = [this]
    {
        std::vector<std::pair<std::string, std::string>> list;
        for (const auto& entry : pluginHost.effects())
            list.emplace_back(entry.id, entry.name);
        return list;
    };
    mixer.onManagePlugins = [this] { pluginHost.showManager(); };
    mixer.isEffectAvailable = [this](model::EffectId id) { return effectPool.find(id) != nullptr; };
    effectPool.setPluginFactory(
        [this](const std::string& id, const std::string& state, double rate, int block)
        { return pluginHost.createEffect(id, state, rate, block); });
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
    pluginWindows.clear(); // editors before their plugins
    shutdownAudio();
}

void MainComponent::documentChanged()
{
    const auto& project = document.project();
    engine.transport().setBeatsPerBar(project.beatsPerBar);
    engine.transport().setTempo(project.tempo);
    publishState();
    updateTitle();
    channelRack.refresh();
    playlist.refresh();
    pianoRoll.refresh();
    mixer.refresh();
    if (effectPanel.isVisible())
        effectPanel.refresh();
    // Close plugin windows whose effect was removed (or replaced by undo).
    for (auto it = pluginWindows.begin(); it != pluginWindows.end();)
    {
        const auto* slot = project.findEffect(it->first);
        if (slot == nullptr || effectPool.find(it->first) == nullptr)
            it = pluginWindows.erase(it);
        else
            ++it;
    }
    refreshTransportBar();
    menuItemsChanged();
}

void MainComponent::openEffectEditor(model::EffectId id)
{
    int track = 0;
    const auto* slot = document.project().findEffect(id, &track);
    if (slot == nullptr)
        return;

    if (!slot->isPlugin())
    {
        effectPanel.show(id);
        if (!effectPanelPlaced)
        {
            effectPanel.setCentrePosition(getLocalBounds().getCentre());
            effectPanelPlaced = true;
        }
        return;
    }

    if (auto it = pluginWindows.find(id); it != pluginWindows.end())
    {
        it->second->toFront(true);
        return;
    }
    auto processor = effectPool.find(id);
    if (processor == nullptr)
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions::makeOptionsOk(
                juce::MessageBoxIconType::WarningIcon, "Plugin not available",
                juce::String::fromUTF8(slot->pluginName.c_str()) +
                    " isn't installed or couldn't be loaded. Scan for plugins in Options > "
                    "Plugins. The effect is kept in the project and passes audio through."),
            nullptr);
        return;
    }
    const auto title =
        juce::String::fromUTF8(slot->pluginName.c_str()) + juce::String::fromUTF8(" \xc2\xb7 ") +
        juce::String::fromUTF8(
            document.project().mixer.tracks[static_cast<std::size_t>(track)].name.c_str());
    auto window = std::make_unique<plugins::PluginWindow>(processor, title);
    window->onClose = [this, id]
    { juce::MessageManager::callAsync([this, id] { pluginWindows.erase(id); }); };
    pluginWindows[id] = std::move(window);
}

void MainComponent::syncPluginStates()
{
    std::vector<std::pair<model::EffectId, std::string>> states;
    for (const auto& track : document.project().mixer.tracks)
        for (const auto& slot : track.effects)
            if (slot.isPlugin())
                if (auto processor = effectPool.find(slot.id))
                    if (auto state = processor->saveState(); state != slot.pluginState)
                        states.emplace_back(slot.id, std::move(state));
    if (states.empty())
        return;
    // Plugin settings live inside the plugin; copying them in isn't an undoable edit.
    document.setViewState(
        [&states](model::Project& p)
        {
            for (auto& [id, state] : states)
                if (auto* slot = p.findEffect(id))
                    slot->pluginState = std::move(state);
        });
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
    tempoSlider.setValue(project.tempo, juce::dontSendNotification);
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
    if (key == KeyPress('n', cmd, 0))
        return newProject(), true;
    if (key == KeyPress('o', cmd, 0))
        return openProject(), true;
    if (key == KeyPress('s', cmd, 0))
        return saveProject(), true;
    if (key == KeyPress('s', cmd | ModifierKeys::shiftModifier, 0))
        return saveProjectAs(), true;
    if (key == KeyPress('r', cmd, 0))
        return exportWav(), true;
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
    {
        auto withKey = [](Item item, const char* key)
        {
            item.shortcutKeyDescription = key;
            return item;
        };
        menu.addItem(withKey(Item("New").setID(newItem), "Ctrl+N"));
        menu.addItem(withKey(Item("Open...").setID(openItem), "Ctrl+O"));
        menu.addItem(withKey(Item("Save").setID(saveItem), "Ctrl+S"));
        menu.addItem(withKey(Item("Save as...").setID(saveAsItem), "Ctrl+Shift+S"));
        menu.addSeparator();
        menu.addItem(withKey(Item("Export WAV...").setID(exportItem), "Ctrl+R"));
        menu.addSeparator();
        menu.addItem(Item("Quit").setID(quitItem));
        break;
    }
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
        menu.addItem(Item("Plugins...").setID(pluginsItem));
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
    case newItem:
        newProject();
        break;
    case openItem:
        openProject();
        break;
    case saveItem:
        saveProject();
        break;
    case saveAsItem:
        saveProjectAs();
        break;
    case exportItem:
        exportWav();
        break;
    case quitItem:
        requestQuit();
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
    case pluginsItem:
        pluginHost.showManager();
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

// ----------------------------------------------------------------------------- File handling

void MainComponent::parentHierarchyChanged()
{
    updateTitle();
}

void MainComponent::updateTitle()
{
    auto* window = getTopLevelComponent();
    if (window == nullptr || window == this)
        return;
    const auto name = currentFile == juce::File() ? juce::String("Untitled")
                                                  : currentFile.getFileNameWithoutExtension();
    window->setName(name + (document.isModified() ? "*" : "") +
                    juce::String::fromUTF8(" \xe2\x80\x93 ") + "Juicy Loops Studio");
}

void MainComponent::confirmDiscard(std::function<void()> then)
{
    if (!document.isModified())
    {
        then();
        return;
    }
    const auto name =
        currentFile == juce::File() ? juce::String("Untitled") : currentFile.getFileName();
    juce::Component::SafePointer<MainComponent> self(this);
    juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsYesNoCancel(
                                     juce::MessageBoxIconType::NoIcon, "Unsaved changes",
                                     "Save the changes to " + name + " first?", "Save",
                                     "Don't save", "Cancel"),
                                 [self, then](int result)
                                 {
                                     if (self == nullptr)
                                         return;
                                     if (result == 1) // Save
                                         self->saveProject(
                                             [then](bool saved)
                                             {
                                                 if (saved)
                                                     then();
                                             });
                                     else if (result == 2) // Don't save
                                         then();
                                 });
}

void MainComponent::requestQuit()
{
    confirmDiscard([] { juce::JUCEApplication::getInstance()->quit(); });
}

void MainComponent::newProject()
{
    confirmDiscard(
        [this]
        {
            engine.transport().stop();
            currentFile = juce::File();
            document.reset(model::makeDefaultProject());
        });
}

void MainComponent::openProject()
{
    confirmDiscard(
        [this]
        {
            fileChooser =
                std::make_unique<juce::FileChooser>("Open project", projectFolder, "*.jls");
            fileChooser->launchAsync(juce::FileBrowserComponent::openMode |
                                         juce::FileBrowserComponent::canSelectFiles,
                                     [this](const juce::FileChooser& fc)
                                     {
                                         if (fc.getResult() != juce::File())
                                             openFile(fc.getResult());
                                     });
        });
}

void MainComponent::openFile(const juce::File& file)
{
    const auto result = model::loadProject(
        file.loadFileAsString().toStdString(), file.getFullPathName().toStdString(),
        [this](const std::string& path) { return sampleLoader.load(juce::File(path)); });

    if (!result.project)
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions::makeOptionsOk(juce::MessageBoxIconType::WarningIcon,
                                                   "Couldn't open " + file.getFileName(),
                                                   juce::String::fromUTF8(result.error.c_str())),
            nullptr);
        return;
    }

    engine.transport().stop();
    currentFile = file;
    projectFolder = file.getParentDirectory();
    document.reset(*result.project);

    if (!result.warnings.empty())
    {
        juce::String message;
        for (std::size_t i = 0; i < result.warnings.size() && i < 10; ++i)
            message << "- " << juce::String::fromUTF8(result.warnings[i].c_str()) << "\n";
        if (result.warnings.size() > 10)
            message << "(and " << static_cast<int>(result.warnings.size() - 10) << " more)";
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions::makeOptionsOk(juce::MessageBoxIconType::WarningIcon,
                                                   "Opened with problems", message),
            nullptr);
    }
}

bool MainComponent::writeProject(const juce::File& file)
{
    syncPluginStates();
    const auto text = model::saveProject(document.project(), file.getFullPathName().toStdString());
    // replaceWithText writes a temporary file first, so a failed save can't corrupt the old one.
    if (!file.replaceWithText(juce::String::fromUTF8(text.c_str())))
    {
        juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOk(
                                         juce::MessageBoxIconType::WarningIcon, "Couldn't save",
                                         "Writing " + file.getFullPathName() + " failed."),
                                     nullptr);
        return false;
    }
    currentFile = file;
    projectFolder = file.getParentDirectory();
    document.markSaved();
    return true;
}

void MainComponent::saveProject(std::function<void(bool)> done)
{
    if (currentFile == juce::File())
    {
        saveProjectAs(std::move(done));
        return;
    }
    const bool saved = writeProject(currentFile);
    if (done)
        done(saved);
}

void MainComponent::saveProjectAs(std::function<void(bool)> done)
{
    const auto suggested =
        currentFile != juce::File() ? currentFile : projectFolder.getChildFile("Untitled.jls");
    fileChooser = std::make_unique<juce::FileChooser>("Save project", suggested, "*.jls");
    fileChooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
            juce::FileBrowserComponent::warnAboutOverwriting,
        [this, done](const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            const bool saved = file != juce::File() &&
                               writeProject(file.withFileExtension(model::projectFileExtension));
            if (done)
                done(saved);
        });
}

void MainComponent::exportWav()
{
    const auto base = currentFile != juce::File()
                          ? currentFile.withFileExtension(".wav")
                          : juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                                .getChildFile("Untitled.wav");
    fileChooser = std::make_unique<juce::FileChooser>("Export WAV", base, "*.wav");
    fileChooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
            juce::FileBrowserComponent::warnAboutOverwriting,
        [this](const juce::FileChooser& fc)
        {
            if (fc.getResult() == juce::File())
                return;
            const auto file = fc.getResult().withFileExtension(".wav");
            const double rate =
                engine.preparedSampleRate() > 0.0 ? engine.preparedSampleRate() : 44100.0;

            // Rendering runs much faster than real time, but blocks the UI while it does.
            juce::MouseCursor::showWaitCursor();
            syncPluginStates();
            const auto audio = model::renderProject(
                document.project(), rate, 2.0,
                [this](const std::string& id, const std::string& state, double r, int block)
                { return pluginHost.createEffect(id, state, r, block); });
            const auto result = io::writeWav(file, audio);
            juce::MouseCursor::hideWaitCursor();

            const int seconds = static_cast<int>(static_cast<double>(audio.left.size()) / rate);
            const auto length =
                juce::String(seconds / 60) + ":" + juce::String(seconds % 60).paddedLeft('0', 2);
            juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOk(
                                             result.wasOk() ? juce::MessageBoxIconType::InfoIcon
                                                            : juce::MessageBoxIconType::WarningIcon,
                                             result.wasOk() ? "Export finished" : "Export failed",
                                             result.wasOk()
                                                 ? file.getFileName() + " (" + length + ")"
                                                 : result.getErrorMessage()),
                                         nullptr);
        });
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
    // JUCE dialogs default to light grey; follow the theme instead.
    options.dialogBackgroundColour = juce::LookAndFeel::getDefaultLookAndFeel().findColour(
        juce::ResizableWindow::backgroundColourId);
    options.content.setOwned(selector.release());
    options.dialogTitle = "Audio settings";
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

} // namespace jls
