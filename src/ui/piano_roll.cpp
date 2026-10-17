#include "ui/piano_roll.h"

#include "ui/theme.h"

#include <cmath>

namespace jls::ui
{
namespace
{

constexpr float edgeGrabWidth = 6.0f;
constexpr int minimumBars = 4;
constexpr int spareBars = 2;

enum SnapId
{
    snapBeat = 1,
    snapStep,
    snapHalfStep,
    snapThirdStep,
    snapNone,
};

bool isBlackKey(int pitch)
{
    switch (pitch % 12)
    {
    case 1:
    case 3:
    case 6:
    case 8:
    case 10:
        return true;
    default:
        return false;
    }
}

} // namespace

juce::String noteName(int pitch)
{
    static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return juce::String(names[pitch % 12]) + juce::String(pitch / 12);
}

// ------------------------------------------------------------------------ Keyboard / Ruler

class PianoRollView::Keyboard final : public juce::Component
{
  public:
    explicit Keyboard(PianoRollView& v) : view(v) {}

    // Clicking a key plays it while the button is held; dragging slides to other keys.
    void mouseDown(const juce::MouseEvent& e) override { play(e); }
    void mouseDrag(const juce::MouseEvent& e) override { play(e); }
    void mouseUp(const juce::MouseEvent&) override { view.stopAudition(); }

    void paint(juce::Graphics& g) override
    {
        const auto& t = currentTheme();
        const int offset = view.viewport.getViewPositionY();
        g.fillAll(colour(t.colours.panel));
        g.setFont(juce::FontOptions(9.5f));
        for (int pitch = 0; pitch < numPitches; ++pitch)
        {
            const float y = view.yForPitch(pitch) - offset;
            if (y > getHeight() || y < -rowHeight)
                continue;
            auto row = juce::Rectangle<float>(0.0f, y, static_cast<float>(getWidth()),
                                              static_cast<float>(rowHeight));
            const bool black = isBlackKey(pitch);
            g.setColour(juce::Colour(0xffeeeeee));
            g.fillRect(row);
            if (black)
            {
                g.setColour(juce::Colour(0xff1a1a1a));
                g.fillRect(row.withTrimmedRight(row.getWidth() * 0.35f));
            }
            if (pitch == view.auditionPitch)
            {
                g.setColour(colour(t.colours.accent));
                g.fillRect(row.withTrimmedRight(black ? row.getWidth() * 0.35f : 0.0f));
            }
            g.setColour(juce::Colours::black.withAlpha(0.25f));
            g.fillRect(row.removeFromBottom(1.0f));
            // Every key is labelled (in the white part, right of a black key); C stands out,
            // and notes outside the highlighted scale are faded.
            const auto& project = view.doc.project();
            const bool inScale = model::isInScale(pitch, project.scaleRoot, project.scale);
            g.setColour(juce::Colour(pitch % 12 == 0 ? 0xff222222 : 0xff666666)
                            .withMultipliedAlpha(inScale ? 1.0f : 0.35f));
            g.drawText(
                noteName(pitch),
                juce::Rectangle<float>(0.0f, y, getWidth() - 4.0f, static_cast<float>(rowHeight)),
                juce::Justification::centredRight);
        }
    }

  private:
    void play(const juce::MouseEvent& e)
    {
        const int pitch = view.pitchAtY(e.position.y + view.viewport.getViewPositionY());
        if (pitch >= 0 && pitch < numPitches)
            view.audition(pitch, model::Note::defaultVelocity);
    }

    PianoRollView& view;
};

class PianoRollView::Ruler final : public juce::Component, public juce::SettableTooltipClient
{
  public:
    explicit Ruler(PianoRollView& v) : view(v)
    {
        setTooltip("Click or drag to move the playhead (switches to PAT mode)");
    }

    // Clicking (or dragging along) the ruler moves the playhead there, snapped.
    void mouseDown(const juce::MouseEvent& e) override { seek(e); }
    void mouseDrag(const juce::MouseEvent& e) override { seek(e); }

