#include "ui/mixer.h"

#include "engine/effects.h"
#include "ui/theme.h"

#include <cmath>

namespace jls::ui
{
namespace
{

constexpr int headerHeight = metrics::panelHeaderHeight;
constexpr int slotsWidth = 230;
constexpr int slotHeight = 26;
constexpr float meterFloorDb = -60.0f;
constexpr float meterFallDbPerTick = 1.5f; // at 30 Hz: ~45 dB per second

void noFocus(juce::Component& c)
{
    c.setWantsKeyboardFocus(false);
    c.setMouseClickGrabsKeyboardFocus(false);
}

float toDb(float linear)
{
    return linear > 1.0e-6f ? 20.0f * std::log10(linear) : -120.0f;
}

juce::String formatValue(const engine::ParamInfo& info, double value)
{
    if (info.stepped && !info.stepNames.empty())
    {
        const auto i = static_cast<std::size_t>(std::lround(value));
        return i < info.stepNames.size() ? juce::String(info.stepNames[i]) : juce::String();
    }
    const juce::String unit(info.unit);
    if (unit == "Hz" && value >= 1000.0)
        return juce::String(value / 1000.0, 2) + " kHz";
    if (unit.isEmpty() && info.max <= 1.0f)
        return juce::String(juce::roundToInt(value * 100.0)) + "%";
    const int decimals = std::abs(value) < 10.0 ? 1 : 0;
    return juce::String(value, decimals) + (unit.isEmpty() || unit == ":1" ? "" : " ") + unit;
}

} // namespace

// ---------------------------------------------------------------------------------- Strip

MixerStrip::MixerStrip(MixerView& owner, int track) : mixer(owner), index(track)
{
    fader.setSliderStyle(juce::Slider::LinearVertical);
    fader.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    fader.setRange(model::MixerTrack::minVolumeDb, model::MixerTrack::maxVolumeDb, 0.01);
    fader.setSkewFactorFromMidPoint(-12.0); // more travel near 0 dB, like a real fader
    fader.setDoubleClickReturnValue(true, 0.0);
    fader.setPopupDisplayEnabled(true, true, nullptr);
    fader.textFromValueFunction = [](double db)
    {
        return db <= model::MixerTrack::minVolumeDb ? juce::String("-inf dB")
                                                    : juce::String(db, 1) + " dB";
    };
    fader.onDragStart = [this] { gesture = mixer.document().newGestureId(); };
    fader.onValueChange = [this]
    {
        const auto db = static_cast<float>(fader.getValue());
        mixer.editTrack(index, [db](model::MixerTrack& t) { t.volumeDb = db; }, gesture);
    };

    pan.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    pan.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    pan.setRange(-1.0, 1.0, 0.0);
    pan.setDoubleClickReturnValue(true, 0.0);
    pan.setPopupDisplayEnabled(true, true, nullptr);
    pan.textFromValueFunction = [](double v)
    {
        const int percent = juce::roundToInt(std::abs(v) * 100.0);
        return percent == 0 ? juce::String("Centred")
                            : juce::String(percent) + (v < 0 ? "% left" : "% right");
    };
    pan.onDragStart = [this] { gesture = mixer.document().newGestureId(); };
    pan.onValueChange = [this]
    {
        const auto value = static_cast<float>(pan.getValue());
        mixer.editTrack(index, [value](model::MixerTrack& t) { t.pan = value; }, gesture);
    };

    mute.setClickingTogglesState(false);
    mute.setTooltip("Mute");
    mute.onClick = [this]
    { mixer.editTrack(index, [](model::MixerTrack& t) { t.muted = !t.muted; }, 0); };
    solo.setTooltip("Solo");
    solo.onClick = [this]
    { mixer.editTrack(index, [](model::MixerTrack& t) { t.solo = !t.solo; }, 0); };

    for (auto* c : std::initializer_list<juce::Component*>{&fader, &pan, &mute, &solo})
    {
        noFocus(*c);
        addAndMakeVisible(c);
    }
    solo.setVisible(index != 0); // soloing the Master means nothing
    shown.fill(meterFloorDb);
}

void MixerStrip::update(const model::MixerTrack& track, bool isSelected)
{
    fader.setValue(track.volumeDb, juce::dontSendNotification);
    pan.setValue(track.pan, juce::dontSendNotification);
    mute.setToggleState(track.muted, juce::dontSendNotification);
    solo.setToggleState(track.solo, juce::dontSendNotification);
    name = juce::String::fromUTF8(track.name.c_str());
    if (isSelected != selected)
        selected = isSelected;
    repaint();
}

void MixerStrip::setLevels(float left, float right)
{
    bool changed = false;
    const float levels[2] = {toDb(left), toDb(right)};
    for (std::size_t i = 0; i < 2; ++i)
    {
        const float next = std::max(levels[i], shown[i] - meterFallDbPerTick);
        const float clamped = std::max(next, meterFloorDb);
        if (std::abs(clamped - shown[i]) > 0.01f)
            changed = true;
        shown[i] = clamped;
    }
    if (changed)
        repaint(getLocalBounds().withTrimmedTop(20).withWidth(22));
}

void MixerStrip::paint(juce::Graphics& g)
{
    const auto& t = currentTheme();
    auto bounds = getLocalBounds().toFloat().reduced(2.0f, 0.0f);
    g.setColour(colour(selected ? t.colours.activeBackground
                                : (index == 0 ? t.colours.panelRaised : t.colours.lane)));
    g.fillRoundedRectangle(bounds, metrics::cornerRadius);
    g.setColour(colour(selected ? t.colours.accent : t.colours.outline));
    g.drawRoundedRectangle(bounds.reduced(0.5f), metrics::cornerRadius, selected ? 1.5f : 1.0f);

    g.setColour(colour(t.colours.text));
    g.setFont(juce::FontOptions(metrics::textSmall));
    g.drawText(name, getLocalBounds().removeFromTop(20).reduced(4, 0), juce::Justification::centred,
               true);

    // Meter: two bars left of the fader, top = +6 dB, red-ish (accent) above 0 dB.
    const auto meter = juce::Rectangle<float>(8.0f, 24.0f, 12.0f, fader.getHeight() - 8.0f);
    for (int side = 0; side < 2; ++side)
    {
        const auto bar = meter.withWidth(5.0f).translated(side * 7.0f, 0.0f);
        g.setColour(colour(t.colours.display));
        g.fillRect(bar);
        const float fraction = juce::jlimit(0.0f, 1.0f,
                                            (shown[static_cast<std::size_t>(side)] - meterFloorDb) /
                                                (model::MixerTrack::maxVolumeDb - meterFloorDb));
        const auto level = bar.withTop(bar.getBottom() - bar.getHeight() * fraction);
        const float zeroY = bar.getBottom() - bar.getHeight() * (-meterFloorDb) /
                                                  (model::MixerTrack::maxVolumeDb - meterFloorDb);
        g.setColour(colour(t.colours.accentAlt));
        g.fillRect(level.withTop(std::max(level.getY(), zeroY)));
        if (level.getY() < zeroY)
        {
            g.setColour(colour(t.colours.accent));
            g.fillRect(level.withBottom(zeroY));
        }
    }

    g.setColour(colour(t.colours.textDim));
    g.setFont(juce::FontOptions(metrics::textTiny));
    g.drawText(index == 0 ? juce::String("M") : juce::String(index),
               getLocalBounds().removeFromBottom(16), juce::Justification::centred);
}

void MixerStrip::resized()
{
    auto area = getLocalBounds().reduced(4, 0);
    area.removeFromTop(20);
    area.removeFromBottom(16);
    auto buttons = area.removeFromBottom(22);
    if (index == 0)
        mute.setBounds(buttons.reduced(8, 1));
    else
    {
        mute.setBounds(buttons.removeFromLeft(buttons.getWidth() / 2).reduced(1));
        solo.setBounds(buttons.reduced(1));
    }
    pan.setBounds(area.removeFromBottom(30).withSizeKeepingCentre(26, 26));
    area.removeFromLeft(20); // meter
    fader.setBounds(area.reduced(0, 2));
}

void MixerStrip::mouseDown(const juce::MouseEvent&)
{
    mixer.selectTrack(index);
}

// ---------------------------------------------------------------------------------- Slots

// The selected track's effect chain. Click an empty slot to add an effect, a full one to edit
// it; right-click for more.
class MixerView::Slots final : public juce::Component
{
  public:
    explicit Slots(MixerView& v) : view(v) {}

