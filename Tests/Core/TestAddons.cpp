#include "Addons.h"
#include "Check.h"

using namespace kp;

namespace
{
struct FakeApi : addons::Api
{
    std::vector<std::string> said, printed, calls;
    std::vector<std::vector<std::string>> added;
    std::map<std::string, std::string> settings;
    Track now;
    double vol = 0;

    void play() override { calls.push_back ("play"); }
    void pause() override { calls.push_back ("pause"); }
    void playPause() override {}
    void stop() override { calls.push_back ("stop"); }
    void next() override { calls.push_back ("next"); }
    void previous() override {}
    void seek (double s) override { calls.push_back ("seek " + std::to_string ((int) s)); }
    double position() override { return 12.5; }
    bool isPlaying() override { return true; }
    bool isPaused() override { return false; }
    double volumeDb() override { return vol; }
    void setVolumeDb (double v) override { vol = v; }
    std::optional<Track> nowPlaying() override { return now; }
    std::vector<Track> query (const std::string& q) override { Track t; t.location = "/q/" + q; return { t, t }; }
    int libraryCount() override { return 3; }
    std::vector<std::string> playlistNames() override { return { "Default", "Jazz" }; }
    int activePlaylist() override { return 1; }
    void setActivePlaylist (int) override {}
    std::vector<Track> playlistItems (int) override { return {}; }
    int createPlaylist (const std::string&) override { return 2; }
    void addToPlaylist (int p, const std::vector<std::string>& l) override { auto v = l; v.insert (v.begin(), std::to_string (p)); added.push_back (v); }
    void clearPlaylist (int) override {}
    void playItem (int, int) override {}
    void queue (const std::string&) override {}
    std::vector<Track> selection() override { return {}; }
    std::optional<Track> readTrack (const std::string& l) override { Track t; t.location = l; t.tags["title"] = { "Read" }; return t; }
    bool writeTags (const std::string& l, const Tags& t) override { said.push_back ("wrote " + l + " " + (t.count ("mood") ? t.at ("mood").front() : "")); return true; }
    void message (const std::string& s) override { said.push_back (s); }
    void print (const std::string&, const std::string& s) override { printed.push_back (s); }
    std::string settingsGet (const std::string& a, const std::string& k) override { return settings[a + "." + k]; }
    void settingsSet (const std::string& a, const std::string& k, const std::string& v) override { settings[a + "." + k] = v; }
};
} // namespace

TEST ("add-ons: header, events, menus, the API")
{
    FakeApi api;
    api.now.location = "/m/x.flac";
    api.now.tags["artist"] = { "Air" };
    api.now.tags["title"] = { "Sexy Boy" };
    addons::Host host (api);
    const std::string code = R"(-- name: Now Playing Notes
-- version: 1.2
-- author: Kallum
-- description: Says what's playing.
kalibre.on("track_started", function(t)
  kalibre.message(kalibre.format("%artist% - %title%", t))
end)
kalibre.menu("Skip", function() kalibre.next(); kalibre.seek(30) end)
kalibre.menu("Add found", function()
  local found = kalibre.library.query("jazz")
  kalibre.playlist.add(2, found)
  print(#found, kalibre.position(), kalibre.playlist.active())
  kalibre.tags.set("/m/x.flac", { mood = "calm" })
  kalibre.settings.set("count", 3)
  print(kalibre.settings.get("count"), kalibre.now_playing().artist, kalibre.format_time(65))
end)
)";
    CHECK (host.load ("notes.lua", code));
    auto* info = host.info ("notes.lua");
    CHECK (info != nullptr);
    CHECK_EQ (info->name, std::string ("Now Playing Notes"));
    CHECK_EQ (info->version, std::string ("1.2"));
    CHECK_EQ (info->author, std::string ("Kallum"));
    host.fire ("track_started", api.now);
    CHECK_EQ (api.said.back(), std::string ("Air - Sexy Boy"));
    auto items = host.menuItems();
    CHECK_EQ (items.size(), (size_t) 2);
    host.runMenu (items[0]);
    CHECK_EQ (api.calls, (std::vector<std::string> { "next", "seek 30" }));
    host.runMenu (items[1]);
    CHECK_EQ (api.added.size(), (size_t) 1);
    CHECK_EQ (api.added[0], (std::vector<std::string> { "1", "/q/jazz", "/q/jazz" }));
    CHECK_EQ (api.printed.size(), (size_t) 2);
    CHECK_EQ (api.printed[0], std::string ("2\t12.5\t2"));
    CHECK_EQ (api.printed[1], std::string ("3\tAir\t1:05"));
    CHECK_EQ (api.said.back(), std::string ("wrote /m/x.flac calm"));
}

TEST ("add-ons: errors are kept, and one that runs away is stopped")
{
    FakeApi api;
    addons::Host host (api);
    host.timeLimitSeconds = 0.2;
    CHECK (! host.load ("bad.lua", "this is not lua"));
    CHECK (! host.info ("bad.lua")->error.empty());
    CHECK (! host.load ("loop.lua", "while true do end"));
    CHECK (host.info ("loop.lua")->error.find ("too long") != std::string::npos);
    // An error in a handler doesn't stop the others.
    CHECK (host.load ("h.lua", "kalibre.on('stopped', function() error('boom') end)\nkalibre.on('stopped', function() kalibre.stop() end)"));
    host.fire ("stopped");
    CHECK (host.info ("h.lua")->error.find ("boom") != std::string::npos);
    CHECK_EQ (api.calls, (std::vector<std::string> { "stop" }));
}

TEST ("add-ons: timers")
{
    FakeApi api;
    addons::Host host (api);
    host.tick (100);
    CHECK (host.load ("t.lua", "n = 0\nkalibre.every(1, function() n = n + 1; kalibre.message('tick ' .. n) end)\n"
                               "kalibre.after(0.5, function() kalibre.message('once') end)"));
    host.tick (100.6);
    CHECK_EQ (api.said, (std::vector<std::string> { "once" }));
    host.tick (101.1);
    host.tick (102.2);
    host.tick (102.3);
    CHECK_EQ (api.said, (std::vector<std::string> { "once", "tick 1", "tick 2" }));
}
