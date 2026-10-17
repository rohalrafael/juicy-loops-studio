// Tree items load their children when first opened. The selected sample is decoded on a
// background thread and handed to the engine's PreviewPlayer as a raw pointer; samples it may
// still be reading are kept in `retired` until PreviewPlayer::consumed() says otherwise.

#include "ui/browser.h"

#include "ui/theme.h"

namespace jls::ui
{
namespace
{

constexpr int headerHeight = 30;
constexpr int waveformHeight = 84;
constexpr int footerHeight = 30;
const juce::String audioWildcard = "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3";

bool isAudioFile(const juce::File& f)
{
    return f.hasFileExtension("wav;aif;aiff;flac;ogg;mp3");
}

juce::String formatSeconds(double seconds)
{
    const int total = static_cast<int>(seconds);
    return juce::String(total / 60) + ":" + juce::String(total % 60).paddedLeft('0', 2) + "." +
           juce::String(static_cast<int>((seconds - total) * 10.0));
}

} // namespace

// ------------------------------------------------------------------------------------- Items

class BrowserPanel::Item final : public juce::TreeViewItem
{
  public:
    Item(BrowserPanel& p, juce::File f, bool isRoot) : panel(p), file(std::move(f)), root(isRoot) {}

    const juce::File& getFile() const { return file; }
    bool mightContainSubItems() override { return file.isDirectory(); }
    juce::String getUniqueName() const override { return file.getFullPathName(); }
    int getItemHeight() const override { return 22; }
    juce::String getTooltip() override { return file.getFullPathName(); }

    void itemOpennessChanged(bool isNowOpen) override
    {
        if (!isNowOpen || getNumSubItems() > 0)
            return;
        // Folders first, then audio files, both by name; hidden files are skipped.
        auto dirs = file.findChildFiles(juce::File::findDirectories, false);
        auto files = file.findChildFiles(juce::File::findFiles, false, audioWildcard);
        const auto byName = [](const juce::File& a, const juce::File& b)
        { return a.getFileName().compareNatural(b.getFileName()) < 0; };
        std::sort(dirs.begin(), dirs.end(), byName);
        std::sort(files.begin(), files.end(), byName);
        for (const auto& list : {dirs, files})
            for (const auto& child : list)
                if (!child.isHidden())
                    addSubItem(new Item(panel, child, false));
    }

    void paintItem(juce::Graphics& g, int width, int height) override
    {
        const auto& t = currentTheme();
        if (isSelected())
        {
            g.setColour(colour(t.colours.accent).withAlpha(0.25f));
            g.fillRect(0, 0, width, height);
        }
        const bool folder = file.isDirectory();
        g.setColour(colour(folder ? t.colours.text : t.colours.textDim));
        g.setFont(juce::FontOptions(13.0f, root ? juce::Font::bold : juce::Font::plain));
        g.drawText(file.getFileName(), 4, 0, width - 6, height, juce::Justification::centredLeft,
                   true);
    }

    void itemClicked(const juce::MouseEvent& e) override
    {
        if (root && e.mods.isPopupMenu())
        {
            juce::PopupMenu menu;
            menu.addItem("Remove from browser", [this] { panel.removeFolder(file); });
            menu.showMenuAsync({});
            return;
        }
        if (!file.isDirectory() && e.mods.isCommandDown())
        {
            // Ctrl+click: put the sample into the selected channel.
            if (panel.onUseSample)
                panel.onUseSample(file);
        }
        else if (!file.isDirectory())
            panel.select(file, e.mods.isShiftDown());
        else if (!e.mods.isPopupMenu())
            setOpen(!isOpen());
    }

    juce::var getDragSourceDescription() override
    {
        return isAudioFile(file) ? juce::var(dragPrefix + file.getFullPathName()) : juce::var();
    }

  private:
    BrowserPanel& panel;
    juce::File file;
    bool root;
};

// The invisible root of the tree: the added folders.
class BrowserPanel::FolderList final : public juce::TreeViewItem
{
  public:
    bool mightContainSubItems() override { return true; }
};

// ---------------------------------------------------------------------------------- Waveform

class BrowserPanel::Waveform final : public juce::Component, public juce::SettableTooltipClient
{
  public:
    std::function<void(double)> onClick; // fraction 0..1 of the sample

