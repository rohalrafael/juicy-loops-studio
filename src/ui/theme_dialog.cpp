#include "ui/theme_dialog.h"

namespace jls::ui
{
namespace
{

constexpr int columns = 4;
constexpr int cardWidth = 200;
constexpr int cardHeight = 132;
constexpr int gap = 12;
constexpr int margin = 20;

} // namespace

// ---------------------------------------------------------------------------------- ThemeCard

ThemeCard::ThemeCard(theme::Theme theme)
    : juce::Button(juce::String::fromUTF8(theme.name.c_str())), cardTheme(std::move(theme))
{
    setTooltip(juce::String::fromUTF8(cardTheme.description.c_str()));
}

void ThemeCard::paintButton(juce::Graphics& g, bool highlighted, bool)
{
    const auto& ui = currentTheme();   // the dialog's own look
    const auto& c = cardTheme.colours; // the theme being previewed
    const bool selected = cardTheme.id == ui.id;
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const float radius = ui.shape.cornerRadius;

    g.setColour(colour(ui.colours.panelRaised));
    g.fillRoundedRectangle(bounds, radius);

    // Preview area in the card's theme.
    auto preview = bounds.removeFromTop(78.0f);
    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(preview.getX(), preview.getY(), preview.getWidth(),
                                 preview.getHeight(), radius, radius, true, true, false, false);
        g.reduceClipRegion(clip);
        g.setColour(colour(c.background));
        g.fillRect(preview);
    }

    const float stepRadius = std::min(cardTheme.shape.stepRadius, 6.0f);
    const int onSteps[] = {0, 2, 4, 7};
    for (int i = 0; i < 8; ++i)
    {
        const bool on = std::find(std::begin(onSteps), std::end(onSteps), i) != std::end(onSteps);
        const juce::Rectangle<float> step(preview.getX() + 12.0f + i * 19.0f +
                                              (i >= 4 ? 4.0f : 0.0f),
                                          preview.getY() + 12.0f, 16.0f, 22.0f);
        if (on && cardTheme.effects.stepGlow)
        {
            juce::Path p;
            p.addRoundedRectangle(step, stepRadius);
            juce::DropShadow(colour(*cardTheme.effects.stepGlow), 6, {}).drawForPath(g, p);
        }
        g.setColour(colour(on ? c.stepOn : (i < 4 ? c.stepOffA : c.stepOffB)));
        g.fillRoundedRectangle(step, stepRadius);
        if (cardTheme.shape.stepOutline > 0.0f)
        {
            g.setColour(colour(c.outline));
            g.drawRoundedRectangle(step.reduced(0.75f), stepRadius, 1.5f);
        }
    }

    const theme::Argb swatches[] = {c.background, c.panel, c.accent, c.accentAlt, c.text};
    for (int i = 0; i < 5; ++i)
    {
        const juce::Rectangle<float> dot(preview.getX() + 12.0f + i * 22.0f, preview.getY() + 46.0f,
                                         18.0f, 18.0f);
        g.setColour(colour(swatches[i]));
        g.fillEllipse(dot);
        g.setColour(juce::Colours::grey.withAlpha(0.45f));
        g.drawEllipse(dot, 1.0f);
    }

    // Name and description in the dialog's own theme.
    auto textArea = bounds.reduced(12.0f, 6.0f);
    g.setColour(colour(ui.colours.text));
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText(getButtonText(), textArea.removeFromTop(20.0f), juce::Justification::centredLeft);
    g.setColour(colour(ui.colours.textDim));
    g.setFont(juce::FontOptions(12.0f));
    const bool custom = cardTheme.id.rfind("custom:", 0) == 0;
    const auto subtitle = custom
                              ? juce::String("Imported") +
                                    (cardTheme.author.empty()
                                         ? juce::String()
                                         : " · " + juce::String::fromUTF8(cardTheme.author.c_str()))
                              : juce::String::fromUTF8(cardTheme.description.c_str());
    g.drawText(subtitle, textArea.removeFromTop(18.0f), juce::Justification::centredLeft, true);

    // Selection / hover outline.
    const auto outline = getLocalBounds().toFloat().reduced(1.0f);
    if (selected)
    {
        g.setColour(colour(ui.colours.accent));
        g.drawRoundedRectangle(outline, radius, 2.5f);
    }
    else
    {
        g.setColour(colour(highlighted ? ui.colours.textDim : ui.colours.outline));
        g.drawRoundedRectangle(outline, radius, 1.0f);
    }
}

// -------------------------------------------------------------------------------- ThemeDialog

