#pragma once

// Options > Theme: a grid of theme cards (built-in and imported) plus import / export / open
// folder buttons. Selecting a card applies the theme to the whole app immediately.

#include "ui/theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <vector>

namespace jls::ui
{

// One theme in the picker: a preview in the theme's own colours plus its name.
class ThemeCard final : public juce::Button
{
  public:
    explicit ThemeCard(theme::Theme theme);

    const theme::Theme& theme() const { return cardTheme; }

    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;

  private:
    theme::Theme cardTheme;
};

// Options > Theme: pick a theme, import/export theme files, open the themes folder.
class ThemeDialog final : public juce::Component, private juce::ChangeListener
{
  public:
    explicit ThemeDialog(ThemeManager& manager);
    ~ThemeDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Opens the dialog in its own (modal) window; theme changes show live behind it.
    static void show(ThemeManager& manager);

  private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void rebuildCards();
    void importTheme();
    void exportTheme();

    ThemeManager& themes;
    std::vector<std::unique_ptr<ThemeCard>> cards;
    juce::TextButton importButton{"Import theme file..."};
    juce::TextButton exportButton{"Export current theme..."};
    juce::TextButton folderButton{"Open themes folder"};
    std::unique_ptr<juce::FileChooser> chooser;
};

} // namespace jls::ui
