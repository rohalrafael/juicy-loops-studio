#include "ui/theme.h"

#include "BinaryData.h"

namespace jls::ui
{
namespace
{

const theme::Theme* activeTheme = nullptr;

constexpr const char* themeSettingKey = "theme";
constexpr const char* customPrefix = "custom:";

} // namespace

const theme::Theme& currentTheme()
{
    return activeTheme != nullptr ? *activeTheme : theme::defaultTheme();
}

// -------------------------------------------------------------------------------- LookAndFeel

void LookAndFeel::applyTheme(const theme::Theme& t, juce::Typeface::Ptr bundledFont)
{
    const auto& c = t.colours;
    setColour(juce::ResizableWindow::backgroundColourId, colour(c.background));
    setColour(juce::DocumentWindow::backgroundColourId, colour(c.background));
    setColour(juce::TextButton::buttonColourId, colour(c.panelRaised));
    setColour(juce::TextButton::buttonOnColourId, colour(c.activeBackground));
    setColour(juce::TextButton::textColourOffId, colour(c.text));
    setColour(juce::TextButton::textColourOnId, colour(c.activeText));
    setColour(juce::ComboBox::backgroundColourId, colour(c.panelRaised));
    setColour(juce::ComboBox::outlineColourId, colour(c.outline));
    setColour(juce::ComboBox::textColourId, colour(c.text));
    setColour(juce::ComboBox::arrowColourId, colour(c.textDim));
    setColour(juce::Label::textColourId, colour(c.text));
    setColour(juce::Slider::thumbColourId, colour(c.accent));
    setColour(juce::Slider::trackColourId, colour(c.display));
    setColour(juce::Slider::backgroundColourId, colour(c.display));
    setColour(juce::Slider::textBoxTextColourId, colour(c.displayText));
    setColour(juce::Slider::textBoxBackgroundColourId, colour(c.display));
    setColour(juce::Slider::textBoxOutlineColourId, colour(c.outline));
    setColour(juce::TextEditor::backgroundColourId, colour(c.display));
    setColour(juce::TextEditor::textColourId, colour(c.text));
    setColour(juce::TextEditor::outlineColourId, colour(c.outline));
    setColour(juce::TextEditor::focusedOutlineColourId, colour(c.accent));
    setColour(juce::PopupMenu::backgroundColourId, colour(c.panel));
    setColour(juce::PopupMenu::textColourId, colour(c.text));
    setColour(juce::PopupMenu::headerTextColourId, colour(c.textDim));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colour(c.activeBackground));
    setColour(juce::PopupMenu::highlightedTextColourId, colour(c.activeText));
    setColour(juce::AlertWindow::backgroundColourId, colour(c.panel));
    setColour(juce::AlertWindow::textColourId, colour(c.text));
    setColour(juce::AlertWindow::outlineColourId, colour(c.outline));
    setColour(juce::TooltipWindow::backgroundColourId, colour(c.panelRaised));
    setColour(juce::TooltipWindow::textColourId, colour(c.text));
    setColour(juce::TooltipWindow::outlineColourId, colour(c.outline));
    setColour(juce::ScrollBar::thumbColourId, colour(c.panelRaised));
    setColour(juce::ListBox::backgroundColourId, colour(c.panel));
    setColour(juce::ListBox::outlineColourId, colour(c.outline));
    setColour(juce::TableHeaderComponent::backgroundColourId, colour(c.header));
    setColour(juce::TableHeaderComponent::textColourId, colour(c.text));
    setColour(juce::TableHeaderComponent::outlineColourId, colour(c.outline));
    setColour(juce::TableHeaderComponent::highlightColourId, colour(c.activeBackground));
    setColour(juce::ListBox::textColourId, colour(c.text));
    setColour(juce::GroupComponent::outlineColourId, colour(c.outline));
    setColour(juce::GroupComponent::textColourId, colour(c.text));
    setColour(juce::ToggleButton::textColourId, colour(c.text));
    setColour(juce::ToggleButton::tickColourId, colour(c.accent));
    setColour(juce::ToggleButton::tickDisabledColourId, colour(c.textDim));

