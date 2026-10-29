#pragma once

// The main window's content. Owns the project (model::Document), the audio engine and the
// effect pool, lays out the transport bar, the Playlist / Piano Roll, the docked Mixer and the
// floating panels, and handles menus and keyboard shortcuts. Whenever the document changes it
// publishes a new engine snapshot and refreshes the views (see docs/architecture.md).

#include "app/live_input.h"
#include "app/stretch_cache.h"
#include "engine/effect_pool.h"
#include "engine/engine.h"
#include "io/sample_loader.h"
#include "model/document.h"
#include "plugins/plugin_host.h"
#include "ui/browser.h"
#include "ui/channel_rack.h"
#include "ui/controls.h"
#include "ui/mixer.h"
#include "ui/oscilloscope.h"
#include "ui/piano_roll.h"
#include "ui/playlist.h"
#include "ui/sampler_panel.h"
#include "ui/theme.h"
#include "ui/title_bar.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <optional>

namespace jls
{

// Top-level component: menu bar, transport bar, the Playlist or Piano Roll, and the floating
// Channel Rack.
// Owns the document and the audio engine.
class MainComponent final : public juce::AudioAppComponent,
                            public juce::MenuBarModel,
                            public juce::ApplicationCommandTarget,
                            public juce::DragAndDropContainer, // Browser -> Channel Rack
                            private juce::ChangeListener,
                            private juce::Timer
{
  public:
    explicit MainComponent(ui::ThemeManager& themes);
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    // Shortcuts also work while another window (Sampler, plugins, the main window's own frame)
    // has the keyboard.
    void forwardShortcuts(juce::Component& window);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void lookAndFeelChanged() override; // display colours of the clock, the rack's shadow
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;
    void focusLost(FocusChangeType cause) override;
    void mouseDown(const juce::MouseEvent& e) override;

    // Quits, after asking to save unsaved changes (called for the window's close button too).
    void requestQuit();
    // Opens a project file without asking (the app's command line); errors and repairs are shown.
    void openFile(const juce::File& file);
    void parentHierarchyChanged() override;

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int index, const juce::String& name) override;
    void menuItemSelected(int itemId, int menuIndex) override;
    void menuBarActivated(bool isActive) override;

    // Commands (app/commands): every shortcut-able action goes through these.
    juce::ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
    void getAllCommands(juce::Array<juce::CommandID>& ids) override;
    void getCommandInfo(juce::CommandID id, juce::ApplicationCommandInfo& info) override;
    bool perform(const InvocationInfo& info) override;

  private:
    void timerCallback() override;
    void documentChanged();
    void refreshTransportBar();
    void setSongMode(bool song);
    void selectPattern(model::PatternId id);
    void newPattern();
    void clonePattern();
    void renamePattern();
    void deletePattern();
    void toggleChannelRack();
    void splitPattern(model::PatternId id); // Split by channel (one undo step)
    void showPlaylist();
    void showPianoRoll();
    void toggleMixer();
    void toggleBrowser();
    void showSampler(model::ChannelId channel, bool toggle);

    // Keyboard focus areas: arrow keys and Enter act on the focused one (outlined). The window
    // itself keeps JUCE's keyboard focus; this is only where navigation keys go.
    enum class Area
    {
        browser,
        channelRack,
        mainView, // the Playlist or the Piano Roll, whichever is showing
        mixer,
    };
    juce::Component* areaComponent(Area area);
    void setFocusArea(Area area);
    void layoutTransportBar(juce::Rectangle<int> bar);
    void raiseFocusedArea();  // stacking of the floating Channel Rack follows the focus
    void updateFocusFrames(); // the focused docked panel's accent frame
    // How many Ctrl+Tab presses reach `area` from the focused one (0 = it has the focus), or -1
    // while it's hidden. Follows Settings > Ctrl+Tab order, and a cycle in progress.
    int tabSteps(Area area);
    void updateWindowButtons(); // their LEDs: the windows' Ctrl+Tab order
    void cycleFocus(int direction);
    void modifierKeysChanged(const juce::ModifierKeys& modifiers) override; // ends an MRU cycle
    void navigate(juce::CommandID command);
    void publishState();
    void showAudioSettings();
    void showAbout();

    // File handling. confirmDiscard() runs `then` right away if there's nothing unsaved,
    // otherwise asks Save / Don't save / Cancel first.
    void confirmDiscard(std::function<void()> then);
    void newProject();
    void openProject();
    void saveProject(std::function<void(bool saved)> done = {});
    void saveProjectAs(std::function<void(bool saved)> done = {});
    bool writeProject(const juce::File& file);
    void exportWav();
    void updateTitle();
    void openEffectEditor(model::EffectId id);
    void showInstrumentEditor(model::ChannelId id, bool toggle);
    void setTypingKeyboard(bool enabled);
    void changeTempo(double delta);
    void selectPatternOffset(int offset); // next (+1) / previous (-1) pattern
    bool editInMainView(ui::EditAction action);
    juce::File shortcutsFile() const;
    // Current keys of a command, e.g. "Ctrl+Shift+M" (several: " or "); empty if none.
    juce::String keysFor(juce::CommandID id) const;
    // Adds the current shortcuts to the tooltips of controls that have commands.
    void updateShortcutHints();
    void changeListenerCallback(juce::ChangeBroadcaster*) override; // shortcuts changed: save
    void syncPluginStates(); // copies live plugin states into the project (before save/export)
    // Plugin panels keep the keyboard after a click into them: whether the mouse is over one,
    // and taking the keyboard back (the panels' button, and automatically once the mouse leaves).
    bool mouseOverPluginPanel() const;
    void returnKeyboard();
    bool mouseWasOverPlugin = false;
    // Copies a channel (Duplicate channel), with its plugin's current settings, without notes.
    void duplicateChannel(model::ChannelId channel);

