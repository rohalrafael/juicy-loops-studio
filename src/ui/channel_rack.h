#pragma once

// The Channel Rack: a floating panel with one row per channel (mute, pan, volume, mixer track,
// name, steps or a note preview). A channel plays a sample or a plugin instrument. Edits go
// through the Document so they are undoable.

#include "engine/transport.h"
#include "io/sample_loader.h"
#include "model/document.h"
#include "ui/plugin_menu.h"
#include "ui/theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

namespace jls::ui
{

class ChannelRack;

// Mute toggle drawn as an LED.
class LedButton final : public juce::Component, public juce::SettableTooltipClient
{
  public:
    void setOn(bool shouldBeOn);
    std::function<void()> onClick;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent&) override;

  private:
    bool on = true;
};

// A channel's loop switch, drawn as a small circular arrow: lit = the channel repeats its notes
// when the pattern is longer than they are. Dimmed while the rack's Loop switch is off.
class LoopToggle final : public juce::Component, public juce::SettableTooltipClient
{
  public:
    void setState(bool on, bool active);
    std::function<void()> onClick;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent&) override;

  private:
    bool on = true;
    bool active = true;
};

// The handle right of the last step column: drag it sideways to make the pattern longer or
// shorter, in groups of 4 steps.
class LengthHandle final : public juce::Component, public juce::SettableTooltipClient
{
  public:
    LengthHandle();
    // (new minimum length in steps, gesture id)
    std::function<void(int, int)> onLengthChanged;
    std::function<int()> currentLength;
    std::function<int()> groupWidth; // pixels per group of 4 steps
    std::function<int()> newGestureId;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

  private:
    int startLength = 16;
    int startGroupWidth = 1;
    int gestureId = 0;
};

// A channel's mixer track number ("--" = Master). Drag up/down or scroll to change it.
class InsertBox final : public juce::Component, public juce::SettableTooltipClient
{
  public:
    void setValue(int track);
    // (new track, gesture id)
    std::function<void(int, int)> onChange;
    std::function<int()> newGestureId;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

  private:
    void change(int track, int gesture);

    int value = 0;
    int dragStartValue = 0;
    int gestureId = 0;
};

// The row of step buttons. Left-click toggles a step and dragging paints that value across
// steps; right-click (and drag) erases, as in FL.
//
// A looping channel shows its repeats greyed from `loopStart` on; clicking one makes the repeats
// up to there real first (see ChannelRow), so what you hear doesn't change.
class StepGrid final : public juce::Component
{
  public:
    // `loopStart`: first step that is a repeat (greyed), or -1 when the channel doesn't loop.
    void setSteps(std::vector<bool> newSteps, int loopStart = -1);
    // Melodic channel: show its notes instead of editable steps; a click calls onOpenNotes.
    void setMelodicPreview(std::vector<model::Note> notes, int lengthInSteps, int loopStart = -1);
    void setPlayingStep(int step);
    // Step button width in pixels (the rack shrinks steps to fit long patterns).
    void setStepWidth(int width);
    std::function<void()> onOpenNotes;

    // (step, on, gestureId): one gesture per mouse press so a drag undoes as one step.
    std::function<void(int, bool, int)> onStepPainted;
    std::function<int()> newGestureId;

    static int widthFor(int numSteps, int stepWidth = metrics::stepWidth);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

  private:
    static int stepX(int step, int stepWidth);
    int stepX(int step) const { return stepX(step, stepWidth); }
    int stepAt(int x) const;
    void paintStep(int step);

    std::vector<bool> steps;
    int loopStart = -1;
    int stepWidth = metrics::stepWidth;
    std::vector<model::Note> melodicNotes;
    bool melodic = false;
    int playingStep = -1;
    bool paintValue = true;
    int gestureId = 0;
    int lastPainted = -1;
};

class ChannelRow final : public juce::Component
{
  public:
    ChannelRow(ChannelRack& owner, model::ChannelId id);

    model::ChannelId channelId() const { return id; }
    void update(const model::Channel& channel, const model::Project& project);
    void setPlayingStep(int step) { steps.setPlayingStep(step); }
    void setStepWidth(int width) { steps.setStepWidth(width); }

    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

  private:
    void showMenu();

    ChannelRack& rack;
    model::ChannelId id;
    LedButton mute;
    juce::Slider pan;
    juce::Slider volume;
    InsertBox insert;
    juce::TextButton name;
    LoopToggle loop;
    StepGrid steps;
    int knobGesture = 0;
    bool isInstrument = false; // plays a plugin instead of a sample
};

// FL-style Channel Rack, a floating panel (drag its title bar): one row per channel with mute, pan,
// volume, name and step buttons. Audio files can be dropped on a row (replace its sample) or
// anywhere else (new channel).
class ChannelRack final : public juce::Component,
                          public juce::FileDragAndDropTarget,
                          private juce::Timer
{
  public:
    ChannelRack(model::Document& document, engine::Transport& transport, io::SampleLoader& loader);

    // Re-reads the document. Call after every document change.
    void refresh();

    int preferredWidth() const;
    int preferredHeight() const;
    // Step width that fits the current pattern into the space right of the rack's position.
    int stepWidth() const;

    // Used by rows.
    model::Document& document() { return doc; }
    void edit(const std::function<void(model::Project&)>& change, int gestureId = 0);
    void chooseSampleFor(model::ChannelId id);
    void rename(model::ChannelId id);
    void select(model::ChannelId id);
    void openPianoRoll(model::ChannelId id);
    // Menu ids firstInstrumentId + i pick listInstruments()[i]; see handleInstrumentMenu().
    static constexpr int manageInstrumentsId = 999;
    static constexpr int firstInstrumentId = 1000;
    juce::PopupMenu instrumentMenu(std::vector<PluginItem>& items) const;
    // Handles a result from instrumentMenu(): makes `id` play that plugin (0 = new channel),
    // or opens Manage plugins. Returns false if `result` wasn't one of its items.
    bool handleInstrumentMenu(int result, model::ChannelId id,
                              const std::vector<PluginItem>& items);
    void showAddMenu();

    // Set by the owner: show the Piano Roll for a channel.
    std::function<void(model::ChannelId)> onOpenPianoRoll;
    // Set by the owner: known instrument plugins, the "Manage plugins" action, and showing a
    // channel's plugin window (`toggle`: hide it if it's already open, as clicking does in FL).
    std::function<std::vector<PluginItem>()> listInstruments;
    std::function<void()> onManagePlugins;
    std::function<void(model::ChannelId, bool toggle)> onShowInstrument;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragMove(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

  private:
    void timerCallback() override;
    void addChannelsFromFiles(const juce::Array<juce::File>& files);
    void replaceSample(model::ChannelId id, const juce::File& file);
    ChannelRow* rowAt(int y);

    model::Document& doc;
    engine::Transport& transport;
    io::SampleLoader& loader;

    std::vector<std::unique_ptr<ChannelRow>> rows;
    juce::TextButton addButton{"+"};
    juce::TextButton loopButton{"Loop"}; // Project::loopChannels
    juce::ComboBox lengthBox;            // pattern length: 16 / 32 / 64 steps
    LengthHandle lengthHandle;
    juce::TextButton closeButton{juce::String::fromUTF8("\xc3\x97")}; // multiplication sign
    juce::ComponentDragger dragger;
    juce::ComponentBoundsConstrainer dragBounds;
    std::unique_ptr<juce::FileChooser> chooser;
    int dropRow = -1; // row under a file drag, or -1 for "new channel"
    bool dragging = false;
    int lastPlayingStep = -2;
};

} // namespace jls::ui