    // Built-in widgets (combo boxes, file choosers, ...) use the V4 colour scheme.
    getCurrentColourScheme().setUIColour(ColourScheme::windowBackground, colour(c.background));
    getCurrentColourScheme().setUIColour(ColourScheme::widgetBackground, colour(c.panelRaised));
    getCurrentColourScheme().setUIColour(ColourScheme::menuBackground, colour(c.panel));
    getCurrentColourScheme().setUIColour(ColourScheme::outline, colour(c.outline));
    getCurrentColourScheme().setUIColour(ColourScheme::defaultText, colour(c.text));
    getCurrentColourScheme().setUIColour(ColourScheme::defaultFill, colour(c.accent));
    getCurrentColourScheme().setUIColour(ColourScheme::highlightedText, colour(c.activeText));
    getCurrentColourScheme().setUIColour(ColourScheme::highlightedFill, colour(c.activeBackground));
    getCurrentColourScheme().setUIColour(ColourScheme::menuText, colour(c.text));

    if (bundledFont != nullptr)
    {
        setDefaultSansSerifTypeface(bundledFont);
    }
    else
    {
        // Not bundled: ask the system for a font with that name (falls back if missing).
        setDefaultSansSerifTypeface(nullptr);
        setDefaultSansSerifTypefaceName(juce::String::fromUTF8(t.font.c_str()));
    }
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                       const juce::Colour& background, bool highlighted, bool down)
{
    const auto& shape = currentTheme().shape;
    const auto bounds = button.getLocalBounds().toFloat().reduced(shape.borderWidth / 2.0f);

    auto fill = background;
    if (down)
        fill = fill.contrasting(0.15f);
    else if (highlighted)
        fill = fill.contrasting(0.07f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, shape.cornerRadius);
    if (shape.borderWidth > 0.0f)
    {
        g.setColour(colour(currentTheme().colours.outline));
        g.drawRoundedRectangle(bounds, shape.cornerRadius, shape.borderWidth);
    }
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                   float position, float startAngle, float endAngle,
                                   juce::Slider& slider)
{
    const auto& c = currentTheme().colours;
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2.0f);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float lineWidth = std::max(2.0f, radius * 0.22f);
    const float arcRadius = radius - lineWidth / 2.0f;
    const juce::PathStrokeType stroke(lineWidth, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded);

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour(colour(c.display));
    g.strokePath(track, stroke);

    // Bipolar knobs (pan) fill from the centre, others from the start.
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float fromAngle = bipolar ? (startAngle + endAngle) / 2.0f : startAngle;
    const float toAngle = startAngle + position * (endAngle - startAngle);

    juce::Path value;
    value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, fromAngle, toAngle, true);
    g.setColour(slider.isEnabled() ? colour(c.accent) : colour(c.textDim));
    g.strokePath(value, stroke);

    const juce::Point<float> tip(centre.x + arcRadius * 0.6f * std::sin(toAngle),
                                 centre.y - arcRadius * 0.6f * std::cos(toAngle));
    g.setColour(colour(c.text));
    g.drawLine({centre, tip}, lineWidth * 0.7f);
}

// ------------------------------------------------------------------------------- ThemeManager

ThemeManager::ThemeManager() : active(theme::defaultTheme())
{
    auto addFont = [this](const char* data, int size)
    {
        if (auto typeface =
                juce::Typeface::createSystemTypefaceFor(data, static_cast<size_t>(size)))
            fonts[typeface->getName().toLowerCase()] = typeface;
    };
    addFont(BinaryData::IBMPlexSans_ttf, BinaryData::IBMPlexSans_ttfSize);
    addFont(BinaryData::BricolageGrotesque_ttf, BinaryData::BricolageGrotesque_ttfSize);
    addFont(BinaryData::Sora_ttf, BinaryData::Sora_ttfSize);
    addFont(BinaryData::Outfit_ttf, BinaryData::Outfit_ttfSize);

    juce::PropertiesFile::Options options;
    options.applicationName = "JuicyLoopsStudio";
    options.folderName = "JuicyLoopsStudio";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
#if JUCE_LINUX || JUCE_BSD
    // JUCE's default on Linux is a folder directly in $HOME; follow the XDG convention instead.
    const auto xdg = juce::SystemStats::getEnvironmentVariable("XDG_CONFIG_HOME", {});
    const auto configDir = xdg.isNotEmpty() ? juce::File(xdg) : juce::File("~/.config");
    settings = std::make_unique<juce::PropertiesFile>(
        configDir.getChildFile("JuicyLoopsStudio/JuicyLoopsStudio.settings"), options);
#else
    settings = std::make_unique<juce::PropertiesFile>(options);
#endif

    activeTheme = &active;
    rescan();
    select(settings->getValue(themeSettingKey, "juicy").toStdString());
}

