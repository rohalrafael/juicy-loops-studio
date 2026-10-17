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

double tempoStep(juce::PropertiesFile& file);
double tempoStepFine(juce::PropertiesFile& file);
double previewSeconds(juce::PropertiesFile& file);
bool focusOrderRecent(juce::PropertiesFile& file);
} // namespace settings

void showSettingsDialog(juce::PropertiesFile& file);
void showShortcutsDialog(juce::KeyPressMappingSet& mappings);

} // namespace jls::ui
