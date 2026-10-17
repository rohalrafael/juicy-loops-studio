#include "ui/sampler_panel.h"

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
    fileLabel.setFont(juce::FontOptions(12.5f));
    stretchHeading.setFont(juce::FontOptions(15.0f, juce::Font::bold));

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

    tempoBox.setSliderStyle(juce::Slider::IncDecButtons);
    tempoBox.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 80, 24);
    tempoBox.setRange(10.0, 999.0, 0.01);
    tempoBox.setTextValueSuffix(" BPM");
    tempoBox.setTooltip("The tempo the sample was made at: Resample and Stretch fit it to the "
                        "project tempo from there");
    tempoBox.onValueChange = [this]
    {
        const double bpm = tempoBox.getValue();
        edit([bpm](model::SamplerSettings& s) { s.originalTempo = bpm; });
    };
    fromProject.setTooltip("Treat the sample as being at the project's tempo (no change now)");
    fromProject.onClick = [this]
    {
        const double tempo = doc.project().tempo;
        edit([tempo](model::SamplerSettings& s) { s.originalTempo = tempo; });
    };

    pitchBox.setSliderStyle(juce::Slider::IncDecButtons);
    pitchBox.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 80, 24);
    pitchBox.setRange(-24.0, 24.0, 1.0);
    pitchBox.setTextValueSuffix(" st");
    pitchBox.setTooltip("Transpose in semitones. In Stretch mode the length stays; otherwise the "
                        "sample also gets faster or slower.");
    pitchBox.onValueChange = [this]
    {
        const double pitch = pitchBox.getValue();
        edit([pitch](model::SamplerSettings& s) { s.pitch = pitch; });
    };

    info.setFont(juce::FontOptions(12.5f));
    for (auto* c : std::initializer_list<juce::Component*>{
             &fileLabel, &stretchHeading, &modeLabel, &modeBox, &tempoLabel, &tempoBox,
             &fromProject, &pitchLabel, &pitchBox, &info})
    {
        c->setWantsKeyboardFocus(false);
        addAndMakeVisible(c);
    }
    setSize(460, 330);
    refresh();
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
    fileLabel.setText(channel->samplePath.empty()
                          ? juce::String("Built-in sound: ") +
                                juce::String::fromUTF8(channel->builtinSample.c_str())
                          : file.getFileName(),
                      juce::dontSendNotification);

    const auto& s = channel->sampler;
    modeBox.setSelectedId(static_cast<int>(s.mode) + 1, juce::dontSendNotification);
    tempoBox.setValue(s.originalTempo > 0.0 ? s.originalTempo : project.tempo,
                      juce::dontSendNotification);
    pitchBox.setValue(s.pitch, juce::dontSendNotification);
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
        info.setText(text, juce::dontSendNotification);
    }
    else
    {
        info.setText("No sample loaded", juce::dontSendNotification);
    }
    return true;
}

void SamplerPanel::paint(juce::Graphics& g)
{
    const auto& t = currentTheme();
    g.fillAll(colour(t.colours.panel));
    auto area = getLocalBounds().reduced(14).removeFromTop(waveformHeight + 24).withTrimmedTop(24);
    g.setColour(colour(t.colours.display));
    g.fillRoundedRectangle(area.toFloat(), 4.0f);
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

void SamplerPanel::resized()
{
    auto area = getLocalBounds().reduced(14);
    fileLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(waveformHeight + 10);
    stretchHeading.setBounds(area.removeFromTop(26));
    const auto row = [&area](juce::Label& label, juce::Component& control, int width)
    {
        auto line = area.removeFromTop(32);
        label.setBounds(line.removeFromLeft(120));
        control.setBounds(line.removeFromLeft(width).reduced(0, 3));
        return line;
    };
    row(modeLabel, modeBox, 150);
    auto tempoLine = row(tempoLabel, tempoBox, 170);
    fromProject.setBounds(tempoLine.removeFromLeft(140).reduced(6, 4));
    row(pitchLabel, pitchBox, 170);
    info.setBounds(area.removeFromTop(26));
}

SamplerWindow::SamplerWindow(model::Document& document, model::ChannelId channel)
    : juce::DocumentWindow("Sampler",
                           juce::LookAndFeel::getDefaultLookAndFeel().findColour(
                               juce::ResizableWindow::backgroundColourId),
                           juce::DocumentWindow::closeButton),
      content(new SamplerPanel(document, channel))
{
    setUsingNativeTitleBar(true);
    setContentOwned(content, true);
    setResizable(false, false);
    centreWithSize(getWidth(), getHeight());
    setVisible(true);
}

void SamplerWindow::closeButtonPressed()
{
    if (onClose)
        onClose();
}

} // namespace jls::ui
