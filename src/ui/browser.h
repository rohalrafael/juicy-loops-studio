#pragma once

// The Browser: a panel docked at the left or right with folders you add and the audio files in
// them. Click a file to hear a short preview (Shift+click: all of it), see its waveform and
// click in it to play from there; drag files onto the Channel Rack.

#include "engine/preview_player.h"
#include "io/sample_loader.h"

#include "ui/controls.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace jls::ui
{

// A drag container of its own: as a native area (app/area_stack) it has no parent to start
// drags from, and its drags (samples onto the Channel Rack or the Playlist) cross windows.
class BrowserPanel final : public juce::Component,
                           public juce::DragAndDropContainer,
                           private juce::Timer
{
  public:
    static constexpr int preferredWidth = 260;
    // Drag descriptions start with this, followed by the file's full path.
    static constexpr const char* dragPrefix = "file:";

    BrowserPanel(engine::PreviewPlayer& preview, juce::PropertiesFile& settings);
    ~BrowserPanel() override;

    bool onRight() const { return right; }
    void setOnRight(bool shouldBeOnRight);
    // Asks for a folder to add (file chooser).
    void chooseFolderToAdd();
    // Play / pause the selected sample's preview (whole sample, from where it paused).
    void playPause();

    // Keyboard (from the owner's commands while the Browser has the focus).
    // Up / down through the visible items; previews samples, and with `load` also puts them into
    // the selected channel (onUseSample), FL's Ctrl+Up / Down.
    void moveSelection(int delta, bool load = false);
    void openSelected(bool open); // folders: open (true) or close (false)
    void confirmSelected();       // Enter: toggle a folder, or use the sample (onUseSample)
    void useSelectedSample();     // Ctrl+Enter: use the selected sample; folders are ignored

    std::function<double()> previewSeconds;             // length of a click preview
    std::function<void(const juce::File&)> onUseSample; // load into the selected channel
    std::function<void()> onSideChanged;                // the owner re-lays out
    std::function<void()> onClose;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override { paintCardFrame(g, *this); }
    void resized() override;

    // Used by the tree items.
    void select(const juce::File& file, bool whole);
    void removeFolder(const juce::File& folder);

  private:
    class Item;
    class FolderList;
    class Waveform;

    void timerCallback() override;
    void loaded(const juce::File& file, std::shared_ptr<const engine::SampleBuffer> sample,
                int generation);
    void setCurrent(std::shared_ptr<const engine::SampleBuffer> sample);
    void startPreview(double startFrame, bool whole);
    void rebuildTree();
    void saveFolders();

    engine::PreviewPlayer& preview;
    juce::PropertiesFile& settings;
    bool right = false;
    juce::StringArray folders;

    IconButton addButton{Icon::addFolder};
    IconButton sideButton{Icon::dockSide};
    IconButton closeButton{Icon::close};
    juce::TreeView tree;
    std::unique_ptr<juce::TreeViewItem> treeRoot;
    std::unique_ptr<Waveform> waveform;
    IconButton playButton{Icon::play, "Play"};
    juce::Label nameLabel;
    std::unique_ptr<juce::FileChooser> chooser;

    // The selected sample. Replaced ones wait in `retired` until the audio thread is done.
    juce::File currentFile;
    std::shared_ptr<const engine::SampleBuffer> current;
    std::vector<std::pair<std::uint64_t, std::shared_ptr<const engine::SampleBuffer>>> retired;
    double pausedAt = 0.0;  // frame to resume from
    bool playWhole = false; // what the pending load should play
    bool pendingPlay = false;

    // Loading happens on a background thread so long files don't freeze the app.
    io::SampleLoader loader; // used only by the loader thread
    std::atomic<int> generation{0};
    juce::ThreadPool loaderThread{1}; // last: stopped first
};

} // namespace jls::ui
