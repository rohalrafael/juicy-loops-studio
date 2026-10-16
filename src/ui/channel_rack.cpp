#include "ui/channel_rack.h"

#include "engine/step_sequencer.h"
#include "ui/theme.h"

#include <cmath>
#include <optional>

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
constexpr int hardShadowOffset = 5; // themes with a hard shadow draw it in this margin
constexpr int headerHeight = 30;    // title bar; drag it to move the rack

// Soft glow around a shape, for themes that have one.
void drawGlow(juce::Graphics& g, const juce::Path& shape, std::optional<theme::Argb> glow,
              int radius)
{
    if (glow)
        juce::DropShadow(colour(*glow), radius, {}).drawForPath(g, shape);
}

} // namespace

// ---------------------------------------------------------------------------------- LedButton

void LedButton::setOn(bool shouldBeOn)
{
    if (on != shouldBeOn)
    {
        on = shouldBeOn;
        repaint();
    }
}

void LedButton::paint(juce::Graphics& g)
{
    const auto& t = currentTheme();
    const auto led = getLocalBounds().toFloat().withSizeKeepingCentre(9.0f, 9.0f);
    if (on)
    {
        juce::Path dot;
        dot.addEllipse(led);
        drawGlow(g, dot, t.effects.ledGlow, 6);
    }
    g.setColour(colour(on ? t.colours.led : t.colours.ledOff));
    g.fillEllipse(led);
    g.setColour(colour(t.colours.outline));
    g.drawEllipse(led, 1.0f);
}

void LedButton::mouseDown(const juce::MouseEvent&)
{
    if (onClick)
        onClick();
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
    for (int s = 0; s < static_cast<int>(steps.size()); ++s)
        if (x < stepX(s) + metrics::stepWidth + metrics::stepGap)
            return s;
    return static_cast<int>(steps.size()) - 1;
}

void StepGrid::setSteps(std::vector<bool> newSteps)
{
    if (melodic || newSteps != steps)
    {
        melodic = false;
        melodicNotes.clear();
        steps = std::move(newSteps);
        repaint();
    }
}

