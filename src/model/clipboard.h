#pragma once

// Copy and paste of notes (Piano Roll) and clips (Playlist). A copy remembers its block: from the
// first item's start (rounded down to the grid) to the last item's end (rounded up), so pasting
// lands right after the original, then after each paste, as whole beats or bars.

#include "model/project.h"

#include <vector>

namespace jls::model
{

template <typename Item> struct Clipboard
{
    std::vector<Item> items; // as copied, at their original positions
    int start = 0;           // block start (ticks for notes, steps for clips)
    int length = 0;          // block length, in the same unit; 0 = empty
    int pastes = 0;          // how many times it was pasted since the copy

    bool empty() const { return items.empty(); }
};

using NoteClipboard = Clipboard<Note>;
using ClipClipboard = Clipboard<Clip>;

// Copies `notes[indices]` with a block rounded to `gridTicks` (e.g. a beat).
NoteClipboard copyNotes(const std::vector<Note>& notes, const std::vector<int>& indices,
                        int gridTicks);
// Copies `clips[indices]` with a block rounded to `gridSteps` (e.g. a bar).
ClipClipboard copyClips(const std::vector<Clip>& clips, const std::vector<int>& indices,
                        int gridSteps);

// The next paste: the items moved one block further than the last paste (counts it).
std::vector<Note> nextPaste(NoteClipboard& clipboard);
std::vector<Clip> nextPaste(ClipClipboard& clipboard);

} // namespace jls::model
