#pragma once

// The Playlist: arranges pattern clips on tracks for song mode. A scrollable PlaylistGrid plus a
// bar ruler and track names that follow its scrolling.

#include "engine/transport.h"
#include "model/clipboard.h"
#include "model/document.h"
#include "ui/edit_action.h"
#include "ui/waveform.h"

#include "engine/sample_buffer.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace jls::ui
{

class PlaylistView;

// The scrollable lanes with pattern clips. Mouse, as in FL:
//   left-click empty space  place the selected pattern (snapped), then drag to move it
//   drag a clip             move it (snapped), also to other tracks
//   drag a clip's right end resize it (a longer clip repeats its pattern)
//   right-click / drag      delete clips
//   Ctrl + drag             select the clips in a rectangle; drag a selected clip to move them
//                           all; Delete removes them, Ctrl+A selects all, Esc deselects;
//                           Ctrl+C / X copy / cut, Ctrl+V pastes after the copied bars
//   Ctrl + mouse wheel      zoom horizontally
class PlaylistGrid final : public juce::Component,
                           public juce::FileDragAndDropTarget, // samples from the desktop
                           public juce::DragAndDropTarget      // Browser samples, picker items
{
  public:
    explicit PlaylistGrid(PlaylistView& owner);

    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragMove(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragMove(const SourceDetails& details) override;
    void itemDragExit(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;

  private:
    void setDropPoint(juce::Point<int> point); // where a drag would land; (-1, -1) = none
    enum class Drag
    {
        none,
        move,
        resize,
        erase,
        select,        // Ctrl + drag: rubber band
        moveSelection, // dragging one of several selected clips moves them all
        trimStart,     // dragging an audio clip's left edge: start later in the sample
    };

    int clipAt(juce::Point<float> p) const; // index into playlist.clips, or -1
    bool onRightEdge(int clipIndex, float x) const;
    bool onLeftEdge(int clipIndex, float x) const; // audio clips only: trims the start
    void eraseAt(juce::Point<float> p);
    juce::Rectangle<float> clipBounds(const model::Clip& clip) const;
    void paintClip(juce::Graphics& g, const model::Clip& clip, bool selected) const;

    PlaylistView& view;
    Drag drag = Drag::none;
    int dragClip = -1;
    int grabOffsetSteps = 0;
    int gestureId = 0;
    juce::Point<float> bandStart, bandEnd;
    std::vector<model::Clip> moveOriginals; // the selected clips when the drag started
    int grabStep = 0;
    int grabTrack = 0;
    juce::Point<int> dropPoint{-1, -1};
};

// Playlist panel: header with snap setting, bar ruler, track names and the grid.
class PlaylistView final : public juce::Component, private juce::Timer
{
  public:
    static constexpr int trackHeight = 34;
    static constexpr int rulerHeight = 22;
    static constexpr int headerHeight = 32;
    static constexpr int trackNameWidth = 110;

    PlaylistView(model::Document& document, engine::Transport& transport);
    ~PlaylistView() override;

    // Re-reads the document. Call after every document change.
    void refresh();

    // Geometry shared by the parts.
    float pixelsPerStep() const { return zoom; }
    int snapSteps() const;
    juce::ComboBox& snapControl() { return snapBox; } // for its shortcut hint

    // Snap one notch finer (+1) or coarser (-1), for keyboard shortcuts.
    void stepSnap(int finer);
    // Moves the playhead to ruler position x (snapped) and switches to song mode.
    void seekToX(float x);

    // Selected clips (indices into playlist.clips). View-only; cleared when it can't be valid.
    std::vector<int> selection;
    bool isSelected(int clip) const;
    void clearSelection();
    // Performs an editing action on the selection (or all notes / clips); returns false if
    // there was nothing to do.
    bool edit(EditAction action);
    int stepAtX(float x) const; // grid coordinates
    float xForStep(double step) const { return static_cast<float>(step) * zoom; }
    int trackAtY(float y) const;
    // Playhead position in steps, or -1 when it shouldn't be shown.
    double playheadStep() const;

    model::Document& document() { return doc; }

    // Audio clips. `loadSample` (set by the owner) decodes a file.
    std::function<std::shared_ptr<const engine::SampleBuffer>(const juce::File&)> loadSample;
    // Places something at grid position (x, y): "file:<path>" (a sample: becomes an audio clip),
    // "pattern:<id>" or "audio:<channel id>" (from the picker).
    void placeAt(const juce::String& what, float x, float y);
    // Steps an audio clip needs to hold all of its channel's sample at the current tempo.
    int stepsFor(const model::Channel& channel) const;
    const Peaks& peaksFor(const engine::SampleBuffer& sample);
    // What clicking empty space places: an audio-clip channel, or 0 = the selected pattern.
    model::ChannelId brushAudio = 0;
    void setPickerVisible(bool shouldShow);
    bool isPickerVisible() const;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setZoom(float pixelsPerStep, float anchorGridX);

  private:
    class Ruler;
    class TrackNames;
    class Picker;
    class SyncedViewport : public juce::Viewport
    {
      public:
        std::function<void()> onScroll;
        void visibleAreaChanged(const juce::Rectangle<int>&) override
        {
            if (onScroll)
                onScroll();
        }
    };

    void timerCallback() override;
    void updateGridSize();

    model::Document& doc;
    engine::Transport& transport;
    model::ClipClipboard clipboard;                         // Ctrl+C / Ctrl+V
    std::map<const engine::SampleBuffer*, Peaks> peakCache; // audio clips' waveforms
    std::unique_ptr<Picker> picker;
    juce::TextButton pickerButton{"Picker"};
    float zoom = 4.0f; // pixels per step: a 4/4 bar is 64 px

    juce::Label title{{}, "Playlist"};
    juce::Label snapLabel{{}, "Snap"};
    juce::ComboBox snapBox;
    std::unique_ptr<Ruler> ruler;
    std::unique_ptr<TrackNames> trackNames;
    SyncedViewport viewport;
    PlaylistGrid grid;
    float lastPlayheadX = -1.0f;
};

} // namespace jls::ui
