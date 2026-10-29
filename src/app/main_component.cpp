// See main_component.h. Keyboard shortcuts follow FL Studio where it has one (Space, L, F4-F9,
// Ctrl+M); they live here, so keyboard focus is steered back to this component after menus,
// dialogs and clicks (see mouseDown, menuBarActivated, timerCallback).

#include "main_component.h"
#include "ui/title_bar.h"

#include "app/commands.h"
#include "app/native_windows.h"
#include "ui/settings_dialog.h"

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

constexpr int transportHeight = ui::metrics::transportControlHeight + 16; // the bar around them
// Docked panels sit edge to edge, this far apart (the background shows as a seam), as in FL.
constexpr int panelGap = 2;

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
      pianoRoll(document, engine.transport()), mixer(document),
      browser(engine.preview(), themeManager.settingsFile()), effectPanel(document),
      projectFolder(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)),
      channelRack(document, engine.transport(), sampleLoader)
{
    auto& transport = engine.transport();

    patButton.setTooltip("Pattern mode: loop the selected pattern");
    songButton.setTooltip("Song mode: play the Playlist");
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
    newPatternButton.setTooltip("New pattern");
    newPatternButton.onClick = [this] { newPattern(); };

    playButton.setTooltip("Play / pause");
    stopButton.setTooltip("Stop and go back to the start");
    tempoDisplay.setTooltip("Tempo: -/+ or the mouse wheel change it by 1 BPM, click the number "
                            "to type one");
    playButton.onClick = [&transport] { transport.togglePause(); };
    stopButton.onClick = [&transport] { transport.stop(); };

    // The -/+ buttons step 1 BPM; typed values keep three decimals.
    tempoDisplay.setRange(engine::Transport::minTempo, engine::Transport::maxTempo);
    tempoDisplay.onChange = [this](double bpm)
    {
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
    metronomeButton.setTooltip("Metronome");
    metronomeButton.onClick = [this]
    { engine.metronome().setEnabled(metronomeButton.getToggleState()); };

    typingButton.setClickingTogglesState(true);
    typingButton.setToggleState(liveInput.isTypingEnabled(), juce::dontSendNotification);
    typingButton.setTooltip("Typing keyboard to piano: Z-M and Q-P play the selected "
                            "channel. MIDI keyboards always play it.");
    typingButton.onClick = [this] { setTypingKeyboard(typingButton.getToggleState()); };

    // Window buttons run their window's command, the same as its key and the View menu.
    for (const auto& [button, command, tooltip] :
         {std::tuple{&playlistButton, jls::commands::showPlaylist, "Playlist"},
          std::tuple{&rackButton, jls::commands::toggleChannelRack, "Channel rack"},
          std::tuple{&pianoRollButton, jls::commands::showPianoRoll, "Piano roll"},
          std::tuple{&mixerButton, jls::commands::toggleMixer, "Mixer"},
          std::tuple{&browserButton, jls::commands::toggleBrowser, "Browser"}})
    {
        button->setTooltip(tooltip);
        button->onClick = [this, id = command] { commandManager.invokeDirectly(id, false); };
    }

    ui::joinSegments({&patButton, &songButton});
    positionLabel.setJustificationType(juce::Justification::centred);
    positionLabel.setFont(ui::displayFont(16.0f));
    positionLabel.setBorderSize({});
    timeLabel.setJustificationType(juce::Justification::centredLeft);
    timeLabel.setFont(ui::displayFont(12.0f));
    timeLabel.setBorderSize({});
    timeLabel.setTooltip("Time: current / total. The song in SONG mode, the pattern in PAT mode.");

    for (auto* c : std::initializer_list<juce::Component*>{
             &patButton, &songButton, &patternBox, &newPatternButton, &playButton, &stopButton,
             &timeSignatureBox, &metronomeButton, &typingButton, &playlistButton, &rackButton,
             &pianoRollButton, &mixerButton, &browserButton})
        noFocus(*c);

    for (auto* c : std::initializer_list<juce::Component*>{
             &patButton, &songButton, &patternBox, &newPatternButton, &playButton, &stopButton,
             &tempoDisplay, &timeSignatureBox, &metronomeButton, &typingButton, &playlistButton,
             &rackButton, &pianoRollButton, &mixerButton, &browserButton, &positionLabel, &scope,
             &timeLabel})
        addAndMakeVisible(c);
    // The areas, back to front (the floating ones last).
    for (auto* c : std::initializer_list<juce::Component*>{&playlist, &pianoRoll, &mixer, &browser})
        areas.add(*c);
    playlist.setVisible(true);
    browser.setVisible(themes.settingsFile().getBoolValue("browserVisible", true));
    browser.previewSeconds = [this] { return ui::settings::previewSeconds(themes.settingsFile()); };
    browser.onSideChanged = [this] { resized(); };
    browser.onClose = [this] { toggleBrowser(); };
    browser.onUseSample = [this](const juce::File& file) { channelRack.loadIntoSelected(file); };
    playlist.loadSample = [this](const juce::File& file) { return sampleLoader.load(file); };
    playlist.onOpenAudioChannel = [this](model::ChannelId id) { showSampler(id, false); };
    playlist.onSplitPattern = [this](model::PatternId id) { splitPattern(id); };
    playlist.onOpenPattern = [this](model::PatternId id)
    {
        // As in FL: select the pattern and bring the Channel Rack to the front with it.
        selectPattern(id);
        channelRack.setVisible(true);
        setFocusArea(Area::channelRack);
        menuItemsChanged();
    };
    pianoRoll.darkKeys = [this] { return ui::settings::pianoRollDarkKeys(themes.settingsFile()); };
    channelRack.onDuplicateChannel = [this](model::ChannelId id) { duplicateChannel(id); };
    channelRack.onSizeChanged = [this] { resized(); };
    channelRack.loopedClickCopies = [this]
    { return ui::settings::loopedStepClickCopies(themes.settingsFile()); };
    channelRack.onShowSampler = [this](model::ChannelId id, bool toggle)
    { showSampler(id, toggle); };
    stretchCache.onReady = [this] { publishState(); };
    areas.add(channelRack); // after the main views, so the floating panels are on top
    channelRack.setVisible(true);
    lookAndFeelChanged(); // the rack's shadow
    areas.add(effectPanel);
    mixer.onOpenEffect = [this](model::EffectId id) { openEffectEditor(id); };
    mixer.listPlugins = [this]
    {
        std::vector<ui::MixerView::PluginItem> list;
        for (const auto& e : pluginHost.effects())
            list.push_back({e.id, e.name, e.manufacturer, e.category});
        return list;
    };
    mixer.onManagePlugins = [this] { pluginHost.showManager(); };
    mixer.isEffectAvailable = [this](model::EffectId id) { return effectPool.find(id) != nullptr; };
    effectPool.setPluginFactory(
        [this](const std::string& id, const std::string& state, double rate, int block)
        { return pluginHost.createEffect(id, state, rate, block); });
    effectPool.setInstrumentFactory(
        [this](const std::string& id, const std::string& state, double rate, int block)
        { return pluginHost.createInstrument(id, state, rate, block); });
    channelRack.onOpenPianoRoll = [this](model::ChannelId) { showPianoRoll(); };
    // Clicking notes and keys in the Piano Roll plays them on its channel, like live input.
    pianoRoll.onAuditionStart = [this](int pitch, float velocity)
    {
        engine.sendLive(
            {engine::LiveEvent::Kind::noteOn, document.project().selectedChannel, pitch, velocity});
    };
    pianoRoll.onAuditionStop = [this](int pitch)
    { engine.sendLive({engine::LiveEvent::Kind::noteOff, 0, pitch, 0.0f}); };
    channelRack.listInstruments = [this]
    {
        std::vector<ui::PluginItem> list;
        for (const auto& e : pluginHost.instruments())
            list.push_back({e.id, e.name, e.manufacturer, e.category});
        return list;
    };
    channelRack.onManagePlugins = [this] { pluginHost.showManager(); };
    channelRack.onShowInstrument = [this](model::ChannelId id, bool toggle)
    { showInstrumentEditor(id, toggle); };

#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(this);
#else
    titleBar = std::make_unique<ui::TitleBar>(*this);
    addAndMakeVisible(*titleBar);
#endif

    // Shortcuts: defaults from app/commands, then the user's changes on top.
    commandManager.registerAllCommandsForTarget(this);
    commandManager.setFirstCommandTarget(this);
    if (auto xml = juce::parseXML(shortcutsFile()))
        commandManager.getKeyMappings()->restoreFromXml(*xml);
    commandManager.getKeyMappings()->addChangeListener(this);
    setApplicationCommandManagerToWatch(&commandManager);
    // The output display remembers waveform / spectrum.
    scope.setMode(themes.settingsFile().getValue("scopeMode") == "spectrum"
                      ? ui::Oscilloscope::Mode::spectrum
                      : ui::Oscilloscope::Mode::waveform);
    scope.onModeChange = [this](ui::Oscilloscope::Mode mode)
    {
        themes.settingsFile().setValue(
            "scopeMode", mode == ui::Oscilloscope::Mode::spectrum ? "spectrum" : "waveform");
    };
    scope.sampleRate = [this]
    {
        auto* device = deviceManager.getCurrentAudioDevice();
        return device != nullptr ? device->getCurrentSampleRate() : 48000.0;
    };

    lookAndFeelChanged();
    updateFocusFrames();
    updateShortcutHints();

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
    commandManager.getKeyMappings()->removeChangeListener(this);
    setApplicationCommandManagerToWatch(nullptr); // the manager goes before the MenuBarModel base
    closeSampler();
    pluginWindows.clear(); // editors before their plugins
    instrumentWindows.clear();
    shutdownAudio();
}

bool MainComponent::keyStateChanged(bool)
{
    return liveInput.updateKeys();
}

void MainComponent::focusLost(FocusChangeType)
{
    // Key-up events go to whoever has focus now: end typed notes so none hang.
    liveInput.releaseKeys();
}

void MainComponent::setTypingKeyboard(bool enabled)
{
    liveInput.setTypingEnabled(enabled);
    typingButton.setToggleState(enabled, juce::dontSendNotification);
    menuItemsChanged();
}

void MainComponent::documentChanged()
{
    const auto& project = document.project();
    liveInput.setTargetChannel(project.selectedChannel);
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
    // The Sampler window follows its channel (and closes with it).
    if (sampler != nullptr && !sampler->refresh())
        closeSampler();
    // Close plugin windows whose effect or instrument was removed or replaced (e.g. by undo).
    for (auto it = pluginWindows.begin(); it != pluginWindows.end();)
    {
        if (project.findEffect(it->first) == nullptr ||
            effectPool.find(it->first).get() != it->second->owner())
            it = pluginWindows.erase(it);
        else
            ++it;
    }
    for (auto it = instrumentWindows.begin(); it != instrumentWindows.end();)
    {
        if (project.findChannel(it->first) == nullptr ||
            effectPool.findInstrument(it->first).get() != it->second->owner())
            it = instrumentWindows.erase(it);
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
            effectPanel.setCentrePosition(areas.toAreaSpace(getLocalBounds()).getCentre());
            effectPanelPlaced = true;
        }
        effectPanel.toFront(false);
        return;
    }

    if (auto it = pluginWindows.find(id); it != pluginWindows.end())
    {
        bringPluginToFront(*it->second);
        return;
    }
    auto processor = effectPool.find(id);
    if (processor == nullptr)
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions::makeOptionsOk(
                juce::MessageBoxIconType::NoIcon, "Plugin not available",
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
    auto* effect = dynamic_cast<plugins::PluginEffect*>(processor.get());
    if (effect == nullptr)
        return;
    auto view = openPluginView(processor, effect->plugin(), title);
    view->setCloseHandler(
        [this, id] { juce::MessageManager::callAsync([this, id] { pluginWindows.erase(id); }); });
    pluginWindows[id] = std::move(view);
    bringPluginToFront(*pluginWindows[id]);
}

void MainComponent::showInstrumentEditor(model::ChannelId id, bool toggle)
{
    const auto* channel = document.project().findChannel(id);
    if (channel == nullptr || !channel->isInstrument())
        return;

    if (auto it = instrumentWindows.find(id); it != instrumentWindows.end())
    {
        // As in FL: a covered plugin comes to the front; one already in front closes.
        const auto* panel = dynamic_cast<const juce::Component*>(it->second.get());
        if (toggle && (panel == nullptr || !areas.isCovered(*panel, &channelRack)))
            instrumentWindows.erase(it);
        else
            bringPluginToFront(*it->second);
        return;
    }
    auto instrument = effectPool.findInstrument(id);
    auto* plugin = dynamic_cast<plugins::PluginInstrument*>(instrument.get());
    if (plugin == nullptr)
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions::makeOptionsOk(
                juce::MessageBoxIconType::NoIcon, "Plugin not available",
                juce::String::fromUTF8(channel->pluginName.c_str()) +
                    " isn't installed or couldn't be loaded. Scan for plugins in Options > "
                    "Plugins. The channel is kept in the project and stays silent."),
            nullptr);
        return;
    }
    const auto title = juce::String::fromUTF8(channel->name.c_str());
    auto view = openPluginView(instrument, plugin->plugin(), title);
    view->setCloseHandler(
        [this, id]
        { juce::MessageManager::callAsync([this, id] { instrumentWindows.erase(id); }); });
    instrumentWindows[id] = std::move(view);
    bringPluginToFront(*instrumentWindows[id]);
}

