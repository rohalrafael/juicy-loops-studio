#include "ui/channel_rack.h"

#include "engine/step_sequencer.h"
#include "ui/browser.h"
#include "ui/floating_panel.h"
#include "ui/theme.h"

#include <algorithm>
#include <cmath>

namespace jls::ui
{
namespace
{

// Keep keyboard focus on the main window so Space etc. always reach the transport.
void noFocus(juce::Component& c)
{
    c.setWantsKeyboardFocus(false);
    c.setMouseClickGrabsKeyboardFocus(false);
}

constexpr int padding = 8;
constexpr int headerHeight = metrics::panelHeaderHeight; // title bar; drag it to move the rack
constexpr int handleWidth = 12; // the length handle after the last step column
constexpr int stepMargin = 3;   // above and below the step keys (and a note preview)

// A row, left to right (see ChannelRow::resized): mute LED, pan and volume knobs, mixer track,
// name, loop switch, steps.
constexpr int columnGap = 4;  // after the LED, each knob and the mixer track
constexpr int loopWidth = 20; // a row's loop switch, after the steps
constexpr int nameX = metrics::ledWidth + columnGap + 2 * (metrics::knobSize + columnGap) +
                      metrics::insertWidth + columnGap;
// Everything in a row except the steps.
constexpr int rowFixedWidth = nameX + metrics::nameWidth + loopWidth;

} // namespace

// ---------------------------------------------------------------------------------- InsertBox

void InsertBox::setValue(int track)
{
    if (track != value)
    {
        value = track;
        repaint();
    }
}

void InsertBox::paint(juce::Graphics& g)
{
    const auto& t = currentTheme();
    const auto r = getLocalBounds().toFloat().reduced(1.0f, 5.0f);
    g.setColour(colour(t.colours.display));
    g.fillRoundedRectangle(r, metrics::cornerRadius);
    g.setColour(colour(value == 0 ? t.colours.displayDim : t.colours.displayText));
    g.setFont(juce::FontOptions(metrics::textSmall));
    g.drawText(value == 0 ? juce::String("--") : juce::String(value), r,
               juce::Justification::centred);
}

void InsertBox::change(int track, int gesture)
{
    track = juce::jlimit(0, model::Mixer::numInserts, track);
    if (track != value && onChange)
        onChange(track, gesture);
}

void InsertBox::mouseDown(const juce::MouseEvent&)
{
    dragStartValue = value;
    gestureId = newGestureId ? newGestureId() : 0;
}

void InsertBox::mouseDrag(const juce::MouseEvent& e)
{
    constexpr int pixelsPerTrack = 6;
    change(dragStartValue - e.getDistanceFromDragStartY() / pixelsPerTrack, gestureId);
}

void InsertBox::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (std::abs(w.deltaY) > 0.0f)
        change(value + (w.deltaY > 0.0f ? 1 : -1), 0);
}

// --------------------------------------------------------------------------------- LoopToggle

void LoopToggle::setState(bool shouldBeOn, bool isActive)
{
    if (on != shouldBeOn || active != isActive)
    {
        on = shouldBeOn;
        active = isActive;
        repaint();
    }
}

void LoopToggle::paint(juce::Graphics& g)
{
    const auto& t = currentTheme();
    auto c = colour(on ? t.colours.accent : t.colours.textDim);
    if (!active)
        c = c.withMultipliedAlpha(0.4f);

    // A circular arrow: an open arc with an arrowhead at its end.
    const auto area = getLocalBounds().toFloat().withSizeKeepingCentre(12.0f, 12.0f);
    const auto centre = area.getCentre();
    const float r = area.getWidth() / 2.0f - 1.0f;
    juce::Path arc;
    arc.addCentredArc(centre.x, centre.y, r, r, 0.0f, 0.5f,
                      juce::MathConstants<float>::twoPi - 0.3f, true);
    g.setColour(c);
    g.strokePath(arc, juce::PathStrokeType(1.6f));
    const juce::Point<float> tip(centre.x + r * std::sin(-0.3f), centre.y - r * std::cos(-0.3f));
    juce::Path head;
    head.addTriangle(tip.x - 3.5f, tip.y - 3.0f, tip.x - 3.5f, tip.y + 3.0f, tip.x + 1.5f, tip.y);
    g.fillPath(head);
}

void LoopToggle::mouseDown(const juce::MouseEvent&)
{
    if (onClick)
        onClick();
}

// ------------------------------------------------------------------------------- LengthHandle

LengthHandle::LengthHandle()
{
    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    setTooltip("Drag to change the pattern length (in groups of 4 steps)");
}

void LengthHandle::paint(juce::Graphics& g)
{
    const auto& t = currentTheme();
    const auto bar = getLocalBounds().toFloat().withSizeKeepingCentre(3.0f, getHeight() - 4.0f);
    g.setColour(colour(isMouseOverOrDragging() ? t.colours.accent : t.colours.outline));
    g.fillRoundedRectangle(bar, 1.5f);
}

void LengthHandle::mouseDown(const juce::MouseEvent&)
{
    startLength = currentLength ? currentLength() : 16;
    startGroupWidth = std::max(1, groupWidth ? groupWidth() : 1);
    gestureId = newGestureId ? newGestureId() : 0;
}

void LengthHandle::mouseDrag(const juce::MouseEvent& e)
{
    constexpr int groupSteps = 4;
    // Measured against the group size at the start of the drag, from where the drag began: the
    // rack resizes while dragging, which would otherwise make the handle jump.
    const int groups = static_cast<int>(
        std::lround(static_cast<float>(e.getDistanceFromDragStartX()) / startGroupWidth));
    const int length =
        std::clamp(startLength + groupSteps * groups, groupSteps, model::Pattern::maxSteps);
    if (onLengthChanged)
        onLengthChanged(length, gestureId);
}

