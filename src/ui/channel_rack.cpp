#include "ui/channel_rack.h"

#include "engine/step_sequencer.h"
#include "ui/theme.h"

#include <cmath>

namespace jls::ui
{
namespace
{

using namespace theme;

// Keep keyboard focus on the main window so Space etc. always reach the transport.
void noFocus(juce::Component& c)
{
    c.setWantsKeyboardFocus(false);
    c.setMouseClickGrabsKeyboardFocus(false);
}

constexpr int padding = 8;

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
    const auto led = getLocalBounds().toFloat().withSizeKeepingCentre(9.0f, 9.0f);
    g.setColour(on ? colours::ledOn : colours::ledOff);
    g.fillEllipse(led);
    g.setColour(colours::panelOutline);
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
    if (newSteps != steps)
    {
        steps = std::move(newSteps);
        repaint();
    }
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
    for (int s = 0; s < static_cast<int>(steps.size()); ++s)
    {
        const auto r =
            juce::Rectangle<int>(stepX(s), 2, metrics::stepWidth, getHeight() - 4).toFloat();
        const auto offColour = (s / 4) % 2 == 0 ? colours::stepOffA : colours::stepOffB;
        g.setColour(steps[static_cast<std::size_t>(s)] ? colours::stepOn : offColour);
        g.fillRoundedRectangle(r, metrics::cornerRadius);

        if (s == playingStep)
        {
            g.setColour(colours::playhead);
            g.drawRoundedRectangle(r.reduced(0.5f), metrics::cornerRadius, 1.5f);
        }
    }
}

void StepGrid::mouseDown(const juce::MouseEvent& e)
{
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
    if (!steps.empty())
        paintStep(stepAt(e.x));
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

    name.onClick = [this] { showMenu(); };
    noFocus(name);

    steps.newGestureId = [this] { return rack.document().newGestureId(); };
    steps.onStepPainted = [this](int step, bool on, int gesture)
    { rack.edit([this, step, on](model::Project& p) { p.pattern.set(id, step, on); }, gesture); };

    addAndMakeVisible(mute);
    addAndMakeVisible(name);
    addAndMakeVisible(steps);
}

void ChannelRow::update(const model::Channel& channel, const model::Pattern& pattern)
{
    mute.setOn(!channel.muted);
    pan.setValue(channel.pan, juce::dontSendNotification);
    volume.setValue(channel.volume, juce::dontSendNotification);
    name.setButtonText(juce::String::fromUTF8(channel.name.c_str()));

    std::vector<bool> stepValues(static_cast<std::size_t>(pattern.lengthInSteps));
    for (int s = 0; s < pattern.lengthInSteps; ++s)
        stepValues[static_cast<std::size_t>(s)] = pattern.isOn(channel.id, s);
    steps.setSteps(std::move(stepValues));
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
        rows[i]->update(project.channels[i], project.pattern);

    if (auto* parent = getParentComponent())
        parent->resized();
}

int ChannelRack::preferredWidth() const
{
    return padding * 2 + metrics::ledWidth + 4 + 2 * (metrics::knobSize + 4) + 4 +
           metrics::nameWidth + 8 + StepGrid::widthFor(doc.project().pattern.lengthInSteps);
}

int ChannelRack::preferredHeight() const
{
    const int n = static_cast<int>(doc.project().channels.size());
    return padding * 2 + n * (metrics::rowHeight + metrics::rowGap) + metrics::rowHeight;
}

void ChannelRack::paint(juce::Graphics& g)
{
    g.setColour(colours::panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
    g.setColour(colours::panelOutline);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);
}

void ChannelRack::paintOverChildren(juce::Graphics& g)
{
    if (!dragging)
        return;
    g.setColour(colours::dropHighlight);
    if (dropRow >= 0 && dropRow < static_cast<int>(rows.size()))
        g.fillRect(rows[static_cast<std::size_t>(dropRow)]->getBounds());
    else
        g.fillRect(addButton.getBounds().withRight(getWidth() - padding));
}

void ChannelRack::resized()
{
    auto area = getLocalBounds().reduced(padding);
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
    int step = -1;
    const int length = doc.project().pattern.lengthInSteps;
    if (transport.isPlaying() && length > 0)
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