std::unique_ptr<plugins::PluginView>
MainComponent::openPluginView(std::shared_ptr<void> owner, juce::AudioPluginInstance& plugin,
                              const juce::String& title)
{
    if (!ui::settings::pluginsInsideApp(themes.settingsFile()))
    {
        auto window = std::make_unique<plugins::PluginWindow>(std::move(owner), plugin, title);
        forwardShortcuts(*window);
        return window;
    }
    // A panel inside the main window. Plugin editors are native windows on Linux, which JUCE
    // can't draw over, so plugin panels stay in front of everything else.
    // Placed before it's added: the editor's native window appears where the panel is then.
    auto panel = std::make_unique<plugins::PluginPanel>(std::move(owner), plugin, title);
    panel->setMovableArea(floatingArea);
    panel->placeInitially(static_cast<int>(pluginWindows.size() + instrumentWindows.size()) % 6);
    panel->setKeyboardReturn([this] { returnKeyboard(); });
    areas.add(*panel);
    panel->setVisible(true);
    panel->toFront(false);
    // Again once the plugin has settled (some size their editor asynchronously), repainting the
    // window so nothing it drew before moving is left behind.
    juce::Component::SafePointer<juce::Component> safe(panel.get());
    juce::Timer::callAfterDelay(300,
                                [safe]
                                {
                                    if (safe == nullptr)
                                        return;
                                    safe->toFront(false);
                                    if (auto* parent = safe->getParentComponent())
                                        parent->repaint();
                                });
    return panel;
}

bool MainComponent::mouseOverPluginPanel() const
{
    const auto mouse = juce::Desktop::getMousePosition();
    for (const auto* views : {&pluginWindows, &instrumentWindows})
        for (const auto& [id, view] : *views)
            if (const auto* panel = dynamic_cast<const ui::FloatingPanel*>(view.get());
                panel != nullptr && panel->isShowing() && panel->getScreenBounds().contains(mouse))
                return true;
    return false;
}

void MainComponent::returnKeyboard()
{
    // Taking JUCE's focus makes an embedded editor that uses XEmbed give up the keyboard. Others
    // (Surge XT, for one) hold the X input focus in their own window, which JUCE won't take back
    // while it's inside ours (ComponentPeer::grabFocus does nothing then): set it directly.
    grabKeyboardFocus();
    if (auto* peer = getPeer())
        app::focusNativeWindow(peer->getNativeHandle());
}

void MainComponent::duplicateChannel(model::ChannelId channel)
{
    if (document.project().findChannel(channel) == nullptr)
        return;
    // The copy takes the plugin's settings as they are now (preset, knobs), not as last saved.
    syncPluginStates();
    document.apply([channel](model::Project& p) { p.duplicateChannel(channel); });
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
    std::vector<std::pair<model::ChannelId, std::string>> instrumentStates;
    for (const auto& channel : document.project().channels)
        if (channel.isInstrument())
            if (auto instrument = effectPool.findInstrument(channel.id))
                if (auto state = instrument->saveState(); state != channel.pluginState)
                    instrumentStates.emplace_back(channel.id, std::move(state));
    if (states.empty() && instrumentStates.empty())
        return;
    // Plugin settings live inside the plugin; copying them in isn't an undoable edit.
    document.setViewState(
        [&states, &instrumentStates](model::Project& p)
        {
            for (auto& [id, state] : states)
                if (auto* slot = p.findEffect(id))
                    slot->pluginState = std::move(state);
            for (auto& [id, state] : instrumentStates)
                if (auto* channel = p.findChannel(id))
                    channel->pluginState = std::move(state);
        });
}

