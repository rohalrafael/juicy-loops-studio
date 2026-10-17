#pragma once

// The Piano Roll: edits the notes of the selected channel in the current pattern. Made of a
// scrollable NoteGrid plus a keyboard, a ruler and a velocity lane that follow its scrolling.

#include "engine/transport.h"
#include "model/document.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace jls::ui
{

class PianoRollView;

// The note grid. Mouse, as in FL:
//   left-click empty space  add a note (same length as the last one), then drag to move it
//                           (the note sounds while the button is held, at its current pitch)
//   drag a note             move it in time and pitch (snapped)
//   drag a note's right end resize it (new notes then use that length)
//   right-click / drag      delete notes
//   Ctrl + mouse wheel      zoom horizontally
class NoteGrid final : public juce::Component
{
  public:
    explicit NoteGrid(PianoRollView& owner);

    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& e) override;
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
    };

    int noteAt(juce::Point<float> p) const; // index into the channel's notes, or -1
    bool onRightEdge(int index, float x) const;
    void eraseAt(juce::Point<float> p);

    PianoRollView& view;
    Drag drag = Drag::none;
    int dragNote = -1;
    int grabOffsetTicks = 0;
    int grabPitch = 0;
    int gestureId = 0;
};

// Piano Roll panel: header, keyboard, ruler, note grid and velocity lane. Edits the notes of
// the selected channel in the current pattern.
class PianoRollView final : public juce::Component, private juce::Timer
{
  public:
    static constexpr int rowHeight = 14;
    static constexpr int numPitches = 128;
    static constexpr int headerHeight = 32;
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

    model::Document& document() { return doc; }
    model::ChannelId channel() const { return doc.project().selectedChannel; }
    const std::vector<model::Note>& notes() const;
    // Applies `change` to the selected channel's notes in the current pattern.
    void editNotes(const std::function<void(std::vector<model::Note>&)>& change, int gestureId);

    int newNoteLength = model::ticksPerStep; // the last length used, as in FL

    // Audition: plays `pitch` on the channel until stopAudition() (or another pitch). Clicking
    // notes and keyboard keys uses it; the owner connects it to the engine.
    void audition(int pitch, float velocity);
    void stopAudition();
    int auditionPitch = -1; // sounding now, or -1
    std::function<void(int pitch, float velocity)> onAuditionStart;
    std::function<void(int pitch)> onAuditionStop;

    void setZoom(float pixelsPerTick, float anchorGridX);
    float pixelsPerTick() const { return zoom; }

    void paint(juce::Graphics& g) override;
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

    juce::Label title{{}, "Piano roll"};
    juce::Label snapLabel{{}, "Snap"};
    juce::ComboBox snapBox;
    juce::Label scaleLabel{{}, "Scale"};
    juce::ComboBox rootBox;  // C .. B
    juce::ComboBox scaleBox; // Off / Major / Minor
    std::unique_ptr<Keyboard> keyboard;
    std::unique_ptr<Ruler> ruler;
    std::unique_ptr<VelocityLane> velocityLane;
    SyncedViewport viewport;
    NoteGrid grid;
    float lastPlayheadX = -1.0f;
    model::ChannelId scrolledFor = 0;
};

// FL's note names: C5 is MIDI note 60.
juce::String noteName(int pitch);

} // namespace jls::ui