    void paint(juce::Graphics& g) override
    {
        const auto& t = currentTheme();
        g.fillAll(colour(t.colours.panel));
        g.setColour(colour(t.colours.outline));
        g.fillRect(0, getHeight() - 1, getWidth(), 1);

        const auto& project = view.doc.project();
        const int barTicks = project.stepsPerBar() * model::ticksPerStep;
        const float offset = static_cast<float>(view.viewport.getViewPositionX());
        g.setFont(juce::FontOptions(11.0f));
        for (int bar = 0; view.xForTick(bar * barTicks) - offset < getWidth(); ++bar)
        {
            const float x = view.xForTick(bar * barTicks) - offset;
            if (x < -40.0f)
                continue;
            g.setColour(colour(t.colours.grid));
            g.fillRect(x, 0.0f, 1.0f, static_cast<float>(getHeight()));
            g.setColour(colour(t.colours.textDim));
            g.drawText(
                juce::String(bar + 1),
                juce::Rectangle<float>(x + 4.0f, 0.0f, 40.0f, static_cast<float>(getHeight())),
                juce::Justification::centredLeft);
        }

        // Pattern end marker.
        const float end =
            view.xForTick(project.lengthInSteps(project.current()) * model::ticksPerStep) - offset;
        g.setColour(colour(t.colours.accent).withAlpha(0.6f));
        g.fillRect(end - 1.0f, 0.0f, 2.0f, static_cast<float>(getHeight()));

        if (const double tick = view.playheadTick(); tick >= 0.0)
        {
            const float x = view.xForTick(tick) - offset;
            juce::Path marker;
            marker.addTriangle(x - 5.0f, 0.0f, x + 5.0f, 0.0f, x, 8.0f);
            g.setColour(colour(t.colours.accent));
            g.fillPath(marker);
        }
    }

  private:
    void seek(const juce::MouseEvent& e)
    {
        const int tick = view.snapDown(
            view.tickAtX(e.position.x + static_cast<float>(view.viewport.getViewPositionX())));
        // The Piano Roll shows the pattern, so jumping in it means pattern mode.
        if (view.doc.project().songMode)
            view.doc.setViewState([](model::Project& p) { p.songMode = false; });
        view.transport.seek(static_cast<double>(std::max(0, tick)) / model::ticksPerBeat);
    }

    PianoRollView& view;
};

// Note velocities as stems; click or drag to set the velocity of the notes under the mouse.
class PianoRollView::VelocityLane final : public juce::Component
{
  public:
    explicit VelocityLane(PianoRollView& v) : view(v) {}

