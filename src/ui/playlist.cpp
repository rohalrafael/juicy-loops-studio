#include "ui/playlist.h"

#include "ui/theme.h"

#include <cmath>

namespace jls::ui
{
namespace
{

constexpr float edgeGrabWidth = 6.0f;  // pixels at a clip's ends that resize / trim it
constexpr float clickTolerance = 4.0f; // a Ctrl+drag smaller than this is a Ctrl+click
constexpr int minimumBars = 64;
constexpr int spareBars = 8; // empty room after the last clip

enum SnapId
{
    snapBar = 1,
    snapBeat,
    snapStep,
};

// Audio clips: the pattern-clip colour leaning towards the second accent, so they stand out.
juce::Colour audioClipColour(const theme::Theme& t)
{
    return colour(t.colours.clip).interpolatedWith(colour(t.colours.accentAlt), 0.35f);
}

// Clips are drawn as data, as in FL and Ableton: a body tinted with the clip's colour and
// outlined in it, a solid name bar on top, and the notes or waveform in the colour itself.
constexpr float nameBarHeight = 14.0f;

void paintClipBody(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour clipColour,
                   bool selected)
{
    const float radius = metrics::cornerRadius;
    g.setColour(clipColour.withAlpha(0.22f));
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(clipColour.withAlpha(0.6f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
    if (selected)
    {
        g.setColour(colour(currentTheme().colours.accentAlt));
        g.drawRoundedRectangle(bounds.reduced(1.0f), radius, 2.0f);
    }
}

void paintClipName(juce::Graphics& g, juce::Rectangle<float> bar, juce::Colour clipColour,
                   const juce::String& name)
{
    g.setColour(clipColour);
    g.fillRect(bar);
    g.setColour(colour(currentTheme().colours.clipText));
    g.setFont(mediumFont(metrics::textSmall));
    g.drawText(name, bar.reduced(4.0f, 0.0f), juce::Justification::centredLeft, true);
}

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

        g.setFont(juce::FontOptions(metrics::textSmall));
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

        if (const float shown = view.shownPlayheadX(); shown >= 0.0f)
        {
            const float x = shown - offset;
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
        g.setFont(juce::FontOptions(metrics::textLarge));
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

bool PlaylistGrid::onLeftEdge(int clipIndex, float x) const
{
    if (clipIndex < 0)
        return false;
    const auto& c = view.document().project().playlist.clips[static_cast<std::size_t>(clipIndex)];
    if (!c.isAudio())
        return false;
    const float start = view.xForStep(c.startStep);
    const float grab = std::min(edgeGrabWidth, (view.xForStep(c.endStep()) - start) / 3.0f);
    return x < start + grab;
}

void PlaylistGrid::mouseMove(const juce::MouseEvent& e)
{
    const int clip = clipAt(e.position);
    setMouseCursor(onRightEdge(clip, e.position.x) || onLeftEdge(clip, e.position.x)
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
        // Right-click deselects (and deletes the clip under the mouse, as always).
        view.clearSelection();
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
    if (dragClip >= 0 && e.mods.isShiftDown())
    {
        // Shift+click adds a clip to the selection, or takes it out.
        if (view.isSelected(dragClip))
            std::erase(view.selection, dragClip);
        else
            view.selection.push_back(dragClip);
        repaint();
        drag = Drag::none;
        dragClip = -1;
        return;
    }
    if (dragClip >= 0 && !view.isSelected(dragClip))
    {
        // Clicking a clip selects it (just it), as in the Piano Roll.
        view.selection = {dragClip};
        repaint();
    }
    const bool onEdge = dragClip >= 0 &&
                        (onRightEdge(dragClip, e.position.x) || onLeftEdge(dragClip, e.position.x));
    if (dragClip >= 0 && !onEdge)
    {
        drag = Drag::moveSelection;
        moveOriginals.clear();
        for (int i : view.selection)
            moveOriginals.push_back(doc.project().playlist.clips[static_cast<std::size_t>(i)]);
        grabStep = view.stepAtX(e.position.x);
        grabTrack = view.trackAtY(e.position.y);
        return;
    }
    if (dragClip < 0)
        view.clearSelection(); // empty space: deselect, then place a clip below
    if (dragClip >= 0)
    {
        // A clip's edges resize or trim just that clip.
        drag = onRightEdge(dragClip, e.position.x)  ? Drag::resize
               : onLeftEdge(dragClip, e.position.x) ? Drag::trimStart
                                                    : Drag::move;
        grabOffsetSteps =
            view.stepAtX(e.position.x) -
            doc.project().playlist.clips[static_cast<std::size_t>(dragClip)].startStep;
        return;
    }

    // Empty space: place the selected pattern (or the audio clip chosen in the picker) here,
    // then keep dragging it.
    const int track = view.trackAtY(e.position.y);
    if (track < 0 || track >= doc.project().playlist.numTracks)
        return;
    const int snap = view.snapSteps();
    const int start = view.stepAtX(e.position.x) / snap * snap;
    const auto* audio = doc.project().findChannel(view.brushAudio);
    const auto audioId = audio != nullptr && audio->audioClip ? audio->id : 0;
    const int audioSteps = audioId != 0 ? view.stepsFor(*audio) : 0;
    doc.apply(
        [track, start, audioId, audioSteps](model::Project& p)
        {
            model::Clip clip;
            clip.pattern = p.current().id;
            clip.track = track;
            clip.startStep = start;
            clip.lengthInSteps = audioId != 0 ? audioSteps : p.lengthInSteps(p.current());
            clip.audio = audioId;
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
    else if (drag == Drag::trimStart)
    {
        // The end stays put; the start moves, and the sample starts as much later (or earlier,
        // but not before the sample's beginning).
        const double wanted = view.stepAtX(e.position.x);
        int start = static_cast<int>(std::lround(wanted / snap)) * snap;
        start = std::clamp(start, clip.startStep - clip.offsetSteps, clip.endStep() - 1);
        start = std::max(0, start);
        const int delta = start - clip.startStep;
        clip.startStep = start;
        clip.lengthInSteps -= delta;
        clip.offsetSteps += delta;
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
        const auto band = juce::Rectangle<float>(bandStart, bandEnd);
        if (band.getWidth() < clickTolerance && band.getHeight() < clickTolerance)
        {
            // Ctrl+click without a drag: the clip under the mouse joins the selection or leaves
            // it, like Shift+click (with nothing selected, that selects just it).
            if (const int index = clipAt(bandStart); index >= 0)
            {
                if (view.isSelected(index))
                    std::erase(view.selection, index);
                else
                    view.selection.push_back(index);
            }
        }
        else
        {
            // Every clip the rectangle touches.
            view.selection.clear();
            const auto& clips = view.document().project().playlist.clips;
            for (std::size_t i = 0; i < clips.size(); ++i)
                if (band.intersects(clipBounds(clips[i])))
                    view.selection.push_back(static_cast<int>(i));
        }
        repaint();
    }
    drag = Drag::none;
    dragClip = -1;
}

void PlaylistGrid::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int index = clipAt(e.position);
    if (index < 0)
        return;
    const auto& clip = view.document().project().playlist.clips[static_cast<std::size_t>(index)];
    if (clip.isAudio() && view.onOpenAudioChannel)
        view.onOpenAudioChannel(clip.audio);
    else if (!clip.isAudio() && view.onOpenPattern)
        view.onOpenPattern(clip.pattern);
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

    // Where a dragged sample or picker item would land.
    if (dropPoint.x >= 0)
    {
        const int snap = view.snapSteps();
        const int step = view.stepAtX(static_cast<float>(dropPoint.x)) / snap * snap;
        const int track = view.trackAtY(static_cast<float>(dropPoint.y));
        g.setColour(colour(t.colours.accentAlt).withAlpha(0.35f));
        g.fillRect(view.xForStep(step),
                   static_cast<float>(track * PlaylistView::trackHeight) + 2.0f,
                   view.xForStep(project.stepsPerBar()), PlaylistView::trackHeight - 5.0f);
    }

    if (project.playlist.clips.empty())
    {
        g.setColour(colour(t.colours.textDim));
        g.setFont(juce::FontOptions(metrics::textLarge));
        g.drawText("Click to place " + juce::String::fromUTF8(project.current().name.c_str()) +
                       juce::String::fromUTF8(" \xc2\xb7 drag to move \xc2\xb7 right-click "
                                              "deletes \xc2\xb7 press L for song mode"),
                   juce::Rectangle<int>(16, 0, 900, PlaylistView::trackHeight * 2),
                   juce::Justification::centredLeft);
    }

    if (const float x = view.shownPlayheadX(); x >= 0.0f)
    {
        g.setColour(colour(t.colours.accent));
        g.fillRect(x - 1.0f, 0.0f, 2.0f, static_cast<float>(getHeight()));
    }
}

// Dropping samples (from the desktop or the Browser) and picker items onto the grid.
bool PlaylistGrid::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
        if (juce::File(f).hasFileExtension("wav;aif;aiff;flac;ogg;mp3"))
            return true;
    return false;
}

void PlaylistGrid::fileDragMove(const juce::StringArray&, int x, int y)
{
    setDropPoint({x, y});
}

void PlaylistGrid::fileDragExit(const juce::StringArray&)
{
    setDropPoint({-1, -1});
}

void PlaylistGrid::filesDropped(const juce::StringArray& files, int x, int y)
{
    setDropPoint({-1, -1});
    // Several files go onto consecutive tracks.
    int track = 0;
    for (const auto& f : files)
        if (juce::File(f).hasFileExtension("wav;aif;aiff;flac;ogg;mp3"))
            view.placeAt("file:" + f, static_cast<float>(x),
                         static_cast<float>(y + PlaylistView::trackHeight * track++));
}

bool PlaylistGrid::isInterestedInDragSource(const SourceDetails& details)
{
    const auto what = details.description.toString();
    return what.startsWith("file:") || what.startsWith("pattern:") || what.startsWith("audio:");
}

void PlaylistGrid::itemDragMove(const SourceDetails& details)
{
    setDropPoint(details.localPosition);
}

void PlaylistGrid::itemDragExit(const SourceDetails&)
{
    setDropPoint({-1, -1});
}

void PlaylistGrid::itemDropped(const SourceDetails& details)
{
    setDropPoint({-1, -1});
    view.placeAt(details.description.toString(), static_cast<float>(details.localPosition.x),
                 static_cast<float>(details.localPosition.y));
}

void PlaylistGrid::setDropPoint(juce::Point<int> point)
{
    if (point != dropPoint)
    {
        dropPoint = point;
        repaint();
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
    if (clip.isAudio())
    {
        // An audio clip: its channel's name and the sample's waveform, at the sample's real
        // length for the current tempo (silence after it if the clip is longer).
        const auto* channel = project.findChannel(clip.audio);
        if (channel == nullptr)
            return;
        const auto bounds = clipBounds(clip);
        if (!g.getClipBounds().toFloat().intersects(bounds))
            return;
        const auto clipColour = audioClipColour(t);
        paintClipBody(g, bounds, clipColour, selected);
        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(bounds.toNearestInt());
        auto body = bounds;
        paintClipName(g, body.removeFromTop(nameBarHeight), clipColour,
                      juce::String::fromUTF8(channel->name.c_str()));
        if (channel->sample != nullptr && channel->sample->numFrames() > 0)
        {
            const auto& peaks = const_cast<PlaylistView&>(view).peaksFor(*channel->sample);
            const double seconds = project.playedSeconds(*channel); // after stretching / speed
            const float width = view.xForStep(seconds / project.secondsPerStep());
            const float mid = body.getCentreY();
            const float half = body.getHeight() / 2.0f - 1.0f;
            g.setColour(clipColour);
            const int columns = static_cast<int>(std::min(width, bounds.getWidth()));
            for (int x = 0; x < columns; ++x)
            {
                // The clip starts offsetSteps into the sample (left trim).
                const float sampleX = x + view.xForStep(clip.offsetSteps);
                if (sampleX >= width)
                    break;
                const auto& [low, high] = peaks[static_cast<std::size_t>(
                    std::min<long long>(static_cast<long long>(peaks.size()) - 1,
                                        static_cast<long long>(sampleX / width * peaks.size())))];
                g.fillRect(bounds.getX() + x, mid - high * half, 1.0f,
                           std::max(1.0f, (high - low) * half));
            }
        }
        return;
    }
    const auto* pattern = project.findPattern(clip.pattern);
    if (pattern == nullptr)
        return;

    const auto bounds = clipBounds(clip);
    if (!g.getClipBounds().toFloat().intersects(bounds))
        return;

    const auto clipColour = colour(t.colours.clip);
    paintClipBody(g, bounds, clipColour, selected);

    juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(bounds.toNearestInt());

    auto body = bounds;
    paintClipName(g, body.removeFromTop(nameBarHeight), clipColour,
                  juce::String::fromUTF8(project.clipLabel(*pattern).c_str()));

    // Miniature of the notes, one thin row per channel; repeats when the clip is longer.
    const int numChannels = static_cast<int>(project.channels.size());
    const int patternSteps = project.lengthInSteps(*pattern);
    if (numChannels > 0 && patternSteps > 0)
    {
        const float rowHeight = std::max(1.0f, (body.getHeight() - 2.0f) / numChannels);
        const float pixelsPerTick = view.pixelsPerStep() / model::ticksPerStep;
        const int patternTicks = patternSteps * model::ticksPerStep;
        const int clipTicks = clip.lengthInSteps * model::ticksPerStep;
        g.setColour(clipColour);
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

        g.setColour(clipColour.withAlpha(0.4f));
        for (int r = patternSteps; r < clip.lengthInSteps; r += patternSteps)
            g.fillRect(bounds.getX() + view.xForStep(r), body.getY(), 1.0f, body.getHeight());
    }
}

// ------------------------------------------------------------------------------------ Picker

// FL's picker panel: every pattern and audio clip. Click one to make it what clicking in the
// grid places; drag one into the grid to place it there.
class PlaylistView::Picker final : public juce::Component
{
  public:
    static constexpr int width = 130;
    static constexpr int rowHeight = 22;

    explicit Picker(PlaylistView& v) : view(v) {}

    void paint(juce::Graphics& g) override
    {
        const auto& t = currentTheme();
        g.fillAll(colour(t.colours.panel));
        g.setColour(colour(t.colours.outline));
        g.fillRect(getWidth() - 1, 0, 1, getHeight());
        const auto& project = view.doc.project();
        int y = 0;
        const auto heading = [&](const char* text)
        {
            g.setColour(colour(t.colours.textDim));
            g.setFont(semiboldFont(metrics::textSmall));
            g.drawText(text, 8, y, getWidth() - 12, rowHeight, juce::Justification::centredLeft);
            y += rowHeight;
        };
        const auto row = [&](const std::string& name, bool chosen, juce::Colour swatch)
        {
            if (chosen)
            {
                g.setColour(colour(t.colours.activeBackground));
                g.fillRect(2, y + 1, getWidth() - 5, rowHeight - 2);
            }
            g.setColour(swatch);
            g.fillRoundedRectangle(8.0f, y + 6.0f, 8.0f, rowHeight - 12.0f, 2.0f);
            g.setColour(colour(chosen ? t.colours.activeText : t.colours.text));
            g.setFont(juce::FontOptions(metrics::textLarge));
            g.drawText(juce::String::fromUTF8(name.c_str()), 22, y, getWidth() - 26, rowHeight,
                       juce::Justification::centredLeft, true);
            y += rowHeight;
        };
        heading("PATTERNS");
        for (const auto& pattern : project.patterns)
            row(pattern.name, view.brushAudio == 0 && pattern.id == project.currentPattern,
                colour(t.colours.clip));
        y += 6;
        heading("AUDIO CLIPS");
        bool any = false;
        for (const auto& channel : project.channels)
            if (channel.audioClip)
            {
                row(channel.name, view.brushAudio == channel.id, audioClipColour(t));
                any = true;
            }
        if (!any)
        {
            g.setColour(colour(t.colours.textDim));
            g.setFont(juce::FontOptions(metrics::textSmall));
            g.drawFittedText("Drop samples on the Playlist", 8, y, getWidth() - 12, rowHeight * 2,
                             juce::Justification::topLeft, 2);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        pressed = itemAt(e.y);
        if (e.mods.isPopupMenu() && pressed.startsWith("pattern:"))
        {
            // Right-click a pattern: what can be done with it here.
            const auto id = static_cast<model::PatternId>(
                pressed.fromFirstOccurrenceOf(":", false, false).getLargeIntValue());
            pressed.clear();
            juce::PopupMenu menu;
            menu.addItem(1, "Split by channel");
            juce::Component::SafePointer<Picker> self(this);
            menu.showMenuAsync(juce::PopupMenu::Options().withMousePosition(),
                               [self, id](int result)
                               {
                                   if (self != nullptr && result == 1 && self->view.onSplitPattern)
                                       self->view.onSplitPattern(id);
                               });
        }
    }

    // Double-click: a pattern opens in the Channel Rack, an audio clip in its Sampler.
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        const auto item = itemAt(e.y);
        const auto id = item.fromFirstOccurrenceOf(":", false, false).getLargeIntValue();
        if (item.startsWith("pattern:") && view.onOpenPattern)
            view.onOpenPattern(static_cast<model::PatternId>(id));
        else if (item.startsWith("audio:") && view.onOpenAudioChannel)
            view.onOpenAudioChannel(static_cast<model::ChannelId>(id));
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (pressed.isEmpty() || e.getDistanceFromDragStart() < 6 || dragging)
            return;
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
        {
            dragging = true;
            // Only the item follows the mouse (JUCE would show a snapshot of the whole picker).
            const juce::Point<int> offset(-10, -rowHeight / 2);
            // May cross windows: areas are native windows on Linux (app/area_stack).
            container->startDragging(pressed, this, dragImage(pressed), true, &offset);
        }
    }

    // What follows the mouse while an item is dragged: a chip like the clip it will place.
    juce::ScaledImage dragImage(const juce::String& item) const
    {
        const auto& t = currentTheme();
        const auto& project = view.doc.project();
        const auto id = item.fromFirstOccurrenceOf(":", false, false).getLargeIntValue();
        juce::String name;
        auto fill = colour(t.colours.clip);
        auto ink = colour(t.colours.clipText);
        if (item.startsWith("audio:"))
        {
            fill = audioClipColour(t);
            ink = colour(t.colours.text);
            for (const auto& channel : project.channels)
                if (channel.id == static_cast<model::ChannelId>(id))
                    name = juce::String::fromUTF8(channel.name.c_str());
        }
        else
        {
            for (const auto& pattern : project.patterns)
                if (pattern.id == static_cast<model::PatternId>(id))
                    name = juce::String::fromUTF8(pattern.name.c_str());
        }

        const auto font = juce::FontOptions(metrics::textLarge);
        const int w = std::clamp(textWidth(font, name) + 16, 48, 200);
        const float scale = juce::Desktop::getInstance().getGlobalScaleFactor();
        juce::Image image(juce::Image::ARGB, juce::roundToInt(static_cast<float>(w) * scale),
                          juce::roundToInt(static_cast<float>(rowHeight) * scale), true);
        juce::Graphics g(image);
        g.addTransform(juce::AffineTransform::scale(scale));
        const auto box = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(w),
                                                static_cast<float>(rowHeight));
        g.setColour(fill);
        g.fillRoundedRectangle(box, metrics::cornerRadius);
        g.setColour(colour(t.colours.outline));
        g.drawRoundedRectangle(box.reduced(0.5f), metrics::cornerRadius, 1.0f);
        g.setColour(ink);
        g.setFont(font);
        g.drawText(name, box.reduced(8.0f, 0.0f), juce::Justification::centredLeft, true);
        return {image, scale};
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (!dragging && pressed.isNotEmpty())
        {
            // Choose what clicking in the grid places.
            if (pressed.startsWith("audio:"))
                view.brushAudio = static_cast<model::ChannelId>(
                    pressed.fromFirstOccurrenceOf(":", false, false).getLargeIntValue());
            else
            {
                view.brushAudio = 0;
                const auto id = static_cast<model::PatternId>(
                    pressed.fromFirstOccurrenceOf(":", false, false).getLargeIntValue());
                if (view.doc.project().currentPattern != id)
                    view.doc.setViewState([id](model::Project& p) { p.currentPattern = id; });
            }
            repaint();
        }
        pressed.clear();
        dragging = false;
    }

  private:
    // "pattern:<id>" / "audio:<id>" for the row at y, or empty.
    juce::String itemAt(int y) const
    {
        const auto& project = view.doc.project();
        int row = y / rowHeight - 1; // after the PATTERNS heading
        if (row >= 0 && row < static_cast<int>(project.patterns.size()))
            return "pattern:" + juce::String(project.patterns[static_cast<std::size_t>(row)].id);
        row = (y - 6) / rowHeight - static_cast<int>(project.patterns.size()) - 2;
        for (const auto& channel : project.channels)
            if (channel.audioClip && row-- == 0)
                return "audio:" + juce::String(channel.id);
        return {};
    }

    PlaylistView& view;
    juce::String pressed;
    bool dragging = false;
};

// ------------------------------------------------------------------------------ PlaylistView

PlaylistView::PlaylistView(model::Document& document, engine::Transport& transportToShow)
    : doc(document), transport(transportToShow), ruler(std::make_unique<Ruler>(*this)),
      trackNames(std::make_unique<TrackNames>(*this)), grid(*this)
{

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

    picker = std::make_unique<Picker>(*this);
    addAndMakeVisible(*picker);
    pickerButton.setClickingTogglesState(true);
    pickerButton.setToggleState(true, juce::dontSendNotification);
    pickerButton.setTooltip("Show the picker: all patterns and audio clips");
    pickerButton.onClick = [this] { setPickerVisible(pickerButton.getToggleState()); };
    pickerButton.setWantsKeyboardFocus(false);
    pickerButton.setMouseClickGrabsKeyboardFocus(false);
    addAndMakeVisible(pickerButton);

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

void PlaylistView::setPickerVisible(bool shouldShow)
{
    picker->setVisible(shouldShow);
    pickerButton.setToggleState(shouldShow, juce::dontSendNotification);
    resized();
}

bool PlaylistView::isPickerVisible() const
{
    return picker->isVisible();
}

int PlaylistView::stepsFor(const model::Channel& channel) const
{
    if (channel.sample == nullptr || channel.sample->numFrames() == 0)
        return doc.project().stepsPerBar();
    const double seconds = doc.project().playedSeconds(channel); // after stretching / speed
    return std::max(1,
                    static_cast<int>(std::ceil(seconds / doc.project().secondsPerStep() - 1e-6)));
}

const Peaks& PlaylistView::peaksFor(const engine::SampleBuffer& sample)
{
    auto it = peakCache.find(&sample);
    if (it == peakCache.end())
        it = peakCache.emplace(&sample, computePeaks(sample)).first;
    return it->second;
}

void PlaylistView::placeAt(const juce::String& what, float x, float y)
{
    const int track = trackAtY(y);
    if (track < 0 || track >= doc.project().playlist.numTracks)
        return;
    const int snap = std::max(1, snapSteps());
    const int start = std::max(0, stepAtX(x) / snap * snap);
    const auto id = what.fromFirstOccurrenceOf(":", false, false);

    model::Clip clip;
    clip.track = track;
    clip.startStep = start;
    if (what.startsWith("pattern:"))
    {
        const auto* pattern =
            doc.project().findPattern(static_cast<model::PatternId>(id.getLargeIntValue()));
        if (pattern == nullptr)
            return;
        clip.pattern = pattern->id;
        clip.lengthInSteps = doc.project().lengthInSteps(*pattern);
        doc.apply([clip](model::Project& p) { p.playlist.clips.push_back(clip); });
        return;
    }
    if (what.startsWith("audio:"))
    {
        const auto* channel =
            doc.project().findChannel(static_cast<model::ChannelId>(id.getLargeIntValue()));
        if (channel == nullptr)
            return;
        clip.audio = channel->id;
        clip.lengthInSteps = stepsFor(*channel);
        doc.apply([clip](model::Project& p) { p.playlist.clips.push_back(clip); });
        return;
    }
    if (what.startsWith("file:") && loadSample)
    {
        // A sample: its audio-clip channel (one per file), and a clip as long as the sample.
        const juce::File file(id);
        auto sample = loadSample(file);
        if (sample == nullptr)
            return;
        const auto path = file.getFullPathName().toStdString();
        const auto name = file.getFileNameWithoutExtension().toStdString();
        doc.apply(
            [&, sample](model::Project& p)
            {
                clip.audio = p.audioClipChannel(path, name, sample);
                clip.lengthInSteps = stepsFor(*p.findChannel(clip.audio));
                p.playlist.clips.push_back(clip);
            });
        brushAudio = clip.audio; // clicking places more of it, as in FL
        picker->repaint();
    }
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
        std::erase_if(pasted,
                      [this](const model::Clip& c)
                      {
                          return c.isAudio() ? doc.project().findChannel(c.audio) == nullptr
                                             : doc.project().findPattern(c.pattern) == nullptr;
                      });
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
    if (action == EditAction::duplicate)
    {
        // The copies land right after the selection's bars and become the selection; the
        // clipboard is untouched.
        auto block =
            model::copyClips(doc.project().playlist.clips, selection, doc.project().stepsPerBar());
        const auto copies = model::nextPaste(block);
        const int first = static_cast<int>(doc.project().playlist.clips.size());
        doc.apply(
            [copies](model::Project& p)
            { p.playlist.clips.insert(p.playlist.clips.end(), copies.begin(), copies.end()); });
        selection.clear();
        for (int i = 0; i < static_cast<int>(copies.size()); ++i)
            selection.push_back(first + i);
        grid.repaint();
        return true;
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
    if (doc.project().findChannel(brushAudio) == nullptr)
        brushAudio = 0;
    picker->repaint();
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
    paintPanelHeader(g, getLocalBounds().removeFromTop(headerHeight), "Playlist", {},
                     pickerButton.getX() - metrics::controlGap);
    paintHeaderLabel(g, "Snap", snapLabelArea);
    g.setColour(colour(t.colours.outline));
    g.fillRect(0, headerHeight + rulerHeight - 1, trackNameWidth, 1);
}

void PlaylistView::resized()
{
    auto area = getLocalBounds();
    HeaderLayout header(area.removeFromTop(headerHeight));
    snapBox.setBounds(header.fromRight(metrics::comboWidth));
    snapLabelArea = header.fromRight(headerLabelWidth("Snap"));
    // Right after the title.
    header.fromLeft(textWidth(semiboldFont(metrics::textLarge), "Playlist") + metrics::controlGap);
    pickerButton.setBounds(header.fromLeft(pickerButton.preferredWidth(metrics::controlHeight)));

    if (picker->isVisible())
        picker->setBounds(area.removeFromLeft(Picker::width));
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

    // Repaint just the old and new playhead strips (the paint draws it at lastPlayheadX).
    const float previous = lastPlayheadX;
    lastPlayheadX = x;
    for (float px : {previous, x})
        if (px >= 0.0f)
            grid.repaint(static_cast<int>(std::floor(px)) - 3, 0, 7, grid.getHeight());
    ruler->repaint();
}

} // namespace jls::ui