void StepGrid::setMelodicPreview(std::vector<model::Note> notes, int lengthInSteps)
{
    melodic = true;
    melodicNotes = std::move(notes);
    steps.assign(static_cast<std::size_t>(lengthInSteps), false);
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
    const auto& t = currentTheme();
    const float radius = t.shape.stepRadius;

    if (melodic)
    {
        // A mini piano roll across the step area, as in FL.
        const auto area = juce::Rectangle<float>(
            0.0f, 3.0f, static_cast<float>(widthFor(static_cast<int>(steps.size()))),
            static_cast<float>(getHeight() - 6));
        g.setColour(colour(t.colours.display));
        g.fillRoundedRectangle(area, radius);
        if (!melodicNotes.empty() && !steps.empty())
        {
            const auto [low, high] = std::minmax_element(
                melodicNotes.begin(), melodicNotes.end(),
                [](const model::Note& a, const model::Note& b) { return a.pitch < b.pitch; });
            const int range = std::max(12, high->pitch - low->pitch + 1);
            const float ticksTotal = static_cast<float>(steps.size() * model::ticksPerStep);
            const float rowHeight = (area.getHeight() - 6.0f) / range;
            const float noteHeight = std::max(3.0f, rowHeight);
            g.setColour(colour(t.colours.stepOn));
            for (const auto& note : melodicNotes)
            {
                const float x = area.getWidth() * note.startTick / ticksTotal;
                const float w =
                    std::max(2.0f, area.getWidth() * note.lengthTicks / ticksTotal - 1.0f);
                const float y =
                    area.getBottom() - 3.0f - (note.pitch - low->pitch) * rowHeight - noteHeight;
                g.fillRect(x, y, w, noteHeight);
            }
        }
        if (playingStep >= 0)
        {
            g.setColour(colour(t.colours.playhead));
            g.fillRect(static_cast<float>(stepX(playingStep)), area.getY(), 2.0f, area.getHeight());
        }
        return;
    }

    for (int s = 0; s < static_cast<int>(steps.size()); ++s)
    {
        const bool on = steps[static_cast<std::size_t>(s)];
        const auto r =
            juce::Rectangle<int>(stepX(s), 3, metrics::stepWidth, getHeight() - 6).toFloat();

        if (on)
        {
            juce::Path shape;
            shape.addRoundedRectangle(r, radius);
            drawGlow(g, shape, t.effects.stepGlow, 8);
        }

        const auto offColour = (s / 4) % 2 == 0 ? t.colours.stepOffA : t.colours.stepOffB;
        g.setColour(colour(on ? t.colours.stepOn : offColour));
        g.fillRoundedRectangle(r, radius);

        if (t.shape.stepOutline > 0.0f)
        {
            g.setColour(colour(t.colours.outline));
            g.drawRoundedRectangle(r.reduced(t.shape.stepOutline / 2.0f), radius,
                                   t.shape.stepOutline);
        }

        if (s == playingStep)
        {
            g.setColour(colour(t.colours.playhead));
            g.drawRoundedRectangle(r.reduced(1.0f), radius, 2.0f);
        }
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
    paintValue = !e.mods.isPopupMenu() && !steps[static_cast<std::size_t>(step)];
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
    if (steps[static_cast<std::size_t>(step)] != paintValue && onStepPainted)
        onStepPainted(step, paintValue, gestureId);
}

// --------------------------------------------------------------------------------- ChannelRow

ChannelRow::ChannelRow(ChannelRack& owner, model::ChannelId channel) : rack(owner), id(channel)
{
    mute.onClick = [this]
    {
        rack.edit(
            [this](model::Project& p)
            {
                if (auto* c = p.findChannel(id))
                    c->muted = !c->muted;
            });
    };
    mute.setTooltip("Mute");

    auto setupKnob = [this](juce::Slider& knob, double min, double max, double reset,
                            std::function<void(model::Channel&, float)> apply)
    {
        knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        knob.setRange(min, max, 0.0);
        knob.setDoubleClickReturnValue(true, reset);
        knob.setPopupDisplayEnabled(true, true, nullptr);
        knob.onDragStart = [this] { knobGesture = rack.document().newGestureId(); };
        knob.onDragEnd = [this] { knobGesture = 0; };
        knob.onValueChange = [this, &knob, apply]
        {
            const auto value = static_cast<float>(knob.getValue());
            rack.edit(
                [this, value, apply](model::Project& p)
                {
                    if (auto* c = p.findChannel(id))
                        apply(*c, value);
                },
                knobGesture);
        };
        noFocus(knob);
        addAndMakeVisible(knob);
    };

    setupKnob(pan, -1.0, 1.0, 0.0, [](model::Channel& c, float v) { c.pan = v; });
    pan.setTooltip("Pan");
    pan.textFromValueFunction = [](double v)
    {
        const int percent = static_cast<int>(std::lround(std::abs(v) * 100.0));
        return percent == 0 ? juce::String("Centred")
                            : juce::String(percent) + (v < 0 ? "% left" : "% right");
    };

    setupKnob(volume, 0.0, 1.0, model::Channel{}.volume,
              [](model::Channel& c, float v) { c.volume = v; });
    volume.setTooltip("Volume");
    volume.textFromValueFunction = [](double v)
    { return juce::String(static_cast<int>(std::lround(v * 100.0))) + "%"; };

    // Left-click selects the channel (the Piano Roll edits the selected one); right-click
    // opens the channel menu, as in FL.
    name.setTooltip("Click to select, right-click for options");
    name.onClick = [this]
    {
        if (!juce::ModifierKeys::currentModifiers.isPopupMenu())
            rack.select(id);
    };
    name.addMouseListener(this, false);
    noFocus(name);
    steps.onOpenNotes = [this] { rack.openPianoRoll(id); };

    steps.newGestureId = [this] { return rack.document().newGestureId(); };
    steps.onStepPainted = [this](int step, bool on, int gesture)
    { rack.edit([this, step, on](model::Project& p) { p.current().set(id, step, on); }, gesture); };

    addAndMakeVisible(mute);
    addAndMakeVisible(name);
    addAndMakeVisible(steps);
}

void ChannelRow::update(const model::Channel& channel, const model::Project& project)
{
    mute.setOn(!channel.muted);
    pan.setValue(channel.pan, juce::dontSendNotification);
    volume.setValue(channel.volume, juce::dontSendNotification);
    name.setButtonText(juce::String::fromUTF8(channel.name.c_str()));
    name.setToggleState(project.selectedChannel == channel.id, juce::dontSendNotification);

    const auto& pattern = project.current();
    const int length = project.lengthInSteps(pattern);
    if (!pattern.hasOnlySteps(channel.id))
    {
        steps.setMelodicPreview(pattern.notesFor(channel.id), length);
        return;
    }
    std::vector<bool> stepValues(static_cast<std::size_t>(length));
    for (int s = 0; s < length; ++s)
        stepValues[static_cast<std::size_t>(s)] = pattern.isOn(channel.id, s);
    steps.setSteps(std::move(stepValues));
}

void ChannelRow::mouseDown(const juce::MouseEvent& e)
{
    if (e.eventComponent == &name && e.mods.isPopupMenu())
        showMenu();
}

void ChannelRow::resized()
{
    auto area = getLocalBounds();
    mute.setBounds(area.removeFromLeft(metrics::ledWidth + 4));
    pan.setBounds(area.removeFromLeft(metrics::knobSize + 4)
                      .withSizeKeepingCentre(metrics::knobSize, metrics::knobSize));
    volume.setBounds(area.removeFromLeft(metrics::knobSize + 4)
                         .withSizeKeepingCentre(metrics::knobSize, metrics::knobSize));
    area.removeFromLeft(4);
    name.setBounds(area.removeFromLeft(metrics::nameWidth).reduced(0, 2));
    area.removeFromLeft(8);
    steps.setBounds(area);
}

void ChannelRow::showMenu()
{
    juce::PopupMenu menu;
    menu.addItem("Piano roll", [this] { rack.openPianoRoll(id); });
    menu.addSeparator();
    menu.addItem("Rename...", [this] { rack.rename(id); });
    menu.addItem("Replace sample...", [this] { rack.chooseSampleFor(id); });
    menu.addSeparator();
    menu.addItem("Delete channel",
                 [this]
                 {
                     // Deleting destroys this row; defer so we're off its call stack.
                     auto& r = rack;
                     const auto channel = id;
                     juce::MessageManager::callAsync(
                         [&r, channel]
                         { r.edit([channel](model::Project& p) { p.removeChannel(channel); }); });
                 });
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&name));
}