void MainComponent::publishState()
{
    // Stretch-mode samples come from the cache; one finishing triggers another snapshot.
    stretchCache.beginSnapshot();
    engine.setState(
        model::toEngineState(document.project(), &effectPool, [this](const model::Channel& channel)
                             { return stretchCache.get(document.project(), channel); }));
    stretchCache.endSnapshot();
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
    tempoDisplay.setValue(project.tempo);
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
    // As in FL: a rack covered by another window (the Playlist, a plugin...) comes to the front
    // first.
    const bool behind = areas.isCovered(channelRack);
    if (channelRack.isVisible() && !behind)
        channelRack.setVisible(false);
    else
    {
        channelRack.setVisible(true);
        setFocusArea(Area::channelRack);
    }
    menuItemsChanged();
}

void MainComponent::showPlaylist()
{
    pianoRoll.setVisible(false);
    playlist.setVisible(true);
    setFocusArea(Area::mainView);
    menuItemsChanged();
}

void MainComponent::showPianoRoll()
{
    playlist.setVisible(false);
    pianoRoll.setVisible(true);
    setFocusArea(Area::mainView);
    pianoRoll.refresh();
    menuItemsChanged();
}

juce::Component* MainComponent::areaComponent(Area area)
{
    switch (area)
    {
    case Area::browser:
        return &browser;
    case Area::channelRack:
        return &channelRack;
    case Area::mainView:
        return pianoRoll.isVisible() ? static_cast<juce::Component*>(&pianoRoll) : &playlist;
    case Area::mixer:
        return &mixer;
    case Area::plugin:
        return stopComponent({Area::plugin, focusPlugin});
    }
    return nullptr;
}

juce::Component* MainComponent::stopComponent(const FocusStop& stop)
{
    if (stop.area != Area::plugin)
        return areaComponent(stop.area);
    const auto panels = pluginPanels();
    return std::find(panels.begin(), panels.end(), stop.plugin) != panels.end() ? stop.plugin
                                                                                : nullptr;
}

std::vector<juce::Component*> MainComponent::pluginPanels() const
{
    std::vector<juce::Component*> panels;
    for (const auto* views : {&pluginWindows, &instrumentWindows})
        for (const auto& [id, view] : *views)
            if (auto* panel = dynamic_cast<ui::FloatingPanel*>(view.get()))
                panels.push_back(panel);
    return panels;
}

std::vector<MainComponent::FocusStop> MainComponent::tabOrder()
{
    std::vector<FocusStop> all;
    if (ui::settings::focusOrderRecent(themes.settingsFile()))
        all = recentAreas;
    else
    {
        all = {{Area::browser}, {Area::channelRack}, {Area::mainView}, {Area::mixer}};
        for (auto* panel : pluginPanels())
            all.push_back({Area::plugin, panel});
    }
    std::vector<FocusStop> showing;
    for (const auto& stop : all)
        if (auto* c = stopComponent(stop); c != nullptr && c->isVisible())
            showing.push_back(stop);
    return showing;
}

void MainComponent::setFocusStop(FocusStop stop)
{
    // A deliberate choice (click, focus command): it becomes the most recent stop.
    cycleStep = 0;
    std::erase(recentAreas, stop);
    recentAreas.insert(recentAreas.begin(), stop);
    if (stop.area != focusArea || stop.plugin != focusPlugin)
    {
        focusArea = stop.area;
        focusPlugin = stop.plugin;
        repaint();
    }
    raiseFocusedArea();
}

void MainComponent::bringPluginToFront(plugins::PluginView& view)
{
    if (auto* panel = dynamic_cast<ui::FloatingPanel*>(&view))
        setFocusStop({Area::plugin, panel});
    else
        view.bringToFront();
}

void MainComponent::forgetClosedPlugins()
{
    // Closed panels leave the order; if the focused one closed, the focus goes back to the
    // stop used before it.
    std::erase_if(recentAreas, [this](const FocusStop& stop)
                  { return stop.area == Area::plugin && stopComponent(stop) == nullptr; });
    if (focusArea != Area::plugin || stopComponent({Area::plugin, focusPlugin}) != nullptr)
        return;
    for (const auto& stop : recentAreas)
        if (auto* c = stopComponent(stop); c != nullptr && c->isVisible())
        {
            setFocusStop(stop);
            return;
        }
    setFocusArea(Area::mainView);
}

void MainComponent::raiseFocusedArea()
{
    updateFocusFrames();
    // The focused area comes to the front, like windows in FL: the Playlist / Piano Roll goes over
    // the floating Channel Rack and plugin windows, the rack over the Playlist.
    if (auto* c = areaComponent(focusArea); c != nullptr && c->isVisible())
        areas.raise(*c);
}

void MainComponent::updateFocusFrames()
{
    // Panels draw their own focus frame (ui::paintCardFrame; the Channel Rack and plugin panels a
    // line along the top), so floating panels above them cover it.
    std::vector<std::pair<juce::Component*, FocusStop>> panels{{&browser, {Area::browser}},
                                                               {&channelRack, {Area::channelRack}},
                                                               {&playlist, {Area::mainView}},
                                                               {&pianoRoll, {Area::mainView}},
                                                               {&mixer, {Area::mixer}}};
    for (auto* panel : pluginPanels())
        panels.push_back({panel, {Area::plugin, panel}});
    const FocusStop focused{focusArea, focusArea == Area::plugin ? focusPlugin : nullptr};
    for (const auto& [panel, stop] : panels)
        if (static_cast<bool>(panel->getProperties()["focused"]) != (stop == focused))
        {
            panel->getProperties().set("focused", stop == focused);
            panel->repaint();
        }
    updateWindowButtons();
}

int MainComponent::tabSteps(Area area)
{
    const auto order = tabOrder();
    const auto index = [&order](const FocusStop& stop)
    { return static_cast<int>(std::find(order.begin(), order.end(), stop) - order.begin()); };
    const int n = static_cast<int>(order.size());
    const int at = index({area});
    if (at == n)
        return -1;
    const int from = index({focusArea, focusArea == Area::plugin ? focusPlugin : nullptr});
    if (from == n) // the focused area was just hidden: count from before the first
        return at + 1;
    return (at - from + n) % n;
}

void MainComponent::updateWindowButtons()
{
    const int main = tabSteps(Area::mainView);
    playlistButton.setOrder(playlist.isVisible() ? main : -1);
    pianoRollButton.setOrder(pianoRoll.isVisible() ? main : -1);
    rackButton.setOrder(tabSteps(Area::channelRack));
    mixerButton.setOrder(tabSteps(Area::mixer));
    browserButton.setOrder(tabSteps(Area::browser));
}

void MainComponent::modifierKeysChanged(const juce::ModifierKeys& modifiers)
{
    // Releasing the modifiers ends a "last used first" cycle on the stop reached.
    if (cycleStep != 0 && !modifiers.isAnyModifierKeyDown())
        setFocusStop({focusArea, focusPlugin});
    juce::Component::modifierKeysChanged(modifiers);
}

void MainComponent::cycleFocus(int direction)
{
    // Like Alt+Tab, through the areas and plugin panels that are showing.
    const auto showing = tabOrder();
    const int n = static_cast<int>(showing.size());
    if (n < 2)
        return;
    if (ui::settings::focusOrderRecent(themes.settingsFile()))
    {
        // Step back in last-used order while the modifier is held; the order only changes once
        // it's released (modifierKeysChanged). The current stop is first until then.
        cycleStep = ((cycleStep + direction) % n + n) % n;
        const auto& stop = showing[static_cast<std::size_t>(cycleStep)];
        focusArea = stop.area;
        focusPlugin = stop.plugin;
        raiseFocusedArea();
        repaint();
        if (!juce::ModifierKeys::currentModifiers.isAnyModifierKeyDown())
            setFocusStop(stop); // invoked without a held modifier: commit right away
        return;
    }
    const FocusStop current{focusArea, focusArea == Area::plugin ? focusPlugin : nullptr};
    const auto it = std::find(showing.begin(), showing.end(), current);
    // A hidden focused area: start before the first (or after the last, going back).
    const int at =
        it != showing.end() ? static_cast<int>(it - showing.begin()) : (direction > 0 ? -1 : n);
    setFocusStop(showing[static_cast<std::size_t>(((at + direction) % n + n) % n)]);
}

