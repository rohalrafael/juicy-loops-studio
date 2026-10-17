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
    duplicate, // copy + paste right after (Ctrl+B)
    slice,     // Piano Roll only, as are the ones below
    moveUp,    // a semitone
    moveDown,
    moveLeft, // by the snap
    moveRight,
    lengthToStep, // shorten notes to one step
};

} // namespace jls::ui
