#pragma once

// Editing actions that the Piano Roll and the Playlist both understand. Keyboard shortcuts
// (app/commands) send them to whichever of the two fills the main area.

namespace jls::ui
{

enum class EditAction
{
    copy,
    cut,
    paste,
    deleteSelection,
    selectAll,
    deselect,
    slice, // Piano Roll only
};

} // namespace jls::ui