    void paint(juce::Graphics& g) override
    {
        const auto& t = currentTheme();
        const auto& project = view.doc.project();
        const auto& track =
            project.mixer.tracks[static_cast<std::size_t>(project.selectedMixerTrack)];

        g.setColour(colour(t.colours.text));
        g.setFont(semiboldFont(metrics::textLarge));
        g.drawText(juce::String::fromUTF8(track.name.c_str()) + " effects",
                   juce::Rectangle<int>(10, 0, getWidth() - 20, 24),
                   juce::Justification::centredLeft);

        g.setFont(juce::FontOptions(metrics::textBody));
        for (int i = 0; i < model::MixerTrack::maxEffects; ++i)
        {
            const auto row = slotBounds(i).toFloat();
            g.setColour(colour(t.colours.lane));
            g.fillRoundedRectangle(row, metrics::cornerRadius);
            g.setColour(colour(t.colours.outline));
            g.drawRoundedRectangle(row.reduced(0.5f), metrics::cornerRadius, 1.0f);

            if (i < static_cast<int>(track.effects.size()))
            {
                const auto& slot = track.effects[static_cast<std::size_t>(i)];
                const auto* info = engine::findEffectType(slot.type);
                const auto led = row.withWidth(row.getHeight()).withSizeKeepingCentre(8.0f, 8.0f);
                g.setColour(colour(slot.enabled ? t.colours.led : t.colours.ledOff));
                g.fillEllipse(led);
                juce::String name =
                    slot.isPlugin()
                        ? juce::String::fromUTF8(slot.pluginName.c_str())
                        : juce::String(info != nullptr ? info->name : slot.type.c_str());
                const bool missing = view.isEffectAvailable && !view.isEffectAvailable(slot.id);
                if (missing)
                    name << " (missing)";
                g.setColour(colour(slot.enabled && !missing ? t.colours.text : t.colours.textDim));
                g.drawText(name, row.withTrimmedLeft(row.getHeight()).withTrimmedRight(4.0f),
                           juce::Justification::centredLeft, true);
            }
            else
            {
                g.setColour(colour(t.colours.textDim));
                g.drawText(i == static_cast<int>(track.effects.size()) ? "+ Add effect" : "",
                           row.reduced(10.0f, 0.0f), juce::Justification::centredLeft);
            }
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto& project = view.doc.project();
        const int track = project.selectedMixerTrack;
        const auto& effects = project.mixer.tracks[static_cast<std::size_t>(track)].effects;
        for (int i = 0; i < model::MixerTrack::maxEffects; ++i)
        {
            if (!slotBounds(i).contains(e.getPosition()))
                continue;
            if (i >= static_cast<int>(effects.size()))
            {
                showAddMenu(track);
                return;
            }
            const auto id = effects[static_cast<std::size_t>(i)].id;
            const bool onLed = e.x < slotBounds(i).getX() + slotHeight;
            if (e.mods.isPopupMenu())
                showSlotMenu(id);
            else if (onLed)
                toggle(id);
            else if (view.onOpenEffect)
                view.onOpenEffect(id);
            return;
        }
    }

  private:
    juce::Rectangle<int> slotBounds(int i) const
    {
        return {8, 28 + i * slotHeight, getWidth() - 16, slotHeight - 4};
    }

    void showAddMenu(int track)
    {
        constexpr int manageId = 999;
        constexpr int firstPluginId = 1000;
        juce::PopupMenu menu;
        int itemId = 1;
        for (const auto& type : engine::effectTypes())
            menu.addItem(itemId++, type.name);

        // Hosted plugins (VST3 / LV2), found by scanning in Options > Plugins.
        // Grouped as manufacturer > category > plugin (the list arrives sorted that way).
        const auto plugins =
            view.listPlugins ? view.listPlugins() : std::vector<MixerView::PluginItem>{};
        const auto pluginMenu = makePluginMenu(plugins, firstPluginId);
        menu.addSeparator();
        menu.addSubMenu("Plugins", pluginMenu);
        menu.addItem(manageId, "Manage plugins...");

        menu.showMenuAsync(
            juce::PopupMenu::Options().withTargetComponent(this),
            [this, track, plugins](int result)
            {
                if (result <= 0)
                    return;
                if (result == manageId)
                {
                    if (view.onManagePlugins)
                        view.onManagePlugins();
                    return;
                }
                model::EffectId added = 0;
                if (result >= firstPluginId)
                {
                    const auto& item = plugins[static_cast<std::size_t>(result - firstPluginId)];
                    view.doc.apply([&added, track, &item](model::Project& p)
                                   { added = p.addPluginEffect(track, item.id, item.name); });
                }
                else
                {
                    const std::string type =
                        engine::effectTypes()[static_cast<std::size_t>(result - 1)].id;
                    view.doc.apply([&added, track, type](model::Project& p)
                                   { added = p.addEffect(track, type); });
                }
                if (added != 0 && view.onOpenEffect)
                    view.onOpenEffect(added);
            });
    }

    void toggle(model::EffectId id)
    {
        view.doc.apply(
            [id](model::Project& p)
            {
                if (auto* slot = p.findEffect(id))
                    slot->enabled = !slot->enabled;
            });
    }

    void move(model::EffectId id, int delta)
    {
        view.doc.apply(
            [id, delta](model::Project& p)
            {
                int track = 0;
                if (p.findEffect(id, &track) == nullptr)
                    return;
                auto& effects = p.mixer.tracks[static_cast<std::size_t>(track)].effects;
                const auto it = std::find_if(effects.begin(), effects.end(),
                                             [id](const auto& s) { return s.id == id; });
                const auto from = it - effects.begin();
                const auto to = from + delta;
                if (to >= 0 && to < static_cast<long>(effects.size()))
                    std::swap(effects[static_cast<std::size_t>(from)],
                              effects[static_cast<std::size_t>(to)]);
            });
    }

    void showSlotMenu(model::EffectId id)
    {
        const auto* slot = view.doc.project().findEffect(id);
        if (slot == nullptr)
            return;
        juce::PopupMenu menu;
        menu.addItem("Edit...",
                     [this, id]
                     {
                         if (view.onOpenEffect)
                             view.onOpenEffect(id);
                     });
        menu.addItem(slot->enabled ? "Bypass" : "Enable", [this, id] { toggle(id); });
        menu.addItem("Move up", [this, id] { move(id, -1); });
        menu.addItem("Move down", [this, id] { move(id, 1); });
        menu.addSeparator();
        menu.addItem("Remove", [this, id]
                     { view.doc.apply([id](model::Project& p) { p.removeEffect(id); }); });
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this));
    }

