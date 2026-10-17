#include "ui/settings_dialog.h"

#include "ui/theme.h"

#include <juce_gui_extra/juce_gui_extra.h>

namespace jls::ui
{
namespace settings
{

double tempoStep(juce::PropertiesFile& file)
{
    return file.getDoubleValue(tempoStepKey, defaultTempoStep);
}

double tempoStepFine(juce::PropertiesFile& file)
{
    return file.getDoubleValue(tempoStepFineKey, defaultTempoStepFine);
}

} // namespace settings

namespace
{

void launch(std::unique_ptr<juce::Component> content, const juce::String& title, bool resizable)
{
    juce::DialogWindow::LaunchOptions options;
    // JUCE dialogs default to light grey; follow the theme instead.
    options.dialogBackgroundColour = juce::LookAndFeel::getDefaultLookAndFeel().findColour(
        juce::ResizableWindow::backgroundColourId);
    options.content.setOwned(content.release());
    options.dialogTitle = title;
    options.useNativeTitleBar = true;
    options.resizable = resizable;
    options.escapeKeyTriggersCloseButton = true;
    options.launchAsync();
}

// One row: a label and a number box with -/+ buttons, saved to the settings file on change.
class NumberSetting final : public juce::Component
{
  public:
    NumberSetting(juce::PropertiesFile& f, const char* k, const juce::String& text, double value)
        : file(f), key(k)
    {
        label.setText(text, juce::dontSendNotification);
        addAndMakeVisible(label);
        box.setSliderStyle(juce::Slider::IncDecButtons);
        box.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 70, 24);
        box.setRange(0.1, 100.0, 0.1);
        box.setTextValueSuffix(" BPM");
        box.setValue(value, juce::dontSendNotification);
        box.onValueChange = [this] { file.setValue(key, box.getValue()); };
        addAndMakeVisible(box);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        box.setBounds(area.removeFromRight(170).reduced(0, 2));
        label.setBounds(area);
    }

  private:
    juce::PropertiesFile& file;
    const char* key;
    juce::Label label;
    juce::Slider box;
};

class SettingsPanel final : public juce::Component
{
  public:
    explicit SettingsPanel(juce::PropertiesFile& file)
        : tempoStep(file, settings::tempoStepKey, "Tempo step", settings::tempoStep(file)),
          tempoStepFine(file, settings::tempoStepFineKey, "Fine tempo step",
                        settings::tempoStepFine(file))
    {
        heading.setText("Tempo", juce::dontSendNotification);
        heading.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        note.setText("How far the Tempo up / down and Tempo up / down (fine) shortcuts change the "
                     "tempo (see Options > Keyboard shortcuts).",
                     juce::dontSendNotification);
        note.setColour(juce::Label::textColourId, colour(currentTheme().colours.textDim));
        for (auto* c :
             std::initializer_list<juce::Component*>{&heading, &tempoStep, &tempoStepFine, &note})
            addAndMakeVisible(c);
        setSize(420, 150);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(16, 12);
        heading.setBounds(area.removeFromTop(24));
        tempoStep.setBounds(area.removeFromTop(32));
        tempoStepFine.setBounds(area.removeFromTop(32));
        note.setBounds(area);
    }

  private:
    juce::Label heading;
    NumberSetting tempoStep;
    NumberSetting tempoStepFine;
    juce::Label note;
};

} // namespace

void showSettingsDialog(juce::PropertiesFile& file)
{
    launch(std::make_unique<SettingsPanel>(file), "Settings", false);
}

void showShortcutsDialog(juce::KeyPressMappingSet& mappings)
{
    auto editor = std::make_unique<juce::KeyMappingEditorComponent>(mappings, true);
    const auto& t = currentTheme();
    editor->setColours(colour(t.colours.panel), colour(t.colours.text));
    editor->setSize(560, 600);
    launch(std::move(editor), "Keyboard shortcuts", true);
}

} // namespace jls::ui
