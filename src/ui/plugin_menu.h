#pragma once

// The grouped plugin menu (manufacturer > category > plugin) shared by the Mixer's effect slots
// and the Channel Rack's instrument choices.

#include <juce_gui_basics/juce_gui_basics.h>

#include <string>
#include <vector>

namespace jls::ui
{

// A known plugin as the UI shows it (filled in by the app from the plugin host).
struct PluginItem
{
    std::string id, name, manufacturer, category;
};

// Items get menu ids firstId + their index. A maker whose plugins all share one category lists
// them directly. `items` must be sorted by manufacturer, then
// category (as PluginHost returns them). An empty list shows a disabled hint instead.
juce::PopupMenu makePluginMenu(const std::vector<PluginItem>& items, int firstId);

} // namespace jls::ui