    MixerView& view;
};

// ------------------------------------------------------------------------------ MixerView

MixerView::MixerView(model::Document& document)
    : doc(document), slots(std::make_unique<Slots>(*this))
{

    for (int i = 0; i <= model::Mixer::numInserts; ++i)
    {
        strips.push_back(std::make_unique<MixerStrip>(*this, i));
        stripHolder.addAndMakeVisible(*strips.back());
    }
    viewport.setViewedComponent(&stripHolder, false);
    viewport.setScrollBarsShown(false, true);
    addAndMakeVisible(viewport);
    addAndMakeVisible(*slots);
    refresh();
}

MixerView::~MixerView() = default;

void MixerView::refresh()
{
    const auto& project = doc.project();
    for (std::size_t i = 0; i < strips.size() && i < project.mixer.tracks.size(); ++i)
        strips[i]->update(project.mixer.tracks[i],
                          static_cast<int>(i) == project.selectedMixerTrack);
    slots->repaint();
    repaint(0, 0, getWidth(), headerHeight); // the subtitle names the selected track
}

void MixerView::updateMeters(const std::function<float(int, int)>& peak)
{
    for (std::size_t i = 0; i < strips.size(); ++i)
        strips[i]->setLevels(peak(static_cast<int>(i), 0), peak(static_cast<int>(i), 1));
}