    void paint(juce::Graphics& g) override
    {
        const auto& t = currentTheme();
        g.fillAll(colour(t.colours.display));
        g.setColour(colour(t.colours.outline));
        g.fillRect(0, 0, getWidth(), 1);

        const float offset = static_cast<float>(view.viewport.getViewPositionX());
        const float usable = static_cast<float>(getHeight() - 8);
        for (const auto& note : view.notes())
        {
            const float x = view.xForTick(note.startTick) - offset;
            if (x < -4.0f || x > getWidth() + 4.0f)
                continue;
            const float h = usable * note.velocity;
            const float top = getHeight() - 2.0f - h;
            g.setColour(colour(t.colours.accent));
            g.fillRect(x, top, 2.0f, h);
            g.fillEllipse(x - 3.0f, top - 3.0f, 8.0f, 8.0f);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        gestureId = view.doc.newGestureId();
        setVelocity(e);
    }

    void mouseDrag(const juce::MouseEvent& e) override { setVelocity(e); }

  private:
    void setVelocity(const juce::MouseEvent& e)
    {
        const float offset = static_cast<float>(view.viewport.getViewPositionX());
        const float velocity = juce::jlimit(
            0.0f, 1.0f, (getHeight() - 2.0f - e.position.y) / static_cast<float>(getHeight() - 8));
        const float x = e.position.x;

        // The nearest stem within a few pixels, plus any notes starting at the same time
        // (a chord's stems overlap, so they're set together).
        const auto& notes = view.notes();
        float best = 8.0f;
        int bestTick = -1;
        for (const auto& note : notes)
        {
            const float distance = std::abs(view.xForTick(note.startTick) - offset - x);
            if (distance <= best)
            {
                best = distance;
                bestTick = note.startTick;
            }
        }
        if (bestTick < 0)
            return;
        std::vector<std::size_t> hit;
        for (std::size_t i = 0; i < notes.size(); ++i)
            if (notes[i].startTick == bestTick)
                hit.push_back(i);

        view.editNotes(
            [&hit, velocity](std::vector<model::Note>& list)
            {
                for (auto i : hit)
                    list[i].velocity = velocity;
            },
            gestureId);
    }

    PianoRollView& view;
    int gestureId = 0;
};

// ---------------------------------------------------------------------------------- NoteGrid

NoteGrid::NoteGrid(PianoRollView& owner) : view(owner)
{
    setWantsKeyboardFocus(false);
    setMouseClickGrabsKeyboardFocus(false);
}

int NoteGrid::noteAt(juce::Point<float> p) const
{
    const auto& notes = view.notes();
    const int pitch = view.pitchAtY(p.y);
    for (int i = static_cast<int>(notes.size()) - 1; i >= 0; --i)
    {
        const auto& n = notes[static_cast<std::size_t>(i)];
        if (n.pitch == pitch && p.x >= view.xForTick(n.startTick) &&
            p.x < view.xForTick(n.endTick()))
            return i;
    }
    return -1;
}

bool NoteGrid::onRightEdge(int index, float x) const
{
    if (index < 0)
        return false;
    const auto& n = view.notes()[static_cast<std::size_t>(index)];
    const float end = view.xForTick(n.endTick());
    const float grab = std::min(edgeGrabWidth, (end - view.xForTick(n.startTick)) / 3.0f);
    return x >= end - grab;
}

void NoteGrid::mouseMove(const juce::MouseEvent& e)
{
    setMouseCursor(onRightEdge(noteAt(e.position), e.position.x)
                       ? juce::MouseCursor::LeftRightResizeCursor
                       : juce::MouseCursor::NormalCursor);
}

juce::Rectangle<float> NoteGrid::noteBounds(const model::Note& note) const
{
    return juce::Rectangle<float>(view.xForTick(note.startTick), view.yForPitch(note.pitch),
                                  view.xForTick(note.lengthTicks), PianoRollView::rowHeight)
        .reduced(0.5f, 1.0f);
}

void NoteGrid::eraseAt(juce::Point<float> p)
{
    const int index = noteAt(p);
    if (index >= 0)
        view.clearSelection(); // indices are about to shift
    if (index >= 0)
        view.editNotes([index](std::vector<model::Note>& list)
                       { list.erase(list.begin() + index); }, gestureId);
}

void NoteGrid::mouseDown(const juce::MouseEvent& e)
{
    if (view.channel() == 0)
        return;
    gestureId = view.document().newGestureId();

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

    dragNote = noteAt(e.position);
    if (dragNote >= 0 && view.isSelected(dragNote))
    {
        const auto& note = view.notes()[static_cast<std::size_t>(dragNote)];
        drag = onRightEdge(dragNote, e.position.x) ? Drag::resizeSelection : Drag::moveSelection;
        moveOriginals.clear();
        for (int i : view.selection)
            moveOriginals.push_back(view.notes()[static_cast<std::size_t>(i)]);
        grabTick = view.tickAtX(e.position.x);
        grabPitch = view.pitchAtY(e.position.y);
        grabNotePitch = note.pitch;
        view.audition(note.pitch, note.velocity);
        return;
    }
    if (!view.selection.empty())
    {
        // A click outside the selection only deselects (FL does the same).
        view.clearSelection();
        drag = Drag::none;
        dragNote = -1;
        return;
    }
    if (dragNote >= 0)
    {
        const auto& note = view.notes()[static_cast<std::size_t>(dragNote)];
        drag = onRightEdge(dragNote, e.position.x) ? Drag::resize : Drag::move;
        grabOffsetTicks = view.tickAtX(e.position.x) - note.startTick;
        grabPitch = note.pitch;
        view.audition(note.pitch, note.velocity);
        return;
    }

    // Empty space: add a note here, then keep dragging it.
    model::Note note;
    note.startTick = view.snapDown(view.tickAtX(e.position.x));
    note.pitch = juce::jlimit(0, PianoRollView::numPitches - 1, view.pitchAtY(e.position.y));
    note.lengthTicks = view.newNoteLength;
    view.editNotes([note](std::vector<model::Note>& list) { list.push_back(note); }, gestureId);
    dragNote = static_cast<int>(view.notes().size()) - 1;
    drag = Drag::move;
    grabOffsetTicks = view.tickAtX(e.position.x) - note.startTick;
    grabPitch = note.pitch;
    view.audition(note.pitch, note.velocity);
}

void NoteGrid::mouseDrag(const juce::MouseEvent& e)
{
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
    if (drag == Drag::moveSelection || drag == Drag::resizeSelection)
    {
        const int snapTicks = std::max(1, view.snapTicks());
        const int ticks =
            static_cast<int>(std::lround(
                static_cast<double>(view.tickAtX(e.position.x) - grabTick) / snapTicks)) *
            snapTicks;
        auto moved = moveOriginals;
        if (drag == Drag::moveSelection)
        {
            // One offset for all, limited so every note stays on the grid.
            int dt = ticks;
            int dp = view.pitchAtY(e.position.y) - grabPitch;
            for (const auto& n : moveOriginals)
            {
                dt = std::max(dt, -n.startTick);
                dp = std::clamp(dp, -n.pitch, PianoRollView::numPitches - 1 - n.pitch);
            }
            for (auto& n : moved)
            {
                n.startTick += dt;
                n.pitch += dp;
            }
            view.audition(grabNotePitch + dp, moveOriginals.front().velocity);
        }
        else
        {
            for (auto& n : moved)
                n.lengthTicks = std::max(snapTicks, n.lengthTicks + ticks);
        }
        const auto selected = view.selection;
        view.editNotes(
            [selected, moved](std::vector<model::Note>& list)
            {
                for (std::size_t i = 0; i < selected.size(); ++i)
                    if (selected[i] < static_cast<int>(list.size()))
                        list[static_cast<std::size_t>(selected[i])] = moved[i];
            },
            gestureId);
        return;
    }
    const auto& notes = view.notes();
    if (dragNote < 0 || dragNote >= static_cast<int>(notes.size()))
        return;

    auto note = notes[static_cast<std::size_t>(dragNote)];
    const auto original = note;
    if (drag == Drag::move)
    {
        note.startTick = std::max(0, view.snap(view.tickAtX(e.position.x) - grabOffsetTicks));
        note.pitch = juce::jlimit(0, PianoRollView::numPitches - 1, view.pitchAtY(e.position.y));
    }
    else if (drag == Drag::resize)
    {
        const int end = view.snap(view.tickAtX(e.position.x));
        note.lengthTicks = std::max(view.snapTicks(), end - note.startTick);
        view.newNoteLength = note.lengthTicks;
    }

    if (note.pitch != original.pitch)
        view.audition(note.pitch, note.velocity); // hear where the note is going
    if (note == original)
        return;
    const int index = dragNote;
    view.editNotes([index, note](std::vector<model::Note>& list)
                   { list[static_cast<std::size_t>(index)] = note; }, gestureId);
}

void NoteGrid::mouseUp(const juce::MouseEvent&)
{
    if (drag == Drag::select)
    {
        // Every note the rectangle touches.
        const auto band = juce::Rectangle<float>(bandStart, bandEnd);
        view.selection.clear();
        const auto& notes = view.notes();
        for (std::size_t i = 0; i < notes.size(); ++i)
            if (band.intersects(noteBounds(notes[i])))
                view.selection.push_back(static_cast<int>(i));
        repaint();
    }
    drag = Drag::none;
    dragNote = -1;
    view.stopAudition();
}

void NoteGrid::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown())
    {
        view.setZoom(view.pixelsPerTick() * (w.deltaY > 0 ? 1.25f : 0.8f), e.position.x);
        return;
    }
    Component::mouseWheelMove(e, w);
}

