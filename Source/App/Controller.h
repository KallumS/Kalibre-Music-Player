/*
    Controller - the player as a whole, on the window's thread: the library,
    the playlists, what plays and what's next, the plug-ins, the jobs and the
    add-ons. Every view asks it, and listens to it for changes.

    It's also what add-ons reach (addons::Api).
*/

#pragma once

#include "Addons.h"
#include "FolderWatcher.h"
#include "Jobs.h"
#include "Library.h"
#include "LibraryScanner.h"
#include "Player.h"
#include "Playlists.h"
#include "Plugins.h"
#include "ReplayGain.h"
#include "Settings.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <deque>
#include <set>

namespace kp
{

class Controller : public addons::Api, private juce::Timer
{
public:
    // `settingsFolder` empty: the usual place. `withAudio`: open the audio device (not in tests).
    explicit Controller (const juce::File& settingsFolder = {}, bool withAudio = true);
    ~Controller() override;

    Settings settings;
    Library library;
    Playlists playlists;
    Player player;
    Plugins plugins;
    Jobs jobs;
    juce::AudioDeviceManager devices;
    std::unique_ptr<addons::Host> addons;

    //==========================================================================
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void libraryChanged() {}
        virtual void playlistsChanged() {}       // lists added, removed, renamed, chosen, their items changed
        virtual void playbackChanged() {}        // a track started, paused, stopped
        virtual void selectionChanged() {}
        virtual void messageChanged() {}
    };
    void addListener (Listener* l) { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    //==========================================================================
    // Tracks: the library's, or read once for files outside it.
    const Track& track (const std::string& location);
    void forget (const std::string& location);     // read it again next time

    //==========================================================================
    // Playback.
    void playItem (int playlist, int index);
    void playPause();
    void play();
    void pause();
    void stop();
    void next();
    void previous();
    void seek (double seconds);
    void playRandom();
    bool playing() const { return player.isPlaying(); }
    bool paused() const { return player.isPaused(); }
    std::optional<Track> nowPlayingTrack() const;
    int playingPlaylist() const { return playlists.playing; }
    int playingIndex() const { return nowIndex; }
    Order order = Order::normal;
    void setOrder (Order);
    bool stopAfterCurrent = false;
    void setVolume (double db);
    double volume() const { return player.volumeDb.load(); }
    // Queue: tracks to play next, before the playlist's own order.
    std::deque<std::pair<int, int>> upNext;     // playlist, index
    void addToQueue (int playlist, int index);

    rg::Settings replayGain;
    void setReplayGain (const rg::Settings&);
    void setCrossfade (double seconds);

    //==========================================================================
    // Playlists.
    Playlist& active() { return playlists.lists[(size_t) playlists.active]; }
    void choosePlaylist (int);
    int newPlaylist (const juce::String& name = {});
    void removePlaylist (int);
    void renamePlaylist (int, const juce::String&);
    // Files, folders, archives and playlist files added (at -1: the end). Returns how many tracks.
    int addFiles (int playlist, const juce::Array<juce::File>&, int at = -1);
    void addLocations (int playlist, const std::vector<std::string>&, int at = -1);
    void removeItems (int playlist, const std::vector<int>&);
    void moveItems (int playlist, const std::vector<int>&, int to);
    void sortPlaylist (int playlist, const std::string& pattern, bool descending);
    void shufflePlaylist (int playlist);
    void playlistEdited (int playlist);          // after any change to its items
    void refreshAutoPlaylists();
    // "Library Selection": what the library browser shows, as foobar's.
    int librarySelectionPlaylist();
    bool exportPlaylist (int playlist, const juce::File&);
    bool importPlaylist (const juce::File&);

    // What's chosen in the playlist shown.
    std::vector<int> selected;
    void setSelection (std::vector<int>);
    std::vector<std::string> selectedLocations();

    //==========================================================================
    // The library.
    void addFolder (const juce::File&);
    void removeFolder (const juce::String&);
    void rescan (const juce::StringArray& folders = {});
    bool scanning() const { return scanner.busy(); }
    juce::String scanStatus;
    // Tags changed on disk (by the tag editor or a job): the library and the views learn it.
    void tracksChanged (const std::vector<std::string>& locations);
    void fileMoved (const std::string& from, const std::string& to);

    //==========================================================================
    // The message line.
    void say (const juce::String&, bool important = false);
    juce::String messageText;
    bool messageImportant = false;

    void saveAll();
    void loadAddons();
    juce::String importAddon (const juce::File&);   // "" or why not
    juce::StringArray addonConsole;

    // addons::Api
    double position() override { return player.position(); }
    bool isPlaying() override { return playing() && ! paused(); }
    bool isPaused() override { return paused(); }
    double volumeDb() override { return volume(); }
    void setVolumeDb (double v) override { setVolume (v); }
    std::optional<Track> nowPlaying() override { return nowPlayingTrack(); }
    std::vector<Track> query (const std::string&) override;
    int libraryCount() override { return (int) library.size(); }
    std::vector<std::string> playlistNames() override;
    int activePlaylist() override { return playlists.active; }
    void setActivePlaylist (int i) override { choosePlaylist (i); }
    std::vector<Track> playlistItems (int) override;
    int createPlaylist (const std::string& name) override { return newPlaylist (juce::String::fromUTF8 (name.c_str())); }
    void addToPlaylist (int p, const std::vector<std::string>& l) override { addLocations (p, l); }
    void clearPlaylist (int) override;
    void queue (const std::string& location) override;
    std::vector<Track> selection() override;
    std::optional<Track> readTrack (const std::string& location) override;
    bool writeTags (const std::string& location, const Tags&) override;
    void message (const std::string& m) override { say (juce::String::fromUTF8 (m.c_str())); }
    void print (const std::string& addon, const std::string& text) override;
    std::string settingsGet (const std::string& addon, const std::string& key) override;
    void settingsSet (const std::string& addon, const std::string& key, const std::string& value) override;

private:
    juce::ListenerList<Listener> listeners;
    LibraryScanner scanner;
    FolderWatcher watcher;
    std::unordered_map<std::string, Track> outside;   // tracks not in the library, read once
    Navigator navigator;
    int nowIndex = -1;
    std::int64_t nextToken = 1;
    struct Placed { int playlist, index; std::string location; };
    std::unordered_map<std::int64_t, Placed> tokens;
    std::unordered_map<std::string, double> measuredGain;   // dB, scanned on the fly
    std::set<std::string> measuring;
    std::optional<Track> nowTrack;
    bool quitting = false;
    double startedAt = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    int saveCountdown = 0;
    bool dirty = false;

    void timerCallback() override;
    void handle (const Player::Event&);
    std::optional<std::pair<int, int>> after (int playlist, int index, bool manual);
    Player::Item itemFor (int playlist, int index);
    double gainFor (const Track&);
    void measureLater (const Track&, std::int64_t token);
    std::vector<std::string> albumKeys (const Playlist&);
    void notifyPlaylists();
    void loadState();
    void applyAudioSettings();
    void expand (const juce::File&, std::vector<std::string>&, int depth = 0);
};

} // namespace kp