// -------------------------------------------------------------------------------- ChannelRack

ChannelRack::ChannelRack(model::Document& document, engine::Transport& transportToShow,
                         io::SampleLoader& sampleLoader)
    : doc(document), transport(transportToShow), loader(sampleLoader)
{
    closeButton.setTooltip("Hide (F6)");
    closeButton.onClick = [this] { setVisible(false); };
    noFocus(closeButton);
    addAndMakeVisible(closeButton);

    dragBounds.setMinimumOnscreenAmounts(headerHeight, 80, 40, 80);

    addButton.setTooltip("Add channel from audio file");
    addButton.onClick = [this] { chooseSampleFor(0); };
    noFocus(addButton);
    addAndMakeVisible(addButton);

    refresh();
    startTimerHz(30);
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

void ChannelRack::openPianoRoll(model::ChannelId id)
{
    select(id);
    if (onOpenPianoRoll)
        onOpenPianoRoll(id);
}

void ChannelRack::refresh()
{
    const auto& project = doc.project();

    // Rebuild rows only if the set or order of channels changed; otherwise update in place
    // (so a knob being dragged isn't destroyed under the mouse).
    bool sameChannels = rows.size() == project.channels.size();
    for (std::size_t i = 0; sameChannels && i < rows.size(); ++i)
        sameChannels = rows[i]->channelId() == project.channels[i].id;

    if (!sameChannels)
    {
        rows.clear();
        for (const auto& channel : project.channels)
        {
            rows.push_back(std::make_unique<ChannelRow>(*this, channel.id));
            addAndMakeVisible(*rows.back());
        }
        lastPlayingStep = -2;
        resized();
    }

    for (std::size_t i = 0; i < rows.size(); ++i)
        rows[i]->update(project.channels[i], project);

    if (auto* parent = getParentComponent())
        parent->resized();
}

int ChannelRack::preferredWidth() const
{
    return hardShadowOffset + padding * 2 + metrics::ledWidth + 4 + 2 * (metrics::knobSize + 4) +
           4 + metrics::nameWidth + 8 +
           StepGrid::widthFor(doc.project().lengthInSteps(doc.project().current()));
}

int ChannelRack::preferredHeight() const
{
    const int n = static_cast<int>(doc.project().channels.size());
    return hardShadowOffset + headerHeight + padding * 2 +
           n * (metrics::rowHeight + metrics::rowGap) + metrics::rowHeight;
}

void ChannelRack::paint(juce::Graphics& g)
{
    const auto& t = currentTheme();
    const float radius = t.shape.windowRadius;
    auto panel = getLocalBounds().toFloat();

    if (t.effects.hardShadow)
    {
        // Solid offset shadow (Citrus): panel shifted up-left, shadow fills the gap.
        g.setColour(colour(t.colours.shadow));
        g.fillRoundedRectangle(
            panel.withTrimmedLeft(hardShadowOffset).withTrimmedTop(hardShadowOffset), radius);
        panel = panel.withTrimmedRight(hardShadowOffset).withTrimmedBottom(hardShadowOffset);
    }

    g.setColour(colour(t.colours.panel));
    g.fillRoundedRectangle(panel, radius);

    // Title bar: "Channel rack" and the pattern being edited.
    auto header = panel.removeFromTop(static_cast<float>(headerHeight));
    {
        juce::Path top;
        top.addRoundedRectangle(header.getX(), header.getY(), header.getWidth(), header.getHeight(),
                                radius, radius, true, true, false, false);
        g.setColour(colour(t.colours.header));
        g.fillPath(top);
        g.setColour(colour(t.colours.outline));
        g.fillRect(header.removeFromBottom(std::max(1.0f, t.shape.borderWidth)));
    }
    auto text = header.reduced(12.0f, 0.0f);
    g.setColour(colour(t.colours.text));
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("Channel rack", text.removeFromLeft(110.0f), juce::Justification::centredLeft);
    g.setColour(colour(t.colours.textDim));
    g.setFont(juce::FontOptions(13.0f));
    g.drawText(juce::String::fromUTF8(doc.project().current().name.c_str()),
               text.withTrimmedRight(30.0f), juce::Justification::centredLeft, true);

    panel = getLocalBounds().toFloat();
    if (t.effects.hardShadow)
        panel = panel.withTrimmedRight(hardShadowOffset).withTrimmedBottom(hardShadowOffset);
    if (t.shape.borderWidth > 0.0f)
    {
        g.setColour(colour(t.colours.outline));
        g.drawRoundedRectangle(panel.reduced(t.shape.borderWidth / 2.0f), radius,
                               t.shape.borderWidth);
    }
}

void ChannelRack::paintOverChildren(juce::Graphics& g)
{
    if (!dragging)
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
    auto header = bounds.removeFromTop(headerHeight);
    closeButton.setBounds(header.removeFromRight(headerHeight + hardShadowOffset)
                              .withTrimmedRight(hardShadowOffset)
                              .reduced(5));
    auto area = bounds.reduced(padding);
    for (auto& row : rows)
    {
        row->setBounds(area.removeFromTop(metrics::rowHeight));
        area.removeFromTop(metrics::rowGap);
    }
    const int nameX = metrics::ledWidth + 4 + 2 * (metrics::knobSize + 4) + 4;
    addButton.setBounds(padding + nameX, area.getY(), metrics::nameWidth, metrics::rowHeight - 4);
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

ChannelRow* ChannelRack::rowAt(int y)
{
    for (auto& row : rows)
        if (y >= row->getY() && y < row->getBottom())
            return row.get();
    return nullptr;
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
                             {
                                 addChannelsFromFiles(fc.getResults());
                                 return;
                             }
                             const auto file = fc.getResult();
                             if (auto sample = loader.load(file))
                                 edit(
                                     [id, sample](model::Project& p)
                                     {
                                         if (auto* c = p.findChannel(id))
                                             c->sample = sample;
                                     });
                         });
}