void MixerView::selectTrack(int track)
{
    if (doc.project().selectedMixerTrack != track)
        doc.setViewState([track](model::Project& p) { p.selectedMixerTrack = track; });
}

void MixerView::editTrack(int track, const std::function<void(model::MixerTrack&)>& change,
                          int gesture)
{
    doc.apply([track, &change](model::Project& p)
              { change(p.mixer.tracks[static_cast<std::size_t>(track)]); }, gesture);
}

void MixerView::paint(juce::Graphics& g)
{
    const auto& t = currentTheme();
    g.fillAll(colour(t.colours.panel));
    const int selected = doc.project().selectedMixerTrack;
    paintPanelHeader(g, getLocalBounds().removeFromTop(headerHeight), "Mixer",
                     selected == 0 ? juce::String("Master") : "Insert " + juce::String(selected));
    g.setColour(colour(t.colours.outline));
    g.fillRect(getWidth() - slotsWidth, headerHeight, 1, getHeight() - headerHeight);
}

void MixerView::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(headerHeight);
    slots->setBounds(area.removeFromRight(slotsWidth).withTrimmedTop(4));
    viewport.setBounds(area.reduced(4, 4));
    const int stripHeight = viewport.getHeight() - viewport.getScrollBarThickness();
    stripHolder.setSize(static_cast<int>(strips.size()) * MixerStrip::width, stripHeight);
    for (std::size_t i = 0; i < strips.size(); ++i)
        strips[i]->setBounds(static_cast<int>(i) * MixerStrip::width, 0, MixerStrip::width,
                             stripHeight);
}