// ----------------------------------------------------------------------------------- StepGrid

int StepGrid::stepX(int step)
{
    return step * (metrics::stepWidth + metrics::stepGap) + (step / 4) * metrics::beatGap;
}

int StepGrid::widthFor(int numSteps)
{
    return numSteps <= 0 ? 0 : stepX(numSteps - 1) + metrics::stepWidth;
}

int StepGrid::stepAt(int x) const
{
    for (int s = 0; s < numSteps(); ++s)
        if (x < stepX(s) + metrics::stepWidth + metrics::stepGap)
            return s;
    return numSteps() - 1;
}

void StepGrid::setSteps(std::vector<bool> newSteps, int newLoopStart)
{
    if (melodic || newSteps != steps || newLoopStart != loopStart)
    {
        melodic = false;
        melodicNotes.clear();
        steps = std::move(newSteps);
        loopStart = newLoopStart;
        repaint();
    }
}

void StepGrid::setMelodicPreview(std::vector<model::Note> notes, int lengthInSteps,
                                 int newLoopStart)
{
    melodic = true;
    melodicNotes = std::move(notes);
    steps.assign(static_cast<std::size_t>(lengthInSteps), false);
    loopStart = newLoopStart;
    repaint();
}

void StepGrid::setPlayingStep(int step)
{
    if (step != playingStep)
    {
        playingStep = step;
        repaint();
    }
}

void StepGrid::paint(juce::Graphics& g)
{
    if (melodic)
    {
        paintNotePreview(g);
        return;
    }
    for (int s = 0; s < numSteps(); ++s)
        paintStepKey(g, s);
}

void StepGrid::paintStepKey(juce::Graphics& g, int step) const
{
    const auto& t = currentTheme();
    const float radius = metrics::stepRadius;
    const bool on = steps[static_cast<std::size_t>(step)];
    const auto r = juce::Rectangle<int>(stepX(step), stepMargin, metrics::stepWidth,
                                        getHeight() - 2 * stepMargin)
                       .toFloat();

    // A drum machine's step key: the key stays grey (a little lighter while on) and a small
    // LED on it lights up. Keys alternate A / B every beat; a greyed repeat is faded.
    const float alpha = isRepeat(step) ? (on ? 0.35f : 0.5f) : 1.0f;
    const auto key = colour((step / 4) % 2 == 0 ? t.colours.stepOffA : t.colours.stepOffB)
                         .withMultipliedAlpha(alpha);
    juce::Path keyShape;
    keyShape.addRoundedRectangle(r, radius);
    fillRaised(g, keyShape, key.brighter(on ? 0.18f : 0.04f));
    g.setColour(juce::Colours::black.withAlpha(0.6f * alpha));
    g.strokePath(keyShape, juce::PathStrokeType(1.0f));

    constexpr float inset = 4.0f; // the LED's distance from the key's sides
    juce::Path led;
    led.addRoundedRectangle(r.getX() + inset, r.getY() + 4.0f, r.getWidth() - 2.0f * inset, 3.0f,
                            1.5f);
    if (on)
    {
        const auto lit = colour(t.colours.stepOn).withMultipliedAlpha(alpha);
        juce::DropShadow(lit, 6, {}).drawForPath(g, led);
        g.setColour(lit.brighter(0.3f));
    }
    else
    {
        g.setColour(juce::Colours::black.withAlpha(0.45f * alpha));
    }
    g.fillPath(led);

    if (step == playingStep)
    {
        g.setColour(colour(t.colours.playhead));
        g.drawRoundedRectangle(r.reduced(1.0f), radius, 2.0f);
    }
}

void StepGrid::paintNotePreview(juce::Graphics& g) const
{
    // A mini piano roll across the step area, as in FL.
    const auto& t = currentTheme();
    const auto area = juce::Rectangle<float>(0.0f, static_cast<float>(stepMargin),
                                             static_cast<float>(widthFor(numSteps())),
                                             static_cast<float>(getHeight() - 2 * stepMargin));
    g.setColour(colour(t.colours.display));
    g.fillRoundedRectangle(area, metrics::stepRadius);
    if (!melodicNotes.empty() && !steps.empty())
    {
        const auto [low, high] = std::minmax_element(melodicNotes.begin(), melodicNotes.end(),
                                                     [](const model::Note& a, const model::Note& b)
                                                     { return a.pitch < b.pitch; });
        const int range = std::max(12, high->pitch - low->pitch + 1);
        const float ticksTotal = static_cast<float>(steps.size() * model::ticksPerStep);
        const float rowHeight = (area.getHeight() - 6.0f) / range;
        const float noteHeight = std::max(3.0f, rowHeight);
        // A looping channel's notes repeat (greyed) from loopStart on.
        const int repeatTicks =
            loopStart > 0 ? loopStart * model::ticksPerStep : static_cast<int>(ticksTotal);
        for (int offset = 0; offset < static_cast<int>(ticksTotal); offset += repeatTicks)
        {
            g.setColour(colour(t.colours.stepOn).withMultipliedAlpha(offset == 0 ? 1.0f : 0.35f));
            for (const auto& note : melodicNotes)
            {
                const int start = note.startTick + offset;
                if (offset > 0 && start >= static_cast<int>(ticksTotal))
                    continue;
                const float x = area.getWidth() * start / ticksTotal;
                const float w =
                    std::max(2.0f, area.getWidth() * note.lengthTicks / ticksTotal - 1.0f);
                const float y =
                    area.getBottom() - 3.0f - (note.pitch - low->pitch) * rowHeight - noteHeight;
                g.fillRect(x, y, w, noteHeight);
            }
        }
    }
    if (playingStep >= 0)
    {
        g.setColour(colour(t.colours.playhead));
        g.fillRect(static_cast<float>(stepX(playingStep)), area.getY(), 2.0f, area.getHeight());
    }
}

