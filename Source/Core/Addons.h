/*
    Addons - the player extended by scripts in Lua 5.4 (decision 0012), the
    language REAPER and Kalibre's own scripts use.

    An add-on is a .lua file (or a folder with main.lua, or a .zip of one) in
    the Add-ons folder; File > Import Add-on... puts one there. Each runs in a
    Lua of its own, so one can't break another, and is stopped if it runs
    away (a few seconds). It reaches the player through the `kalibre` table:

        kalibre.on("track_started", function(track) ... end)
        kalibre.menu("Copy Now Playing", function() ... end)
        kalibre.every(10, function() ... end)
        kalibre.play(), kalibre.next(), kalibre.now_playing(),
        kalibre.library.query("genre IS jazz"), kalibre.format("%artist%", t),
        kalibre.playlist.add(...), kalibre.tags.set(...), kalibre.message(...)

    The whole API is in docs/ADD-ONS.md. Everything runs on the window's
    thread; the player reaches the app through Api, which the app provides
    (and the tests fake).

    No JUCE in here.
*/

#pragma once

#include "Track.h"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct lua_State;

namespace kp::addons
{

class Api
{
public:
    virtual ~Api() = default;

    // Playback.
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void playPause() = 0;
    virtual void stop() = 0;
    virtual void next() = 0;
    virtual void previous() = 0;
    virtual void seek (double seconds) = 0;
    virtual double position() = 0;
    virtual bool isPlaying() = 0;
    virtual bool isPaused() = 0;
    virtual double volumeDb() = 0;
    virtual void setVolumeDb (double) = 0;
    virtual std::optional<Track> nowPlaying() = 0;

    // The library and playlists.
    virtual std::vector<Track> query (const std::string&) = 0;
    virtual int libraryCount() = 0;
    virtual std::vector<std::string> playlistNames() = 0;
    virtual int activePlaylist() = 0;
    virtual void setActivePlaylist (int) = 0;
    virtual std::vector<Track> playlistItems (int playlist) = 0;
    virtual int createPlaylist (const std::string& name) = 0;
    virtual void addToPlaylist (int playlist, const std::vector<std::string>& locations) = 0;
    virtual void clearPlaylist (int playlist) = 0;
    virtual void playItem (int playlist, int index) = 0;
    virtual void queue (const std::string& location) = 0;
    virtual std::vector<Track> selection() = 0;

    // Tags.
    virtual std::optional<Track> readTrack (const std::string& location) = 0;
    virtual bool writeTags (const std::string& location, const Tags&) = 0;

    // Words.
    virtual void message (const std::string&) = 0;                          // the window's message line
    virtual void print (const std::string& addon, const std::string&) = 0;  // the add-on console
    virtual std::string settingsGet (const std::string& addon, const std::string& key) = 0;
    virtual void settingsSet (const std::string& addon, const std::string& key, const std::string& value) = 0;
};

struct Info
{
    std::string id;            // the file or folder's name
    std::string name, version, author, description;
    std::string error;         // the last error, if any
    bool enabled = true;
    bool loaded = false;
};

// What the header comments say: "-- name: ...", "-- version: ...".
Info readInfo (const std::string& id, const std::string& code);

struct MenuItem
{
    std::string addon, title;
    int ref = 0;
};

class Host
{
public:
    explicit Host (Api&);
    ~Host();

    // Loads (or reloads) an add-on from its code. False, with the error in its Info, if it failed.
    bool load (const std::string& id, const std::string& code);
    void unload (const std::string& id);
    void unloadAll();
    const std::vector<Info>& addons() const { return infos; }
    Info* info (const std::string& id);

    // Events: "startup", "shutdown", "track_started", "track_finished", "paused",
    // "resumed", "stopped", "seeked", "volume_changed", "library_changed", "selection_changed".
    void fire (const std::string& event);
    void fire (const std::string& event, const Track&);
    void fire (const std::string& event, double);

    std::vector<MenuItem> menuItems() const;
    void runMenu (const MenuItem&);

    // Runs the timers that are due; called a few times a second.
    void tick (double nowSeconds);

    double timeLimitSeconds = 3.0;

    struct State;

private:
    Api& api;
    std::vector<Info> infos;
    std::map<std::string, std::unique_ptr<State>> states;
    double now = 0;
    void failed (const std::string& id, const std::string& error);
    friend struct Bindings;
};

} // namespace kp::addons
