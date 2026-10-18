#include "ui/sampler_panel.h"

#include "ui/floating_panel.h"

#include "ui/theme.h"

namespace jls::ui
{
namespace
{

constexpr int waveformHeight = 110;

juce::String seconds(double s)
{
    return juce::String(s, 2) + " s";
}

} // namespace

SamplerPanel::SamplerPanel(model::Document& document, model::ChannelId channel)
    : doc(document), id(channel)
{
    modeBox.addItem("Off", 1);
    modeBox.addItem("Resample", 2);
    modeBox.addItem("Stretch", 3);
    modeBox.setTooltip("Off: plays at its own speed. Resample: speed follows the tempo and the "
                       "pitch moves with it. Stretch: length follows the tempo, pitch stays.");
    modeBox.onChange = [this]
    {
        const auto mode = static_cast<model::StretchMode>(modeBox.getSelectedId() - 1);
        const double tempo = doc.project().tempo;
        edit(
            [mode, tempo](model::SamplerSettings& s)
            {
                s.mode = mode;
                if (s.originalTempo <= 0.0) // following the tempo needs a starting point
                    s.originalTempo = tempo;
            });
    };

    tempoBox.setRange(10.0, 999.0);
    tempoBox.setTooltip("The tempo the sample was made at: Resample and Stretch fit it to the "
                        "project tempo from there");
    tempoBox.onChange = [this](double bpm)
    { edit([bpm](model::SamplerSettings& s) { s.originalTempo = bpm; }); };
    fromProject.setTooltip("Treat the sample as being at the project's tempo (no change now)");
    fromProject.onClick = [this]
    {
        const double tempo = doc.project().tempo;
        edit([tempo](model::SamplerSettings& s) { s.originalTempo = tempo; });
    };

    pitchBox.setRange(-24.0, 24.0);
    pitchBox.showPlus = true;
    pitchBox.setTooltip("Transpose in semitones. In Stretch mode the length stays; otherwise the "
                        "sample also gets faster or slower.");
    pitchBox.onChange = [this](double pitch)
    { edit([pitch](model::SamplerSettings& s) { s.pitch = pitch; }); };

    for (auto* c : std::initializer_list<juce::Component*>{&modeBox, &fromProject})
    {
        c->setWantsKeyboardFocus(false);
        c->setMouseClickGrabsKeyboardFocus(false);
    }
    // Right column: cutting and the envelope.
    cutItself.setClickingTogglesState(true);
    cutItself.setTooltip("A new note stops this channel's note that's still sounding");
    cutItself.onClick = [this]
    {
        const bool on = cutItself.getToggleState();
        edit([on](model::SamplerSettings& s) { s.cutItself = on; });
    };
    for (auto* box : {&cutGroupBox, &cutByBox})
    {
        box->addItem("None", 1);
        for (int group = 1; group <= model::SamplerSettings::maxCutGroup; ++group)
            box->addItem(juce::String(group), group + 1);
        box->setWantsKeyboardFocus(false);
    }
    cutGroupBox.setTooltip("Cut: notes on this channel stop the channels cut by this group");
    cutByBox.setTooltip("Cut by: this channel stops when a channel in this group plays (closed "
                        "hat cuts open hat: give both the same number)");
    cutGroupBox.onChange = [this]
    {
        const int group = cutGroupBox.getSelectedId() - 1;
        edit([group](model::SamplerSettings& s) { s.cutGroup = std::max(0, group); });
    };
    cutByBox.onChange = [this]
    {
        const int group = cutByBox.getSelectedId() - 1;
        edit([group](model::SamplerSettings& s) { s.cutBy = std::max(0, group); });
    };
    envelopeOn.setClickingTogglesState(true);
    envelopeOn.setTooltip("Shape the volume with the envelope. Off: the sample plays to its end; "
                          "on: it releases where the note ends");
    envelopeOn.onClick = [this]
    {
        const bool on = envelopeOn.getToggleState();
        edit([on](model::SamplerSettings& s) { s.envelope.on = on; });
    };
    envelope.newGestureId = [this] { return doc.newGestureId(); };
    envelope.onChange = [this](const model::Envelope& e, int gesture)
    {
        edit(
            [e](model::SamplerSettings& s)
            {
                const bool on = s.envelope.on;
                s.envelope = e;
                s.envelope.on = on; // the knobs don't switch it
            },
            gesture);
    };
    for (auto* c : std::initializer_list<juce::Component*>{&cutItself, &envelopeOn})
        c->setMouseClickGrabsKeyboardFocus(false);
    second.addSection("Cut");
    second.addRow("Cut itself", {{&cutItself, cutItself.preferredWidth(metrics::controlHeight)}});
    second.addRow("Cut group", {{&cutGroupBox, metrics::comboWidth}});
    second.addRow("Cut by", {{&cutByBox, metrics::comboWidth}});
    second.addSection("Envelope");
    second.addRow("Volume envelope",
                  {{&envelopeOn, envelopeOn.preferredWidth(metrics::controlHeight) + 8}});
    second.addComponent(envelope, EnvelopeEditor::preferredHeight);
    addAndMakeVisible(second);

    const int field = tempoBox.preferredWidth(metrics::controlHeight);
    addSection("Sample");
    fileNote = addNote(" ");
    addPainted(waveformHeight,
               [this](juce::Graphics& g, juce::Rectangle<int> area) { paintWaveform(g, area); });
    infoNote = addNote(" ");
    addSection("Time stretching");
    addRow("Mode", {{&modeBox, 150}});
    addRow("Original tempo",
           {{&tempoBox, field},
            {&fromProject, buttonWidth("Use project tempo", metrics::controlHeight)}});
    addRow("Pitch", {{&pitchBox, pitchBox.preferredWidth(metrics::controlHeight)}});
    constexpr int leftWidth = 480;
    constexpr int rightWidth = 420;
    itemsWidth = leftWidth;
    setSize(leftWidth + rightWidth,
            std::max(preferredHeight(leftWidth), second.preferredHeight(rightWidth)));
    refresh();
}

void SamplerPanel::resized()
{
    FormPanel::resized();
    second.setBounds(getLocalBounds().withTrimmedLeft(itemsWidth));
}

SamplerPanel::~SamplerPanel() = default;

void SamplerPanel::edit(const std::function<void(model::SamplerSettings&)>& change, int gesture)
{
    const auto channel = id;
    doc.apply(
        [channel, change](model::Project& p)
        {
            if (auto* c = p.findChannel(channel))
                change(c->sampler);
        },
        gesture);
}

bool SamplerPanel::refresh()
{
    const auto& project = doc.project();
    const auto* channel = project.findChannel(id);
    if (channel == nullptr || channel->isInstrument())
        return false;

    if (channel->sample.get() != peaksFor)
    {
        peaksFor = channel->sample.get();
        peaks = channel->sample != nullptr ? computePeaks(*channel->sample) : Peaks{};
        repaint();
    }
    const auto file = juce::File(juce::String::fromUTF8(channel->samplePath.c_str()));
    setNote(fileNote, channel->samplePath.empty()
                          ? juce::String("Built-in sound: ") +
                                juce::String::fromUTF8(channel->builtinSample.c_str())
                          : file.getFileName());
    if (auto* panel = findParentComponentOfClass<FloatingPanel>())
        panel->setTitle("Sampler", juce::String::fromUTF8(channel->name.c_str()));

    const auto& s = channel->sampler;
    modeBox.setSelectedId(static_cast<int>(s.mode) + 1, juce::dontSendNotification);
    tempoBox.setValue(s.originalTempo > 0.0 ? s.originalTempo : project.tempo);
    pitchBox.setValue(s.pitch);
    cutItself.setToggleState(s.cutItself, juce::dontSendNotification);
    cutGroupBox.setSelectedId(s.cutGroup + 1, juce::dontSendNotification);
    cutByBox.setSelectedId(s.cutBy + 1, juce::dontSendNotification);
    envelopeOn.setToggleState(s.envelope.on, juce::dontSendNotification);
    envelope.setEnvelope(s.envelope);
    const bool follows = s.mode != model::StretchMode::off;
    tempoBox.setEnabled(follows);
    fromProject.setEnabled(follows);

    if (channel->sample != nullptr && channel->sample->numFrames() > 0)
    {
        const double length = channel->sample->numFrames() / channel->sample->sampleRate;
        juce::String text = "Sample " + seconds(length);
        switch (s.mode)
        {
        case model::StretchMode::off:
            text << ", plays " << seconds(project.playedSeconds(*channel));
            break;
        case model::StretchMode::resample:
            text << ", plays " << juce::String(project.playbackSpeed(*channel), 2)
                 << "x as fast: " << seconds(project.playedSeconds(*channel));
            break;
        case model::StretchMode::stretch:
            text << ", stretched to " << seconds(project.playedSeconds(*channel)) << " at "
                 << juce::String(project.tempo, 1) << " BPM";
            break;
        }
        setNote(infoNote, text);
    }
    else
    {
        setNote(infoNote, "No sample loaded");
    }
    return true;
}

void SamplerPanel::paintWaveform(juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto& t = currentTheme();
    paintDisplayBox(g, area.toFloat());
    if (peaks.empty())
        return;
    const auto wave = area.toFloat().reduced(6.0f);
    const float mid = wave.getCentreY();
    const float half = wave.getHeight() / 2.0f;
    g.setColour(colour(t.colours.accentAlt));
    const int width = static_cast<int>(wave.getWidth());
    for (int x = 0; x < width; ++x)
    {
        const auto& [low, high] =
            peaks[static_cast<std::size_t>(x * static_cast<int>(peaks.size()) / width)];
        g.fillRect(wave.getX() + x, mid - high * half, 1.0f, std::max(1.0f, (high - low) * half));
    }
}

} // namespace jls::ui
