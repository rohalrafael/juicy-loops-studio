#include "ui/playlist.h"

#include "ui/theme.h"

#include <cmath>

namespace jls::ui
{
namespace
{

constexpr float edgeGrabWidth = 6.0f; // pixels at a clip's right end that resize it
constexpr int minimumBars = 64;
constexpr int spareBars = 8; // empty room after the last clip

enum SnapId
{
    snapBar = 1,
    snapBeat,
    snapStep,
};

} // namespace

// ---------------------------------------------------------------------------- Ruler / names

class PlaylistView::Ruler final : public juce::Component, public juce::SettableTooltipClient
{
  public:
    explicit Ruler(PlaylistView& v) : view(v)
    {
        setTooltip("Click or drag to move the playhead (switches to SONG mode)");
    }

    // Clicking (or dragging along) the ruler moves the playhead there, snapped.
    void mouseDown(const juce::MouseEvent& e) override { view.seekToX(e.position.x); }
    void mouseDrag(const juce::MouseEvent& e) override { view.seekToX(e.position.x); }

    void paint(juce::Graphics& g) override
    {
        const auto& t = currentTheme();
        g.fillAll(colour(t.colours.panel));
        g.setColour(colour(t.colours.outline));
        g.fillRect(0, getHeight() - 1, getWidth(), 1);

        const auto& project = view.doc.project();
        const int bar = project.stepsPerBar();
        const float offset = static_cast<float>(view.viewport.getViewPositionX());
        const float barWidth = view.xForStep(bar);
        const int every = barWidth < 28.0f ? 4 : (barWidth < 48.0f ? 2 : 1); // keep labels apart

        g.setFont(juce::FontOptions(11.0f));
        const int first = static_cast<int>(offset / barWidth);
        for (int b = first; view.xForStep(b * bar) - offset < static_cast<float>(getWidth()); ++b)
        {
            const float x = view.xForStep(b * bar) - offset;
            g.setColour(colour(t.colours.grid));
            g.fillRect(x, 0.0f, 1.0f, static_cast<float>(getHeight()));
            if (b % every == 0)
            {
                g.setColour(colour(t.colours.textDim));
                g.drawText(
                    juce::String(b + 1),
                    juce::Rectangle<float>(x + 4.0f, 0.0f, 40.0f, static_cast<float>(getHeight())),
                    juce::Justification::centredLeft);
            }
        }

        if (const double step = view.playheadStep(); step >= 0.0)
        {
            const float x = view.xForStep(step) - offset;
            juce::Path marker;
            marker.addTriangle(x - 5.0f, 0.0f, x + 5.0f, 0.0f, x, 8.0f);
            g.setColour(colour(t.colours.accent));
            g.fillPath(marker);
        }
    }

  private:
    PlaylistView& view;
};

class PlaylistView::TrackNames final : public juce::Component
{
  public:
    explicit TrackNames(PlaylistView& v) : view(v) { setInterceptsMouseClicks(false, false); }

    void paint(juce::Graphics& g) override
    {
        const auto& t = currentTheme();
        g.fillAll(colour(t.colours.panel));
        const int offset = view.viewport.getViewPositionY();
        g.setFont(juce::FontOptions(13.0f));
        for (int track = 0; track < view.doc.project().playlist.numTracks; ++track)
        {
            const int y = track * trackHeight - offset;
            if (y > getHeight())
                break;
            auto row = juce::Rectangle<int>(0, y, getWidth(), trackHeight);
            g.setColour(colour(t.colours.outline));
            g.fillRect(row.removeFromBottom(1));
            g.setColour(colour(t.colours.text));
            g.drawText("Track " + juce::String(track + 1), row.reduced(10, 0),
                       juce::Justification::centredLeft);
        }
        g.setColour(colour(t.colours.outline));
        g.fillRect(getWidth() - 1, 0, 1, getHeight());
    }