    ui::ThemeManager& themes;

    engine::Engine engine;
    engine::EffectPool effectPool; // effect processors and instruments live here between snapshots
    LiveInput liveInput{engine, deviceManager};     // computer keyboard + MIDI input
    juce::ApplicationCommandManager commandManager; // shortcuts, menus (app/commands)
    plugins::PluginHost pluginHost;
    model::Document document;
    io::SampleLoader sampleLoader;

    juce::TextButton patButton{"PAT"};
    juce::TextButton songButton{"SONG"};
    juce::ComboBox patternBox;
    ui::IconButton newPatternButton{ui::Icon::add};
    ui::IconButton playButton{ui::Icon::play}; // shows pause while playing
    ui::IconButton stopButton{ui::Icon::stop};
    ui::NumberField tempoDisplay{"BPM", 3, "000.000"};
    juce::ComboBox timeSignatureBox;
    ui::IconButton metronomeButton{ui::Icon::metronome, "Metronome"};
    ui::IconButton typingButton{ui::Icon::keys, "Keys"}; // typing keyboard to piano (Ctrl+T)
    // Window buttons, in shortcut order (F5, F6, F7, F9, Alt+F8). Their LEDs show the Ctrl+Tab
    // order (updateWindowButtons).
    ui::WindowButton playlistButton{ui::Icon::playlist, "Playlist"};
    ui::WindowButton rackButton{ui::Icon::channelRack, "Channel rack"};
    ui::WindowButton pianoRollButton{ui::Icon::pianoRoll, "Piano roll"};
    ui::WindowButton mixerButton{ui::Icon::mixer, "Mixer"};
    ui::WindowButton browserButton{ui::Icon::browser, "Browser"};
    juce::Label positionLabel;              // bar:beat:step, in the clock display
    juce::Label timeLabel;                  // next to it: current / total time of what's looping
    ui::Oscilloscope scope{engine.scope()}; // right end of the transport bar, when there's room
    // Painted by paint(): the transport bar's background, its dividers and the clock's box.
    juce::Rectangle<int> transportBarBounds, clockBounds;
    std::vector<int> transportDividers; // x positions
    ui::PlaylistView playlist;
    ui::PianoRollView pianoRoll; // shares the main area with the Playlist (F5 / F7)
    ui::MixerView mixer;         // docked below them (F9)
    ui::BrowserPanel browser;    // docked at the left or right (Alt+F8)
    Area focusArea = Area::mainView;
    // "Last used first" focus order: most recent first; while cycling (modifier held), the order
    // is frozen and `cycleStep` says how far back we are.
    std::vector<Area> recentAreas{Area::mainView, Area::channelRack, Area::browser, Area::mixer};
    int cycleStep = 0; // 0 = not cycling
    ui::EffectPanel effectPanel;
    bool effectPanelPlaced = false;
    juce::File currentFile;   // where the project was last saved/opened; empty if never
    juce::File projectFolder; // where file choosers start: the last project's folder
    std::unique_ptr<juce::FileChooser> fileChooser;
    // Plugin editors (panels inside the window, or separate windows: Settings).
    std::map<model::EffectId, std::unique_ptr<plugins::PluginView>> pluginWindows;
    std::map<model::ChannelId, std::unique_ptr<plugins::PluginView>> instrumentWindows;
    std::unique_ptr<plugins::PluginView> openPluginView(std::shared_ptr<void> owner,
                                                        juce::AudioPluginInstance& plugin,
                                                        const juce::String& title);
    // The Sampler: one at a time, like FL's channel window, floating inside the main window.
    std::unique_ptr<ui::FloatingPanel> samplerPanel;
    ui::SamplerPanel* sampler = nullptr;             // inside samplerPanel
    std::optional<juce::Point<int>> samplerPosition; // where it was last (reopens there)
    void closeSampler();
    juce::Rectangle<int> floatingArea; // where floating panels may go (below the transport bar)
    StretchCache stretchCache;         // Stretch-mode samples for the current tempo
    double formatRate = 0.0;           // engine format the effect pool was last set up for
    int formatBlock = 0;
    ui::ChannelRack channelRack;
    // The floating rack casts a shadow (the theme's shadow colour) on the panels below it.
    std::unique_ptr<juce::DropShadower> rackShadow;
    bool channelRackPlaced = false;
    juce::TooltipWindow tooltips{this, 600};
    // Menus, window name and window buttons (not on macOS: native title bar and menu bar).
    std::unique_ptr<ui::TitleBar> titleBar;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace jls