void MainComponent::navigate(juce::CommandID command)
{
    using namespace jls::commands;
    if (auto* c = areaComponent(focusArea); c == nullptr || !c->isVisible())
        return;
    switch (focusArea)
    {
    case Area::browser:
        if (command == navUp || command == navDown)
            browser.moveSelection(command == navUp ? -1 : 1);
        else if (command == navUpLoad || command == navDownLoad)
            browser.moveSelection(command == navUpLoad ? -1 : 1, true);
        else if (command == navLeft || command == navRight)
            browser.openSelected(command == navRight);
        else if (command == navConfirm)
            browser.confirmSelected();
        break;
    case Area::channelRack:
        if (command == navUp || command == navDown)
            channelRack.selectOffset(command == navUp ? -1 : 1);
        break;
    case Area::mixer:
        if (command == navLeft || command == navRight)
        {
            const int track =
                std::clamp(document.project().selectedMixerTrack + (command == navLeft ? -1 : 1), 0,
                           static_cast<int>(document.project().mixer.tracks.size()) - 1);
            document.setViewState([track](model::Project& p) { p.selectedMixerTrack = track; });
        }
        break;
    case Area::mainView:
        // Piano Roll: Ctrl+Up / Down move the selected notes an octave (FL).
        if (pianoRoll.isVisible() && (command == navUpLoad || command == navDownLoad))
            pianoRoll.edit(command == navUpLoad ? ui::EditAction::moveOctaveUp
                                                : ui::EditAction::moveOctaveDown);
        break;
    case Area::plugin: // the plugin's own keys need a click into it
        break;
    }
}

void MainComponent::showSampler(model::ChannelId id, bool toggle)
{
    const auto* channel = document.project().findChannel(id);
    if (channel == nullptr || channel->isInstrument())
        return;
    if (sampler != nullptr && sampler->channel() == id)
    {
        // As in FL: a covered window comes to the front; one already in front closes.
        if (toggle && !areas.isCovered(*samplerPanel, &channelRack))
            closeSampler();
        else
            samplerPanel->toFront(false);
        return;
    }
    closeSampler();
    auto content = std::make_unique<ui::SamplerPanel>(document, id);
    sampler = content.get();
    samplerPanel = std::make_unique<ui::FloatingPanel>(std::move(content));
    samplerPanel->onClose = [this] { juce::MessageManager::callAsync([this] { closeSampler(); }); };
    areas.add(*samplerPanel);
    samplerPanel->setVisible(true);
    samplerPanel->setMovableArea(floatingArea);
    if (samplerPosition)
        samplerPanel->setTopLeftPosition(*samplerPosition);
    else
        samplerPanel->placeInitially(0);
    samplerPanel->setMovableArea(floatingArea); // keeps a remembered position reachable
    sampler->refresh();                         // its title names the channel
    samplerPanel->toFront(false);
}

void MainComponent::closeSampler()
{
    if (samplerPanel == nullptr)
        return;
    samplerPosition = samplerPanel->getPosition();
    sampler = nullptr;
    samplerPanel.reset();
}

void MainComponent::forwardShortcuts(juce::Component& window)
{
    // Clicking a channel opens its window, which then has the keyboard: without this, Ctrl+A,
    // Ctrl+Tab, Space... did nothing until the main window was clicked again. (A plugin's own
    // editor can still keep keys it uses itself.)
    window.addKeyListener(commandManager.getKeyMappings());
}

void MainComponent::splitPattern(model::PatternId id)
{
    const auto* pattern = document.project().findPattern(id);
    if (pattern == nullptr)
        return;
    int channelsWithNotes = 0;
    for (const auto& channel : document.project().channels)
        if (!pattern->notesFor(channel.id).empty())
            ++channelsWithNotes;
    if (channelsWithNotes < 2)
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions::makeOptionsOk(
                juce::MessageBoxIconType::NoIcon, "Split by channel",
                juce::String::fromUTF8(pattern->name.c_str()) +
                    " has notes on fewer than two channels: nothing to split."),
            nullptr);
        return;
    }
    document.apply([id](model::Project& p) { p.splitByChannel(id); });
}

