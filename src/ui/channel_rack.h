#pragma once

#include "engine/transport.h"
#include "io/sample_loader.h"
#include "model/document.h"

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

// The row of step buttons. Left-click toggles a step and dragging paints that value across
// steps; right-click (and drag) erases, as in FL.
class StepGrid final : public juce::Component
{
  public:
    void setSteps(std::vector<bool> newSteps);
    void setPlayingStep(int step);

    // (step, on, gestureId): one gesture per mouse press so a drag undoes as one step.
    std::function<void(int, bool, int)> onStepPainted;
    std::function<int()> newGestureId;

    static int widthFor(int numSteps);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

  private:
    static int stepX(int step);
    int stepAt(int x) const;
    void paintStep(int step);

    std::vector<bool> steps;
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
    void update(const model::Channel& channel, const model::Pattern& pattern);
    void setPlayingStep(int step) { steps.setPlayingStep(step); }

    void resized() override;

  private:
    void showMenu();

    ChannelRack& rack;
    model::ChannelId id;
    LedButton mute;
    juce::Slider pan;
    juce::Slider volume;
    juce::TextButton name;
    StepGrid steps;
    int knobGesture = 0;
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

    // Used by rows.
    model::Document& document() { return doc; }
    void edit(const std::function<void(model::Project&)>& change, int gestureId = 0);
    void chooseSampleFor(model::ChannelId id);
    void rename(model::ChannelId id);

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
    ChannelRow* rowAt(int y);

    model::Document& doc;
    engine::Transport& transport;
    io::SampleLoader& loader;

    std::vector<std::unique_ptr<ChannelRow>> rows;
    juce::TextButton addButton{"+"};
    juce::TextButton closeButton{juce::String::fromUTF8("\xc3\x97")}; // multiplication sign
    juce::ComponentDragger dragger;
    juce::ComponentBoundsConstrainer dragBounds;
    std::unique_ptr<juce::FileChooser> chooser;
    int dropRow = -1; // row under a file drag, or -1 for "new channel"
    bool dragging = false;
    int lastPlayingStep = -2;
};

} // namespace jls::ui
