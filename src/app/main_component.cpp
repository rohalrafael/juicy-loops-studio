// See main_component.h. Keyboard shortcuts follow FL Studio where it has one (Space, L, F4-F9,
// Ctrl+M); they live here, so keyboard focus is steered back to this component after menus,
// dialogs and clicks (see mouseDown, menuBarActivated, timerCallback).

#include "main_component.h"

#include "app/commands.h"
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

constexpr int menuBarHeight = 24;
constexpr int transportHeight = ui::metrics::transportControlHeight + 16; // the bar around them
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
             &timeSignatureBox, &metronomeButton, &typingButton})
        noFocus(*c);

    for (auto* c : std::initializer_list<juce::Component*>{
             &patButton, &songButton, &patternBox, &newPatternButton, &playButton, &stopButton,
             &tempoDisplay, &timeSignatureBox, &metronomeButton, &typingButton, &positionLabel,
             &timeLabel, &playlist})
        addAndMakeVisible(c);
    addChildComponent(pianoRoll);
    addChildComponent(mixer);
    addChildComponent(browser);
    browser.setVisible(themes.settingsFile().getBoolValue("browserVisible", true));
    browser.previewSeconds = [this] { return ui::settings::previewSeconds(themes.settingsFile()); };
    browser.onSideChanged = [this] { resized(); };
    browser.onClose = [this] { toggleBrowser(); };
    browser.onUseSample = [this](const juce::File& file) { channelRack.loadIntoSelected(file); };
    playlist.loadSample = [this](const juce::File& file) { return sampleLoader.load(file); };
    playlist.onOpenAudioChannel = [this](model::ChannelId id) { showSampler(id, false); };
    playlist.onOpenPattern = [this](model::PatternId id)
    {
        // As in FL: select the pattern and bring the Channel Rack to the front with it.
        selectPattern(id);
        channelRack.setVisible(true);
        setFocusArea(Area::channelRack);
        menuItemsChanged();
    };
    channelRack.onShowSampler = [this](model::ChannelId id, bool toggle)
    { showSampler(id, toggle); };
    stretchCache.onReady = [this] { publishState(); };
    addAndMakeVisible(channelRack); // after the main views, so the floating panels are on top
    addChildComponent(effectPanel);
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
    menuBar = std::make_unique<juce::MenuBarComponent>(this);
    addAndMakeVisible(*menuBar);
#endif

    // Shortcuts: defaults from app/commands, then the user's changes on top.
    commandManager.registerAllCommandsForTarget(this);
    commandManager.setFirstCommandTarget(this);
    if (auto xml = juce::parseXML(shortcutsFile()))
        commandManager.getKeyMappings()->restoreFromXml(*xml);
    commandManager.getKeyMappings()->addChangeListener(this);
    setApplicationCommandManagerToWatch(&commandManager);
    lookAndFeelChanged();
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
    samplerWindow.reset();
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
    if (samplerWindow != nullptr)
    {
        if (!samplerWindow->panel().refresh())
            samplerWindow.reset();
        else if (const auto* c = project.findChannel(samplerWindow->panel().channel()))
            samplerWindow->setName("Sampler - " + juce::String::fromUTF8(c->name.c_str()));
    }
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
    auto window = std::make_unique<plugins::PluginWindow>(processor, effect->plugin(), title);
    window->onClose = [this, id]
    { juce::MessageManager::callAsync([this, id] { pluginWindows.erase(id); }); };
    pluginWindows[id] = std::move(window);
}

void MainComponent::showInstrumentEditor(model::ChannelId id, bool toggle)
{
    const auto* channel = document.project().findChannel(id);
    if (channel == nullptr || !channel->isInstrument())
        return;

    if (auto it = instrumentWindows.find(id); it != instrumentWindows.end())
    {
        if (toggle)
            instrumentWindows.erase(it);
        else
            it->second->toFront(true);
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
    auto window = std::make_unique<plugins::PluginWindow>(instrument, plugin->plugin(), title);
    window->onClose = [this, id]
    { juce::MessageManager::callAsync([this, id] { instrumentWindows.erase(id); }); };
    instrumentWindows[id] = std::move(window);
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
    // As in FL: a rack hidden behind the Playlist / Piano Roll comes to the front first.
    const bool behind =
        getIndexOfChildComponent(&channelRack) <
        std::max(getIndexOfChildComponent(&playlist), getIndexOfChildComponent(&pianoRoll));
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
    }
    return nullptr;
}