void StepGrid::mouseDown(const juce::MouseEvent& e)
{
    if (melodic)
    {
        if (!e.mods.isPopupMenu() && onOpenNotes)
            onOpenNotes();
        return;
    }
    if (steps.empty())
        return;
    const int step = stepAt(e.x);
    // A greyed (looped) step isn't toggled: left-click keeps or adds a note there. Right-click
    // removes it when the loop is copied in first (Settings), and leaves it alone otherwise (see
    // paintStep and ChannelRow).
    paintValue =
        !e.mods.isPopupMenu() && (isRepeat(step) || !steps[static_cast<std::size_t>(step)]);
    gestureId = newGestureId ? newGestureId() : 0;
    lastPainted = -1;
    paintStep(step);
}

void StepGrid::mouseDrag(const juce::MouseEvent& e)
{
    if (melodic || steps.empty())
        return;

    // Fast drags report positions several steps apart: paint everything in between too.
    const int target = stepAt(e.x);
    if (lastPainted < 0)
    {
        paintStep(target);
        return;
    }
    const int direction = target > lastPainted ? 1 : -1;
    for (int s = lastPainted + direction; s != target + direction; s += direction)
        paintStep(s);
}

void StepGrid::paintStep(int step)
{
    if (step == lastPainted || step < 0)
        return;
    lastPainted = step;

    const bool on = steps[static_cast<std::size_t>(step)];
    bool changes = on != paintValue;
    if (isRepeat(step))
    {
        // A left-click on a repeat is reported even when it's already on, so the note becomes
        // real. Erasing only touches repeats that have a note (empty ones stay looped), and only
        // when the loop is copied in first: as in FL, erasing otherwise leaves the loop alone.
        changes = paintValue || (on && copiesLoop && copiesLoop());
    }
    if (changes && onStepPainted)
        onStepPainted(step, paintValue, gestureId);
}

// --------------------------------------------------------------------------------- ChannelRow

ChannelRow::ChannelRow(ChannelRack& owner, model::ChannelId channel) : rack(owner), id(channel)
{
    setUpMute();
    setUpKnobs();
    setUpInsert();
    setUpName();
    setUpLoop();
    setUpSteps();
}

void ChannelRow::editChannel(const std::function<void(model::Channel&)>& change, int gestureId)
{
    rack.edit(
        [channel = id, &change](model::Project& p)
        {
            if (auto* c = p.findChannel(channel))
                change(*c);
        },
        gestureId);
}

void ChannelRow::setUpMute()
{
    mute.setTooltip("Mute. Ctrl+click or right-click: solo (again: all play)");
    mute.onClick = [this](bool solo)
    {
        if (solo)
            rack.edit([this](model::Project& p) { p.soloChannel(id); });
        else
            editChannel([](model::Channel& c) { c.muted = !c.muted; });
    };
    addAndMakeVisible(mute);
}

void ChannelRow::setUpKnob(juce::Slider& knob, double min, double max, double reset,
                           std::function<void(model::Channel&, float)> apply)
{
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob.setRange(min, max, 0.0);
    knob.setDoubleClickReturnValue(true, reset);
    knob.setPopupDisplayEnabled(true, true, nullptr);
    // One gesture per drag, so a drag undoes as one step.
    knob.onDragStart = [this] { knobGesture = rack.document().newGestureId(); };
    knob.onDragEnd = [this] { knobGesture = 0; };
    knob.onValueChange = [this, &knob, apply]
    {
        const auto value = static_cast<float>(knob.getValue());
        editChannel([&apply, value](model::Channel& c) { apply(c, value); }, knobGesture);
    };
    noFocus(knob);
    addAndMakeVisible(knob);
}

void ChannelRow::setUpKnobs()
{
    setUpKnob(pan, -1.0, 1.0, 0.0, [](model::Channel& c, float v) { c.pan = v; });
    pan.setTooltip("Pan");
    pan.textFromValueFunction = [](double v)
    {
        const int percent = static_cast<int>(std::lround(std::abs(v) * 100.0));
        return percent == 0 ? juce::String("Centred")
                            : juce::String(percent) + (v < 0 ? "% left" : "% right");
    };

    setUpKnob(volume, 0.0, 1.0, model::Channel{}.volume,
              [](model::Channel& c, float v) { c.volume = v; });
    volume.setTooltip("Volume");
    volume.textFromValueFunction = [](double v)
    { return juce::String(static_cast<int>(std::lround(v * 100.0))) + "%"; };
}

void ChannelRow::setUpInsert()
{
    insert.setTooltip("Mixer track (drag or scroll; -- = Master)");
    insert.newGestureId = [this] { return rack.document().newGestureId(); };
    insert.onChange = [this](int track, int gesture)
    { editChannel([track](model::Channel& c) { c.mixerTrack = track; }, gesture); };
    addAndMakeVisible(insert);
}

