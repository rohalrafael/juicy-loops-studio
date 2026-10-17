#pragma once

// The main window's content. Owns the project (model::Document), the audio engine and the
// effect pool, lays out the transport bar, the Playlist / Piano Roll, the docked Mixer and the
// floating panels, and handles menus and keyboard shortcuts. Whenever the document changes it
// publishes a new engine snapshot and refreshes the views (see docs/architecture.md).

#include "app/live_input.h"
#include "engine/effect_pool.h"
#include "engine/engine.h"
#include "io/sample_loader.h"
#include "model/document.h"
#include "plugins/plugin_host.h"
#include "ui/channel_rack.h"
#include "ui/mixer.h"
#include "ui/piano_roll.h"
#include "ui/playlist.h"
#include "ui/theme.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace jls
{

// Top-level component: menu bar, transport bar, the Playlist or Piano Roll, and the floating
// Channel Rack.
// Owns the document and the audio engine.
class MainComponent final : public juce::AudioAppComponent,
                            public juce::MenuBarModel,
                            public juce::ApplicationCommandTarget,
                            private juce::ChangeListener,
                            private juce::Timer
{
  public:
    explicit MainComponent(ui::ThemeManager& themes);
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;
    void focusLost(FocusChangeType cause) override;
    void mouseDown(const juce::MouseEvent& e) override;

    // Quits, after asking to save unsaved changes (called for the window's close button too).
    void requestQuit();
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
    void showPlaylist();
    void showPianoRoll();
    void toggleMixer();
    void publishState();
    void showAudioSettings();
    void showAbout();

    // File handling. confirmDiscard() runs `then` right away if there's nothing unsaved,
    // otherwise asks Save / Don't save / Cancel first.
    void confirmDiscard(std::function<void()> then);
    void newProject();
    void openProject();
    void openFile(const juce::File& file);
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
    juce::TextButton newPatternButton{"+"};
    juce::TextButton playButton{"Play"};
    juce::TextButton stopButton{"Stop"};
    juce::Slider tempoSlider;
    juce::ComboBox timeSignatureBox;
    juce::TextButton metronomeButton{"Metronome"};
    juce::TextButton typingButton{"Keys"}; // typing keyboard to piano (Ctrl+T)
    juce::Label positionLabel;
    juce::Label timeLabel; // clock under the position: current / total time of what's looping
    ui::PlaylistView playlist;
    ui::PianoRollView pianoRoll; // shares the main area with the Playlist (F5 / F7)
    ui::MixerView mixer;         // docked below them (F9)
    ui::EffectPanel effectPanel;
    bool effectPanelPlaced = false;
    juce::File currentFile;   // where the project was last saved/opened; empty if never
    juce::File projectFolder; // where file choosers start: the last project's folder
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::map<model::EffectId, std::unique_ptr<plugins::PluginWindow>> pluginWindows;
    std::map<model::ChannelId, std::unique_ptr<plugins::PluginWindow>> instrumentWindows;
    double formatRate = 0.0; // engine format the effect pool was last set up for
    int formatBlock = 0;
    ui::ChannelRack channelRack;
    bool channelRackPlaced = false;
    juce::TooltipWindow tooltips{this, 600};
    std::unique_ptr<juce::MenuBarComponent> menuBar; // not used on macOS (native menu bar)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace jls