void MainComponent::setFocusArea(Area area)
{
    // A deliberate choice (click, focus command): it becomes the most recent area.
    cycleStep = 0;
    std::erase(recentAreas, area);
    recentAreas.insert(recentAreas.begin(), area);
    if (area != focusArea)
    {
        focusArea = area;
        repaint();
    }
    raiseFocusedArea();
}

void MainComponent::raiseFocusedArea()
{
    // The floating Channel Rack sits in front of the Playlist / Piano Roll only while it has the
    // focus; focusing the main view puts the rack behind it, like windows in FL.
    if (focusArea == Area::channelRack)
        channelRack.toFront(false);
    else if (focusArea == Area::mainView)
        channelRack.toBehind(getIndexOfChildComponent(&playlist) <
                                     getIndexOfChildComponent(&pianoRoll)
                                 ? static_cast<juce::Component*>(&playlist)
                                 : &pianoRoll);
}

void MainComponent::modifierKeysChanged(const juce::ModifierKeys& modifiers)
{
    // Releasing the modifiers ends a "last used first" cycle on the area reached.
    if (cycleStep != 0 && !modifiers.isAnyModifierKeyDown())
        setFocusArea(focusArea);
    juce::Component::modifierKeysChanged(modifiers);
}

void MainComponent::cycleFocus(int direction)
{
    if (ui::settings::focusOrderRecent(themes.settingsFile()))
    {
        // Like Alt+Tab: step back through the areas in last-used order while the modifier is
        // held; the order only changes once it's released (modifierKeysChanged).
        std::vector<Area> showing;
        for (auto area : recentAreas)
            if (auto* c = areaComponent(area); c != nullptr && c->isVisible())
                showing.push_back(area);
        if (showing.size() < 2)
            return;
        // The current area is first in `showing` until it changes.
        const int n = static_cast<int>(showing.size());
        cycleStep = ((cycleStep + direction) % n + n) % n;
        focusArea = showing[static_cast<std::size_t>(cycleStep)];
        raiseFocusedArea();
        repaint();
        if (!juce::ModifierKeys::currentModifiers.isAnyModifierKeyDown())
            setFocusArea(focusArea); // invoked without a held modifier: commit right away
        return;
    }

    // Like Alt+Tab, through the areas that are showing.
    const std::array order{Area::browser, Area::channelRack, Area::mainView, Area::mixer};
    const auto current = std::find(order.begin(), order.end(), focusArea) - order.begin();
    for (int step = 1; step <= 4; ++step)
    {
        const auto next = order[static_cast<std::size_t>((current + direction * step + 8) % 4)];
        if (auto* c = areaComponent(next); c != nullptr && c->isVisible())
        {
            setFocusArea(next);
            return;
        }
    }
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
        break; // nothing yet
    }
}

void MainComponent::paintOverChildren(juce::Graphics& g)
{
    auto* c = areaComponent(focusArea);
    if (c == nullptr || !c->isVisible())
        return;
    const auto& t = ui::currentTheme();
    g.setColour(ui::colour(t.colours.accent).withAlpha(0.85f));
    // Inside the area's bounds: when it moves (the Channel Rack can be dragged), JUCE repaints
    // exactly its old and new bounds, so an outline drawn outside them would leave a trail.
    g.drawRoundedRectangle(c->getBounds().toFloat().reduced(0.75f), t.shape.windowRadius, 1.5f);
}