void ChannelRow::setUpName()
{
    // Like FL: left-click selects the channel (the Piano Roll edits the selected one) and shows
    // or hides its window, the plugin's or the Sampler. Right-click opens the channel menu (see
    // mouseDown).
    name.onClick = [this]
    {
        if (juce::ModifierKeys::currentModifiers.isPopupMenu())
            return;
        rack.select(id);
        if (isInstrument && rack.onShowInstrument)
            rack.onShowInstrument(id, true);
        else if (!isInstrument && rack.onShowSampler)
            rack.onShowSampler(id, true);
    };
    name.addMouseListener(this, false);
    noFocus(name);
    addAndMakeVisible(name);
}

void ChannelRow::setUpLoop()
{
    loop.setTooltip("Loop this channel: repeat its notes when the pattern is longer");
    loop.onClick = [this] { editChannel([](model::Channel& c) { c.loop = !c.loop; }); };
    addAndMakeVisible(loop);
}

void ChannelRow::setUpSteps()
{
    steps.onOpenNotes = [this] { rack.openPianoRoll(id); };
    steps.newGestureId = [this] { return rack.document().newGestureId(); };
    steps.copiesLoop = [this] { return rack.copiesLoopOnClick(); };
    steps.onStepPainted = [this](int step, bool on, int gesture) { setStep(step, on, gesture); };
    addAndMakeVisible(steps);
}

void ChannelRow::setStep(int step, bool on, int gestureId)
{
    rack.edit(
        [this, step, on, copyLoop = rack.copiesLoopOnClick()](model::Project& p)
        {
            auto& pattern = p.current();
            // A greyed step repeats something: the channel's own loop, or the whole pattern (the
            // steps past where it ends). When the loop is copied in, the repeats up to there
            // become real first (doubling as needed), so clicking doesn't change what else plays.
            // Otherwise (as in FL) only the clicked step is set: the channel's loop grows to
            // include it and its other repeats go.
            const int loopLength = p.loopLengthInSteps(pattern, id);
            const int repeat = loopLength > 0 ? loopLength : p.lengthInSteps(pattern);
            if (copyLoop && step >= repeat)
            {
                int target = repeat;
                while (target <= step)
                    target *= 2;
                pattern.fillLoop(id, repeat, std::min(target, p.gridLengthInSteps(pattern)));
            }
            pattern.set(id, step, on);
        },
        gestureId);
}

void ChannelRow::update(const model::Channel& channel, const model::Project& project)
{
    mute.setOn(!channel.muted);
    pan.setValue(channel.pan, juce::dontSendNotification);
    volume.setValue(channel.volume, juce::dontSendNotification);
    name.setButtonText(juce::String::fromUTF8(channel.name.c_str()));
    isInstrument = channel.isInstrument();
    name.setTooltip(isInstrument ? "Plugin: " + juce::String::fromUTF8(channel.pluginName.c_str()) +
                                       "\nClick to select and show the plugin, right-click for "
                                       "options"
                                 : juce::String("Click to select, right-click for options"));
    insert.setValue(channel.mixerTrack);
    const bool isSelected = project.selectedChannel == channel.id;
    name.setToggleState(isSelected, juce::dontSendNotification);
    if (isSelected != selected)
    {
        selected = isSelected;
        repaint();
    }

    loop.setState(channel.loop, project.loopChannels);

    // The rack shows `shown` steps; the pattern plays `length` of them and then starts over, and
    // a looping channel repeats every `loopLength`. Repeats are greyed and show what they play.
    const auto& pattern = project.current();
    const int shown = project.gridLengthInSteps(pattern);
    const int length = project.lengthInSteps(pattern);
    const int loopLength = project.loopLengthInSteps(pattern, channel.id); // 0 = no own loop
    const int repeat = loopLength > 0 ? loopLength : length;
    const int greyFrom = repeat < shown ? repeat : -1;
    if (!pattern.hasOnlySteps(channel.id))
    {
        steps.setMelodicPreview(pattern.notesFor(channel.id), shown, greyFrom);
        return;
    }
    std::vector<bool> stepValues(static_cast<std::size_t>(shown));
    for (int s = 0; s < shown; ++s)
        stepValues[static_cast<std::size_t>(s)] = pattern.isOn(channel.id, s % length % repeat);
    steps.setSteps(std::move(stepValues), greyFrom);
}

void ChannelRow::mouseDown(const juce::MouseEvent& e)
{
    // The name button's clicks arrive here too (a mouse listener): right-click opens the menu.
    if (e.eventComponent == &name && e.mods.isPopupMenu())
        showMenu();
}

void ChannelRow::paint(juce::Graphics& g)
{
    if (!selected)
        return;
    // The selected channel (the Piano Roll's, and where the Browser loads samples).
    const auto& t = currentTheme();
    g.setColour(colour(t.colours.activeBackground).withAlpha(0.6f));
    g.fillRoundedRectangle(getLocalBounds().toFloat().expanded(2.0f, 1.0f).withTrimmedRight(2.0f),
                           metrics::cornerRadius);
}

void ChannelRow::resized()
{
    auto area = getLocalBounds();
    mute.setBounds(area.removeFromLeft(metrics::ledWidth + columnGap));
    pan.setBounds(area.removeFromLeft(metrics::knobSize + columnGap)
                      .withSizeKeepingCentre(metrics::knobSize, metrics::knobSize));
    volume.setBounds(area.removeFromLeft(metrics::knobSize + columnGap)
                         .withSizeKeepingCentre(metrics::knobSize, metrics::knobSize));
    insert.setBounds(area.removeFromLeft(metrics::insertWidth));
    area.removeFromLeft(columnGap);
    name.setBounds(area.removeFromLeft(metrics::nameWidth).reduced(0, 2));
    loop.setBounds(area.removeFromRight(loopWidth)); // at the end of the row
    steps.setBounds(area);
}