void NoteGrid::paint(juce::Graphics& g)
{
    const auto& t = currentTheme();
    const auto& project = view.document().project();
    const auto clip = g.getClipBounds();

    // Rows: black-key rows a little darker, a stronger line under each B (octave boundary).
    for (int pitch = 0; pitch < PianoRollView::numPitches; ++pitch)
    {
        const float y = view.yForPitch(pitch);
        if (y > clip.getBottom() || y + PianoRollView::rowHeight < clip.getY())
            continue;
        // Without a scale: black-key rows a little darker. With one: notes outside it darker,
        // its notes lighter, and the root tinted with the accent.
        const auto lane = colour(t.colours.lane);
        juce::Colour rowColour = isBlackKey(pitch) ? lane.darker(0.25f) : lane;
        if (project.scale != model::Scale::none)
        {
            if (!model::isInScale(pitch, project.scaleRoot, project.scale))
                rowColour = lane.darker(0.45f);
            else if ((pitch - project.scaleRoot) % 12 == 0)
                rowColour = lane.interpolatedWith(colour(t.colours.accent), 0.14f);
            else
                rowColour = lane.brighter(0.12f);
        }
        g.setColour(rowColour);
        g.fillRect(static_cast<float>(clip.getX()), y, static_cast<float>(clip.getWidth()),
                   static_cast<float>(PianoRollView::rowHeight));
        if (pitch % 12 == 0)
        {
            g.setColour(colour(t.colours.grid));
            g.fillRect(static_cast<float>(clip.getX()), y + PianoRollView::rowHeight - 1.0f,
                       static_cast<float>(clip.getWidth()), 1.0f);
        }
    }

    // Columns: bars, beats, and steps when there's room.
    const int barTicks = project.stepsPerBar() * model::ticksPerStep;
    const bool showSteps = view.xForTick(model::ticksPerStep) >= 8.0f;
    const int first = view.tickAtX(static_cast<float>(clip.getX())) / model::ticksPerStep;
    const int last = view.tickAtX(static_cast<float>(clip.getRight())) / model::ticksPerStep + 1;
    for (int step = first; step <= last; ++step)
    {
        const int tick = step * model::ticksPerStep;
        float alpha = 0.2f;
        if (tick % barTicks == 0)
            alpha = 1.0f;
        else if (tick % model::ticksPerBeat == 0)
            alpha = 0.5f;
        else if (!showSteps)
            continue;
        g.setColour(colour(t.colours.grid).withMultipliedAlpha(alpha));
        g.fillRect(view.xForTick(tick), static_cast<float>(clip.getY()), 1.0f,
                   static_cast<float>(clip.getHeight()));
    }

    // Dim everything after the pattern's end.
    const float end = view.xForTick(project.lengthInSteps(project.current()) * model::ticksPerStep);
    if (end < clip.getRight())
    {
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.fillRect(end, static_cast<float>(clip.getY()), clip.getRight() - end,
                   static_cast<float>(clip.getHeight()));
    }

    // Notes: brighter for higher velocity.
    const float radius = std::min(3.0f, t.shape.cornerRadius);
    g.setFont(juce::FontOptions(10.0f));
    const auto& notes = view.notes();
    for (std::size_t i = 0; i < notes.size(); ++i)
    {
        const auto& note = notes[i];
        const auto r = noteBounds(note);
        if (!clip.toFloat().intersects(r))
            continue;
        const bool selected = view.isSelected(static_cast<int>(i));
        g.setColour(colour(t.colours.stepOn).withMultipliedBrightness(0.6f + 0.4f * note.velocity));
        g.fillRoundedRectangle(r, radius);
        g.setColour(selected ? colour(t.colours.accentAlt)
                             : colour(t.colours.outline).withAlpha(0.8f));
        g.drawRoundedRectangle(selected ? r.reduced(0.5f) : r, radius, selected ? 2.0f : 1.0f);
        if (r.getWidth() > 28.0f)
        {
            g.setColour(colour(t.colours.clipText));
            g.drawText(noteName(note.pitch), r.reduced(3.0f, 0.0f),
                       juce::Justification::centredLeft, false);
        }
    }

    if (drag == Drag::select)
    {
        const auto band = juce::Rectangle<float>(bandStart, bandEnd);
        g.setColour(colour(t.colours.accentAlt).withAlpha(0.15f));
        g.fillRect(band);
        g.setColour(colour(t.colours.accentAlt));
        g.drawRect(band, 1.0f);
    }

    if (view.channel() == 0)
    {
        g.setColour(colour(t.colours.textDim));
        g.drawText("Select a channel in the Channel Rack to edit its notes", getLocalBounds(),
                   juce::Justification::centred);
    }

    if (const double tick = view.playheadTick(); tick >= 0.0)
    {
        g.setColour(colour(t.colours.accent));
        g.fillRect(view.xForTick(tick) - 1.0f, static_cast<float>(clip.getY()), 2.0f,
                   static_cast<float>(clip.getHeight()));
    }
}

