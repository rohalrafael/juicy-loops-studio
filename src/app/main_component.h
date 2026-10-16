#pragma once

#include "engine/engine.h"
#include "io/sample_loader.h"
#include "model/document.h"
#include "ui/channel_rack.h"
#include "ui/playlist.h"
#include "ui/theme.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace jls
{

// Top-level component: menu bar, transport bar, the Playlist and the floating Channel Rack.
// Owns the document and the audio engine.
class MainComponent final : public juce::AudioAppComponent,
                            public juce::MenuBarModel,
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
    void mouseDown(const juce::MouseEvent& e) override;

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int index, const juce::String& name) override;
    void menuItemSelected(int itemId, int menuIndex) override;
    void menuBarActivated(bool isActive) override;

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
    void showAudioSettings();
    void showAbout();

    ui::ThemeManager& themes;

    engine::Engine engine;
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
    juce::Label positionLabel;
    ui::PlaylistView playlist;
    ui::ChannelRack channelRack;
    bool channelRackPlaced = false;
    juce::TooltipWindow tooltips{this, 600};
    std::unique_ptr<juce::MenuBarComponent> menuBar; // not used on macOS (native menu bar)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

} // namespace jls
