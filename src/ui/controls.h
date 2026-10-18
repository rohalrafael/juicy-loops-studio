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

// Label font for a control of the given height (Medium weight): 12 px in panels, 13 px in the
// transport bar.
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

// Docked panels are cards: rounded corners (masked in the window background) and a thin border,
// in the accent colour while the panel has the keyboard focus (its "focused" property, set by
// MainComponent). Call from the panel's paintOverChildren() so it's drawn over its children.
void paintCardFrame(juce::Graphics& g, juce::Component& panel);

// A recessed box for numbers (the tempo and the clock).
void paintDisplayBox(juce::Graphics& g, juce::Rectangle<float> area);

// A number in a recessed display box: [−] 130.000 BPM [+] (the transport's tempo, the
// Sampler's tempo and pitch, settings). The buttons and the mouse wheel step by `step`; click the
// number to type one. The font scales with the height.
class NumberField final : public juce::Component, public juce::SettableTooltipClient
{
  public:
    // `widest` is the longest text the number can show (sizes the field), e.g. "000.000".
    NumberField(const juce::String& unit = {}, int decimals = 0,
                const juce::String& widest = "000");

    void setRange(double minimum, double maximum);
    void setValue(double newValue); // doesn't call onChange
    double getValue() const { return value; }
    void setTooltip(const juce::String& tooltip) override; // shown on all its parts
    int preferredWidth(int height) const;

    std::function<void(double)> onChange;
    double step = 1.0;
    bool showPlus = false; // "+3" for positive values (semitones)

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    void lookAndFeelChanged() override;

  private:
    void change(double newValue);
    void updateLabel();
    float numberHeight() const;

    juce::String unit;
    int decimals;
    juce::String widest;
    double value = 0.0;
    double minimum = 0.0;
    double maximum = 100.0;
    IconButton down{Icon::minus};
    IconButton up{Icon::add};
    juce::Label number;
};

// Section headings ("TEMPO"), dim notes and dialog footers, as on the panels.
void paintSectionHeading(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area);
void paintNote(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area);
int noteHeight(const juce::String& text, int width);
void paintDialogFooter(juce::Graphics& g, juce::Rectangle<int> area);
inline constexpr int dialogFooterHeight = 44;

// A JUCE-made component (the plugin list, the audio device selector) in a themed dialog body:
// the panel background, a section heading and a note above it, and the usual margins.
class DialogFrame final : public juce::Component
{
  public:
    DialogFrame(std::unique_ptr<juce::Component> content, const juce::String& heading,
                const juce::String& note, int contentWidth, int contentHeight);

    void paint(juce::Graphics& g) override;
    void resized() override;

  private:
    int topHeight() const;

    std::unique_ptr<juce::Component> content;
    juce::String heading, note;
};

// A settings form: sections with a heading, rows of a label and controls, notes and painted
// areas, laid out top to bottom. It paints its own text, so theme changes restyle it.
class FormPanel : public juce::Component
{
  public:
    static constexpr int margin = 16;
    static constexpr int labelWidth = 140;

    void addSection(const juce::String& heading);
    // Controls with their widths, left to right after the label.
    void addRow(const juce::String& label,
                std::initializer_list<std::pair<juce::Component*, int>> controls);
    // Returns an index for setNote().
    int addNote(const juce::String& text);
    void setNote(int index, const juce::String& text);
    void addPainted(int height, std::function<void(juce::Graphics&, juce::Rectangle<int>)> painter);
    // The height the items need at `width` (plus `extra` below, e.g. a footer).
    int preferredHeight(int width) const;

    void paint(juce::Graphics& g) override;
    void resized() override;

  protected:
    // Below the items (for a footer); laid out by subclasses.
    juce::Rectangle<int> itemsArea() const;
    int footerHeight = 0;

  private:
    struct Item
    {
        enum class Kind
        {
            section,
            row,
            note,
            painted
        } kind;
        juce::String text;
        std::vector<std::pair<juce::Component*, int>> controls;
        int height = 0;
        std::function<void(juce::Graphics&, juce::Rectangle<int>)> painter;
        juce::Rectangle<int> bounds;
    };
    int heightOf(const Item& item, int width) const;
    std::vector<Item> items;
};

} // namespace jls::ui