// ---------------------------------------------------------------------------- EffectPanel

namespace
{
constexpr int panelHeaderHeight = metrics::panelHeaderHeight;
constexpr int controlWidth = 84;
constexpr int controlHeight = 96;
} // namespace

EffectPanel::EffectPanel(model::Document& document) : doc(document)
{
    setOpaque(true); // paints every pixel: a plain native window (app/area_stack)
    closeButton.setTooltip("Close");
    closeButton.onClick = [this] { setVisible(false); };
    enabled.setTooltip("Turn the effect on or off (bypass)");
    enabled.onClick = [this]
    {
        const auto id = effectId;
        doc.apply(
            [id](model::Project& p)
            {
                if (auto* slot = p.findEffect(id))
                    slot->enabled = !slot->enabled;
            });
    };
    for (auto* c : std::initializer_list<juce::Component*>{&closeButton, &enabled})
    {
        noFocus(*c);
        addAndMakeVisible(c);
    }
}

void EffectPanel::setMovableArea(juce::Rectangle<int> area)
{
    movableArea = area;
    keepTitleBarReachable(*this, movableArea);
}

void EffectPanel::load(model::EffectId id)
{
    effectId = id;
    builtFor.clear();
    refresh();
}

void EffectPanel::show(model::EffectId id)
{
    load(id);
    setVisible(true);
    toFront(false);
}

void EffectPanel::refresh()
{
    int track = 0;
    const auto* slot = doc.project().findEffect(effectId, &track);
    if (slot == nullptr)
    {
        setVisible(false);
        return;
    }
    if (slot->type != builtFor)
        rebuild();

    const auto* info = engine::findEffectType(slot->type);
    heading = juce::String(info != nullptr ? info->name : slot->type.c_str()) +
              juce::String::fromUTF8(" \xc2\xb7 ") +
              juce::String::fromUTF8(
                  doc.project().mixer.tracks[static_cast<std::size_t>(track)].name.c_str());
    enabled.setToggleState(slot->enabled, juce::dontSendNotification);

    for (std::size_t i = 0; i < controls.size() && i < slot->params.size(); ++i)
    {
        if (auto* slider = dynamic_cast<juce::Slider*>(controls[i].get()))
            slider->setValue(slot->params[i], juce::dontSendNotification);
        else if (auto* box = dynamic_cast<juce::ComboBox*>(controls[i].get()))
            box->setSelectedItemIndex(static_cast<int>(std::lround(slot->params[i])),
                                      juce::dontSendNotification);
    }
    repaint();
}

