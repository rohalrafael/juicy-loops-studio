#pragma once

// Options > Settings (app-wide preferences such as the tempo steps) and Options > Keyboard
// shortcuts (JUCE's key-mapping editor). Both are non-modal windows; changes apply at once.

#include <juce_gui_basics/juce_gui_basics.h>

namespace jls::ui
{

// App-wide settings stored in the settings file.
namespace settings
{
inline constexpr const char* tempoStepKey = "tempoStep";
inline constexpr const char* tempoStepFineKey = "tempoStepFine";
inline constexpr double defaultTempoStep = 5.0; // BPM, Tempo up/down
inline constexpr double defaultTempoStepFine = 1.0;
inline constexpr const char* previewSecondsKey = "previewSeconds";
inline constexpr double defaultPreviewSeconds = 5.0; // Browser: length of a click preview
// Ctrl+Tab order: "fixed" (Browser, Channel Rack, Playlist, Mixer) or "recent" (last used first,
// like Alt+Tab).
inline constexpr const char* focusOrderKey = "focusOrder";
// Interface size: scales the whole UI (text included), e.g. 1.25 for 125%.
inline constexpr const char* uiScaleKey = "uiScale";
double uiScale(juce::PropertiesFile& file);
void applyUiScale(juce::PropertiesFile& file);
// Plugin editors: "inside" (panels in the main window, the default) or "separate" (windows).
inline constexpr const char* pluginWindowsKey = "pluginWindows";
// Clicking a greyed (looped) step in the Channel Rack: "discard" (as in FL, the default: the new
// step is real, that channel's repeats are dropped) or "copy" (the repeats are copied in first,
// so nothing else changes).
inline constexpr const char* loopedStepClickKey = "loopedStepClick";
// The Piano Roll's keyboard: "ivory" (the default) or "dark" (in the panel colours).
inline constexpr const char* pianoRollKeysKey = "pianoRollKeys";

double tempoStep(juce::PropertiesFile& file);
double tempoStepFine(juce::PropertiesFile& file);
double previewSeconds(juce::PropertiesFile& file);
bool focusOrderRecent(juce::PropertiesFile& file);
bool pluginsInsideApp(juce::PropertiesFile& file);
bool loopedStepClickCopies(juce::PropertiesFile& file);
bool pianoRollDarkKeys(juce::PropertiesFile& file);
} // namespace settings

void showSettingsDialog(juce::PropertiesFile& file);
void showShortcutsDialog(juce::KeyPressMappingSet& mappings);

} // namespace jls::ui
