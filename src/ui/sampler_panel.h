#pragma once

// The Sampler: a sample channel's settings window (FL's channel settings for its sampler). For
// now: the sample's waveform and its time-stretching (Off / Resample / Stretch, original tempo,
// pitch). Drum channels and audio clips alike, since audio clips are channels too.

#include "model/document.h"
#include "ui/waveform.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace jls::ui
{

class SamplerPanel final : public juce::Component
{
  public:
    SamplerPanel(model::Document& document, model::ChannelId channel);
    ~SamplerPanel() override;

    model::ChannelId channel() const { return id; }
    // Re-reads the document. Returns false if the channel is gone (close the window).
    bool refresh();

    void paint(juce::Graphics& g) override;
    void resized() override;

  private:
    void edit(const std::function<void(model::SamplerSettings&)>& change, int gesture = 0);

    model::Document& doc;
    model::ChannelId id;
    const engine::SampleBuffer* peaksFor = nullptr;
    Peaks peaks;

    juce::Label fileLabel;
    juce::Label stretchHeading{{}, "Time stretching"};
    juce::Label modeLabel{{}, "Mode"};
    juce::ComboBox modeBox;
    juce::Label tempoLabel{{}, "Original tempo"};
    juce::Slider tempoBox;
    juce::TextButton fromProject{"Use project tempo"};
    juce::Label pitchLabel{{}, "Pitch"};
    juce::Slider pitchBox;
    juce::Label info;
    int gestureId = 0;
};

// The window around a SamplerPanel.
class SamplerWindow final : public juce::DocumentWindow
{
  public:
    SamplerWindow(model::Document& document, model::ChannelId channel);

    SamplerPanel& panel() { return *content; }
    std::function<void()> onClose;
    void closeButtonPressed() override;

  private:
    SamplerPanel* content;
};

} // namespace jls::ui
