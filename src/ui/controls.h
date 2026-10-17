#pragma once

// Shared building blocks for a consistent look: drawn icons, IconButton (icon, icon + label, or
// label; can toggle), segmented buttons, panel headers, the recessed display boxes of the
// transport bar and its tempo display. Sizes come from ui::metrics, colours from the active
// theme, so every panel lines up and restyles with the theme.

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <initializer_list>

namespace jls::ui
{

// Icons drawn as vector paths (no image files), in the control's text colour.
enum class Icon
{
    none,
    add,
    minus,
    close,
    dockSide,  // the Browser: dock on the other side
    addFolder, // the Browser: add a folder
    play,
    pause,
    stop,
    loop,
    metronome,
    keys, // typing keyboard
    slice,
    list, // the Playlist's picker
    chevronDown,
};

// Draws `icon` centred in `area`, scaled to fit, stroked (or filled) in `colour`.
void drawIcon(juce::Graphics& g, Icon icon, juce::Rectangle<float> area, juce::Colour colour);

// Label font for a control of the given height: 12 px in panels, 13 px in the transport bar.
juce::FontOptions controlFont(int controlHeight);
// Font for numbers in displays (tempo, clock): the bundled IBM Plex Mono, whose digits all
// have the same width so values don't jitter while they change.
juce::FontOptions displayFont(float height);
// Width of `text` in `font`, rounded up.
int textWidth(const juce::FontOptions& font, const juce::String& text);

// A button with an icon, a label or both, drawn like the theme's other buttons. Toggle buttons
// (setClickingTogglesState) show the active colours while on.
class IconButton : public juce::Button
{
  public:
    explicit IconButton(Icon icon, const juce::String& label = {});

    void setIcon(Icon newIcon);
    Icon getIcon() const { return icon; }
    // The width that fits the icon and label at `height` (square for an icon alone).
    int preferredWidth(int height) const;
    // Flat (no border, a quieter fill): the -/+ buttons inside the tempo display.
    void setFlat(bool shouldBeFlat);

    void paintButton(juce::Graphics& g, bool highlighted, bool down) override;

  private:
    Icon icon;
    bool flat = false;
};

// Joins buttons left to right into one segmented control (PAT | SONG, 16 | 32 | 64): shared
// borders, rounded only at the outer ends.
void joinSegments(std::initializer_list<juce::Button*> buttons);

// Width for a TextButton segment or button with `text` at `height`.
int buttonWidth(const juce::String& text, int height);

// The header every panel has: header fill, a line below, the title and a dimmer subtitle (e.g.
// the pattern), cut off before `textRight`. `topRadius` rounds the top corners (floating
// panels).
void paintPanelHeader(juce::Graphics& g, juce::Rectangle<int> header, const juce::String& title,
                      const juce::String& subtitle = {}, int textRight = -1,
                      float topRadius = 0.0f);

// A dim label in a header ("Snap", "Scale"), right-aligned against the control after it.
void paintHeaderLabel(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area);
int headerLabelWidth(const juce::String& text);

// Places a header's controls: each is control-high and vertically centred, taken from the right
// (or left) with the standard gap in between.
class HeaderLayout
{
  public:
    explicit HeaderLayout(juce::Rectangle<int> header, int controlHeight = -1);
    juce::Rectangle<int> fromRight(int width);
    juce::Rectangle<int> fromLeft(int width);
    void space(int pixels) { area.removeFromRight(pixels); }
    // What's left in the middle: where the title and subtitle go.
    juce::Rectangle<int> remaining() const { return area; }

  private:
    juce::Rectangle<int> area;
    int height;
};

// A recessed box for numbers (the tempo and the clock).
void paintDisplayBox(juce::Graphics& g, juce::Rectangle<float> area);

// The transport's tempo: [−] 130.000 BPM [+] in a display box. The buttons step by `step`
// BPM; click the number to type a tempo (kept to three decimals); the mouse wheel steps too.
class TempoDisplay final : public juce::Component, public juce::SettableTooltipClient
{
  public:
    TempoDisplay();

    void setRange(double minimum, double maximum);
    void setValue(double bpm); // doesn't call onChange
    double getValue() const { return value; }
    void setTooltip(const juce::String& tooltip) override; // shown on all its parts
    int preferredWidth(int height) const;

    std::function<void(double bpm)> onChange;
    double step = 1.0;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    void lookAndFeelChanged() override;

  private:
    void change(double bpm);
    void updateLabel();

    double value = 120.0;
    double minimum = 10.0;
    double maximum = 999.0;
    IconButton down{Icon::minus};
    IconButton up{Icon::add};
    juce::Label number;
};

} // namespace jls::ui