// ----------------------------------------------------------------------------- PianoRollView

PianoRollView::PianoRollView(model::Document& document, engine::Transport& transportToShow)
    : doc(document), transport(transportToShow), keyboard(std::make_unique<Keyboard>(*this)),
      ruler(std::make_unique<Ruler>(*this)), velocityLane(std::make_unique<VelocityLane>(*this)),
      grid(*this)
{
    title.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(title);
    addAndMakeVisible(snapLabel);

    snapBox.addItem("Beat", snapBeat);
    snapBox.addItem("Step", snapStep);
    snapBox.addItem(juce::String::fromUTF8("\xc2\xbd step"), snapHalfStep);
    snapBox.addItem(juce::String::fromUTF8("\xe2\x85\x93 step"), snapThirdStep);
    snapBox.addItem("None", snapNone);
    snapBox.setSelectedId(snapStep, juce::dontSendNotification);
    snapBox.setWantsKeyboardFocus(false);
    addAndMakeVisible(snapBox);

    // Scale highlight: Off / Major / Minor, on a root note.
    addAndMakeVisible(scaleLabel);
    scaleBox.addItem("Off", 1);
    scaleBox.addItem("Major", 2);
    scaleBox.addItem("Minor", 3);
    scaleBox.setTooltip("Highlight the notes of a scale");
    rootBox.setTooltip("Root note of the scale");
    for (int root = 0; root < 12; ++root)
        rootBox.addItem(noteName(root + 60).dropLastCharacters(1), root + 1);
    const auto setScale = [this]
    {
        const auto scale = static_cast<model::Scale>(std::max(0, scaleBox.getSelectedId() - 1));
        const int root = std::max(0, rootBox.getSelectedId() - 1);
        const auto& p = doc.project();
        if (scale != p.scale || root != p.scaleRoot)
            doc.setViewState(
                [scale, root](model::Project& project)
                {
                    project.scale = scale;
                    project.scaleRoot = root;
                });
    };
    scaleBox.onChange = setScale;
    rootBox.onChange = setScale;
    for (auto* box : {&scaleBox, &rootBox})
    {
        box->setWantsKeyboardFocus(false);
        addAndMakeVisible(*box);
    }

    viewport.setViewedComponent(&grid, false);
    viewport.setScrollBarsShown(true, true);
    viewport.onScroll = [this]
    {
        keyboard->repaint();
        ruler->repaint();
        velocityLane->repaint();
    };
    addAndMakeVisible(viewport);
    addAndMakeVisible(*keyboard);
    addAndMakeVisible(*ruler);
    addAndMakeVisible(*velocityLane);

    refresh();
    startTimerHz(30);
}

