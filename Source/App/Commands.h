/*
    Commands - everything the player does from a menu or a key (decision
    0015). Each has a name, a menu, and default keys; Preferences >
    Keyboard Shortcuts changes the keys, and they're kept.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace kp::cmd
{

enum : juce::CommandID
{
    // File
    openFiles = 0x1000, addFiles, addFolder, newPlaylist, loadPlaylist, savePlaylist, removePlaylist, renamePlaylist,
    newAutoPlaylist, libraryFolders, rescanLibrary, importAddon, preferences, playCd, ripCd,
    // Edit
    selectAll, removeSelected, cropSelected, removeDuplicates, removeDead, sortByArtist, sortByAlbum, sortByTitle, sortByPath, sortRandom,
    search, properties, guessTags, renameFiles, moveFiles, copyFiles, convert, rgTrack, rgAlbum, rgRemove, queueSelected,
    showInFinder, copyLocations,
    // Playback
    playPause, play, pause, stop, next, previous, seekForward, seekBack, volumeUp, volumeDown, mute, stopAfterCurrent, playRandom,
    orderNormal, orderRepeatPlaylist, orderRepeatTrack, orderShuffleTracks, orderShuffleAlbums, orderRandom,
    rgOff, rgTrackMode, rgAlbumMode, rgSmartMode, equalizerOn,
    // View
    editLayout, resetLayout, textBigger, textSmaller, keyboardShortcuts, addonManager, addonConsole, showJobs,
    // Help
    about, titleFormatHelp, queryHelp, addonsHelp,
};

const juce::Array<juce::CommandID>& all();
void describe (juce::CommandID, juce::ApplicationCommandInfo&);

} // namespace kp::cmd
