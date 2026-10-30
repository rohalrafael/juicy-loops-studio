// Application entry point: creates the ThemeManager (which owns the LookAndFeel), then the main
// window, and opens a project named on the command line. Shutdown closes dialogs first because
// they reference the ThemeManager.

#include "app/native_windows.h"
#include "main_component.h"
#include "model/project_file.h"
#include "ui/settings_dialog.h"
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
#if JUCE_MAC
        setUsingNativeTitleBar(true);
#else
        // Our own title bar (menus, name, window buttons) is part of the content, as in FL.
        setUsingNativeTitleBar(false);
        setTitleBarHeight(0);
#endif
        auto* content = new MainComponent(themes);
        setContentOwned(content, true);
        // Keys that reach the window itself (focus not on the content yet) still run shortcuts.
        content->forwardShortcuts(*this);
        setResizable(true, false); // without the native frame: JUCE's own resizable border
        centreWithSize(getWidth(), getHeight());
        setVisible(true);
        // Start with the keyboard, so shortcuts work before the first click.
        toFront(true);
        content->grabKeyboardFocus();
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

    // Not "here" where an area's native window covers the window: JUCE's look-ups across
    // windows (a drag's drop target) then go on to the area (app/area_stack).
    bool hitTest(int x, int y) override
    {
        if (auto* content = dynamic_cast<MainComponent*>(getContentComponent());
            content != nullptr && content->areaCovers(localPointToGlobal(juce::Point<int>(x, y))))
            return false;
        return DocumentWindow::hitTest(x, y);
    }

    // Where the window manager can resize the window by invisible edges (app/native_windows), the
    // content fills it: no border of JUCE's, the theme's background colour, around it.
    juce::BorderSize<int> getContentComponentBorder() const override
    {
        if (resizesByEdges)
            return {};
        return DocumentWindow::getContentComponentBorder();
    }

    void lookAndFeelChanged() override
    {
        DocumentWindow::lookAndFeelChanged();
        setBackgroundColour(ui::colour(ui::currentTheme().colours.background));
    }

  private:
#if JUCE_LINUX || JUCE_BSD
    const bool resizesByEdges = app::canResizeNativeWindowEdges();
#else
    const bool resizesByEdges = false;
#endif

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
        ui::settings::applyUiScale(themes->settingsFile());
        mainWindow = std::make_unique<MainWindow>(getApplicationName(), *themes);

        // `juicy-loops-studio song.jlp` opens the project (file managers pass it this way too).
        for (const auto& arg : getCommandLineParameterArray())
        {
            const auto file = juce::File::getCurrentWorkingDirectory().getChildFile(arg.unquoted());
            if (file.existsAsFile() && (file.hasFileExtension(model::projectFileExtension) ||
                                        file.hasFileExtension(model::legacyProjectFileExtension)))
            {
                if (auto* content = dynamic_cast<MainComponent*>(mainWindow->getContentComponent()))
                    content->openFile(file);
                break;
            }
        }
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
