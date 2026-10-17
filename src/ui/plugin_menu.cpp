#include "ui/plugin_menu.h"

namespace jls::ui
{

juce::PopupMenu makePluginMenu(const std::vector<PluginItem>& items, int firstId)
{
    juce::PopupMenu menu;
    for (std::size_t i = 0; i < items.size();)
    {
        juce::PopupMenu makerMenu;
        const auto& maker = items[i].manufacturer;

        // A maker with a single category (e.g. all "Instrument") skips that level.
        std::size_t end = i;
        while (end < items.size() && items[end].manufacturer == maker)
            ++end;
        if (items[end - 1].category == items[i].category)
        {
            for (; i < end; ++i)
                makerMenu.addItem(firstId + static_cast<int>(i),
                                  juce::String::fromUTF8(items[i].name.c_str()));
            menu.addSubMenu(juce::String::fromUTF8(maker.c_str()), makerMenu);
            continue;
        }

        while (i < items.size() && items[i].manufacturer == maker)
        {
            juce::PopupMenu categoryMenu;
            const auto& category = items[i].category;
            while (i < items.size() && items[i].manufacturer == maker &&
                   items[i].category == category)
            {
                categoryMenu.addItem(firstId + static_cast<int>(i),
                                     juce::String::fromUTF8(items[i].name.c_str()));
                ++i;
            }
            makerMenu.addSubMenu(juce::String::fromUTF8(category.c_str()), categoryMenu);
        }
        menu.addSubMenu(juce::String::fromUTF8(maker.c_str()), makerMenu);
    }
    if (items.empty())
        menu.addItem(-1, "No plugins found yet: scan in Manage plugins", false);
    return menu;
}

} // namespace jls::ui