namespace
{

// The channel menu's items (instrument plugins use ChannelRack's ids).
enum ChannelMenuId
{
    pianoRollId = 1,
    showPluginId,
    samplerId,
    renameId,
    replaceSampleId,
    deleteId,
    clearId,
    duplicateId,
    fillFirstId = 100 // + an index into fillEvery
};
constexpr std::array fillEvery{1, 2, 4, 8}; // the Fill submenu: every 1, 2, 4 or 8 steps

juce::PopupMenu fillMenu()
{
    juce::PopupMenu fill;
    for (std::size_t i = 0; i < fillEvery.size(); ++i)
        fill.addItem(fillFirstId + static_cast<int>(i),
                     fillEvery[i] == 1 ? juce::String("Every step")
                                       : "Every " + juce::String(fillEvery[i]) + " steps");
    return fill;
}

void deleteChannel(ChannelRack& rack, model::ChannelId channel)
{
    rack.edit([channel](model::Project& p) { p.removeChannel(channel); });
}

// Runs the channel menu's choice. It takes the rack and the id, not the row: the row may be gone
// by the time the menu returns (e.g. an undo removed the channel).
void handleChannelMenu(ChannelRack& rack, model::ChannelId channel, int result,
                       const std::vector<PluginItem>& instruments)
{
    if (const int fill = result - fillFirstId;
        fill >= 0 && fill < static_cast<int>(fillEvery.size()))
    {
        const int every = fillEvery[static_cast<std::size_t>(fill)];
        rack.edit(
            [channel, every](model::Project& p)
            {
                auto& pattern = p.current();
                pattern.fillSteps(channel, every, p.gridLengthInSteps(pattern));
            });
        return;
    }
    switch (result)
    {
    case 0:
        return;
    case pianoRollId:
        rack.openPianoRoll(channel);
        return;
    case showPluginId:
        if (rack.onShowInstrument)
            rack.onShowInstrument(channel, false);
        return;
    case samplerId:
        if (rack.onShowSampler)
            rack.onShowSampler(channel, false);
        return;
    case renameId:
        rack.rename(channel);
        return;
    case replaceSampleId:
        rack.chooseSampleFor(channel);
        return;
    case deleteId:
        deleteChannel(rack, channel);
        return;
    case clearId:
        rack.edit([channel](model::Project& p) { p.current().clearNotes(channel); });
        return;
    case duplicateId:
        if (rack.onDuplicateChannel)
            rack.onDuplicateChannel(channel);
        return;
    default:
        rack.handleInstrumentMenu(result, channel, instruments);
    }
}

} // namespace

void ChannelRow::showMenu()
{
    std::vector<PluginItem> instruments;
    juce::PopupMenu menu;
    menu.addItem(pianoRollId, "Piano roll");
    if (isInstrument)
        menu.addItem(showPluginId, "Show plugin");
    else
        menu.addItem(samplerId, "Sampler...");
    // Fill replaces the channel's notes, melodic ones too (undo brings them back).
    menu.addSubMenu("Fill", fillMenu());
    menu.addItem(clearId, "Clear");
    menu.addItem(duplicateId, "Duplicate");
    menu.addSeparator();
    menu.addItem(renameId, "Rename...");
    menu.addItem(replaceSampleId, isInstrument ? "Use a sample instead..." : "Replace sample...");
    menu.addSubMenu(isInstrument ? "Replace instrument" : "Use an instrument plugin",
                    rack.instrumentMenu(instruments));
    menu.addSeparator();
    juce::PopupMenu::Item deleteItem("Delete channel");
    deleteItem.itemID = deleteId;
    deleteItem.shortcutKeyDescription = "D";
    menu.addItem(std::move(deleteItem));

    // The callbacks only use the rack and the id (see handleChannelMenu).
    auto& r = rack;
    const auto channel = id;
    // D deletes the channel while the menu is open, as in FL.
    auto keys = std::make_shared<std::unique_ptr<MenuKeys>>();
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&name),
                       [&r, channel, instruments, keys](int result)
                       {
                           keys->reset();
                           handleChannelMenu(r, channel, result, instruments);
                       });
    *keys = std::make_unique<MenuKeys>(std::vector<std::pair<char, std::function<void()>>>{
        {'d', [&r, channel] { deleteChannel(r, channel); }}});
}

// -------------------------------------------------------------------------------- ChannelRack

ChannelRack::ChannelRack(model::Document& document, engine::Transport& transportToShow,
                         io::SampleLoader& sampleLoader)
    : doc(document), transport(transportToShow), loader(sampleLoader)
{
    setOpaque(true); // paints every pixel: a plain native window (app/area_stack)
    // Cached as an image so dragging the rack doesn't redraw every row each frame.
    setBufferedToImage(true);
    setUpButtons();
    setUpLengthHandle();
    refresh();
    startTimerHz(30);
}