PianoRollView::~PianoRollView() = default;

const std::vector<model::Note>& PianoRollView::notes() const
{
    return doc.project().current().notesFor(channel());
}

void PianoRollView::editNotes(const std::function<void(std::vector<model::Note>&)>& change,
                              int gestureId)
{
    const auto id = channel();
    if (id == 0)
        return;
    doc.apply([id, &change](model::Project& p) { change(p.current().notes[id]); }, gestureId);
}

int PianoRollView::tickAtX(float x) const
{
    return std::max(0, static_cast<int>(std::floor(x / zoom)));
}

int PianoRollView::pitchAtY(float y) const
{
    return numPitches - 1 - static_cast<int>(std::floor(y / rowHeight));
}

int PianoRollView::snapTicks() const
{
    switch (snapBox.getSelectedId())
    {
    case snapBeat:
        return model::ticksPerBeat;
    case snapHalfStep:
        return model::ticksPerStep / 2;
    case snapThirdStep:
        return model::ticksPerStep / 3;
    case snapNone:
        return 1;
    default:
        return model::ticksPerStep;
    }
}

int PianoRollView::snap(double tick) const
{
    const int s = snapTicks();
    return static_cast<int>(std::lround(tick / s)) * s;
}

int PianoRollView::snapDown(double tick) const
{
    const int s = snapTicks();
    return static_cast<int>(std::floor(tick / s)) * s;
}

