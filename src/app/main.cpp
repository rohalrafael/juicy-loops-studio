// Application entry point: creates the ThemeManager (which owns the LookAndFeel), then the main
// window. Shutdown closes dialogs first because they reference the ThemeManager.

#include "main_component.h"
#include "ui/theme.h"

#include <juce_gui_extra/juce_gui_extra.h>

namespace jls
{

class MainWindow final : public juce::DocumentWindow
{
  public:
    MainWindow(const juce::String& name, ui::ThemeManager& themes)
        : DocumentWindow(name, ui::colour(ui::currentTheme().colours.background), allButtons)
    {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainComponent(themes), true);
        setResizable(true, true);
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

    void lookAndFeelChanged() override
    {
        DocumentWindow::lookAndFeelChanged();
        setBackgroundColour(ui::colour(ui::currentTheme().colours.background));
    }

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

class Application final : public juce::JUCEApplication
{
  public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        themes = std::make_unique<ui::ThemeManager>();
        juce::LookAndFeel::setDefaultLookAndFeel(&themes->lookAndFeel());
        mainWindow = std::make_unique<MainWindow>(getApplicationName(), *themes);
    }

    void shutdown() override
    {
        // Close any open dialogs (e.g. Theme) first: they hold a reference to `themes`.
        auto& desktop = juce::Desktop::getInstance();
        for (int i = desktop.getNumComponents(); --i >= 0;)
            if (auto* dialog = dynamic_cast<juce::DialogWindow*>(desktop.getComponent(i)))
                delete dialog;

        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
        themes = nullptr;
    }

    void systemRequestedQuit() override
    {
        // Ask to save unsaved changes first.
        if (mainWindow != nullptr)
            if (auto* content = dynamic_cast<MainComponent*>(mainWindow->getContentComponent()))
            {
                content->requestQuit();
                return;
            }
        quit();
    }

  private:
    std::unique_ptr<ui::ThemeManager> themes; // owns the LookAndFeel: must outlive all windows
    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace jls

START_JUCE_APPLICATION(jls::Application)
