#pragma once

// The Sampler's envelope editor, as in FL's channel window: a drawing of the DAHDSR shape and a
// knob per stage (delay, attack, hold, decay, sustain, release), each with its value below.
// Times go from 0 to 10 s on a curved scale (fine control near 0); sustain is a level.

#include "model/project.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>

namespace jls::ui
{

class EnvelopeEditor final : public juce::Component
{
  public:
    static constexpr int preferredHeight = 150;

    EnvelopeEditor();

    void setEnvelope(const model::Envelope& envelope); // doesn't call onChange
    // A knob changed (one gesture per drag, from newGestureId).
    std::function<void(const model::Envelope&, int gesture)> onChange;
    std::function<int()> newGestureId;

    void paint(juce::Graphics& g) override;
    void resized() override;

  private:
    model::Envelope current;
    std::array<juce::Slider, 6> knobs;
    juce::Rectangle<int> graph;
    int gesture = 0;
};

} // namespace jls::ui
