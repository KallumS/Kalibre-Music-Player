#include "Commands.h"

namespace kp::cmd
{
using juce::KeyPress;
using juce::ModifierKeys;

const juce::Array<juce::CommandID>& all()
{
    static const juce::Array<juce::CommandID> ids = []
    {
        juce::Array<juce::CommandID> a;
        for (juce::CommandID c = openFiles; c <= addonsHelp; ++c) a.add (c);
        return a;
    }();
    return ids;
}

void describe (juce::CommandID id, juce::ApplicationCommandInfo& info)
{
    const auto cmd = ModifierKeys::commandModifier, shift = ModifierKeys::shiftModifier, alt = ModifierKeys::altModifier;
    const juce::String dots = juce::String::fromUTF8 ("\xe2\x80\xa6");
    auto set = [&] (const juce::String& name, const char* category, const juce::String& desc)
    {
        info.setInfo (name, desc.isEmpty() ? name : desc, category, 0);
    };
    auto key = [&] (int code, int mods = 0) { info.addDefaultKeypress (code, mods); };
    switch (id)
    {
        case openFiles: set ("Open" + dots, "File", "Choose files to play, in place of the playlist's"); key ('o', cmd); break;
        case addFiles: set ("Add Files" + dots, "File", "Add files to the playlist"); key ('o', cmd | shift); break;
        case addFolder: set ("Add Folder" + dots, "File", "Add a folder's music to the playlist"); break;
        case newPlaylist: set ("New Playlist", "File", {}); key ('n', cmd); break;
        case newAutoPlaylist: set ("New Autoplaylist" + dots, "File", "A playlist that holds what a query finds"); key ('n', cmd | shift); break;
        case loadPlaylist: set ("Load Playlist" + dots, "File", "Open an M3U or PLS playlist as a new playlist"); break;
        case savePlaylist: set ("Save Playlist" + dots, "File", "Save the playlist as an M3U file"); key ('s', cmd); break;
        case removePlaylist: set ("Remove Playlist", "File", {}); key ('w', cmd); break;
        case renamePlaylist: set ("Rename Playlist" + dots, "File", {}); break;
        case libraryFolders: set ("Library Folders" + dots, "File", "The folders the library watches"); break;
        case rescanLibrary: set ("Look Through the Library Again", "File", {}); break;
        case importAddon: set ("Import Add-on" + dots, "File", "Add a Lua add-on (.lua, a folder or a .zip)"); break;
        case preferences: set ("Preferences" + dots, "File", {}); key (',', cmd); break;
        case playCd: set ("Play Audio CD", "File", {}); break;
        case ripCd: set ("Rip Audio CD" + dots, "File", {}); break;

        case selectAll: set ("Select All", "Edit", {}); key ('a', cmd); break;
        case removeSelected: set ("Remove from Playlist", "Edit", {}); key (KeyPress::deleteKey); key (KeyPress::backspaceKey); break;
        case cropSelected: set ("Keep Only the Chosen", "Edit", "Remove everything else from the playlist"); break;
        case removeDuplicates: set ("Remove Duplicates", "Edit", {}); break;
        case removeDead: set ("Remove Missing Files", "Edit", {}); break;
        case sortByArtist: set ("Sort by Artist", "Edit", {}); break;
        case sortByAlbum: set ("Sort by Album", "Edit", {}); break;
        case sortByTitle: set ("Sort by Title", "Edit", {}); break;
        case sortByPath: set ("Sort by File Path", "Edit", {}); break;
        case sortRandom: set ("Shuffle the Playlist", "Edit", {}); break;
        case search: set ("Search the Library", "Edit", {}); key ('f', cmd); break;
        case properties: set ("Edit Tags" + dots, "Edit", "The chosen tracks' tags and details"); key ('i', cmd); break;
        case guessTags: set ("Tags from File Names" + dots, "Edit", {}); break;
        case renameFiles: set ("Rename Files" + dots, "Edit", "Rename the chosen files by a pattern"); break;
        case moveFiles: set ("Move Files" + dots, "Edit", "Move the chosen files into folders by a pattern"); break;
        case copyFiles: set ("Copy Files" + dots, "Edit", {}); break;
        case convert: set ("Convert" + dots, "Edit", "Convert the chosen tracks to another format"); key ('k', cmd | shift); break;
        case rgTrack: set ("Scan ReplayGain per Track", "Edit", "Measure each track's loudness and keep it in its tags"); break;
        case rgAlbum: set ("Scan ReplayGain as Albums", "Edit", "Measure each album's and track's loudness and keep it in their tags"); break;
        case rgRemove: set ("Remove ReplayGain Tags", "Edit", {}); break;
        case queueSelected: set ("Play Next", "Edit", "Queue the chosen tracks after this one"); key ('q', 0); break;
        case showInFinder: set ("Show in Finder", "Edit", {}); key ('r', cmd | shift); break;
        case copyLocations: set ("Copy File Paths", "Edit", {}); key ('c', cmd | alt); break;

        case playPause: set ("Play/Pause", "Playback", {}); key (KeyPress::spaceKey); break;
        case play: set ("Play", "Playback", {}); break;
        case pause: set ("Pause", "Playback", {}); break;
        case stop: set ("Stop", "Playback", {}); key ('.', cmd); break;
        case next: set ("Next", "Playback", {}); key (KeyPress::rightKey, cmd); break;
        case previous: set ("Previous", "Playback", {}); key (KeyPress::leftKey, cmd); break;
        case seekForward: set ("Forward 5 Seconds", "Playback", {}); key (KeyPress::rightKey, cmd | shift); break;
        case seekBack: set ("Back 5 Seconds", "Playback", {}); key (KeyPress::leftKey, cmd | shift); break;
        case volumeUp: set ("Louder", "Playback", {}); key (KeyPress::upKey, cmd); break;
        case volumeDown: set ("Quieter", "Playback", {}); key (KeyPress::downKey, cmd); break;
        case mute: set ("Mute", "Playback", {}); key (KeyPress::downKey, cmd | alt); break;
        case stopAfterCurrent: set ("Stop After This Track", "Playback", {}); break;
        case playRandom: set ("Play a Random Track", "Playback", {}); break;
        case orderNormal: set ("In Order", "Playback", {}); break;
        case orderRepeatPlaylist: set ("Repeat Playlist", "Playback", {}); break;
        case orderRepeatTrack: set ("Repeat Track", "Playback", {}); break;
        case orderShuffleTracks: set ("Shuffle Tracks", "Playback", {}); break;
        case orderShuffleAlbums: set ("Shuffle Albums", "Playback", {}); break;
        case orderRandom: set ("Random", "Playback", {}); break;
        case rgOff: set ("ReplayGain Off", "Playback", {}); break;
        case rgTrackMode: set ("ReplayGain by Track", "Playback", {}); break;
        case rgAlbumMode: set ("ReplayGain by Album", "Playback", {}); break;
        case rgSmartMode: set ("ReplayGain by Album When Playing Albums", "Playback", {}); break;
        case equalizerOn: set ("Equaliser On", "Playback", {}); break;

        case editLayout: set ("Edit Layout", "View", "Rearrange the window's panels"); key ('e', cmd); break;
        case resetLayout: set ("Back to the First Layout", "View", {}); break;
        case textBigger: set ("Bigger Text", "View", {}); key ('=', cmd); break;
        case textSmaller: set ("Smaller Text", "View", {}); key ('-', cmd); break;
        case keyboardShortcuts: set ("Keyboard Shortcuts" + dots, "View", {}); break;
        case addonManager: set ("Add-ons" + dots, "View", {}); break;
        case addonConsole: set ("Add-on Console", "View", {}); break;
        case showJobs: set ("Jobs", "View", {}); break;

        case about: set ("About Kalibre Player", "Help", {}); break;
        case titleFormatHelp: set ("Title Formatting", "Help", {}); break;
        case queryHelp: set ("Search Queries", "Help", {}); break;
        case addonsHelp: set ("Writing Add-ons", "Help", {}); break;
        default: break;
    }
}

} // namespace kp::cmd
