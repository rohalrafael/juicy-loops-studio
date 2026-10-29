#pragma once

// Every user action that can have a keyboard shortcut, with its default keys (FL Studio's where
// FL has one). MainComponent performs them through JUCE's ApplicationCommandManager, which
// also shows the keys in the menus and lets the user change them (Options > Keyboard
// shortcuts). The user's changes are saved in <settings>/shortcuts.xml by command id.

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace jls::commands
{

// Saved in shortcuts.xml: never renumber or reuse an id; add new ones at the end of a group.
enum ID : juce::CommandID
{
    // File
    newProject = 0x1001,
    openProject,
    saveProject,
    saveProjectAs,
    exportWav,
    quit,

    // Edit (act on the Piano Roll or the Playlist, whichever fills the main area)
    undo = 0x1101,
    redo,
    copy,
    cut,
    paste,
    deleteSelection,
    selectAll,
    deselect,
    duplicate,
    moveSelectionUp,
    moveSelectionDown,
    moveSelectionLeft,
    moveSelectionRight,

    // Transport
    playStop = 0x1201,
    playPause,
    toggleSongMode,
    toggleMetronome,
    tempoUp,
    tempoDown,
    tempoUpFine,
    tempoDownFine,

    // Patterns
    newPattern = 0x1301,
    clonePattern,
    renamePattern,
    deletePattern,
    nextPattern,
    previousPattern,
    splitByChannel,
    makeUnique, // the selected Playlist clips get their own copy of their pattern

    // View
    showPlaylist = 0x1401,
    toggleChannelRack,
    showPianoRoll,
    toggleMixer,
    toggleBrowser,
    browserSwitchSide,
    browserAddFolder,
    previewPlayPause,
    browserUseSample,

    // Piano Roll / Playlist editing
    sliceNotes = 0x1501,
    sliceFiner,
    sliceCoarser,
    snapFiner,
    snapCoarser,
    notesToStepLength,

    // Channel Rack
    toggleRackLoop = 0x1601,
    rackShow16,
    rackShow32,
    rackShow64,
    rackShowAudioClips,
    clearChannel,     // the selected channel's notes in the current pattern
    duplicateChannel, // the selected channel, with its sound and settings, without notes

    // Input
    toggleTypingKeyboard = 0x1701,

    // Navigation: which area has the keyboard, and moving around in it
    focusNext = 0x1901,
    focusPrevious,
    focusBrowser,
    focusChannelRack,
    focusMainView,
    focusMixer,
    navUp,
    navDown,
    navLeft,
    navRight,
    navConfirm,
    navUpLoad,
    navDownLoad,

    // Options / Help
    showSettings = 0x1801,
    showShortcuts,
    showThemes,
    showPlugins,
    showAudioSettings,
    showAbout,
};

struct Definition
{
    ID id;
    const char* name;        // shown in menus and the shortcuts window
    const char* category;    // groups in the shortcuts window
    const char* description; // tooltip-length explanation
    std::vector<juce::KeyPress> defaultKeys;
};

// All commands, grouped by category.
const std::vector<Definition>& all();
const Definition* find(juce::CommandID id);

} // namespace jls::commands