ThemeDialog::ThemeDialog(ThemeManager& manager) : themes(manager)
{
    title.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    hint.setFont(juce::FontOptions(13.0f));
    addAndMakeVisible(title);
    addAndMakeVisible(hint);

    importButton.onClick = [this] { importTheme(); };
    exportButton.onClick = [this] { exportTheme(); };
    folderButton.onClick = [this]
    {
        const auto folder = themes.themesFolder();
        folder.createDirectory();
        folder.startAsProcess();
    };
    for (auto* b : {&importButton, &exportButton, &folderButton})
        addAndMakeVisible(b);

    themes.addChangeListener(this);
    rebuildCards();
}

ThemeDialog::~ThemeDialog()
{
    themes.removeChangeListener(this);
}

void ThemeDialog::changeListenerCallback(juce::ChangeBroadcaster*)
{
    hint.setColour(juce::Label::textColourId, colour(currentTheme().colours.textDim));
    importButton.setColour(juce::TextButton::buttonColourId, colour(currentTheme().colours.accent));
    importButton.setColour(juce::TextButton::textColourOffId,
                           colour(currentTheme().colours.background));

    // The list changes when a theme is imported; otherwise just repaint the selection.
    if (themes.available().size() != cards.size())
        rebuildCards();
    repaint();
}

void ThemeDialog::rebuildCards()
{
    cards.clear();
    for (const auto& t : themes.available())
    {
        auto card = std::make_unique<ThemeCard>(t);
        const auto id = t.id;
        card->onClick = [this, id] { themes.select(id); };
        addAndMakeVisible(*card);
        cards.push_back(std::move(card));
    }

    const int rows = std::max(1, (static_cast<int>(cards.size()) + columns - 1) / columns);
    setSize(margin * 2 + columns * cardWidth + (columns - 1) * gap,
            margin * 2 + 30 + 24 + rows * cardHeight + (rows - 1) * gap + 20 + 36);
    changeListenerCallback(nullptr);
    resized();
}

void ThemeDialog::paint(juce::Graphics& g)
{
    g.fillAll(colour(currentTheme().colours.panel));
}

void ThemeDialog::resized()
{
    auto area = getLocalBounds().reduced(margin);
    title.setBounds(area.removeFromTop(30));
    hint.setBounds(area.removeFromTop(24));
    area.removeFromTop(8);

    auto buttons = area.removeFromBottom(36);
    importButton.setBounds(buttons.removeFromLeft(190));
    buttons.removeFromLeft(8);
    exportButton.setBounds(buttons.removeFromLeft(200));
    buttons.removeFromLeft(8);
    folderButton.setBounds(buttons.removeFromLeft(180));

    for (std::size_t i = 0; i < cards.size(); ++i)
    {
        const int col = static_cast<int>(i) % columns;
        const int row = static_cast<int>(i) / columns;
        cards[i]->setBounds(area.getX() + col * (cardWidth + gap),
                            area.getY() + row * (cardHeight + gap), cardWidth, cardHeight);
    }
}

void ThemeDialog::importTheme()
{
    chooser = std::make_unique<juce::FileChooser>("Import theme", juce::File(), "*.json");
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File())
                return;
            const auto result = themes.importFile(file);
            if (!result.ok || result.message.isNotEmpty())
                juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOk(
                                                 result.ok ? juce::MessageBoxIconType::InfoIcon
                                                           : juce::MessageBoxIconType::WarningIcon,
                                                 result.ok ? "Theme imported" : "Import failed",
                                                 result.message),
                                             nullptr);
        });
}

void ThemeDialog::exportTheme()
{
    const auto suggested =
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile(juce::String::fromUTF8(themes.current().name.c_str()) + ".json");
    chooser = std::make_unique<juce::FileChooser>("Export theme", suggested, "*.json");
    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
            juce::FileBrowserComponent::warnAboutOverwriting,
        [this](const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File())
                return;
            const auto result = themes.exportCurrent(file.withFileExtension(".json"));
            if (result.failed())
                juce::AlertWindow::showAsync(juce::MessageBoxOptions::makeOptionsOk(
                                                 juce::MessageBoxIconType::WarningIcon,
                                                 "Export failed", result.getErrorMessage()),
                                             nullptr);
        });
}

void ThemeDialog::show(ThemeManager& manager)
{
    juce::DialogWindow::LaunchOptions options;
    // JUCE dialogs default to light grey; follow the theme instead.
    options.dialogBackgroundColour = juce::LookAndFeel::getDefaultLookAndFeel().findColour(
        juce::ResizableWindow::backgroundColourId);
    options.content.setOwned(new ThemeDialog(manager));
    options.dialogTitle = "Theme";
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.escapeKeyTriggersCloseButton = true;
    options.launchAsync();
}

} // namespace jls::ui
