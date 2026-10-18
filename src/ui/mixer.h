#pragma once

// The docked Mixer panel (strips with meters, faders, pan, mute/solo and effect slots for the
// selected track) and the floating EffectPanel that edits one effect's parameters.

#include "model/document.h"
#include "ui/plugin_menu.h"

#include "ui/controls.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace jls::ui
{

class MixerView;

// One mixer track: name, stereo peak meter, fader, pan, mute and solo. Click it to select it.
class MixerStrip final : public juce::Component
{
  public:
    static constexpr int width = 66;

    MixerStrip(MixerView& owner, int track);

    void update(const model::MixerTrack& track, bool selected);
    void setLevels(float left, float right); // linear peaks since the last call

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;

  private:
    MixerView& mixer;
    int index;
    juce::Slider fader;
    juce::Slider pan;
    juce::TextButton mute{"M"};
    juce::TextButton solo{"S"};
    std::array<float, 2> shown{}; // meter levels with fall-off, in dB
    bool selected = false;
    juce::String name;
    int gesture = 0;
};

// The mixer panel: strips for Master + inserts, and the selected track's effect slots.
class MixerView final : public juce::Component
{
  public:
    static constexpr int preferredHeight = 268;

    explicit MixerView(model::Document& document);
    ~MixerView() override;

    // Re-reads the document. Call after every document change.
    void refresh();
    // Feeds meter levels: peak(track, side) returns the linear peak since the last call.
    void updateMeters(const std::function<float(int, int)>& peak);

    // Set by the owner: open the editor for an effect.
    std::function<void(model::EffectId)> onOpenEffect;
    // Set by the owner: known plugin effects, the "Manage plugins" action, and whether an effect
    // could be loaded (plugins may be missing on this machine).
    using PluginItem = ui::PluginItem;
    std::function<std::vector<PluginItem>()> listPlugins;
    std::function<void()> onManagePlugins;
    std::function<bool(model::EffectId)> isEffectAvailable;

    model::Document& document() { return doc; }
    void selectTrack(int track);
    void editTrack(int track, const std::function<void(model::MixerTrack&)>& change, int gesture);

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override { paintCardFrame(g, *this); }
    void resized() override;

  private:
    class Slots;

    model::Document& doc;
    juce::Viewport viewport;
    juce::Component stripHolder;
    std::vector<std::unique_ptr<MixerStrip>> strips;
    std::unique_ptr<Slots> slots;
};

// Floating editor for one effect: a control per parameter. Drag the title bar to move it.
class EffectPanel final : public juce::Component
{
  public:
    explicit EffectPanel(model::Document& document);

    void show(model::EffectId id);
    // Re-reads the document; hides itself if its effect was removed.
    void refresh();
    model::EffectId effect() const { return effectId; }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

  private:
    void rebuild();
    void setParam(std::size_t index, float value, int gesture);

    model::Document& doc;
    model::EffectId effectId = 0;
    std::string builtFor; // effect type the controls were built for
    juce::String heading;
    IconButton closeButton{Icon::close};
    IconButton enabled{Icon::none, "On"};                   // lit while the effect isn't bypassed
    std::vector<std::unique_ptr<juce::Component>> controls; // Slider or ComboBox per param
    std::vector<std::unique_ptr<juce::Label>> labels;
    juce::ComponentDragger dragger;
    juce::ComponentBoundsConstrainer dragBounds;
    bool movingPanel = false; // the current press started on the title bar
    int gesture = 0;
};

} // namespace jls::ui