void MainComponent::toggleBrowser()
{
    browser.setVisible(!browser.isVisible());
    themes.settingsFile().setValue("browserVisible", browser.isVisible());
    resized();
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

void MainComponent::layoutTransportBar(juce::Rectangle<int> bar)
{
    // Groups: mode and pattern | play, stop | tempo, clock | time signature, metronome, keys, and
    // at the right end the window buttons, then the oscilloscope. The window buttons only get
    // their labels when the oscilloscope still fits beside them; when the window is narrower, the
    // oscilloscope goes, then the metronome's and keys' labels, the clock's time, the time
    // signature, and finally the toggles and the window buttons.
    constexpr int h = ui::metrics::transportControlHeight;
    constexpr int gap = ui::metrics::controlGap;
    constexpr int dividerSpace = 2 * gap + 1;
    const int playWidth = h * 7 / 5;
    const int positionWidth = ui::textWidth(ui::displayFont(16.0f), "000:4:4") + 12;
    const int timeWidth = ui::textWidth(ui::displayFont(12.0f), "00:00.000 / 00:00.000") + 10;
    metronomeButton.setButtonText("Metronome");
    typingButton.setButtonText("Keys");

    const std::array<std::pair<ui::WindowButton*, const char*>, 5> windows{
        {{&playlistButton, "Playlist"},
         {&rackButton, "Channel rack"},
         {&pianoRollButton, "Piano roll"},
         {&mixerButton, "Mixer"},
         {&browserButton, "Browser"}}};
    std::array<int, 5> labelledWidths{};
    int windowsLabelled = dividerSpace;
    for (std::size_t i = 0; i < windows.size(); ++i)
    {
        windows[i].first->setButtonText(windows[i].second);
        labelledWidths[i] = windows[i].first->preferredWidth(h);
        windowsLabelled += labelledWidths[i] + gap;
    }
    const int windowsIconsOnly = dividerSpace + static_cast<int>(windows.size()) * (h + gap);

    // `core` stages: 0 everything, 1 metronome and keys as icons, 2 no clock time, 3 no time
    // signature, 4 no toggles. The window buttons come off before each of those.
    const auto coreWidth = [&](int core)
    {
        int w = ui::buttonWidth("PAT", h) + ui::buttonWidth("SONG", h) + gap + 150 + gap + h +
                dividerSpace + 2 * playWidth + gap + dividerSpace + tempoDisplay.preferredWidth(h) +
                gap + positionWidth + (core < 2 ? timeWidth : 0);
        if (core < 4)
            w += dividerSpace + (core < 3 ? ui::metrics::comboWidth - 4 + gap : 0) +
                 (core < 1 ? metronomeButton.preferredWidth(h) + typingButton.preferredWidth(h)
                           : 2 * h) +
                 gap;
        return w;
    };
    // Stage 0: window buttons with labels (and room for the oscilloscope); 1: icons only; 2-5:
    // core stages 1-4 (icons only, and no window buttons at the last).
    constexpr int minScopeWidth = 120;
    constexpr int maxScopeWidth = 220;
    const auto widths = [&](int stage)
    {
        const int core = std::max(0, stage - 1);
        if (stage == 0)
            return coreWidth(0) + windowsLabelled + minScopeWidth + gap;
        return coreWidth(core) + (core < 4 ? windowsIconsOnly : 0);
    };
    int stage = 0;
    while (stage < 5 && widths(stage) > bar.getWidth())
        ++stage;
    const int core = std::max(0, stage - 1);
    if (core >= 1)
    {
        metronomeButton.setButtonText({});
        typingButton.setButtonText({});
    }

    transportDividers.clear();
    const auto take = [&bar](int width)
    {
        auto r = bar.removeFromLeft(width).withSizeKeepingCentre(width, h);
        bar.removeFromLeft(gap);
        return r;
    };
    const auto divider = [&]
    {
        transportDividers.push_back(bar.getX());
        bar.removeFromLeft(gap + 1);
    };

    patButton.setBounds(bar.removeFromLeft(ui::buttonWidth("PAT", h))
                            .withSizeKeepingCentre(ui::buttonWidth("PAT", h), h));
    songButton.setBounds(take(ui::buttonWidth("SONG", h)));
    patternBox.setBounds(take(150));
    newPatternButton.setBounds(take(h));
    divider();
    playButton.setBounds(take(playWidth));
    stopButton.setBounds(take(playWidth));
    divider();
    tempoDisplay.setBounds(take(tempoDisplay.preferredWidth(h)));
    clockBounds = take(positionWidth + (core < 2 ? timeWidth : 0));
    {
        auto clock = clockBounds;
        positionLabel.setBounds(clock.removeFromLeft(positionWidth));
        timeLabel.setVisible(core < 2);
        timeLabel.setBounds(clock);
    }
    timeSignatureBox.setVisible(core < 3);
    metronomeButton.setVisible(core < 4);
    typingButton.setVisible(core < 4);
    if (core < 4)
    {
        divider();
        if (core < 3)
            timeSignatureBox.setBounds(take(ui::metrics::comboWidth - 4));
        metronomeButton.setBounds(take(metronomeButton.preferredWidth(h)));
        typingButton.setBounds(take(typingButton.preferredWidth(h)));
    }

    // The oscilloscope takes the room that's left at the right end; it's the first to go.
    const int windowsWidth = stage == 0 ? windowsLabelled : core < 4 ? windowsIconsOnly : 0;
    const int spare = bar.getWidth() - windowsWidth;
    const bool roomForScope = core == 0 && spare >= minScopeWidth;
    scope.setVisible(roomForScope);
    if (roomForScope)
    {
        const int width = std::min(maxScopeWidth, spare);
        scope.setBounds(bar.removeFromRight(width).withSizeKeepingCentre(width, h));
        bar.removeFromRight(gap);
    }

    // The window buttons, right-aligned next to the oscilloscope (or the bar's end).
    for (std::size_t i = 0; i < windows.size(); ++i)
    {
        auto* button = windows[i].first;
        button->setVisible(windowsWidth > 0);
        if (stage > 0)
            button->setButtonText({});
    }
    if (windowsWidth > 0)
    {
        for (std::size_t i = windows.size(); i-- > 0;)
        {
            const int width = stage == 0 ? labelledWidths[i] : h;
            windows[i].first->setBounds(bar.removeFromRight(width).withSizeKeepingCentre(width, h));
            bar.removeFromRight(gap);
        }
        transportDividers.push_back(bar.getRight() - 1);
    }
}

void MainComponent::lookAndFeelChanged()
{
    const auto& c = ui::currentTheme().colours;
    positionLabel.setColour(juce::Label::textColourId, ui::colour(c.displayText));
    timeLabel.setColour(juce::Label::textColourId, ui::colour(c.displayDim));

    // A shadow only when the rack is an ordinary child component: a native child window can't be
    // see-through over its siblings (no compositing between child windows).
    if (areas.isNative())
    {
        rackShadow.reset();
        return;
    }
    rackShadow =
        std::make_unique<juce::DropShadower>(juce::DropShadow(ui::colour(c.shadow), 18, {0, 6}));
    rackShadow->setOwner(&channelRack);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    // The transport bar: a full-width strip with a slight sheen, a line below, dividers between
    // its groups, the clock's box.
    const auto& t = ui::currentTheme();
    const auto bar = transportBarBounds.toFloat();
    g.setGradientFill(juce::ColourGradient::vertical(
        ui::colour(t.colours.header).brighter(0.06f), bar.getY(),
        ui::colour(t.colours.header).darker(0.12f), bar.getBottom()));
    g.fillRect(bar);
    g.setColour(ui::colour(t.colours.outline));
    g.fillRect(bar.withTop(bar.getBottom() - 1.0f));
    g.setColour(ui::colour(t.colours.ledOff));
    for (int x : transportDividers)
        g.fillRect(static_cast<float>(x), bar.getCentreY() - 12.0f, 1.0f, 24.0f);
    if (!clockBounds.isEmpty())
        ui::paintDisplayBox(g, clockBounds.toFloat());
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    if (titleBar)
        titleBar->setBounds(bounds.removeFromTop(ui::TitleBar::height));

    auto area = bounds;
    transportBarBounds = area.removeFromTop(transportHeight);
    layoutTransportBar(transportBarBounds.reduced(8, 0));

    area.removeFromTop(panelGap);
    const auto panelsArea = area; // everything below the transport bar
    if (browser.isVisible())
    {
        // Full height next to everything below the transport bar.
        areas.setBounds(browser, browser.onRight()
                                     ? area.removeFromRight(ui::BrowserPanel::preferredWidth)
                                     : area.removeFromLeft(ui::BrowserPanel::preferredWidth));
        if (browser.onRight())
            area.removeFromRight(panelGap);
        else
            area.removeFromLeft(panelGap);
    }
    if (mixer.isVisible())
    {
        const int height = std::min(ui::MixerView::preferredHeight, area.getHeight() / 2);
        areas.setBounds(mixer, area.removeFromBottom(height));
        area.removeFromBottom(panelGap);
    }
    areas.setBounds(playlist, area);
    areas.setBounds(pianoRoll, area);

    // The rack floats: place it once, then keep wherever the user dragged it (sizes follow
    // the channel count).
    const int width = channelRack.preferredWidth();
    const int height = channelRack.preferredHeight();
    if (!channelRackPlaced && !area.isEmpty())
    {
        // Below the first tracks, so the Playlist's "click to place" hint stays visible.
        areas.setBounds(channelRack,
                        {area.getX() + 140,
                         area.getY() + ui::PlaylistView::headerHeight +
                             ui::PlaylistView::rulerHeight + 3 * ui::PlaylistView::trackHeight,
                         width, height});
        channelRackPlaced = true;
    }
    else
    {
        channelRack.setSize(width, height);
    }
    // Floating areas move in the areas' own coordinates (the main window's, when native).
    floatingArea = areas.toAreaSpace(panelsArea);
    channelRack.setMovableArea(floatingArea);
    if (samplerPanel != nullptr)
        samplerPanel->setMovableArea(floatingArea);
    for (auto& [id, view] : pluginWindows)
        if (auto* panel = dynamic_cast<ui::FloatingPanel*>(view.get()))
            panel->setMovableArea(floatingArea);
    for (auto& [id, view] : instrumentWindows)
        if (auto* panel = dynamic_cast<ui::FloatingPanel*>(view.get()))
            panel->setMovableArea(floatingArea);
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    // The typing keyboard owns its note keys while it's on; everything else is a command, with
    // the keys from app/commands (or the user's own, from Keyboard shortcuts).
    if (liveInput.isNoteKey(key))
        return true; // played in keyStateChanged()
    return commandManager.getKeyMappings()->keyPressed(key, this);
}

juce::StringArray MainComponent::getMenuBarNames()
{
    return {"File", "Edit", "Patterns", "View", "Transport", "Options", "Help"};
}

juce::PopupMenu MainComponent::getMenuForIndex(int index, const juce::String&)
{
    // Command items show their current shortcut and tick/disabled state (getCommandInfo).
    juce::PopupMenu menu;
    const auto add = [this, &menu](std::initializer_list<int> ids)
    {
        for (int id : ids)
        {
            if (id == 0)
                menu.addSeparator();
            else
                menu.addCommandItem(&commandManager, id);
        }
    };
    switch (index)
    {
    case 0:
        add({jls::commands::newProject, jls::commands::openProject, jls::commands::saveProject,
             jls::commands::saveProjectAs, 0, jls::commands::exportWav, 0, jls::commands::quit});
        break;
    case 1:
        add({jls::commands::undo,
             jls::commands::redo,
             0,
             jls::commands::cut,
             jls::commands::copy,
             jls::commands::paste,
             jls::commands::deleteSelection,
             0,
             jls::commands::selectAll,
             jls::commands::deselect,
             jls::commands::duplicate,
             0,
             jls::commands::moveSelectionUp,
             jls::commands::moveSelectionDown,
             jls::commands::moveSelectionLeft,
             jls::commands::moveSelectionRight,
             jls::commands::notesToStepLength,
             0,
             jls::commands::sliceNotes,
             jls::commands::sliceFiner,
             jls::commands::sliceCoarser,
             0,
             jls::commands::snapFiner,
             jls::commands::snapCoarser});
        break;
    case 2:
        add({jls::commands::newPattern, jls::commands::clonePattern, jls::commands::renamePattern,
             jls::commands::deletePattern, jls::commands::splitByChannel, jls::commands::makeUnique,
             0, jls::commands::nextPattern, jls::commands::previousPattern});
        break;
    case 3:
    {
        add({jls::commands::showPlaylist, jls::commands::toggleChannelRack,
             jls::commands::showPianoRoll, jls::commands::toggleMixer, jls::commands::toggleBrowser,
             jls::commands::browserSwitchSide, jls::commands::browserAddFolder,
             jls::commands::browserUseSample, 0});
        juce::PopupMenu rack;
        for (int id :
             {jls::commands::toggleRackLoop, jls::commands::rackShow16, jls::commands::rackShow32,
              jls::commands::rackShow64, jls::commands::rackShowAudioClips})
            rack.addCommandItem(&commandManager, id);
        rack.addSeparator();
        for (int id : {jls::commands::clearChannel, jls::commands::duplicateChannel})
            rack.addCommandItem(&commandManager, id);
        menu.addSubMenu("Channel rack options", rack);
        break;
    }
    case 4:
        add({jls::commands::playStop, jls::commands::playPause, jls::commands::toggleSongMode,
             jls::commands::toggleMetronome, 0, jls::commands::tempoUp, jls::commands::tempoDown,
             jls::commands::tempoUpFine, jls::commands::tempoDownFine});
        break;
    case 5:
        add({jls::commands::showSettings, jls::commands::showShortcuts, 0,
             jls::commands::showThemes, jls::commands::showPlugins,
             jls::commands::showAudioSettings, 0, jls::commands::toggleTypingKeyboard});
        break;
    case 6:
        add({jls::commands::showAbout});
        break;
    default:
        break;
    }
    return menu;
}

void MainComponent::mouseDown(const juce::MouseEvent& e)
{
    // A click in an area gives it the navigation keys (arrows, Enter); a click on a plugin panel
    // (its title bar: clicks into the editor go to the plugin) makes it the focused stop.
    std::vector<FocusStop> stops{
        {Area::browser}, {Area::channelRack}, {Area::mixer}, {Area::mainView}};
    for (auto* panel : pluginPanels())
        stops.insert(stops.begin(), {Area::plugin, panel});
    for (const auto& stop : stops)
        if (auto* c = stopComponent(stop);
            c != nullptr && c->isVisible() &&
            (c == e.eventComponent || c->isParentOf(e.eventComponent)))
        {
            setFocusStop(stop);
            break;
        }

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

void MainComponent::menuItemSelected(int, int)
{
    // All menu items are commands (see perform()).
}

// ------------------------------------------------------------------------------------ Commands

void MainComponent::getAllCommands(juce::Array<juce::CommandID>& ids)
{
    for (const auto& definition : jls::commands::all())
        ids.add(definition.id);
}

void MainComponent::getCommandInfo(juce::CommandID id, juce::ApplicationCommandInfo& info)
{
    const auto* definition = jls::commands::find(id);
    if (definition == nullptr)
        return;
    info.setInfo(definition->name, definition->description, definition->category, 0);
    for (const auto& key : definition->defaultKeys)
        info.addDefaultKeypress(key.getKeyCode(), key.getModifiers());

    // State shown in menus; inactive commands do nothing when their key is pressed.
    const auto& project = document.project();
    using namespace jls::commands;
    switch (id)
    {
    case undo:
        info.setActive(document.canUndo());
        break;
    case redo:
        info.setActive(document.canRedo());
        break;
    case sliceNotes:
    case sliceFiner:
    case sliceCoarser:
    case moveSelectionUp:
    case moveSelectionDown:
    case moveSelectionLeft:
    case moveSelectionRight:
    case notesToStepLength:
        info.setActive(pianoRoll.isVisible());
        break;
    case jls::commands::deletePattern:
        info.setActive(project.patterns.size() > 1);
        break;
    case toggleSongMode:
        info.setTicked(project.songMode);
        break;
    case toggleMetronome:
        info.setTicked(engine.metronome().isEnabled());
        break;
    case jls::commands::showPlaylist:
        info.setTicked(playlist.isVisible());
        break;
    case jls::commands::toggleChannelRack:
        info.setTicked(channelRack.isVisible());
        break;
    case jls::commands::showPianoRoll:
        info.setTicked(pianoRoll.isVisible());
        break;
    case jls::commands::toggleMixer:
        info.setTicked(mixer.isVisible());
        break;
    case jls::commands::toggleBrowser:
        info.setTicked(browser.isVisible());
        break;
    case browserSwitchSide:
        info.setTicked(browser.onRight());
        break;
    case toggleRackLoop:
        info.setTicked(project.loopChannels);
        break;
    case rackShow16:
    case rackShow32:
    case rackShow64:
        info.setTicked(project.current().gridSteps == 16 << (id - rackShow16));
        break;
    case toggleTypingKeyboard:
        info.setTicked(liveInput.isTypingEnabled());
        break;
    case rackShowAudioClips:
        info.setTicked(project.showAudioClips);
        break;
    case jls::commands::makeUnique:
        info.setActive(playlist.isVisible() && !playlist.selection.empty());
        break;
    case clearChannel:
    case jls::commands::duplicateChannel:
        info.setActive(project.findChannel(project.selectedChannel) != nullptr);
        break;
    default:
        break;
    }
}

bool MainComponent::perform(const InvocationInfo& info)
{
    auto& transport = engine.transport();
    auto& settingsFile = themes.settingsFile();
    using namespace jls::commands;
    switch (info.commandID)
    {
    case jls::commands::newProject:
        newProject();
        break;
    case jls::commands::openProject:
        openProject();
        break;
    case jls::commands::saveProject:
        saveProject();
        break;
    case jls::commands::saveProjectAs:
        saveProjectAs();
        break;
    case jls::commands::exportWav:
        exportWav();
        break;
    case quit:
        requestQuit();
        break;

    case undo:
        document.undo();
        break;
    case redo:
        document.redo();
        break;
    // With the Channel Rack focused, copy / cut / paste act on the selected channel's notes.
    case jls::commands::copy:
        if (focusArea == Area::channelRack && channelRack.isVisible())
            channelRack.copySelectedChannel();
        else
            editInMainView(ui::EditAction::copy);
        break;
    case cut:
        if (focusArea == Area::channelRack && channelRack.isVisible())
            channelRack.cutSelectedChannel();
        else
            editInMainView(ui::EditAction::cut);
        break;
    case paste:
        if (focusArea == Area::channelRack && channelRack.isVisible())
            channelRack.pasteIntoSelectedChannel();
        else
            editInMainView(ui::EditAction::paste);
        break;
    case deleteSelection:
        // With the Channel Rack focused: delete the selected channel (undoable), as in FL.
        if (focusArea == Area::channelRack && channelRack.isVisible())
        {
            if (const auto id = document.project().selectedChannel; id != 0)
                document.apply([id](model::Project& p) { p.removeChannel(id); });
        }
        else
            editInMainView(ui::EditAction::deleteSelection);
        break;
    case jls::commands::duplicate:
        editInMainView(ui::EditAction::duplicate);
        break;
    case moveSelectionUp:
        pianoRoll.edit(ui::EditAction::moveUp);
        break;
    case moveSelectionDown:
        pianoRoll.edit(ui::EditAction::moveDown);
        break;
    case moveSelectionLeft:
        pianoRoll.edit(ui::EditAction::moveLeft);
        break;
    case moveSelectionRight:
        pianoRoll.edit(ui::EditAction::moveRight);
        break;
    case notesToStepLength:
        pianoRoll.edit(ui::EditAction::lengthToStep);
        break;
    case selectAll:
        editInMainView(ui::EditAction::selectAll);
        break;
    case deselect:
        editInMainView(ui::EditAction::deselect);
        break;

    case playStop:
        transport.togglePlay();
        break;
    case playPause:
        transport.togglePause();
        break;
    case toggleSongMode:
        setSongMode(!document.project().songMode);
        break;
    case toggleMetronome:
        engine.metronome().toggleEnabled();
        break;
    case tempoUp:
        changeTempo(ui::settings::tempoStep(settingsFile));
        break;
    case tempoDown:
        changeTempo(-ui::settings::tempoStep(settingsFile));
        break;
    case tempoUpFine:
        changeTempo(ui::settings::tempoStepFine(settingsFile));
        break;
    case tempoDownFine:
        changeTempo(-ui::settings::tempoStepFine(settingsFile));
        break;

    case jls::commands::newPattern:
        newPattern();
        break;
    case jls::commands::clonePattern:
        clonePattern();
        break;
    case jls::commands::splitByChannel:
        splitPattern(document.project().currentPattern);
        break;
    case jls::commands::makeUnique:
        if (!playlist.selection.empty())
            document.apply([clips = playlist.selection](model::Project& p)
                           { p.makeClipsUnique(clips); });
        break;
    case jls::commands::renamePattern:
        renamePattern();
        break;
    case jls::commands::deletePattern:
        deletePattern();
        break;
    case nextPattern:
        selectPatternOffset(1);
        break;
    case previousPattern:
        selectPatternOffset(-1);
        break;

    case jls::commands::showPlaylist:
        showPlaylist();
        break;
    case jls::commands::toggleChannelRack:
        toggleChannelRack();
        break;
    case jls::commands::showPianoRoll:
        showPianoRoll();
        break;
    case jls::commands::toggleMixer:
        toggleMixer();
        break;
    case jls::commands::toggleBrowser:
        toggleBrowser();
        break;
    case browserSwitchSide:
        browser.setOnRight(!browser.onRight());
        break;
    case focusNext:
        cycleFocus(1);
        break;
    case focusPrevious:
        cycleFocus(-1);
        break;
    case focusBrowser:
        if (!browser.isVisible())
            toggleBrowser();
        setFocusArea(Area::browser);
        break;
    case focusChannelRack:
        if (!channelRack.isVisible())
            toggleChannelRack();
        setFocusArea(Area::channelRack);
        break;
    case focusMainView:
        setFocusArea(Area::mainView);
        break;
    case focusMixer:
        if (!mixer.isVisible())
            toggleMixer();
        setFocusArea(Area::mixer);
        break;
    case navUp:
    case navDown:
    case navLeft:
    case navRight:
    case navConfirm:
    case navUpLoad:
    case navDownLoad:
        navigate(static_cast<juce::CommandID>(info.commandID));
        break;
    case browserAddFolder:
        browser.chooseFolderToAdd();
        break;
    case previewPlayPause:
        browser.playPause();
        break;
    case browserUseSample:
        browser.useSelectedSample();
        break;

    case sliceNotes:
        pianoRoll.edit(ui::EditAction::slice);
        break;
    case sliceFiner:
        pianoRoll.stepSliceSize(1);
        break;
    case sliceCoarser:
        pianoRoll.stepSliceSize(-1);
        break;
    case snapFiner:
    case snapCoarser:
    {
        const int finer = info.commandID == snapFiner ? 1 : -1;
        if (pianoRoll.isVisible())
            pianoRoll.stepSnap(finer);
        else
            playlist.stepSnap(finer);
        break;
    }

    case toggleRackLoop:
        document.apply([](model::Project& p) { p.loopChannels = !p.loopChannels; });
        break;
    case rackShow16:
    case rackShow32:
    case rackShow64:
    {
        const int steps = 16 << (info.commandID - rackShow16);
        document.apply([steps](model::Project& p) { p.current().gridSteps = steps; });
        break;
    }

    case toggleTypingKeyboard:
        setTypingKeyboard(!liveInput.isTypingEnabled());
        break;
    case rackShowAudioClips:
        document.setViewState([](model::Project& p) { p.showAudioClips = !p.showAudioClips; });
        break;
    case clearChannel:
        if (const auto channel = document.project().selectedChannel; channel != 0)
            document.apply([channel](model::Project& p) { p.current().clearNotes(channel); });
        break;
    case jls::commands::duplicateChannel:
        duplicateChannel(document.project().selectedChannel);
        break;

    case showSettings:
        ui::showSettingsDialog(settingsFile);
        break;
    case showShortcuts:
        ui::showShortcutsDialog(*commandManager.getKeyMappings());
        break;
    case showThemes:
        ui::ThemeDialog::show(themes);
        break;
    case showPlugins:
        pluginHost.showManager();
        break;
    case jls::commands::showAudioSettings:
        MainComponent::showAudioSettings();
        break;
    case jls::commands::showAbout:
        MainComponent::showAbout();
        break;
    default:
        return false;
    }
    return true;
}

bool MainComponent::editInMainView(ui::EditAction action)
{
    // The Piano Roll and the Playlist share the main area; edit whichever is showing.
    return pianoRoll.isVisible() ? pianoRoll.edit(action) : playlist.edit(action);
}

void MainComponent::changeTempo(double delta)
{
    const double bpm = std::clamp(document.project().tempo + delta, engine::Transport::minTempo,
                                  engine::Transport::maxTempo);
    if (std::abs(bpm - document.project().tempo) > 1.0e-9)
        document.apply([bpm](model::Project& p) { p.tempo = bpm; });
}

void MainComponent::selectPatternOffset(int offset)
{
    const auto& patterns = document.project().patterns;
    const auto it = std::find_if(patterns.begin(), patterns.end(), [this](const model::Pattern& p)
                                 { return p.id == document.project().currentPattern; });
    const int index = static_cast<int>(it - patterns.begin()) + offset;
    if (index >= 0 && index < static_cast<int>(patterns.size()))
        selectPattern(patterns[static_cast<std::size_t>(index)].id);
}

juce::File MainComponent::shortcutsFile() const
{
    return themes.themesFolder().getParentDirectory().getChildFile("shortcuts.xml");
}

juce::String MainComponent::keysFor(juce::CommandID id) const
{
    // JUCE describes keys as "ctrl + shift + cursor up"; show "Ctrl+Shift+Up".
    juce::StringArray keys;
    for (const auto& key : commandManager.getKeyMappings()->getKeyPressesAssignedToCommand(id))
    {
        auto text = key.getTextDescription();
        for (const auto& [from, to] :
             std::initializer_list<std::pair<const char*, const char*>>{{"ctrl + ", "Ctrl+"},
                                                                        {"command + ", "Cmd+"},
                                                                        {"shift + ", "Shift+"},
                                                                        {"alt + ", "Alt+"},
                                                                        {"option + ", "Option+"},
                                                                        {"cursor up", "Up"},
                                                                        {"cursor down", "Down"},
                                                                        {"cursor left", "Left"},
                                                                        {"cursor right", "Right"},
                                                                        {"spacebar", "Space"},
                                                                        {"escape", "Esc"},
                                                                        {"delete", "Delete"},
                                                                        {"backspace", "Backspace"},
                                                                        {"numpad ", "Numpad "}})
            text = text.replace(from, to);
        keys.add(text);
    }
    return keys.joinIntoString(" or ");
}

void MainComponent::updateShortcutHints()
{
    using namespace jls::commands;
    // A control's own tooltip text is remembered the first time, then the keys are appended.
    const auto hint = [this](juce::Component& component,
                             std::initializer_list<std::pair<const char*, juce::CommandID>> uses)
    {
        auto* client = dynamic_cast<juce::SettableTooltipClient*>(&component);
        if (client == nullptr)
            return;
        auto& props = component.getProperties();
        if (!props.contains("baseTooltip"))
            props.set("baseTooltip", client->getTooltip());
        juce::StringArray parts;
        for (const auto& [label, id] : uses)
            if (const auto keys = keysFor(id); keys.isNotEmpty())
                parts.add(juce::String(label).isEmpty() ? keys : juce::String(label) + ": " + keys);
        const auto base = props["baseTooltip"].toString();
        const auto text = parts.isEmpty() ? base : base + " (" + parts.joinIntoString(", ") + ")";
        client->setTooltip(text);
        // Composite controls (the tempo display) show it on their parts too.
        for (auto* child : component.getChildren())
            if (auto* childClient = dynamic_cast<juce::SettableTooltipClient*>(child))
                childClient->setTooltip(text);
    };
    hint(patButton, {{"", toggleSongMode}});
    hint(songButton, {{"", toggleSongMode}});
    hint(newPatternButton, {{"", jls::commands::newPattern}});
    hint(playButton, {{"", playPause}});
    hint(stopButton, {{"play/stop", playStop}});
    hint(tempoDisplay, {{"up", tempoUp},
                        {"down", tempoDown},
                        {"fine up", tempoUpFine},
                        {"fine down", tempoDownFine}});
    hint(metronomeButton, {{"", toggleMetronome}});
    hint(typingButton, {{"", toggleTypingKeyboard}});
    hint(playlistButton, {{"", jls::commands::showPlaylist}});
    hint(rackButton, {{"", jls::commands::toggleChannelRack}});
    hint(pianoRollButton, {{"", jls::commands::showPianoRoll}});
    hint(mixerButton, {{"", jls::commands::toggleMixer}});
    hint(browserButton, {{"", jls::commands::toggleBrowser}});
    hint(channelRack.loopSwitch(), {{"", toggleRackLoop}});
    for (int i = 0; i < 3; ++i)
        hint(channelRack.lengthButton(i), {{"", rackShow16 + i}});
    hint(channelRack.hideButton(), {{"", jls::commands::toggleChannelRack}});
    hint(pianoRoll.sliceControl(), {{"", sliceNotes}});
    hint(pianoRoll.sliceSizeControl(), {{"smaller", sliceFiner}, {"larger", sliceCoarser}});
    hint(pianoRoll.snapControl(), {{"finer", snapFiner}, {"coarser", snapCoarser}});
    hint(playlist.snapControl(), {{"finer", snapFiner}, {"coarser", snapCoarser}});
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    updateShortcutHints();
    // Only the differences from the defaults, so later default changes still reach the user.
    if (auto xml = commandManager.getKeyMappings()->createXml(true))
        xml->writeTo(shortcutsFile());
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
    if (titleBar)
        titleBar->titleChanged();
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
                std::make_unique<juce::FileChooser>("Open project", projectFolder, "*.jlp;*.jls");
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
            juce::MessageBoxOptions::makeOptionsOk(juce::MessageBoxIconType::NoIcon,
                                                   "Couldn't open " + file.getFileName(),
                                                   juce::String::fromUTF8(result.error.c_str())),
            nullptr);
        return;
    }

    engine.transport().stop();
    currentFile = file;
    if (file.hasFileExtension(model::legacyProjectFileExtension))
    {
        // An old .jls project: saving writes a .jlp beside it and leaves the original alone. If
        // that .jlp already exists, Save asks where to save instead of overwriting it.
        const auto renamed = file.withFileExtension(model::projectFileExtension);
        currentFile = renamed.exists() ? juce::File() : renamed;
    }
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
            juce::MessageBoxOptions::makeOptionsOk(juce::MessageBoxIconType::NoIcon,
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
                                         juce::MessageBoxIconType::NoIcon, "Couldn't save",
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
        currentFile != juce::File()
            ? currentFile
            : projectFolder.getChildFile(juce::String("Untitled") + model::projectFileExtension);
    fileChooser = std::make_unique<juce::FileChooser>("Save project", suggested, "*.jlp");
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

            // Rendering runs much faster than real time, but blocks the UI while it does. It
            // uses the live effects and instruments (no reloading; some plugins restore their
            // state asynchronously), so live audio is held off meanwhile.
            juce::MouseCursor::showWaitCursor();
            engine.transport().pause();
            syncPluginStates();
            model::RenderedAudio audio;
            {
                const juce::ScopedLock audioLock(deviceManager.getAudioCallbackLock());
                const int block = engine.preparedBlockSize() > 0 ? engine.preparedBlockSize() : 512;
                if (std::abs(formatRate - rate) > 0.5 || formatBlock != block)
                    effectPool.setFormat(rate, block); // no device yet: set the pool up for it
                audio = model::renderProject(
                    document.project(), rate, 2.0, effectPool, block,
                    [this](const model::Channel& channel)
                    { return stretchCache.getNow(document.project(), channel); });
            }
            const auto result = io::writeWav(file, audio);
            juce::MouseCursor::hideWaitCursor();

            const int seconds = static_cast<int>(static_cast<double>(audio.left.size()) / rate);
            const auto length =
                juce::String(seconds / 60) + ":" + juce::String(seconds % 60).paddedLeft('0', 2);
            juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOk(
                                             juce::MessageBoxIconType::NoIcon,
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
                "The bundled fonts (Inter and IBM Plex Mono) are under the "
                "SIL Open Font License.\n\nNot affiliated with Image-Line."),
        nullptr);
}