void ChannelRack::setUpButtons()
{
    closeButton.setTooltip("Hide");
    closeButton.onClick = [this] { setVisible(false); };
    noFocus(closeButton);
    addAndMakeVisible(closeButton);

    addButton.setTooltip("Add a channel: sample or instrument plugin");
    addButton.onClick = [this] { showAddMenu(); };
    noFocus(addButton);
    addAndMakeVisible(addButton);

    loopButton.setClickingTogglesState(true);
    loopButton.setTooltip("Loop: channels whose notes are shorter than the pattern repeat them "
                          "(each channel's loop switch is at the end of its row)");
    loopButton.onClick = [this]
    {
        const bool on = loopButton.getToggleState();
        edit([on](model::Project& p) { p.loopChannels = on; });
    };
    noFocus(loopButton);
    addAndMakeVisible(loopButton);

    // Steps shown, as three joined buttons: 16 | 32 | 64.
    for (std::size_t i = 0; i < lengthButtons.size(); ++i)
    {
        auto& button = lengthButtons[i];
        const int steps = 16 << i;
        button.setClickingTogglesState(false);
        button.setTooltip("Show " + juce::String(steps) +
                          " steps (only bars with notes play; drag the handle after the steps "
                          "for other lengths)");
        button.setConnectedEdges(
            (i > 0 ? juce::Button::ConnectedOnLeft : 0) |
            (i + 1 < lengthButtons.size() ? juce::Button::ConnectedOnRight : 0));
        button.onClick = [this, steps] { setGridSteps(steps); };
        noFocus(button);
        addAndMakeVisible(button);
    }
}

void ChannelRack::setUpLengthHandle()
{
    lengthHandle.currentLength = [this] { return stepsShown(); };
    lengthHandle.groupWidth = [] { return StepGrid::widthFor(8) - StepGrid::widthFor(4); };
    lengthHandle.newGestureId = [this] { return doc.newGestureId(); };
    lengthHandle.onLengthChanged = [this](int steps, int gesture) { setGridSteps(steps, gesture); };
    addAndMakeVisible(lengthHandle);
}

void ChannelRack::setGridSteps(int steps, int gestureId)
{
    if (steps != doc.project().current().gridSteps)
        edit([steps](model::Project& p) { p.current().gridSteps = steps; }, gestureId);
}

int ChannelRack::stepsShown() const
{
    return doc.project().gridLengthInSteps(doc.project().current());
}

void ChannelRack::edit(const std::function<void(model::Project&)>& change, int gestureId)
{
    doc.apply(change, gestureId);
}

void ChannelRack::select(model::ChannelId id)
{
    if (doc.project().selectedChannel != id)
        doc.setViewState([id](model::Project& p) { p.selectedChannel = id; });
}

std::vector<const model::Channel*> ChannelRack::shownChannels() const
{
    const auto& project = doc.project();
    std::vector<const model::Channel*> shown;
    for (const auto& channel : project.channels)
        if (!channel.audioClip || project.showAudioClips)
            shown.push_back(&channel);
    return shown;
}

void ChannelRack::selectOffset(int offset)
{
    const auto channels = shownChannels();
    if (channels.empty())
        return;
    const auto it = std::find_if(channels.begin(), channels.end(), [this](const model::Channel* c)
                                 { return c->id == doc.project().selectedChannel; });
    const int index = it == channels.end() ? (offset > 0 ? -1 : static_cast<int>(channels.size()))
                                           : static_cast<int>(it - channels.begin());
    const int next = std::clamp(index + offset, 0, static_cast<int>(channels.size()) - 1);
    select(channels[static_cast<std::size_t>(next)]->id);
}

void ChannelRack::loadIntoSelected(const juce::File& file)
{
    const auto id = doc.project().selectedChannel;
    if (doc.project().findChannel(id) != nullptr)
        replaceSample(id, file);
    else
        addChannelsFromFiles({file});
}

void ChannelRack::openPianoRoll(model::ChannelId id)
{
    select(id);
    if (onOpenPianoRoll)
        onOpenPianoRoll(id);
}

juce::PopupMenu ChannelRack::instrumentMenu(std::vector<PluginItem>& items) const
{
    // Instrument plugins (VST3 / LV2), found by scanning in Options > Plugins.
    items = listInstruments ? listInstruments() : std::vector<PluginItem>{};
    auto menu = makePluginMenu(items, firstInstrumentId);
    menu.addSeparator();
    menu.addItem(manageInstrumentsId, "Manage plugins...");
    return menu;
}

bool ChannelRack::handleInstrumentMenu(int result, model::ChannelId id,
                                       const std::vector<PluginItem>& items)
{
    if (result == manageInstrumentsId)
    {
        if (onManagePlugins)
            onManagePlugins();
        return true;
    }
    const int index = result - firstInstrumentId;
    if (index < 0 || index >= static_cast<int>(items.size()))
        return false;

    const auto& item = items[static_cast<std::size_t>(index)];
    model::ChannelId target = id;
    edit(
        [&target, &item](model::Project& p)
        {
            if (target == 0)
                target = p.addInstrumentChannel(item.id, item.name);
            else if (auto* c = p.findChannel(target))
                c->setInstrument(item.id, item.name);
        });
    if (target == 0)
        return true;
    select(target);
    // Open the plugin right away so it can be set up, like FL does for new channels.
    if (onShowInstrument)
        onShowInstrument(target, false);
    return true;
}

void ChannelRack::showAddMenu()
{
    constexpr int sampleId = 1;
    std::vector<PluginItem> instruments;
    juce::PopupMenu menu;
    menu.addItem(sampleId, "Sample...");
    menu.addSubMenu("Instrument", instrumentMenu(instruments));
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addButton),
                       [this, instruments](int result)
                       {
                           if (result == sampleId)
                               chooseSampleFor(0);
                           else
                               handleInstrumentMenu(result, 0, instruments);
                       });
}