ThemeManager::~ThemeManager()
{
    if (activeTheme == &active)
        activeTheme = nullptr;
}

std::vector<theme::Theme> ThemeManager::available() const
{
    auto all = theme::builtInThemes();
    all.insert(all.end(), userThemes.begin(), userThemes.end());
    return all;
}

juce::File ThemeManager::themesFolder() const
{
    return settings->getFile().getParentDirectory().getChildFile("themes");
}

void ThemeManager::rescan()
{
    userThemes.clear();
    auto files = themesFolder().findChildFiles(juce::File::findFiles, false, "*.json");
    files.sort();
    for (const auto& file : files)
    {
        auto result = theme::parseTheme(file.loadFileAsString().toStdString());
        if (!result.theme)
            continue; // unusable files are reported when imported, skipped here
        result.theme->id = customPrefix + file.getFileName().toStdString();
        userThemes.push_back(std::move(*result.theme));
    }
}

void ThemeManager::select(const std::string& id)
{
    const auto all = available();
    const auto it = std::find_if(all.begin(), all.end(), [&](const auto& t) { return t.id == id; });
    active = it != all.end() ? *it : theme::defaultTheme();

    settings->setValue(themeSettingKey, juce::String(active.id));
    settings->saveIfNeeded();
    apply();
}

juce::Typeface::Ptr ThemeManager::bundledFont(const std::string& family) const
{
    const auto it = fonts.find(juce::String::fromUTF8(family.c_str()).toLowerCase());
    return it != fonts.end() ? it->second : nullptr;
}

void ThemeManager::apply()
{
    laf.applyTheme(active, bundledFont(active.font));

    // Repaint everything that's on screen with the new look.
    auto& desktop = juce::Desktop::getInstance();
    for (int i = 0; i < desktop.getNumComponents(); ++i)
        if (auto* window = desktop.getComponent(i))
            window->sendLookAndFeelChange();

    sendChangeMessage();
}

ThemeManager::ImportResult ThemeManager::importFile(const juce::File& file)
{
    const auto parsed = theme::parseTheme(file.loadFileAsString().toStdString());
    if (!parsed.theme)
        return {false, "Couldn't import " + file.getFileName() + ": " +
                           juce::String::fromUTF8(parsed.error.c_str())};

    const auto folder = themesFolder();
    if (!folder.createDirectory())
        return {false, "Couldn't create the themes folder: " + folder.getFullPathName()};

    auto destination = file;
    if (file.getParentDirectory() != folder)
    {
        destination =
            folder.getNonexistentChildFile(file.getFileNameWithoutExtension(), ".json", false);
        if (!file.copyFileTo(destination))
            return {false, "Couldn't copy the theme into " + folder.getFullPathName()};
    }

    rescan();
    select(customPrefix + destination.getFileName().toStdString());

    juce::String message;
    if (!parsed.warnings.empty())
    {
        message << "Imported \"" << juce::String::fromUTF8(parsed.theme->name.c_str())
                << "\", but some settings were skipped:\n";
        for (std::size_t i = 0; i < parsed.warnings.size() && i < 8; ++i)
            message << "\n- " << juce::String::fromUTF8(parsed.warnings[i].c_str());
        if (parsed.warnings.size() > 8)
            message << "\n(and " << static_cast<int>(parsed.warnings.size() - 8) << " more)";
    }
    return {true, message};
}

juce::Result ThemeManager::exportCurrent(const juce::File& destination) const
{
    if (destination.replaceWithText(juce::String::fromUTF8(theme::serializeTheme(active).c_str())))
        return juce::Result::ok();
    return juce::Result::fail("Couldn't write " + destination.getFullPathName());
}

} // namespace jls::ui