void MainComponent::showSampler(model::ChannelId id, bool toggle)
{
    const auto* channel = document.project().findChannel(id);
    if (channel == nullptr || channel->isInstrument())
        return;
    if (samplerWindow != nullptr && samplerWindow->panel().channel() == id)
    {
        if (toggle)
            samplerWindow.reset();
        else
            samplerWindow->toFront(true);
        return;
    }
    samplerWindow = std::make_unique<ui::SamplerWindow>(document, id);
    samplerWindow->setName("Sampler - " + juce::String::fromUTF8(channel->name.c_str()));
    samplerWindow->onClose = [this]
    { juce::MessageManager::callAsync([this] { samplerWindow.reset(); }); };
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
    // Groups: mode and pattern | play, stop | tempo, clock | time signature, metronome, keys.
    // When the window is narrow, the last group loses its labels, then the clock its time, then
    // the time signature and finally the toggles.
    constexpr int h = ui::metrics::transportControlHeight;
    constexpr int gap = ui::metrics::controlGap;
    constexpr int dividerSpace = 2 * gap + 1;
    const int playWidth = h * 7 / 5;
    const int positionWidth = ui::textWidth(ui::displayFont(16.0f), "000:4:4") + 12;
    const int timeWidth = ui::textWidth(ui::displayFont(12.0f), "00:00.000 / 00:00.000") + 10;
    metronomeButton.setButtonText("Metronome");
    typingButton.setButtonText("Keys");

    const auto widths = [&](int stage)
    {
        int w = ui::buttonWidth("PAT", h) + ui::buttonWidth("SONG", h) + gap + 150 + gap + h +
                dividerSpace + 2 * playWidth + gap + dividerSpace + tempoDisplay.preferredWidth(h) +
                gap + positionWidth + (stage < 2 ? timeWidth : 0);
        if (stage < 4)
            w += dividerSpace + (stage < 3 ? ui::metrics::comboWidth - 4 + gap : 0) +
                 (stage < 1 ? metronomeButton.preferredWidth(h) + typingButton.preferredWidth(h)
                            : 2 * h) +
                 gap;
        return w;
    };
    int stage = 0;
    while (stage < 4 && widths(stage) > bar.getWidth())
        ++stage;
    if (stage >= 1)
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
    clockBounds = take(positionWidth + (stage < 2 ? timeWidth : 0));
    {
        auto clock = clockBounds;
        positionLabel.setBounds(clock.removeFromLeft(positionWidth));
        timeLabel.setVisible(stage < 2);
        timeLabel.setBounds(clock);
    }
    timeSignatureBox.setVisible(stage < 3);
    metronomeButton.setVisible(stage < 4);
    typingButton.setVisible(stage < 4);
    if (stage < 4)
    {
        divider();
        if (stage < 3)
            timeSignatureBox.setBounds(take(ui::metrics::comboWidth - 4));
        metronomeButton.setBounds(take(metronomeButton.preferredWidth(h)));
        typingButton.setBounds(take(typingButton.preferredWidth(h)));
    }
}

void MainComponent::lookAndFeelChanged()
{
    const auto& c = ui::currentTheme().colours;
    positionLabel.setColour(juce::Label::textColourId, ui::colour(c.displayText));
    timeLabel.setColour(juce::Label::textColourId, ui::colour(c.displayDim));
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    // The transport bar: a header-coloured strip, dividers between its groups, the clock's box.
    const auto& t = ui::currentTheme();
    const auto bar = transportBarBounds.toFloat().reduced(t.shape.borderWidth / 2.0f);
    g.setColour(ui::colour(t.colours.header));
    g.fillRoundedRectangle(bar, t.shape.windowRadius);
    if (t.shape.borderWidth > 0.0f)
    {
        g.setColour(ui::colour(t.colours.outline));
        g.drawRoundedRectangle(bar, t.shape.windowRadius, t.shape.borderWidth);
    }
    g.setColour(ui::colour(t.colours.ledOff));
    for (int x : transportDividers)
        g.fillRect(static_cast<float>(x), bar.getCentreY() - 12.0f, 1.0f, 24.0f);
    if (!clockBounds.isEmpty())
        ui::paintDisplayBox(g, clockBounds.toFloat());
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    if (menuBar)
        menuBar->setBounds(bounds.removeFromTop(menuBarHeight));

    auto area = bounds.reduced(margin);
    transportBarBounds = area.removeFromTop(transportHeight);
    layoutTransportBar(transportBarBounds.reduced(8, 0));

    area.removeFromTop(margin);
    const auto panelsArea = area; // everything below the transport bar
    if (browser.isVisible())
    {
        // Full height next to everything below the transport bar.
        browser.setBounds(browser.onRight()
                              ? area.removeFromRight(ui::BrowserPanel::preferredWidth)
                              : area.removeFromLeft(ui::BrowserPanel::preferredWidth));
        if (browser.onRight())
            area.removeFromRight(margin / 2);
        else
            area.removeFromLeft(margin / 2);
    }
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
    channelRack.setMovableArea(panelsArea);
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
             jls::commands::deletePattern, 0, jls::commands::nextPattern,
             jls::commands::previousPattern});
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
    // A click in an area gives it the navigation keys (arrows, Enter).
    for (auto area : {Area::browser, Area::channelRack, Area::mixer, Area::mainView})
        if (auto* c = areaComponent(area);
            c != nullptr && c->isVisible() &&
            (c == e.eventComponent || c->isParentOf(e.eventComponent)))
        {
            setFocusArea(area);
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
    case jls::commands::copy:
        editInMainView(ui::EditAction::copy);
        break;
    case cut:
        editInMainView(ui::EditAction::cut);
        break;
    case paste:
        editInMainView(ui::EditAction::paste);
        break;
    case deleteSelection:
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