void ChannelRack::addChannelsFromFiles(const juce::Array<juce::File>& files)
{
    std::vector<std::pair<std::string, std::shared_ptr<const engine::SampleBuffer>>> loaded;
    for (const auto& file : files)
        if (auto sample = loader.load(file))
            loaded.emplace_back(file.getFileNameWithoutExtension().toStdString(), sample);

    if (!loaded.empty())
        edit(
            [&loaded](model::Project& p)
            {
                for (auto& [channelName, sample] : loaded)
                    p.addChannel(channelName, sample);
            });
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
    int row = -1;
    if (auto* r = rowAt(y))
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (rows[i].get() == r)
                row = static_cast<int>(i);

    if (!dragging || row != dropRow)
    {
        dragging = true;
        dropRow = row;
        repaint();
    }
}

void ChannelRack::fileDragExit(const juce::StringArray&)
{
    dragging = false;
    repaint();
}

void ChannelRack::filesDropped(const juce::StringArray& paths, int, int y)
{
    dragging = false;
    repaint();

    juce::Array<juce::File> files;
    for (const auto& path : paths)
        if (loader.canLoad(juce::File(path)))
            files.add(juce::File(path));
    if (files.isEmpty())
        return;

    // Dropped on a row: replace that channel's sample (first file). Elsewhere: new channels.
    if (auto* row = rowAt(y))
    {
        if (auto sample = loader.load(files.getFirst()))
        {
            const auto id = row->channelId();
            edit(
                [id, sample](model::Project& p)
                {
                    if (auto* c = p.findChannel(id))
                        c->sample = sample;
                });
        }
        return;
    }
    addChannelsFromFiles(files);
}

} // namespace jls::ui

namespace jls::ui
{

void ChannelRack::mouseDown(const juce::MouseEvent& e)
{
    if (e.y < headerHeight)
        dragger.startDraggingComponent(this, e);
}

void ChannelRack::mouseDrag(const juce::MouseEvent& e)
{
    if (e.mouseDownPosition.y < headerHeight)
        dragger.dragComponent(this, e, &dragBounds);
}

} // namespace jls::ui