  private:
    PlaylistView& view;
};

// ------------------------------------------------------------------------------ PlaylistGrid

PlaylistGrid::PlaylistGrid(PlaylistView& owner) : view(owner)
{
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
}

int PlaylistGrid::clipAt(juce::Point<float> p) const
{
    const auto& clips = view.document().project().playlist.clips;
    const int track = view.trackAtY(p.y);
    // Last drawn is on top, so search backwards.
    for (int i = static_cast<int>(clips.size()) - 1; i >= 0; --i)
    {
        const auto& c = clips[static_cast<std::size_t>(i)];
        if (c.track == track && p.x >= view.xForStep(c.startStep) &&
            p.x < view.xForStep(c.endStep()))
            return i;
    }
    return -1;
}

bool PlaylistGrid::onRightEdge(int clipIndex, float x) const
{
    if (clipIndex < 0)
        return false;
    const auto& c = view.document().project().playlist.clips[static_cast<std::size_t>(clipIndex)];
    const float end = view.xForStep(c.endStep());
    const float grab = std::min(edgeGrabWidth, (end - view.xForStep(c.startStep)) / 3.0f);
    return x >= end - grab;
}

void PlaylistGrid::mouseMove(const juce::MouseEvent& e)
{
    setMouseCursor(onRightEdge(clipAt(e.position), e.position.x)
                       ? juce::MouseCursor::LeftRightResizeCursor
                       : juce::MouseCursor::NormalCursor);
}

void PlaylistGrid::eraseAt(juce::Point<float> p)
{
    const int index = clipAt(p);
    if (index < 0)
        return;
    view.clearSelection(); // indices are about to shift
    view.document().apply(
        [index](model::Project& project)
        {
            auto& clips = project.playlist.clips;
            clips.erase(clips.begin() + index);
        },
        gestureId);
}

void PlaylistGrid::mouseDown(const juce::MouseEvent& e)
{
    auto& doc = view.document();
    gestureId = doc.newGestureId();

    if (e.mods.isPopupMenu())
    {
        drag = Drag::erase;
        eraseAt(e.position);
        return;
    }

    if (e.mods.isCommandDown())
    {
        drag = Drag::select;
        bandStart = bandEnd = e.position;
        return;
    }

    dragClip = clipAt(e.position);
    if (dragClip >= 0 && view.isSelected(dragClip) && !onRightEdge(dragClip, e.position.x))
    {
        drag = Drag::moveSelection;
        moveOriginals.clear();
        for (int i : view.selection)
            moveOriginals.push_back(doc.project().playlist.clips[static_cast<std::size_t>(i)]);
        grabStep = view.stepAtX(e.position.x);
        grabTrack = view.trackAtY(e.position.y);
        return;
    }
    if (!view.selection.empty())
    {
        // A click outside the selection only deselects (FL does the same).
        view.clearSelection();
        drag = Drag::none;
        dragClip = -1;
        return;
    }
    if (dragClip >= 0)
    {
        drag = onRightEdge(dragClip, e.position.x) ? Drag::resize : Drag::move;
        grabOffsetSteps =
            view.stepAtX(e.position.x) -
            doc.project().playlist.clips[static_cast<std::size_t>(dragClip)].startStep;
        return;
    }

    // Empty space: place the selected pattern here, then keep dragging it.
    const int track = view.trackAtY(e.position.y);
    if (track < 0 || track >= doc.project().playlist.numTracks)
        return;
    const int snap = view.snapSteps();
    const int start = view.stepAtX(e.position.x) / snap * snap;
    doc.apply(
        [track, start](model::Project& p)
        {
            model::Clip clip;
            clip.pattern = p.current().id;
            clip.track = track;
            clip.startStep = start;
            clip.lengthInSteps = p.lengthInSteps(p.current());
            p.playlist.clips.push_back(clip);
        },
        gestureId);
    dragClip = static_cast<int>(doc.project().playlist.clips.size()) - 1;
    drag = Drag::move;
    grabOffsetSteps = view.stepAtX(e.position.x) - start;
}

void PlaylistGrid::mouseDrag(const juce::MouseEvent& e)
{
    auto& doc = view.document();
    if (drag == Drag::erase)
    {
        eraseAt(e.position);
        return;
    }
    if (drag == Drag::select)
    {
        bandEnd = e.position;
        repaint();
        return;
    }
    if (drag == Drag::moveSelection)
    {
        // One offset for all, limited so every clip stays on the grid.
        const int snap = view.snapSteps();
        int steps = static_cast<int>(std::lround(
                        static_cast<double>(view.stepAtX(e.position.x) - grabStep) / snap)) *
                    snap;
        int tracks = view.trackAtY(e.position.y) - grabTrack;
        const int numTracks = doc.project().playlist.numTracks;
        for (const auto& clip : moveOriginals)
        {
            steps = std::max(steps, -clip.startStep);
            tracks = std::clamp(tracks, -clip.track, numTracks - 1 - clip.track);
        }
        const auto selected = view.selection;
        const auto originals = moveOriginals;
        doc.apply(
            [selected, originals, steps, tracks](model::Project& p)
            {
                for (std::size_t i = 0; i < selected.size(); ++i)
                {
                    auto& clip = p.playlist.clips[static_cast<std::size_t>(selected[i])];
                    clip.startStep = originals[i].startStep + steps;
                    clip.track = originals[i].track + tracks;
                }
            },
            gestureId);
        return;
    }
    if (dragClip < 0 || dragClip >= static_cast<int>(doc.project().playlist.clips.size()))
        return;

    auto clip = doc.project().playlist.clips[static_cast<std::size_t>(dragClip)];
    const auto original = clip;
    const int snap = view.snapSteps();

    if (drag == Drag::move)
    {
        const double wanted = view.stepAtX(e.position.x) - grabOffsetSteps;
        clip.startStep = std::max(0, static_cast<int>(std::lround(wanted / snap)) * snap);
        clip.track =
            std::clamp(view.trackAtY(e.position.y), 0, doc.project().playlist.numTracks - 1);
    }
    else if (drag == Drag::resize)
    {
        const double end = view.stepAtX(e.position.x);
        const int snappedEnd = static_cast<int>(std::lround(end / snap)) * snap;
        clip.lengthInSteps = std::max(snap, snappedEnd - clip.startStep);
    }

    if (clip == original)
        return;
    const int index = dragClip;
    doc.apply([index, clip](model::Project& p)
              { p.playlist.clips[static_cast<std::size_t>(index)] = clip; }, gestureId);
}

void PlaylistGrid::mouseUp(const juce::MouseEvent&)
{
    if (drag == Drag::select)
    {
        // Every clip the rectangle touches.
        const auto band = juce::Rectangle<float>(bandStart, bandEnd);
        view.selection.clear();
        const auto& clips = view.document().project().playlist.clips;
        for (std::size_t i = 0; i < clips.size(); ++i)
            if (band.intersects(clipBounds(clips[i])))
                view.selection.push_back(static_cast<int>(i));
        repaint();
    }
    drag = Drag::none;
    dragClip = -1;
}

void PlaylistGrid::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown())
    {
        const float factor = w.deltaY > 0 ? 1.25f : 0.8f;
        view.setZoom(view.pixelsPerStep() * factor, e.position.x);
        return;
    }
    Component::mouseWheelMove(e, w); // let the viewport scroll
}

