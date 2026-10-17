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

bool focusOrderRecent(juce::PropertiesFile& file)
{
    return file.getValue(focusOrderKey, "fixed") == "recent";
}

double previewSeconds(juce::PropertiesFile& file)
{
    return file.getDoubleValue(previewSecondsKey, defaultPreviewSeconds);
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
    NumberSetting(juce::PropertiesFile& f, const char* k, const juce::String& text, double value,
                  const juce::String& suffix = " BPM")
        : file(f), key(k)
    {
        label.setText(text, juce::dontSendNotification);
        addAndMakeVisible(label);
        box.setSliderStyle(juce::Slider::IncDecButtons);
        box.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 70, 24);
        box.setRange(0.1, 100.0, 0.1);
        box.setTextValueSuffix(suffix);
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

// One row: a label and a choice, saved to the settings file as one of `values`.
class ChoiceSetting final : public juce::Component
{
  public:
    ChoiceSetting(juce::PropertiesFile& f, const char* k, const juce::String& text,
                  const juce::StringArray& labels, const juce::StringArray& v)
        : file(f), key(k), values(v)
    {
        label.setText(text, juce::dontSendNotification);
        addAndMakeVisible(label);
        box.addItemList(labels, 1);
        box.setSelectedItemIndex(std::max(0, values.indexOf(file.getValue(key, values[0]))),
                                 juce::dontSendNotification);
        box.onChange = [this] { file.setValue(key, values[box.getSelectedItemIndex()]); };
        addAndMakeVisible(box);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        box.setBounds(area.removeFromRight(170).reduced(0, 3));
        label.setBounds(area);
    }

  private:
    juce::PropertiesFile& file;
    const char* key;
    juce::StringArray values;
    juce::Label label;
    juce::ComboBox box;
};

class SettingsPanel final : public juce::Component
{
  public:
    explicit SettingsPanel(juce::PropertiesFile& file)
        : tempoStep(file, settings::tempoStepKey, "Tempo step", settings::tempoStep(file)),
          tempoStepFine(file, settings::tempoStepFineKey, "Fine tempo step",
                        settings::tempoStepFine(file)),
          previewLength(file, settings::previewSecondsKey, "Click preview length",
                        settings::previewSeconds(file), " s"),
          focusOrder(file, settings::focusOrderKey, "Ctrl+Tab order", {"Fixed", "Last used first"},
                     {"fixed", "recent"})
    {
        focusHeading.setText("Focus", juce::dontSendNotification);
        focusHeading.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        focusNote.setText("Last used first works like Alt+Tab: press once to go back to the "
                          "previous area, keep the modifier held to go further back.",
                          juce::dontSendNotification);
        focusNote.setColour(juce::Label::textColourId, colour(currentTheme().colours.textDim));
        browserHeading.setText("Browser", juce::dontSendNotification);
        browserHeading.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        browserNote.setText("Clicking a sample plays this much of it; Shift+click plays it all.",
                            juce::dontSendNotification);
        browserNote.setColour(juce::Label::textColourId, colour(currentTheme().colours.textDim));
        heading.setText("Tempo", juce::dontSendNotification);
        heading.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        note.setText("How far the Tempo up / down and Tempo up / down (fine) shortcuts change the "
                     "tempo (see Options > Keyboard shortcuts).",
                     juce::dontSendNotification);
        note.setColour(juce::Label::textColourId, colour(currentTheme().colours.textDim));
        for (auto* c : std::initializer_list<juce::Component*>{
                 &heading, &tempoStep, &tempoStepFine, &note, &browserHeading, &previewLength,
                 &browserNote, &focusHeading, &focusOrder, &focusNote})
            addAndMakeVisible(c);
        setSize(420, 380);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(16, 12);
        heading.setBounds(area.removeFromTop(24));
        tempoStep.setBounds(area.removeFromTop(32));
        tempoStepFine.setBounds(area.removeFromTop(32));
        note.setBounds(area.removeFromTop(40));
        area.removeFromTop(8);
        browserHeading.setBounds(area.removeFromTop(24));
        previewLength.setBounds(area.removeFromTop(32));
        browserNote.setBounds(area.removeFromTop(36));
        area.removeFromTop(8);
        focusHeading.setBounds(area.removeFromTop(24));
        focusOrder.setBounds(area.removeFromTop(32));
        focusNote.setBounds(area);
    }

  private:
    juce::Label heading;
    NumberSetting tempoStep;
    NumberSetting tempoStepFine;
    juce::Label note;
    juce::Label browserHeading;
    NumberSetting previewLength;
    juce::Label browserNote;
    juce::Label focusHeading;
    ChoiceSetting focusOrder;
    juce::Label focusNote;
};

// JUCE's key-mapping editor plus Import / Export of whole shortcut sets (an XML file).
class ShortcutsPanel final : public juce::Component
{
  public:
    explicit ShortcutsPanel(juce::KeyPressMappingSet& m) : mappings(m), editor(m, true)
    {
        const auto& t = currentTheme();
        editor.setColours(colour(t.colours.panel), colour(t.colours.text));
        addAndMakeVisible(editor);

        importButton.setTooltip("Load a shortcut set from a file (replaces the current keys)");
        exportButton.setTooltip("Save all current shortcuts to a file");
        importButton.onClick = [this] { importSet(); };
        exportButton.onClick = [this] { exportSet(); };
        addAndMakeVisible(importButton);
        addAndMakeVisible(exportButton);
        setSize(560, 640);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto bar = area.removeFromBottom(40).reduced(10, 6);
        importButton.setBounds(bar.removeFromLeft(90));
        bar.removeFromLeft(6);
        exportButton.setBounds(bar.removeFromLeft(90));
        editor.setBounds(area);
    }

  private:
    static juce::File startFolder()
    {
        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    }

    void exportSet()
    {
        chooser = std::make_unique<juce::FileChooser>(
            "Export shortcuts", startFolder().getChildFile("juicy-loops-shortcuts.xml"), "*.xml");
        chooser->launchAsync(
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                if (file == juce::File())
                    return;
                // All keys, not just changes, so the file means the same on any version.
                if (auto xml = mappings.createXml(false);
                    xml == nullptr || !xml->writeTo(file.withFileExtension(".xml")))
                    message("Export failed", "Couldn't write " + file.getFullPathName() + ".");
            });
    }

    void importSet()
    {
        chooser = std::make_unique<juce::FileChooser>("Import shortcuts", startFolder(), "*.xml");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                if (file == juce::File())
                    return;
                const auto xml = juce::parseXML(file);
                if (xml == nullptr || !xml->hasTagName("KEYMAPPINGS") ||
                    !mappings.restoreFromXml(*xml))
                    message("Import failed",
                            file.getFileName() + " isn't a Juicy Loops Studio shortcut file.");
                // On success the mapping set announces the change: the editor refreshes and
                // the app saves it as the user's shortcuts.
            });
    }

    static void message(const juce::String& title, const juce::String& text)
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions::makeOptionsOk(juce::MessageBoxIconType::NoIcon, title, text),
            nullptr);
    }

    juce::KeyPressMappingSet& mappings;
    juce::KeyMappingEditorComponent editor;
    juce::TextButton importButton{"Import..."};
    juce::TextButton exportButton{"Export..."};
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace

void showSettingsDialog(juce::PropertiesFile& file)
{
    launch(std::make_unique<SettingsPanel>(file), "Settings", false);
}

void showShortcutsDialog(juce::KeyPressMappingSet& mappings)
{
    launch(std::make_unique<ShortcutsPanel>(mappings), "Keyboard shortcuts", true);
}

} // namespace jls::ui
