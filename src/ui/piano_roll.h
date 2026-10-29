#pragma once

// The Piano Roll: edits the notes of the selected channel in the current pattern. Made of a
// scrollable NoteGrid plus a keyboard, a ruler and a velocity lane that follow its scrolling.

#include "engine/transport.h"
#include "model/clipboard.h"
#include "model/document.h"
#include "ui/edit_action.h"

#include "ui/controls.h"
#include "ui/theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace jls::ui
{

class PianoRollView;

// The note grid. Mouse, as in FL:
//   left-click empty space  add a note (same length as the last one), then drag to move it
//                           (the note sounds while the button is held, at its current pitch);
//                           with a selection, the click only deselects
//   click a note            select it; dragging then moves it in time and pitch (snapped)
//   drag a note's right end resize it (new notes then use that length)
//   Shift + click a note    add it to the selection, or take it out
//   right-click / drag      deselect and delete notes
//   Ctrl + drag             select the notes in a rectangle; drag a selected note to move them
//                           all (its right end resizes them all)
// The keyboard edits the selection through PianoRollView::edit(): Delete, Ctrl+A, Esc,
// Ctrl+C / X / V, Ctrl+B (duplicate), Shift + arrows (move), Shift+D (shorten to a step).
//   Ctrl + mouse wheel      zoom horizontally
class NoteGrid final : public juce::Component
{
  public:
    explicit NoteGrid(PianoRollView& owner);

    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

  private:
    enum class Drag
    {
        none,
        move,
        resize,
        erase,
        select,          // Ctrl + drag: rubber band
        moveSelection,   // dragging one of the selected notes moves them all
        resizeSelection, // dragging a selected note's right end resizes them all
    };

    int noteAt(juce::Point<float> p) const; // index into the channel's notes, or -1
    bool onRightEdge(int index, float x) const;
    void eraseAt(juce::Point<float> p);
    juce::Rectangle<float> noteBounds(const model::Note& note) const;

    PianoRollView& view;
    Drag drag = Drag::none;
    int dragNote = -1;
    int grabOffsetTicks = 0;
    int grabPitch = 0;
    int gestureId = 0;
    juce::Point<float> bandStart, bandEnd;
    std::vector<model::Note> moveOriginals; // the selected notes when the drag started
    int grabTick = 0;
    int grabNotePitch = 0; // pitch of the note that was grabbed
};

// Piano Roll panel: header, keyboard, ruler, note grid and velocity lane. Edits the notes of
// the selected channel in the current pattern.
class PianoRollView final : public juce::Component, private juce::Timer
{
  public:
    static constexpr int rowHeight = 14;
    static constexpr int numPitches = 128;
    static constexpr int headerHeight = metrics::panelHeaderHeight;
    static constexpr int rulerHeight = 22;
    static constexpr int keyboardWidth = 64;
    static constexpr int velocityHeight = 72;

    PianoRollView(model::Document& document, engine::Transport& transport);
    ~PianoRollView() override;

    // Re-reads the document. Call after every document change.
    void refresh();
    // Scrolls so the channel's notes (or C5 when there are none) are in view.
    void scrollToNotes();

    // Geometry shared by the parts (grid coordinates).
    float xForTick(double tick) const { return static_cast<float>(tick) * zoom; }
    int tickAtX(float x) const;
    float yForPitch(int pitch) const
    {
        return static_cast<float>((numPitches - 1 - pitch) * rowHeight);
    }
    int pitchAtY(float y) const;
    int snapTicks() const;
    int snap(double tick) const; // nearest snap point
    int snapDown(double tick) const;
    double playheadTick() const; // within the pattern, or -1 when not shown
    // Where the playhead is drawn (grid x, -1 = hidden): see PlaylistView::shownPlayheadX().
    float shownPlayheadX() const { return lastPlayheadX; }

    model::Document& document() { return doc; }
    model::ChannelId channel() const { return doc.project().selectedChannel; }
    const std::vector<model::Note>& notes() const;
    // Applies `change` to the selected channel's notes in the current pattern.
    void editNotes(const std::function<void(std::vector<model::Note>&)>& change, int gestureId);

    int newNoteLength = model::ticksPerStep; // the last length used, as in FL

    // Selected notes (indices into notes()). View-only; cleared when the channel or pattern
    // changes, and pruned when notes disappear (e.g. undo).
    std::vector<int> selection;
    bool isSelected(int note) const;
    void clearSelection();
    // Performs an editing action on the selection (or all notes / clips); returns false if
    // there was nothing to do.
    bool edit(EditAction action);

    // Controls the owner decorates with shortcut hints (tooltips).
    juce::Button& sliceControl() { return sliceButton; }
    juce::ComboBox& sliceSizeControl() { return sliceBox; }
    juce::ComboBox& snapControl() { return snapBox; }

    // Snap one notch finer (+1) or coarser (-1), for keyboard shortcuts.
    void stepSnap(int finer);
    // Slice size one notch smaller (+1) or larger (-1).
    void stepSliceSize(int smaller);
    // The slicer: cuts the selected notes (all of the channel's notes when none are selected)
    // into pieces of the chosen size (slice box), keeping the pieces selected.
    void sliceNotes();

    // Audition: plays `pitch` on the channel until stopAudition() (or another pitch). Clicking
    // notes and keyboard keys uses it; the owner connects it to the engine.
    void audition(int pitch, float velocity);
    void stopAudition();
    int auditionPitch = -1; // sounding now, or -1

    // The key under the mouse, on the keyboard or in its row of the grid: the keyboard shows its
    // name (only C and the scale's root are labelled otherwise). -1 for none.
    void setHoverPitch(int pitch);
    int hoverPitch = -1;
    // Set by the owner (Settings > Piano roll keys): dark keys in the panel colours instead of
    // ivory ones.
    std::function<bool()> darkKeys;
    std::function<void(int pitch, float velocity)> onAuditionStart;
    std::function<void(int pitch)> onAuditionStop;

    void setZoom(float pixelsPerTick, float anchorGridX);
    float pixelsPerTick() const { return zoom; }

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override { paintCardFrame(g, *this); }
    void resized() override;

  private:
    class Keyboard;
    class Ruler;
    class VelocityLane;
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
    float zoom = 16.0f / model::ticksPerStep; // a step is 16 px

    juce::String subtitle;              // the channel and pattern, after the title
    juce::Rectangle<int> snapLabelArea; // "Snap" and "Scale", painted
    juce::Rectangle<int> scaleLabelArea;
    juce::ComboBox snapBox;
    IconButton sliceButton{Icon::slice, "Slice"};
    juce::ComboBox sliceBox; // piece size, as an id in ticks
    juce::ComboBox rootBox;  // C .. B
    juce::ComboBox scaleBox; // Off / Major / Minor
    std::unique_ptr<Keyboard> keyboard;
    std::unique_ptr<Ruler> ruler;
    std::unique_ptr<VelocityLane> velocityLane;
    SyncedViewport viewport;
    NoteGrid grid;
    float lastPlayheadX = -1.0f;
    model::NoteClipboard clipboard;        // Ctrl+C / Ctrl+V, also across channels and patterns
    model::ChannelId selectionChannel = 0; // what the selection belongs to
    model::PatternId selectionPattern = 0;
    model::ChannelId scrolledFor = 0;
};

// FL's note names: C5 is MIDI note 60.
juce::String noteName(int pitch);

} // namespace jls::ui