void PlaylistGrid::paint(juce::Graphics& g)
{
    const auto& t = currentTheme();
    const auto& project = view.document().project();
    const auto clipArea = g.getClipBounds();

    g.fillAll(colour(t.colours.lane));

    // Track separators.
    g.setColour(colour(t.colours.outline));
    for (int track = 1; track <= project.playlist.numTracks; ++track)
        g.fillRect(clipArea.getX(), track * PlaylistView::trackHeight - 1, clipArea.getWidth(), 1);

    // Bar lines, and fainter beat lines when there's room.
    const int bar = project.stepsPerBar();
    const bool showBeats = view.xForStep(model::stepsPerBeat) >= 10.0f;
    const int firstStep = std::max(0, view.stepAtX(static_cast<float>(clipArea.getX())) - 1);
    const int lastStep = view.stepAtX(static_cast<float>(clipArea.getRight())) + 1;
    for (int s = firstStep; s <= lastStep; ++s)
    {
        const bool isBar = s % bar == 0;
        if (!isBar && !(showBeats && s % model::stepsPerBeat == 0))
            continue;
        g.setColour(colour(t.colours.grid).withMultipliedAlpha(isBar ? 1.0f : 0.45f));
        g.fillRect(view.xForStep(s), 0.0f, 1.0f, static_cast<float>(getHeight()));
    }

    for (std::size_t i = 0; i < project.playlist.clips.size(); ++i)
        paintClip(g, project.playlist.clips[i], view.isSelected(static_cast<int>(i)));

    if (drag == Drag::select)
    {
        const auto band = juce::Rectangle<float>(bandStart, bandEnd);
        g.setColour(colour(t.colours.accentAlt).withAlpha(0.15f));
        g.fillRect(band);
        g.setColour(colour(t.colours.accentAlt));
        g.drawRect(band, 1.0f);
    }

    if (project.playlist.clips.empty())
    {
        g.setColour(colour(t.colours.textDim));
        g.setFont(juce::FontOptions(14.0f));
        g.drawText("Click to place " + juce::String::fromUTF8(project.current().name.c_str()) +
                       juce::String::fromUTF8(" \xc2\xb7 drag to move \xc2\xb7 right-click "
                                              "deletes \xc2\xb7 press L for song mode"),
                   juce::Rectangle<int>(16, 0, 900, PlaylistView::trackHeight * 2),
                   juce::Justification::centredLeft);
    }

    if (const double step = view.playheadStep(); step >= 0.0)
    {
        g.setColour(colour(t.colours.accent));
        g.fillRect(view.xForStep(step) - 1.0f, 0.0f, 2.0f, static_cast<float>(getHeight()));
    }
}

