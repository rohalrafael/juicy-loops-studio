#pragma once

#include "engine/transport.h"
#include "model/document.h"

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
//   Ctrl + mouse wheel      zoom horizontally
class PlaylistGrid final : public juce::Component
{
  public:
    explicit PlaylistGrid(PlaylistView& owner);

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

    int clipAt(juce::Point<float> p) const; // index into playlist.clips, or -1
    bool onRightEdge(int clipIndex, float x) const;
    void eraseAt(juce::Point<float> p);
    void paintClip(juce::Graphics& g, const model::Clip& clip) const;

    PlaylistView& view;
    Drag drag = Drag::none;
    int dragClip = -1;
    int grabOffsetSteps = 0;
    int gestureId = 0;
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
    int stepAtX(float x) const; // grid coordinates
    float xForStep(double step) const { return static_cast<float>(step) * zoom; }
    int trackAtY(float y) const;
    // Playhead position in steps, or -1 when it shouldn't be shown.
    double playheadStep() const;

    model::Document& document() { return doc; }

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setZoom(float pixelsPerStep, float anchorGridX);

  private:
    class Ruler;
    class TrackNames;
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
