#pragma once

// Our own title bar for the main window (instead of the system's, as in FL): the menus on the
// left, the window's name in the middle, minimize / maximize / close on the right. Drag the
// empty space to move the window; double-click it to maximize or restore. Not used on macOS,
// which keeps its native title bar and menu bar.
//
// TitleButton is the small flat window button, also used by the LookAndFeel for the title bars
// it draws on other windows (dialogs, separate plugin windows).

#include "ui/controls.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace jls::ui
{

// A flat window button: just the icon, a fill on hover (close: the accent colour).
class TitleButton final : public juce::Button
{
  public:
    TitleButton(Icon icon, bool isClose);
    void setIcon(Icon newIcon);
    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;

  private:
    Icon icon;
    bool close;
};

// Other windows (dialogs, separate plugin windows) get the title bar our LookAndFeel draws
// instead of the system's (not on macOS). Call on a window, or on a dialog right after launching.
void useOwnTitleBar(juce::DocumentWindow* window);

class TitleBar final : public juce::Component
{
  public:
    static constexpr int height = 32;

    explicit TitleBar(juce::MenuBarModel& menus);

    // Call when the window's name changes (it's drawn in the middle).
    void titleChanged() { repaint(); }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

  private:
    juce::DocumentWindow* window() const;
    void toggleMaximised();
    void updateMaximiseIcon();

    juce::MenuBarComponent menuBar;
    TitleButton minimise{Icon::windowMinimize, false};
    TitleButton maximise{Icon::windowMaximize, false};
    TitleButton close{Icon::close, true};
    juce::ComponentDragger dragger;
};

} // namespace jls::ui