juce::Rectangle<float> PlaylistGrid::clipBounds(const model::Clip& clip) const
{
    return juce::Rectangle<float>(view.xForStep(clip.startStep),
                                  static_cast<float>(clip.track * PlaylistView::trackHeight) + 2.0f,
                                  view.xForStep(clip.lengthInSteps),
                                  PlaylistView::trackHeight - 5.0f)
        .reduced(0.5f, 0.0f);
}

void PlaylistGrid::paintClip(juce::Graphics& g, const model::Clip& clip, bool selected) const
{
    const auto& t = currentTheme();
    const auto& project = view.document().project();
    const auto* pattern = project.findPattern(clip.pattern);
    if (pattern == nullptr)
        return;

    const auto bounds = clipBounds(clip);
    if (!g.getClipBounds().toFloat().intersects(bounds))
        return;

    const float radius = std::min(t.shape.cornerRadius, 6.0f);
    g.setColour(colour(t.colours.clip));
    g.fillRoundedRectangle(bounds, radius);
    if (selected)
    {
        g.setColour(colour(t.colours.accentAlt));
        g.drawRoundedRectangle(bounds.reduced(1.0f), radius, 2.0f);
    }

    juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(bounds.toNearestInt());

    // Name strip.
    auto body = bounds;
    const auto strip = body.removeFromTop(13.0f);
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRect(strip);
    g.setColour(colour(t.colours.clipText));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText(juce::String::fromUTF8(pattern->name.c_str()), strip.reduced(4.0f, 0.0f),
               juce::Justification::centredLeft, true);

    // Miniature of the notes, one thin row per channel; repeats when the clip is longer.
    const int numChannels = static_cast<int>(project.channels.size());
    const int patternSteps = project.lengthInSteps(*pattern);
    if (numChannels > 0 && patternSteps > 0)
    {
        const float rowHeight = std::max(1.0f, (body.getHeight() - 2.0f) / numChannels);
        const float pixelsPerTick = view.pixelsPerStep() / model::ticksPerStep;
        const int patternTicks = patternSteps * model::ticksPerStep;
        const int clipTicks = clip.lengthInSteps * model::ticksPerStep;
        g.setColour(colour(t.colours.clipText).withAlpha(0.85f));
        for (int repeat = 0; repeat < clipTicks; repeat += patternTicks)
            for (int ch = 0; ch < numChannels; ++ch)
                for (const auto& note :
                     pattern->notesFor(project.channels[static_cast<std::size_t>(ch)].id))
                {
                    const int start = repeat + note.startTick;
                    if (start >= clipTicks)
                        continue;
                    const int end = std::min(clipTicks, start + note.lengthTicks);
                    g.fillRect(bounds.getX() + start * pixelsPerTick,
                               body.getY() + 1.0f + ch * rowHeight,
                               std::max(1.0f, (end - start) * pixelsPerTick - 1.0f),
                               std::max(1.0f, rowHeight - 1.0f));
                }

        g.setColour(colour(t.colours.clipText).withAlpha(0.35f));
        for (int r = patternSteps; r < clip.lengthInSteps; r += patternSteps)
            g.fillRect(bounds.getX() + view.xForStep(r), body.getY(), 1.0f, body.getHeight());
    }

    if (t.shape.stepOutline > 0.0f || t.shape.borderWidth > 1.0f)
    {
        g.setColour(colour(t.colours.outline));
        g.drawRoundedRectangle(bounds.reduced(t.shape.borderWidth / 2.0f), radius,
                               t.shape.borderWidth);
    }
}