void ChannelRack::refresh()
{
    const auto& project = doc.project();

    // Rebuild rows only if the set or order of channels changed; otherwise update in place
    // (so a knob being dragged isn't destroyed under the mouse).
    const auto shown = shownChannels();
    bool sameChannels = rows.size() == shown.size();
    for (std::size_t i = 0; sameChannels && i < rows.size(); ++i)
        sameChannels = rows[i]->channelId() == shown[i]->id;

    if (!sameChannels)
    {
        rows.clear();
        for (const auto* channel : shown)
        {
            rows.push_back(std::make_unique<ChannelRow>(*this, channel->id));
            addAndMakeVisible(*rows.back());
        }
        lastPlayingStep = -2;
    }

    for (std::size_t i = 0; i < rows.size(); ++i)
        rows[i]->update(*shown[i], project);

    loopButton.setToggleState(project.loopChannels, juce::dontSendNotification);
    const int gridSteps = project.current().gridSteps;
    for (std::size_t i = 0; i < lengthButtons.size(); ++i)
        lengthButtons[i].setToggleState(gridSteps == (16 << i), juce::dontSendNotification);
    resized(); // places new rows; the step area (and the handle) follow the pattern length

    if (onSizeChanged)
        onSizeChanged();
}

int ChannelRack::preferredWidth() const
{
    return padding * 2 + rowFixedWidth + handleWidth + StepGrid::widthFor(stepsShown());
}

int ChannelRack::preferredHeight() const
{
    const int n = static_cast<int>(shownChannels().size());
    return headerHeight + padding * 2 + n * (metrics::rowHeight + metrics::rowGap) +
           metrics::rowHeight;
}

void ChannelRack::paint(juce::Graphics& g)
{
    // Title bar: "Channel rack" and the pattern being edited.
    paintFloatingPanel(g, *this, "Channel rack",
                       juce::String::fromUTF8(doc.project().current().name.c_str()),
                       loopButton.getX() - metrics::controlGap);
    paintFloatingBorder(g, *this);
}

void ChannelRack::paintOverChildren(juce::Graphics& g)
{
    // Focus (the "focused" property, set by MainComponent): a line in the accent colour along the
    // top, as on docked panels. Drawn here rather than by the main window so floating panels
    // above the rack cover it.
    if (getProperties()["focused"])
    {
        g.setColour(colour(currentTheme().colours.accent));
        g.fillRect(
            getLocalBounds().toFloat().reduced(metrics::windowRadius, 0.0f).withHeight(2.0f));
    }
    if (!dragOver)
        return;
    g.setColour(colour(currentTheme().colours.accent).withAlpha(0.25f));
    if (dropRow >= 0 && dropRow < static_cast<int>(rows.size()))
        g.fillRect(rows[static_cast<std::size_t>(dropRow)]->getBounds());
    else
        g.fillRect(addButton.getBounds().withRight(getWidth() - padding));
}

void ChannelRack::resized()
{
    auto bounds = getLocalBounds();
    // A long pattern can make the rack wider than the window: the header's controls stay at the
    // right edge of the part that's in view.
    constexpr int minHeaderWidth = 300;
    auto headerArea = bounds.removeFromTop(headerHeight);
    if (!movableArea.isEmpty())
        headerArea.setRight(
            std::min(getWidth(), std::max(minHeaderWidth, movableArea.getRight() - getX())));
    HeaderLayout header(headerArea);
    closeButton.setBounds(header.fromRight(metrics::controlHeight));
    // 16 | 32 | 64 as one segmented control: the segments touch.
    const int segment = buttonWidth("64", metrics::controlHeight);
    auto lengths = header.fromRight(3 * segment);
    for (auto& button : lengthButtons)
        button.setBounds(lengths.removeFromLeft(segment));
    loopButton.setBounds(header.fromRight(loopButton.preferredWidth(metrics::controlHeight)));

    auto area = bounds.reduced(padding);
    const int rowsTop = area.getY();
    for (auto& row : rows)
    {
        row->setBounds(area.removeFromTop(metrics::rowHeight).withTrimmedRight(handleWidth));
        area.removeFromTop(metrics::rowGap);
    }
    // The handle sits right after the last step column, the height of the rows.
    const int stepsRight = padding + rowFixedWidth + StepGrid::widthFor(stepsShown());
    lengthHandle.setBounds(stepsRight + 2, rowsTop, handleWidth - 2,
                           std::max(metrics::rowHeight, area.getY() - rowsTop - metrics::rowGap));
    addButton.setBounds(padding + nameX, area.getY(), metrics::nameWidth, metrics::rowHeight - 4);
}

void ChannelRack::mouseDown(const juce::MouseEvent& e)
{
    // Remembered here: e.mouseDownPosition is relative to where the rack is *now*, so after
    // moving the rack up it no longer lies in the title bar (dragging up used to get stuck).
    movingRack = e.y < headerHeight;
    if (movingRack)
        dragger.start(*this);
}

void ChannelRack::mouseDrag(const juce::MouseEvent&)
{
    if (movingRack)
    {
        dragger.drag(*this);
        keepTitleBarReachable(*this, movableArea);
    }
}

void ChannelRack::setMovableArea(juce::Rectangle<int> area)
{
    movableArea = area;
    keepTitleBarReachable(*this, movableArea);
    resized(); // the header's controls follow the visible part (see resized())
}

void ChannelRack::timerCallback()
{
    // Only in pattern mode: in song mode the playhead belongs to the Playlist.
    int step = -1;
    const auto& project = doc.project();
    const int length = project.lengthInSteps(project.current());
    if (transport.isPlaying() && !project.songMode && length > 0)
        step = static_cast<int>(std::floor(transport.positionInBeats() * engine::stepsPerBeat)) %
               length;

    if (step != lastPlayingStep)
    {
        lastPlayingStep = step;
        for (auto& row : rows)
            row->setPlayingStep(step);
    }
}