void EffectPanel::rebuild()
{
    controls.clear();
    labels.clear();
    const auto* slot = doc.project().findEffect(effectId);
    const auto* info = slot != nullptr ? engine::findEffectType(slot->type) : nullptr;
    if (info == nullptr)
        return;
    builtFor = slot->type;

    for (std::size_t i = 0; i < info->params.size(); ++i)
    {
        const auto& param = info->params[i];
        auto label = std::make_unique<juce::Label>(juce::String(), param.name);
        label->setJustificationType(juce::Justification::centred);
        label->setFont(juce::FontOptions(metrics::textBody));
        addAndMakeVisible(*label);
        labels.push_back(std::move(label));

        if (param.stepped && !param.stepNames.empty())
        {
            auto box = std::make_unique<juce::ComboBox>();
            for (std::size_t n = 0; n < param.stepNames.size(); ++n)
                box->addItem(param.stepNames[n], static_cast<int>(n) + 1);
            box->onChange = [this, i, b = box.get()]
            { setParam(i, static_cast<float>(b->getSelectedItemIndex()), 0); };
            noFocus(*box);
            addAndMakeVisible(*box);
            controls.push_back(std::move(box));
            continue;
        }

        auto slider = std::make_unique<juce::Slider>(juce::Slider::RotaryVerticalDrag,
                                                     juce::Slider::TextBoxBelow);
        slider->setRange(param.min, param.max, 0.0);
        if (param.logarithmic)
            slider->setSkewFactorFromMidPoint(
                std::sqrt(static_cast<double>(param.min) * param.max));
        slider->setDoubleClickReturnValue(true, param.defaultValue);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, controlWidth - 8, 18);
        slider->textFromValueFunction = [param](double v) { return formatValue(param, v); };
        slider->updateText(); // the value may not change below, so format the text now
        slider->onDragStart = [this] { gesture = doc.newGestureId(); };
        slider->onValueChange = [this, i, s = slider.get()]
        { setParam(i, static_cast<float>(s->getValue()), gesture); };
        noFocus(*slider);
        addAndMakeVisible(*slider);
        controls.push_back(std::move(slider));
    }

    const int columns = std::max(1, std::min(static_cast<int>(controls.size()), 5));
    const int rows = (static_cast<int>(controls.size()) + columns - 1) / columns;
    setSize(std::max(260, columns * controlWidth + 24),
            panelHeaderHeight + 12 + rows * controlHeight + 8);
    resized();
}

void EffectPanel::setParam(std::size_t index, float value, int gestureId)
{
    const auto id = effectId;
    doc.apply(
        [id, index, value](model::Project& p)
        {
            if (auto* slot = p.findEffect(id); slot != nullptr && index < slot->params.size())
                slot->params[index] = value;
        },
        gestureId);
}

void EffectPanel::paint(juce::Graphics& g)
{
    g.fillAll(colour(currentTheme().colours.background)); // corners (see paintFloatingPanel)
    const auto& t = currentTheme();
    const auto bounds = getLocalBounds().toFloat();
    const float radius = metrics::windowRadius;
    g.setColour(colour(t.colours.panel));
    g.fillRoundedRectangle(bounds, radius);

    paintPanelHeader(g, getLocalBounds().removeFromTop(panelHeaderHeight), heading, {},
                     enabled.getX() - metrics::controlGap, radius);
    g.setColour(colour(t.colours.outline));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, std::max(1.0f, metrics::borderWidth));
}

void EffectPanel::resized()
{
    HeaderLayout header(getLocalBounds().removeFromTop(panelHeaderHeight));
    closeButton.setBounds(header.fromRight(metrics::controlHeight));
    enabled.setBounds(header.fromRight(metrics::controlHeight));

    const int columns = std::max(1, std::min(static_cast<int>(controls.size()), 5));
    for (std::size_t i = 0; i < controls.size(); ++i)
    {
        const int col = static_cast<int>(i) % columns;
        const int row = static_cast<int>(i) / columns;
        auto cell = juce::Rectangle<int>(12 + col * controlWidth,
                                         panelHeaderHeight + 8 + row * controlHeight, controlWidth,
                                         controlHeight);
        labels[i]->setBounds(cell.removeFromTop(18));
        if (dynamic_cast<juce::ComboBox*>(controls[i].get()) != nullptr)
            controls[i]->setBounds(cell.withSizeKeepingCentre(controlWidth - 8, 26));
        else
            controls[i]->setBounds(cell.reduced(4, 0));
    }
}

void EffectPanel::mouseDown(const juce::MouseEvent& e)
{
    // Remembered: e.mouseDownPosition is relative to where the panel is now (see ChannelRack).
    movingPanel = e.y < panelHeaderHeight;
    if (movingPanel)
        dragger.start(*this);
}

void EffectPanel::mouseDrag(const juce::MouseEvent&)
{
    if (movingPanel)
    {
        dragger.drag(*this);
        keepTitleBarReachable(*this, movableArea); // so it can always be grabbed again
    }
}

} // namespace jls::ui