double PianoRollView::playheadTick() const
{
    const auto& project = doc.project();
    if (project.songMode)
        return -1.0;
    const double position = transport.positionInBeats() * model::ticksPerBeat;
    if (!transport.isPlaying() && position <= 0.0)
        return -1.0;
    const double length = project.lengthInSteps(project.current()) * model::ticksPerStep;
    return length > 0.0 ? std::fmod(position, length) : -1.0;
}

void PianoRollView::audition(int pitch, float velocity)
{
    if (pitch == auditionPitch)
        return;
    stopAudition();
    auditionPitch = pitch;
    if (onAuditionStart)
        onAuditionStart(pitch, velocity);
    keyboard->repaint();
}

void PianoRollView::stopAudition()
{
    if (auditionPitch < 0)
        return;
    if (onAuditionStop)
        onAuditionStop(auditionPitch);
    auditionPitch = -1;
    keyboard->repaint();
}

bool PianoRollView::isSelected(int note) const
{
    return std::find(selection.begin(), selection.end(), note) != selection.end();
}

void PianoRollView::clearSelection()
{
    if (!selection.empty())
    {
        selection.clear();
        grid.repaint();
    }
}

bool PianoRollView::keyCommand(const juce::KeyPress& key)
{
    if (channel() == 0)
        return false;
    if (key == juce::KeyPress('a', juce::ModifierKeys::commandModifier, 0))
    {
        selection.clear();
        for (int i = 0; i < static_cast<int>(notes().size()); ++i)
            selection.push_back(i);
        grid.repaint();
        return true;
    }
    if (selection.empty())
        return false;
    if (key == juce::KeyPress::escapeKey)
    {
        clearSelection();
        return true;
    }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        auto doomed = selection;
        std::sort(doomed.begin(), doomed.end(), std::greater<>());
        selection.clear();
        editNotes(
            [doomed](std::vector<model::Note>& list)
            {
                for (int i : doomed)
                    if (i < static_cast<int>(list.size()))
                        list.erase(list.begin() + i);
            },
            0);
        return true;
    }
    return false;
}

void PianoRollView::refresh()
{
    const auto& project = doc.project();
    // The selection belongs to one channel's notes in one pattern.
    if (selectionChannel != channel() || selectionPattern != project.currentPattern)
    {
        selection.clear();
        selectionChannel = channel();
        selectionPattern = project.currentPattern;
    }
    const int count = static_cast<int>(notes().size());
    std::erase_if(selection, [count](int i) { return i >= count; });
    const auto* ch = project.findChannel(channel());
    juce::String text("Piano roll");
    if (ch != nullptr)
        text << juce::String::fromUTF8(" \xe2\x80\x93 ") << juce::String::fromUTF8(ch->name.c_str())
             << juce::String::fromUTF8(" \xc2\xb7 ")
             << juce::String::fromUTF8(project.current().name.c_str());
    title.setText(text, juce::dontSendNotification);
    scaleBox.setSelectedId(static_cast<int>(project.scale) + 1, juce::dontSendNotification);
    rootBox.setSelectedId(project.scaleRoot + 1, juce::dontSendNotification);
    rootBox.setEnabled(project.scale != model::Scale::none);
    updateGridSize();
    grid.repaint();
    keyboard->repaint();
    ruler->repaint();
    velocityLane->repaint();

    if (channel() != scrolledFor && isShowing())
        scrollToNotes();
}

