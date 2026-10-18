#include "ui/settings_dialog.h"

#include "ui/controls.h"

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

bool pluginsInsideApp(juce::PropertiesFile& file)
{
    return file.getValue(pluginWindowsKey, "inside") != "separate";
}

double uiScale(juce::PropertiesFile& file)
{
    return std::clamp(file.getDoubleValue(uiScaleKey, 1.0), 0.75, 2.0);
}

void applyUiScale(juce::PropertiesFile& file)
{
    juce::Desktop::getInstance().setGlobalScaleFactor(static_cast<float>(uiScale(file)));
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

// Options > Settings, as a form: each control saves to the settings file when it changes.
class SettingsPanel final : public FormPanel
{
  public:
    explicit SettingsPanel(juce::PropertiesFile& f) : file(f)
    {
        number(tempoStep, settings::tempoStepKey, settings::tempoStep(file));
        number(tempoStepFine, settings::tempoStepFineKey, settings::tempoStepFine(file));
        number(previewLength, settings::previewSecondsKey, settings::previewSeconds(file));

        const juce::StringArray values{"fixed", "recent"};
        focusOrder.addItemList({"Fixed", "Last used first"}, 1);
        focusOrder.setSelectedItemIndex(
            std::max(0, values.indexOf(file.getValue(settings::focusOrderKey, values[0]))),
            juce::dontSendNotification);
        focusOrder.onChange = [this, values]
        { file.setValue(settings::focusOrderKey, values[focusOrder.getSelectedItemIndex()]); };

        // Interface size: ids are percentages.
        for (int percent : {100, 110, 125, 150})
            interfaceSize.addItem(juce::String(percent) + " %", percent);
        interfaceSize.setSelectedId(juce::roundToInt(settings::uiScale(file) * 100.0),
                                    juce::dontSendNotification);
        interfaceSize.onChange = [this]
        {
            file.setValue(settings::uiScaleKey, interfaceSize.getSelectedId() / 100.0);
            settings::applyUiScale(file);
        };

        pluginWindows.addItemList({"Inside the app", "Separate windows"}, 1);
        pluginWindows.setSelectedItemIndex(settings::pluginsInsideApp(file) ? 0 : 1,
                                           juce::dontSendNotification);
        pluginWindows.onChange = [this]
        {
            file.setValue(settings::pluginWindowsKey,
                          pluginWindows.getSelectedItemIndex() == 0 ? "inside" : "separate");
        };

        const int field = tempoStep.preferredWidth(metrics::controlHeight);
        addSection("Tempo");
        addRow("Tempo step", {{&tempoStep, field}});
        addRow("Fine tempo step", {{&tempoStepFine, field}});
        addNote("How far Tempo up / down and Tempo up / down (fine) change the tempo (see "
                "Options > Keyboard shortcuts).");
        addSection("Browser");
        addRow("Click preview length", {{&previewLength, field}});
        addNote("Clicking a sample plays this much of it; Shift+click plays it all.");
        addSection("Focus");
        addRow("Ctrl+Tab order", {{&focusOrder, 160}});
        addNote("Last used first works like Alt+Tab: press once to go back to the previous area, "
                "keep the modifier held to go further back.");
        addSection("Display");
        addRow("Interface size", {{&interfaceSize, 160}});
        addNote("Scales the whole interface, text included: larger is easier to read on "
                "high-resolution screens.");
        addSection("Plugins");
        addRow("Plugin windows", {{&pluginWindows, 160}});
        addNote("Inside the app shows plugin editors as panels in the main window. If a plugin "
                "misbehaves there, choose separate windows. Applies to editors opened from now "
                "on.");
        setSize(440, preferredHeight(440));
    }

  private:
    void number(NumberField& field, const char* key, double value)
    {
        field.setRange(0.1, 100.0);
        field.step = 0.1;
        field.setValue(value);
        field.onChange = [this, key](double v) { file.setValue(key, v); };
    }

    juce::PropertiesFile& file;
    NumberField tempoStep{"BPM", 1, "00.0"};
    NumberField tempoStepFine{"BPM", 1, "00.0"};
    NumberField previewLength{"s", 1, "00.0"};
    juce::ComboBox focusOrder;
    juce::ComboBox pluginWindows;
    juce::ComboBox interfaceSize;
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

    void paint(juce::Graphics& g) override
    {
        g.fillAll(colour(currentTheme().colours.panel));
        paintDialogFooter(g, getLocalBounds().removeFromBottom(dialogFooterHeight));
    }

    void resized() override
    {
        auto area = getLocalBounds();
        HeaderLayout footer(area.removeFromBottom(dialogFooterHeight));
        importButton.setBounds(footer.fromLeft(buttonWidth("Import...", metrics::controlHeight)));
        exportButton.setBounds(footer.fromLeft(buttonWidth("Export...", metrics::controlHeight)));
        editor.setBounds(area.reduced(6, 4));
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