int ChannelRack::rowIndexAt(int y) const
{
    for (std::size_t i = 0; i < rows.size(); ++i)
        if (y >= rows[i]->getY() && y < rows[i]->getBottom())
            return static_cast<int>(i);
    return -1;
}

void ChannelRack::chooseSampleFor(model::ChannelId id)
{
    // id 0 = add new channel(s); otherwise replace that channel's sample.
    const bool adding = id == 0;
    chooser =
        std::make_unique<juce::FileChooser>(adding ? "Add channel" : "Replace sample", juce::File(),
                                            "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    auto chooserFlags =
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    if (adding)
        chooserFlags |= juce::FileBrowserComponent::canSelectMultipleItems;

    chooser->launchAsync(chooserFlags,
                         [this, id, adding](const juce::FileChooser& fc)
                         {
                             if (adding)
                                 addChannelsFromFiles(fc.getResults());
                             else
                                 replaceSample(id, fc.getResult());
                         });
}

void ChannelRack::replaceSample(model::ChannelId id, const juce::File& file)
{
    const auto sample = loader.load(file);
    if (sample == nullptr)
        return;
    const auto path = file.getFullPathName().toStdString();
    const auto name = file.getFileNameWithoutExtension().toStdString();
    edit(
        [id, sample, path, name](model::Project& p)
        {
            if (auto* c = p.findChannel(id))
            {
                c->setSample(sample, path); // also turns an instrument channel into a sampler
                c->name = name;             // named after its sample, as in FL
            }
        });
}

void ChannelRack::addChannelsFromFiles(const juce::Array<juce::File>& files)
{
    struct Loaded
    {
        std::string name, path;
        std::shared_ptr<const engine::SampleBuffer> sample;
    };
    std::vector<Loaded> loaded;
    for (const auto& file : files)
        if (auto sample = loader.load(file))
            loaded.push_back({file.getFileNameWithoutExtension().toStdString(),
                              file.getFullPathName().toStdString(), sample});

    if (!loaded.empty())
        edit(
            [&loaded](model::Project& p)
            {
                for (const auto& l : loaded)
                    p.findChannel(p.addChannel(l.name, l.sample))->samplePath = l.path;
            });
}

void ChannelRack::copySelectedChannel()
{
    const auto& project = doc.project();
    if (project.findChannel(project.selectedChannel) != nullptr)
        channelClipboard = project.current().notesFor(project.selectedChannel);
}

void ChannelRack::cutSelectedChannel()
{
    copySelectedChannel();
    if (const auto channel = doc.project().selectedChannel; channelClipboard && channel != 0)
        edit([channel](model::Project& p) { p.current().clearNotes(channel); });
}

void ChannelRack::pasteIntoSelectedChannel()
{
    const auto channel = doc.project().selectedChannel;
    if (!channelClipboard || doc.project().findChannel(channel) == nullptr)
        return;
    edit([channel, notes = *channelClipboard](model::Project& p)
         { p.current().notes[channel] = notes; });
}

void ChannelRack::rename(model::ChannelId id)
{
    const auto* channel = doc.project().findChannel(id);
    if (channel == nullptr)
        return;

    auto* window = new juce::AlertWindow("Rename channel", {}, juce::MessageBoxIconType::NoIcon);
    window->addTextEditor("name", juce::String::fromUTF8(channel->name.c_str()));
    window->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    window->enterModalState(true,
                            juce::ModalCallbackFunction::create(
                                [this, id, window](int result)
                                {
                                    const auto newName =
                                        window->getTextEditorContents("name").trim();
                                    if (result == 1 && newName.isNotEmpty())
                                        edit(
                                            [id, newName](model::Project& p)
                                            {
                                                if (auto* c = p.findChannel(id))
                                                    c->name = newName.toStdString();
                                            });
                                }),
                            true);
}

bool ChannelRack::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files)
        if (loader.canLoad(juce::File(path)))
            return true;
    return false;
}

void ChannelRack::fileDragMove(const juce::StringArray&, int, int y)
{
    const int row = rowIndexAt(y);
    if (!dragOver || row != dropRow)
    {
        dragOver = true;
        dropRow = row;
        repaint();
    }
}

void ChannelRack::fileDragExit(const juce::StringArray&)
{
    dragOver = false;
    repaint();
}

void ChannelRack::filesDropped(const juce::StringArray& paths, int, int y)
{
    dragOver = false;
    repaint();

    juce::Array<juce::File> files;
    for (const auto& path : paths)
        if (loader.canLoad(juce::File(path)))
            files.add(juce::File(path));
    if (files.isEmpty())
        return;

    // Dropped on a row: replace that channel's sample (first file). Elsewhere: new channels.
    if (const int row = rowIndexAt(y); row >= 0)
    {
        replaceSample(rows[static_cast<std::size_t>(row)]->channelId(), files.getFirst());
        return;
    }
    addChannelsFromFiles(files);
}

bool ChannelRack::isInterestedInDragSource(const SourceDetails& details)
{
    return details.description.toString().startsWith(BrowserPanel::dragPrefix);
}

void ChannelRack::itemDragMove(const SourceDetails& details)
{
    fileDragMove({}, details.localPosition.x, details.localPosition.y);
}

void ChannelRack::itemDragExit(const SourceDetails&)
{
    fileDragExit({});
}

void ChannelRack::itemDropped(const SourceDetails& details)
{
    const auto path = details.description.toString().fromFirstOccurrenceOf(BrowserPanel::dragPrefix,
                                                                           false, false);
    filesDropped(juce::StringArray(path), details.localPosition.x, details.localPosition.y);
}

} // namespace jls::ui