// ------------------------------------------------------------------------------ PlaylistView

PlaylistView::PlaylistView(model::Document& document, engine::Transport& transportToShow)
    : doc(document), transport(transportToShow), ruler(std::make_unique<Ruler>(*this)),
      trackNames(std::make_unique<TrackNames>(*this)), grid(*this)
{
    title.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(title);
    addAndMakeVisible(snapLabel);

    snapBox.addItem("Bar", snapBar);
    snapBox.addItem("Beat", snapBeat);
    snapBox.addItem("Step", snapStep);
    snapBox.setTooltip("Snap: where clips land and how they move");
    snapBox.setSelectedId(snapBar, juce::dontSendNotification);
    snapBox.setWantsKeyboardFocus(false);
    addAndMakeVisible(snapBox);

    viewport.setViewedComponent(&grid, false);
    viewport.setScrollBarsShown(true, true);
    viewport.onScroll = [this]
    {
        ruler->repaint();
        trackNames->repaint();
    };
    addAndMakeVisible(viewport);
    addAndMakeVisible(*ruler);
    addAndMakeVisible(*trackNames);

    refresh();
    startTimerHz(30);
}

PlaylistView::~PlaylistView() = default;

void PlaylistView::seekToX(float x)
{
    const double step = (x + static_cast<float>(viewport.getViewPositionX())) / zoom;
    const int snap = std::max(1, snapSteps());
    const int snapped = std::max(0, static_cast<int>(step / snap) * snap);
    // The Playlist is song time, so jumping in it means song mode.
    if (!doc.project().songMode)
        doc.setViewState([](model::Project& p) { p.songMode = true; });
    transport.seek(static_cast<double>(snapped) / model::stepsPerBeat);
}

int PlaylistView::snapSteps() const
{
    switch (snapBox.getSelectedId())
    {
    case snapStep:
        return 1;
    case snapBeat:
        return model::stepsPerBeat;
    default:
        return doc.project().stepsPerBar();
    }
}

int PlaylistView::stepAtX(float x) const
{
    return std::max(0, static_cast<int>(std::floor(x / zoom)));
}

int PlaylistView::trackAtY(float y) const
{
    return static_cast<int>(std::floor(y / trackHeight));
}

double PlaylistView::playheadStep() const
{
    const auto& project = doc.project();
    const int songLength = project.songLengthInSteps();
    if (!project.songMode)
        return -1.0;
    const double position = transport.positionInBeats() * model::stepsPerBeat;
    if (!transport.isPlaying() && position <= 0.0)
        return -1.0;
    // An empty song doesn't loop: show where the ruler was clicked.
    return songLength > 0 ? std::fmod(position, static_cast<double>(songLength)) : position;
}

bool PlaylistView::isSelected(int clip) const
{
    return std::find(selection.begin(), selection.end(), clip) != selection.end();
}

void PlaylistView::clearSelection()
{
    if (!selection.empty())
    {
        selection.clear();
        grid.repaint();
    }
}

bool PlaylistView::edit(EditAction action)
{
    if (action == EditAction::selectAll)
    {
        selection.clear();
        for (int i = 0; i < static_cast<int>(doc.project().playlist.clips.size()); ++i)
            selection.push_back(i);
        grid.repaint();
        return true;
    }
    if (action == EditAction::paste)
    {
        if (clipboard.empty())
            return true;
        // Right after the copied bars (or the last paste); the pasted clips get selected.
        auto pasted = model::nextPaste(clipboard);
        std::erase_if(pasted, [this](const model::Clip& c)
                      { return doc.project().findPattern(c.pattern) == nullptr; });
        const int first = static_cast<int>(doc.project().playlist.clips.size());
        doc.apply(
            [pasted](model::Project& p)
            { p.playlist.clips.insert(p.playlist.clips.end(), pasted.begin(), pasted.end()); });
        selection.clear();
        for (int i = 0; i < static_cast<int>(pasted.size()); ++i)
            selection.push_back(first + i);
        grid.repaint();
        return true;
    }
    if (selection.empty())
        return false;
    if (action == EditAction::copy || action == EditAction::cut)
    {
        clipboard =
            model::copyClips(doc.project().playlist.clips, selection, doc.project().stepsPerBar());
        if (action == EditAction::copy)
            return true;
        return edit(EditAction::deleteSelection); // cut = copy + delete
    }
    if (action == EditAction::deselect)
    {
        clearSelection();
        return true;
    }
    if (action == EditAction::deleteSelection)
    {
        auto doomed = selection;
        std::sort(doomed.begin(), doomed.end(), std::greater<>());
        selection.clear();
        doc.apply(
            [doomed](model::Project& p)
            {
                for (int i : doomed)
                    if (i < static_cast<int>(p.playlist.clips.size()))
                        p.playlist.clips.erase(p.playlist.clips.begin() + i);
            });
        return true;
    }
    return false;
}