void MainComponent::timerCallback()
{
    areas.update(); // the main window exists only after construction (and may be recreated)
    forgetClosedPlugins();
    // JUCE tells the component under the mouse when modifiers change, which reaches us only
    // through its parents: not from an area that is a native window of its own. Catch the
    // release that ends a "last used first" cycle here too.
    if (cycleStep != 0 && !juce::ModifierKeys::getCurrentModifiersRealtime().isAnyModifierKeyDown())
        setFocusStop({focusArea, focusPlugin});

    engine.collectGarbage();
    // Windows are shown and hidden from many places (commands, menus, their close buttons):
    // keep the window buttons' LEDs in step. Cheap when nothing changed.
    updateWindowButtons();

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

    // Safety net for shortcuts: while our window is the focused one, the keyboard must be ours.
    // Focus can end up nowhere (a dialog or popup closed) or stay in another of our windows:
    // with focus-follows-mouse (Hyprland's default) moving the mouse back from the Sampler
    // focuses this window without a click, and JUCE kept sending keys to the Sampler's
    // component. Never steals focus from other apps, nor from a text field in an area (the
    // areas are windows of their own on Linux, so not our children).
    if (auto* peer = getPeer(); peer != nullptr && peer->isFocused() && isShowing())
        if (auto* focused = getCurrentlyFocusedComponent();
            focused == nullptr ||
            (focused != this && !isParentOf(focused) &&
             !(areas.holds(focused) && dynamic_cast<juce::TextEditor*>(focused) != nullptr)))
            grabKeyboardFocus();
    // A plugin's editor keeps the keyboard after a click into it, so typed notes stopped until
    // the main window was clicked: give it back once the mouse leaves the plugin panels (after
    // any drag). Only while our window has the focus, so other apps are never affected.
    if (const bool overPlugin = mouseOverPluginPanel(); overPlugin)
        mouseWasOverPlugin = true;
    else if (mouseWasOverPlugin && !juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
    {
        mouseWasOverPlugin = false;
        if (auto* peer = getPeer(); peer != nullptr && peer->isFocused())
            returnKeyboard();
    }

    // Typed notes can't hang if the window loses focus without a focusLost() (e.g. alt-tab
    // while a child window had focus).
    if (liveInput.anyKeyDown())
        if (auto* peer = getPeer(); peer == nullptr || !peer->isFocused())
            liveInput.releaseKeys();

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
    const double secondsPerStep = project.secondsPerStep();
    timeLabel.setText(juce::String(model::formatTime(steps * secondsPerStep)) + " / " +
                          juce::String(model::formatTime(loopSteps * secondsPerStep)),
                      juce::dontSendNotification);
    positionLabel.setText(juce::String(bar) + ":" + juce::String(beat) + ":" + juce::String(step),
                          juce::dontSendNotification);

    playButton.setIcon(transport.isPlaying() ? ui::Icon::pause : ui::Icon::play);
    metronomeButton.setToggleState(engine.metronome().isEnabled(), juce::dontSendNotification);
}

void MainComponent::showAudioSettings()
{
    auto selector = std::make_unique<juce::AudioDeviceSelectorComponent>(deviceManager, 0, 2, 0, 2,
                                                                         true, true, true, false);
    selector->setItemHeight(ui::metrics::controlHeight);
    auto frame = std::make_unique<ui::DialogFrame>(
        std::move(selector), "Audio and MIDI",
        "The output device, sample rate and buffer size, and which MIDI inputs play the selected "
        "channel (new devices are switched on automatically).",
        500, 370);

    juce::DialogWindow::LaunchOptions options;
    // JUCE dialogs default to light grey; follow the theme instead.
    options.dialogBackgroundColour = juce::LookAndFeel::getDefaultLookAndFeel().findColour(
        juce::ResizableWindow::backgroundColourId);
    options.content.setOwned(frame.release());
    options.dialogTitle = "Audio settings";
    options.useNativeTitleBar = false; // ours: ui::useOwnTitleBar
    options.resizable = false;
    ui::useOwnTitleBar(options.launchAsync());
}

} // namespace jls