    void setSample(const engine::SampleBuffer* sample)
    {
        // Min/max per bucket, computed once, so drawing stays cheap for long files.
        peaks.clear();
        frames = sample != nullptr ? sample->numFrames() : 0;
        if (frames > 0)
        {
            constexpr int buckets = 1024;
            peaks.resize(buckets);
            for (int b = 0; b < buckets; ++b)
            {
                const int from = static_cast<int>(static_cast<long long>(frames) * b / buckets);
                const int to = std::max(
                    from + 1, static_cast<int>(static_cast<long long>(frames) * (b + 1) / buckets));
                float low = 0.0f, high = 0.0f;
                for (const auto& channel : sample->channels)
                    for (int i = from; i < to && i < frames; ++i)
                    {
                        low = std::min(low, channel[static_cast<std::size_t>(i)]);
                        high = std::max(high, channel[static_cast<std::size_t>(i)]);
                    }
                peaks[static_cast<std::size_t>(b)] = {low, high};
            }
        }
        position = -1.0;
        repaint();
    }

    void setPosition(double frame)
    {
        if (std::abs(frame - position) >= 1.0) // repaint only when it moved a frame or more
        {
            position = frame;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        const auto& t = currentTheme();
        auto area = getLocalBounds().toFloat().reduced(6.0f, 4.0f);
        g.setColour(colour(t.colours.display));
        g.fillRoundedRectangle(area, 4.0f);
        if (peaks.empty())
        {
            g.setColour(colour(t.colours.textDim));
            g.setFont(juce::FontOptions(12.0f));
            g.drawText("Click a sample to hear it", area, juce::Justification::centred);
            return;
        }
        area = area.reduced(4.0f, 4.0f);
        const float mid = area.getCentreY();
        const float half = area.getHeight() / 2.0f;
        g.setColour(colour(t.colours.accentAlt));
        const int width = static_cast<int>(area.getWidth());
        for (int x = 0; x < width; ++x)
        {
            const auto& [low, high] =
                peaks[static_cast<std::size_t>(x * static_cast<int>(peaks.size()) / width)];
            g.fillRect(area.getX() + x, mid - high * half, 1.0f,
                       std::max(1.0f, (high - low) * half));
        }
        if (position >= 0.0 && frames > 0)
        {
            g.setColour(colour(t.colours.playhead));
            g.fillRect(area.getX() + static_cast<float>(position / frames) * area.getWidth(),
                       area.getY(), 1.5f, area.getHeight());
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (frames > 0 && onClick)
            onClick(std::clamp((e.position.x - 10.0) / std::max(1.0, getWidth() - 20.0), 0.0, 1.0));
    }

  private:
    std::vector<std::pair<float, float>> peaks;
    int frames = 0;
    double position = -1.0;
};

// ------------------------------------------------------------------------------ BrowserPanel

BrowserPanel::BrowserPanel(engine::PreviewPlayer& p, juce::PropertiesFile& s)
    : preview(p), settings(s), waveform(std::make_unique<Waveform>())
{
    right = settings.getBoolValue("browserOnRight", false);
    folders = juce::StringArray::fromLines(settings.getValue("browserFolders"));
    folders.removeEmptyStrings();

    addButton.setTooltip("Add a folder to the browser");
    addButton.onClick = [this] { chooseFolderToAdd(); };
    sideButton.setTooltip("Move the browser to the other side");
    sideButton.onClick = [this] { setOnRight(!right); };
    closeButton.setTooltip("Hide the browser");
    closeButton.onClick = [this]
    {
        if (onClose)
            onClose();
    };
    for (auto* b : {&addButton, &sideButton, &closeButton, &playButton})
    {
        b->setWantsKeyboardFocus(false);
        b->setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(b);
    }
    sideButton.setButtonText(right ? "<" : ">");

    tree.setRootItemVisible(false);
    tree.setDefaultOpenness(false);
    tree.setWantsKeyboardFocus(false);
    tree.setColour(juce::TreeView::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(tree);
    rebuildTree();

    waveform->setTooltip("Click to play from there");
    waveform->onClick = [this](double fraction)
    {
        if (current != nullptr)
            startPreview(fraction * current->numFrames(), true);
    };
    addAndMakeVisible(*waveform);

    playButton.setTooltip("Play / pause the whole sample");
    playButton.onClick = [this] { playPause(); };
    nameLabel.setFont(juce::FontOptions(12.0f));
    nameLabel.setMinimumHorizontalScale(0.7f);
    addAndMakeVisible(nameLabel);
    startTimerHz(30);
}

BrowserPanel::~BrowserPanel()
{
    loaderThread.removeAllJobs(true, 4000);
    tree.setRootItem(nullptr);
}

void BrowserPanel::setOnRight(bool shouldBeOnRight)
{
    right = shouldBeOnRight;
    settings.setValue("browserOnRight", right);
    sideButton.setButtonText(right ? "<" : ">");
    if (onSideChanged)
        onSideChanged();
}

void BrowserPanel::moveSelection(int delta, bool load)
{
    const int rows = tree.getNumRowsInTree();
    if (rows == 0)
        return;
    auto* selected = tree.getSelectedItem(0);
    const int row = selected != nullptr ? selected->getRowNumberInTree() : (delta > 0 ? -1 : rows);
    auto* item = tree.getItemOnRow(std::clamp(row + delta, 0, rows - 1));
    if (item == nullptr || item == selected)
        return;
    item->setSelected(true, true);
    tree.scrollToKeepItemVisible(item);
    // Landing on a sample previews it, as clicking would.
    if (auto* entry = dynamic_cast<Item*>(item);
        entry != nullptr && !entry->getFile().isDirectory())
    {
        select(entry->getFile(), false);
        if (load && onUseSample)
            onUseSample(entry->getFile());
    }
}

void BrowserPanel::openSelected(bool open)
{
    auto* item = tree.getSelectedItem(0);
    if (item == nullptr)
        return;
    if (item->mightContainSubItems() && item->isOpen() != open)
        item->setOpen(open);
    else if (!open && item->getParentItem() != nullptr && item->getParentItem() != treeRoot.get())
    {
        // Left on a file or a closed folder: go up to the folder it's in.
        item->getParentItem()->setSelected(true, true);
        tree.scrollToKeepItemVisible(item->getParentItem());
    }
}

void BrowserPanel::confirmSelected()
{
    auto* entry = dynamic_cast<Item*>(tree.getSelectedItem(0));
    if (entry == nullptr)
        return;
    if (entry->getFile().isDirectory())
        entry->setOpen(!entry->isOpen());
    else if (onUseSample)
        onUseSample(entry->getFile());
}

void BrowserPanel::chooseFolderToAdd()
{
    chooser = std::make_unique<juce::FileChooser>(
        "Add a folder to the browser",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory));
    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectDirectories,
                         [this](const juce::FileChooser& fc)
                         {
                             const auto folder = fc.getResult();
                             if (!folder.isDirectory())
                                 return;
                             folders.addIfNotAlreadyThere(folder.getFullPathName());
                             saveFolders();
                             rebuildTree();
                         });
}

void BrowserPanel::removeFolder(const juce::File& folder)
{
    folders.removeString(folder.getFullPathName());
    saveFolders();
    // Rebuild after the click that asked for it has finished (it came from a tree item).
    juce::Component::SafePointer<BrowserPanel> self(this);
    juce::MessageManager::callAsync(
        [self]
        {
            if (self != nullptr)
                self->rebuildTree();
        });
}

void BrowserPanel::saveFolders()
{
    settings.setValue("browserFolders", folders.joinIntoString("\n"));
}

void BrowserPanel::rebuildTree()
{
    tree.setRootItem(nullptr);
    treeRoot = std::make_unique<FolderList>();
    for (const auto& path : folders)
        if (const juce::File folder(path); folder.isDirectory())
            treeRoot->addSubItem(new Item(*this, folder, true));
    tree.setRootItem(treeRoot.get());
}

void BrowserPanel::select(const juce::File& file, bool whole)
{
    if (file == currentFile && current != nullptr)
    {
        startPreview(0.0, whole);
        return;
    }
    // Decode in the background; the newest request wins.
    setCurrent(nullptr);
    currentFile = file;
    nameLabel.setText(file.getFileName() + "  (loading...)", juce::dontSendNotification);
    playWhole = whole;
    pendingPlay = true;
    const int request = ++generation;
    juce::Component::SafePointer<BrowserPanel> self(this);
    loaderThread.addJob(
        [this, self, file, request]
        {
            if (generation.load() != request)
                return juce::ThreadPoolJob::jobHasFinished; // superseded
            auto sample = loader.load(file);
            juce::MessageManager::callAsync(
                [self, file, sample, request]
                {
                    if (self != nullptr)
                        self->loaded(file, sample, request);
                });
            return juce::ThreadPoolJob::jobHasFinished;
        });
}

void BrowserPanel::loaded(const juce::File& file,
                          std::shared_ptr<const engine::SampleBuffer> sample, int request)
{
    if (request != generation.load() || file != currentFile)
        return;
    if (sample == nullptr)
    {
        nameLabel.setText(file.getFileName() + "  (can't read)", juce::dontSendNotification);
        return;
    }
    setCurrent(std::move(sample));
    nameLabel.setText(file.getFileName() + "  " +
                          formatSeconds(current->numFrames() / current->sampleRate),
                      juce::dontSendNotification);
    if (pendingPlay)
        startPreview(0.0, playWhole);
    pendingPlay = false;
}

void BrowserPanel::setCurrent(std::shared_ptr<const engine::SampleBuffer> sample)
{
    // Whatever the preview was playing stays alive until the audio thread has let go of it.
    const auto stopped = preview.stop();
    if (current != nullptr)
        retired.emplace_back(stopped, std::move(current));
    current = std::move(sample);
    pausedAt = 0.0;
    waveform->setSample(current.get());
}

void BrowserPanel::startPreview(double startFrame, bool whole)
{
    if (current == nullptr)
        return;
    const double length = current->numFrames();
    const double limit = (previewSeconds ? previewSeconds() : 5.0) * current->sampleRate;
    preview.play(current.get(), startFrame, whole ? length : std::min(length, startFrame + limit));
}

void BrowserPanel::playPause()
{
    if (current == nullptr)
        return;
    if (preview.isPlaying())
    {
        pausedAt = preview.position();
        preview.stop();
        return;
    }
    startPreview(pausedAt < current->numFrames() - 1 ? pausedAt : 0.0, true);
}

void BrowserPanel::timerCallback()
{
    const auto done = preview.consumed();
    std::erase_if(retired, [done](const auto& entry) { return entry.first <= done; });

    const bool playing = preview.isPlaying() && current != nullptr;
    playButton.setButtonText(playing ? "Pause" : "Play");
    waveform->setPosition(playing ? preview.position() : (pausedAt > 0.0 ? pausedAt : -1.0));
}

void BrowserPanel::paint(juce::Graphics& g)
{
    const auto& t = currentTheme();
    g.fillAll(colour(t.colours.panel));
    auto header = getLocalBounds().removeFromTop(headerHeight);
    g.setColour(colour(t.colours.header));
    g.fillRect(header);
    g.setColour(colour(t.colours.text));
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("Browser", header.reduced(10, 0), juce::Justification::centredLeft);
    if (folders.isEmpty())
    {
        g.setColour(colour(t.colours.textDim));
        g.setFont(juce::FontOptions(12.0f));
        g.drawFittedText("Add a folder with + to browse its samples.", tree.getBounds().reduced(12),
                         juce::Justification::centredTop, 3);
    }
    // Separator on the side facing the rest of the app.
    g.setColour(colour(t.colours.outline));
    g.fillRect(right ? 0 : getWidth() - 1, 0, 1, getHeight());
}

void BrowserPanel::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop(headerHeight).reduced(4);
    closeButton.setBounds(header.removeFromRight(24));
    header.removeFromRight(2);
    sideButton.setBounds(header.removeFromRight(24));
    header.removeFromRight(2);
    addButton.setBounds(header.removeFromRight(24));

    auto footer = area.removeFromBottom(footerHeight).reduced(6, 3);
    playButton.setBounds(footer.removeFromLeft(56));
    footer.removeFromLeft(6);
    nameLabel.setBounds(footer);
    waveform->setBounds(area.removeFromBottom(waveformHeight));
    tree.setBounds(area.reduced(2));
}

} // namespace jls::ui