void PianoRollView::scrollToNotes()
{
    scrolledFor = channel();
    const auto& list = notes();
    int pitch = model::Note::defaultPitch;
    if (!list.empty())
    {
        int sum = 0;
        for (const auto& n : list)
            sum += n.pitch;
        pitch = sum / static_cast<int>(list.size());
    }
    const int y = static_cast<int>(yForPitch(pitch)) - viewport.getHeight() / 2;
    viewport.setViewPosition(viewport.getViewPositionX(), std::max(0, y));
}

void PianoRollView::updateGridSize()
{
    const auto& project = doc.project();
    const int barTicks = project.stepsPerBar() * model::ticksPerStep;
    const int patternTicks = project.lengthInSteps(project.current()) * model::ticksPerStep;
    const int bars = std::max(minimumBars, patternTicks / barTicks + spareBars);
    const int width = static_cast<int>(std::ceil(xForTick(bars * barTicks)));
    grid.setSize(std::max(width, viewport.getMaximumVisibleWidth()), numPitches * rowHeight);
}

void PianoRollView::setZoom(float pixels, float anchorGridX)
{
    const float clamped =
        juce::jlimit(2.0f / model::ticksPerStep, 96.0f / model::ticksPerStep, pixels);
    if (std::abs(clamped - zoom) < 1.0e-4f)
        return;
    const float anchorTick = anchorGridX / zoom;
    const float onScreen = anchorGridX - static_cast<float>(viewport.getViewPositionX());
    zoom = clamped;
    updateGridSize();
    viewport.setViewPosition(std::max(0, static_cast<int>(anchorTick * zoom - onScreen)),
                             viewport.getViewPositionY());
    refresh();
}

void PianoRollView::paint(juce::Graphics& g)
{
    const auto& t = currentTheme();
    g.fillAll(colour(t.colours.panel));
    auto header = getLocalBounds().removeFromTop(headerHeight);
    g.setColour(colour(t.colours.header));
    g.fillRect(header);
    g.setColour(colour(t.colours.outline));
    g.fillRect(header.removeFromBottom(1));
}

void PianoRollView::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop(headerHeight).reduced(10, 4);
    snapBox.setBounds(header.removeFromRight(90));
    snapLabel.setBounds(header.removeFromRight(44));
    header.removeFromRight(12);
    scaleBox.setBounds(header.removeFromRight(84));
    rootBox.setBounds(header.removeFromRight(58));
    scaleLabel.setBounds(header.removeFromRight(46));
    title.setBounds(header);

    auto top = area.removeFromTop(rulerHeight);
    top.removeFromLeft(keyboardWidth);
    ruler->setBounds(top);

    auto bottom = area.removeFromBottom(velocityHeight);
    bottom.removeFromLeft(keyboardWidth);
    velocityLane->setBounds(bottom);

    keyboard->setBounds(area.removeFromLeft(keyboardWidth));
    viewport.setBounds(area);
    updateGridSize();

    if (scrolledFor == 0 && isShowing())
        scrollToNotes();
}

void PianoRollView::timerCallback()
{
    const double tick = playheadTick();
    const float x = tick >= 0.0 ? xForTick(tick) : -1.0f;
    if (std::abs(x - lastPlayheadX) < 0.5f)
        return;
    for (float px : {lastPlayheadX, x})
        if (px >= 0.0f)
            grid.repaint(static_cast<int>(px) - 2, 0, 5, grid.getHeight());
    lastPlayheadX = x;
    ruler->repaint();
}

} // namespace jls::ui
