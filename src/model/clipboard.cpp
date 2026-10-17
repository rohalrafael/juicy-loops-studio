#include "model/clipboard.h"

#include <algorithm>

namespace jls::model
{
namespace
{

template <typename Item, typename Start, typename End>
Clipboard<Item> copy(const std::vector<Item>& all, const std::vector<int>& indices, int grid,
                     Start startOf, End endOf)
{
    Clipboard<Item> clipboard;
    int first = 0;
    int last = 0;
    for (int i : indices)
    {
        if (i < 0 || i >= static_cast<int>(all.size()))
            continue;
        const auto& item = all[static_cast<std::size_t>(i)];
        first = clipboard.items.empty() ? startOf(item) : std::min(first, startOf(item));
        last = clipboard.items.empty() ? endOf(item) : std::max(last, endOf(item));
        clipboard.items.push_back(item);
    }
    if (clipboard.items.empty())
        return clipboard;
    grid = std::max(1, grid);
    clipboard.start = first / grid * grid;
    const int end = (last + grid - 1) / grid * grid;
    clipboard.length = std::max(grid, end - clipboard.start);
    return clipboard;
}

} // namespace

NoteClipboard copyNotes(const std::vector<Note>& notes, const std::vector<int>& indices,
                        int gridTicks)
{
    return copy(
        notes, indices, gridTicks, [](const Note& n) { return n.startTick; },
        [](const Note& n) { return n.endTick(); });
}

ClipClipboard copyClips(const std::vector<Clip>& clips, const std::vector<int>& indices,
                        int gridSteps)
{
    return copy(
        clips, indices, gridSteps, [](const Clip& c) { return c.startStep; },
        [](const Clip& c) { return c.endStep(); });
}

std::vector<Note> nextPaste(NoteClipboard& clipboard)
{
    const int offset = clipboard.length * ++clipboard.pastes;
    auto notes = clipboard.items;
    for (auto& note : notes)
        note.startTick += offset;
    return notes;
}

std::vector<Clip> nextPaste(ClipClipboard& clipboard)
{
    const int offset = clipboard.length * ++clipboard.pastes;
    auto clips = clipboard.items;
    for (auto& clip : clips)
        clip.startStep += offset;
    return clips;
}

} // namespace jls::model