namespace
{
// Moves a combo box's selection by `delta` items (clamped), notifying its onChange.
void stepComboBox(juce::ComboBox& box, int delta)
{
    const int index = std::clamp(box.getSelectedItemIndex() + delta, 0, box.getNumItems() - 1);
    box.setSelectedItemIndex(index, juce::sendNotificationSync);
}
} // namespace

void PlaylistView::stepSnap(int finer)
{
    stepComboBox(snapBox, finer); // items go from coarse to fine
}

void PlaylistView::refresh()
{
    // Undo/redo or other edits may have removed clips: forget what no longer exists.
    const int count = static_cast<int>(doc.project().playlist.clips.size());
    std::erase_if(selection, [count](int i) { return i >= count; });
    updateGridSize();
    grid.repaint();
    ruler->repaint();
    trackNames->repaint();
}

void PlaylistView::updateGridSize()
{
    const auto& project = doc.project();
    const int bar = project.stepsPerBar();
    const int bars = std::max(minimumBars, project.songLengthInSteps() / bar + spareBars);
    grid.setSize(std::max(static_cast<int>(std::ceil(xForStep(bars * bar))),
                          viewport.getMaximumVisibleWidth()),
                 project.playlist.numTracks * trackHeight);
}

void PlaylistView::setZoom(float pixelsPerStep, float anchorGridX)
{
    const float clamped = std::clamp(pixelsPerStep, 1.0f, 24.0f);
    if (std::abs(clamped - zoom) < 0.001f)
        return;
    // Keep the step under the mouse in place.
    const float anchorStep = anchorGridX / zoom;
    const float anchorOnScreen = anchorGridX - static_cast<float>(viewport.getViewPositionX());
    zoom = clamped;
    updateGridSize();
    viewport.setViewPosition(std::max(0, static_cast<int>(anchorStep * zoom - anchorOnScreen)),
                             viewport.getViewPositionY());
    refresh();
}

void PlaylistView::paint(juce::Graphics& g)
{
    const auto& t = currentTheme();
    g.fillAll(colour(t.colours.panel));
    auto header = getLocalBounds().removeFromTop(headerHeight);
    g.setColour(colour(t.colours.header));
    g.fillRect(header);
    g.setColour(colour(t.colours.outline));
    g.fillRect(header.removeFromBottom(1));
    g.fillRect(0, headerHeight + rulerHeight - 1, trackNameWidth, 1);
}

void PlaylistView::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop(headerHeight).reduced(10, 4);
    title.setBounds(header.removeFromLeft(100));
    snapBox.setBounds(header.removeFromRight(80));
    snapLabel.setBounds(header.removeFromRight(44));

    auto top = area.removeFromTop(rulerHeight);
    top.removeFromLeft(trackNameWidth);
    ruler->setBounds(top);

    trackNames->setBounds(area.removeFromLeft(trackNameWidth));
    viewport.setBounds(area);
    updateGridSize();
}

void PlaylistView::timerCallback()
{
    const double step = playheadStep();
    const float x = step >= 0.0 ? xForStep(step) : -1.0f;
    if (std::abs(x - lastPlayheadX) < 0.5f)
        return;

    // Repaint just the old and new playhead strips.
    for (float px : {lastPlayheadX, x})
        if (px >= 0.0f)
            grid.repaint(static_cast<int>(px) - 2, 0, 5, grid.getHeight());
    lastPlayheadX = x;
    ruler->repaint();
}

} // namespace jls::ui
