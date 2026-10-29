#pragma once

// The Channel Rack: a floating panel with one row per channel (mute, pan, volume, mixer track,
// name, steps or a note preview). A channel plays a sample or a plugin instrument. Edits go
// through the Document so they are undoable.

#include "engine/transport.h"
#include "io/sample_loader.h"
#include "model/document.h"
#include "ui/controls.h"
#include "ui/plugin_menu.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace jls::ui
{

class ChannelRack;

// Mute toggle drawn as an LED. Ctrl+click or right-click asks for solo instead (as in FL).
class LedButton final : public juce::Component, public juce::SettableTooltipClient
{
  public:
    void setOn(bool shouldBeOn);
    std::function<void(bool solo)> onClick;

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
// A looping channel shows its repeats greyed from `loopStart` on. Left-clicking one sets that
// step (a looped note stays). When the loop is copied in (copiesLoop), the repeats up to there
// become real first (see ChannelRow), so what you hear doesn't change, and right-click (and drag)
// removes repeats that have a note; empty ones stay looped. Otherwise, as in FL, right-click and
// erasing leave greyed steps alone.
class StepGrid final : public juce::Component
{
  public:
    // `loopStart`: first step that is a repeat (greyed), or -1 when the channel doesn't loop.
    void setSteps(std::vector<bool> newSteps, int loopStart = -1);
    // Melodic channel: show its notes instead of editable steps; a click calls onOpenNotes.
    void setMelodicPreview(std::vector<model::Note> notes, int lengthInSteps, int loopStart = -1);
    void setPlayingStep(int step);
    std::function<void()> onOpenNotes;

    // (step, on, gestureId): one gesture per mouse press so a drag undoes as one step.
    std::function<void(int, bool, int)> onStepPainted;
    std::function<int()> newGestureId;
    // Whether clicking a greyed (looped) step copies the loop in first (Settings); if not, as in
    // FL, right-click and erasing leave greyed steps alone.
    std::function<bool()> copiesLoop;

    // Width of `numSteps` step keys with their gaps. Steps are always full size: a long pattern
    // makes the rack wider, past the window's edge if need be (drag it to see the rest).
    static int widthFor(int numSteps);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

  private:
    static int stepX(int step);
    int numSteps() const { return static_cast<int>(steps.size()); }
    int stepAt(int x) const;
    bool isRepeat(int step) const { return loopStart >= 0 && step >= loopStart; }
    void paintStepKey(juce::Graphics& g, int step) const;
    void paintNotePreview(juce::Graphics& g) const;
    // Applies the press's paint value to `step`, once per step and press.
    void paintStep(int step);

    std::vector<bool> steps;
    int loopStart = -1;
    std::vector<model::Note> melodicNotes;
    bool melodic = false;
    int playingStep = -1;
    bool paintValue = true; // what the current press sets steps to (see mouseDown)
    int gestureId = 0;
    int lastPainted = -1; // the step the current press painted last
};

// One channel's row in the rack, left to right: mute LED, pan and volume knobs, mixer track,
// name button, loop switch and the steps. Its edits go through the rack.
class ChannelRow final : public juce::Component
{
  public:
    ChannelRow(ChannelRack& owner, model::ChannelId id);

    model::ChannelId channelId() const { return id; }
    void update(const model::Channel& channel, const model::Project& project);
    void setPlayingStep(int step) { steps.setPlayingStep(step); }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

  private:
    void setUpMute();
    void setUpKnob(juce::Slider& knob, double min, double max, double reset,
                   std::function<void(model::Channel&, float)> apply);
    void setUpKnobs();
    void setUpInsert();
    void setUpName();
    void setUpLoop();
    void setUpSteps();
    // Applies `change` to this row's channel as an undoable edit.
    void editChannel(const std::function<void(model::Channel&)>& change, int gestureId = 0);
    // Sets a step clicked or dragged over in the steps (see StepGrid), as an undoable edit.
    void setStep(int step, bool on, int gestureId);
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
    int knobGesture = 0;       // the gesture of the knob being dragged
    bool isInstrument = false; // plays a plugin instead of a sample
    bool selected = false;     // the selected channel: its row is highlighted
};

// FL-style Channel Rack, a floating panel (drag its title bar): one row per channel with mute, pan,
// volume, name and step buttons. Audio files can be dropped on a row (replace its sample) or
// anywhere else (new channel).
class ChannelRack final : public juce::Component,
                          public juce::FileDragAndDropTarget,
                          public juce::DragAndDropTarget, // files dragged from the Browser
                          private juce::Timer
{
  public:
    ChannelRack(model::Document& document, engine::Transport& transport, io::SampleLoader& loader);

    // Re-reads the document. Call after every document change.
    void refresh();

    int preferredWidth() const;
    int preferredHeight() const;

    // Used by rows.
    model::Document& document() { return doc; }
    void edit(const std::function<void(model::Project&)>& change, int gestureId = 0);
    void chooseSampleFor(model::ChannelId id);
    void rename(model::ChannelId id);
    void select(model::ChannelId id);
    // Ctrl+C / Ctrl+X / Ctrl+V with the rack focused: the selected channel's notes (steps too) in
    // the current pattern. Pasting replaces the selected channel's notes, also in another pattern
    // or channel, as in FL. Cut and paste are undoable.
    void copySelectedChannel();
    void cutSelectedChannel();
    void pasteIntoSelectedChannel();
    // The channels shown as rows: all but audio-clip channels, unless "Show audio clips" is on.
    std::vector<const model::Channel*> shownChannels() const;
    // Selects the channel `offset` rows below (negative: above) the selected one.
    void selectOffset(int offset);
    // Loads an audio file into the selected channel (a new channel if none is selected).
    void loadIntoSelected(const juce::File& file);
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

    // Controls the owner decorates with shortcut hints (tooltips).
    juce::Button& loopSwitch() { return loopButton; }
    juce::Button& lengthButton(int index) { return lengthButtons[static_cast<std::size_t>(index)]; }
    juce::Button& hideButton() { return closeButton; }

    // Set by the owner: show the Piano Roll for a channel.
    std::function<void(model::ChannelId)> onOpenPianoRoll;
    // Set by the owner: known instrument plugins, the "Manage plugins" action, and showing a
    // channel's plugin window (`toggle`: hide it if it's already open, as clicking does in FL).
    std::function<std::vector<PluginItem>()> listInstruments;
    std::function<void()> onManagePlugins;
    std::function<void(model::ChannelId, bool toggle)> onShowInstrument;
    // Set by the owner: show a sample channel's Sampler window (`toggle` as above).
    std::function<void(model::ChannelId, bool toggle)> onShowSampler;
    // Set by the owner (Settings > Clicking a looped step): true copies a channel's loop in before
    // a click on a greyed step, so nothing else changes; false (unset, as in FL) discards it.
    std::function<bool()> loopedClickCopies;
    // Set by the owner: copy a channel with its sound and settings, without notes (the owner
    // first collects the plugin's current state).
    std::function<void(model::ChannelId)> onDuplicateChannel;
    bool copiesLoopOnClick() const { return loopedClickCopies && loopedClickCopies(); }

    // Where the rack may be dragged (the owner's area below the transport bar). The title bar
    // always stays inside it, so the rack can't end up where it can't be grabbed again.
    void setMovableArea(juce::Rectangle<int> area);

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    // The header's controls follow the visible part of the rack (see resized()).
    void moved() override { resized(); }
    void parentSizeChanged() override { resized(); }
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragMove(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // Files dragged from the Browser (description "file:<path>") act like dropped files.
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragMove(const SourceDetails& details) override;
    void itemDragExit(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;

  private:
    // What copySelectedChannel() took (empty = nothing copied yet).
    std::optional<std::vector<model::Note>> channelClipboard;
    void setUpButtons();
    void setUpLengthHandle();
    // The pattern length shown (the 16 | 32 | 64 buttons and the length handle).
    void setGridSteps(int steps, int gestureId = 0);
    // The number of steps the rows show.
    int stepsShown() const;
    void timerCallback() override;
    void addChannelsFromFiles(const juce::Array<juce::File>& files);
    void replaceSample(model::ChannelId id, const juce::File& file);
    // The index of the row at `y`, or -1 if there's none.
    int rowIndexAt(int y) const;

    model::Document& doc;
    engine::Transport& transport;
    io::SampleLoader& loader;

    std::vector<std::unique_ptr<ChannelRow>> rows;
    IconButton addButton{Icon::add};
    IconButton loopButton{Icon::loop, "Loop"}; // Project::loopChannels
    // Steps shown: 16 | 32 | 64 (none lit after dragging the handle to another length).
    std::array<juce::TextButton, 3> lengthButtons{juce::TextButton{"16"}, juce::TextButton{"32"},
                                                  juce::TextButton{"64"}};
    LengthHandle lengthHandle;
    IconButton closeButton{Icon::close};
    juce::ComponentDragger dragger;
    juce::Rectangle<int> movableArea;
    bool movingRack = false; // the current mouse press started on the title bar
    std::unique_ptr<juce::FileChooser> chooser;
    bool dragOver = false;    // files are being dragged over the rack
    int dropRow = -1;         // the row under them, or -1 for "new channel"
    int lastPlayingStep = -2; // -2: none sent yet, so new rows get the playhead on the next tick
};

} // namespace jls::ui
