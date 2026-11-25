#include "Addons.h"
#include "TitleFormat.h"

#include <chrono>
#include <sstream>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

namespace kp::addons
{

struct Host::State
{
    std::string id;
    lua_State* L = nullptr;
    std::map<std::string, std::vector<int>> handlers;
    struct Item { std::string title; int ref; };
    std::vector<Item> menu;
    struct Timer { double interval, due; int ref; bool repeat; bool live = true; };
    std::vector<Timer> timers;
    std::chrono::steady_clock::time_point deadline;
    bool timedOut = false;
    Host* host = nullptr;

    ~State() { if (L) lua_close (L); }
};

namespace
{
using State = Host::State;

State& stateOf (lua_State* L)
{
    lua_getfield (L, LUA_REGISTRYINDEX, "kalibre.state");
    auto* s = static_cast<State*> (lua_touserdata (L, -1));
    lua_pop (L, 1);
    return *s;
}

void hook (lua_State* L, lua_Debug*)
{
    auto& s = stateOf (L);
    if (std::chrono::steady_clock::now() > s.deadline)
    {
        s.timedOut = true;
        luaL_error (L, "it ran for too long and was stopped");
    }
}

//==============================================================================
// Tracks to and from Lua.

void pushTrack (lua_State* L, const Track& t)
{
    lua_newtable (L);
    lua_pushstring (L, t.location.c_str()); lua_setfield (L, -2, "location");
    lua_pushnumber (L, t.length); lua_setfield (L, -2, "length");
    lua_pushinteger (L, t.sampleRate); lua_setfield (L, -2, "samplerate");
    lua_pushinteger (L, t.channels); lua_setfield (L, -2, "channels");
    lua_pushinteger (L, t.bitrate); lua_setfield (L, -2, "bitrate");
    lua_pushinteger (L, t.bitsPerSample); lua_setfield (L, -2, "bitspersample");
    lua_pushstring (L, t.codec.c_str()); lua_setfield (L, -2, "codec");
    lua_pushinteger (L, (lua_Integer) t.fileSize); lua_setfield (L, -2, "filesize");
    lua_newtable (L);
    for (auto& [k, values] : t.tags)
    {
        lua_newtable (L);
        int i = 1;
        for (auto& v : values) { lua_pushstring (L, v.c_str()); lua_rawseti (L, -2, i++); }
        lua_setfield (L, -2, k.c_str());
    }
    lua_setfield (L, -2, "tags");
    // The usual fields, as title formatting makes them, for convenience.
    tf::TrackFields f (t);
    for (const char* name : { "artist", "title", "album", "album artist", "date", "genre", "tracknumber", "discnumber" })
    {
        std::string v;
        f.field (name, v);
        lua_pushstring (L, v.c_str());
        std::string key = name;
        for (auto& c : key) if (c == ' ') c = '_';
        lua_setfield (L, -2, key.c_str());
    }
}

void pushTracks (lua_State* L, const std::vector<Track>& tracks)
{
    lua_createtable (L, (int) tracks.size(), 0);
    int i = 1;
    for (auto& t : tracks) { pushTrack (L, t); lua_rawseti (L, -2, i++); }
}

Tags readTags (lua_State* L, int index)
{
    Tags tags;
    index = lua_absindex (L, index);
    luaL_checktype (L, index, LUA_TTABLE);
    lua_pushnil (L);
    while (lua_next (L, index) != 0)
    {
        if (lua_type (L, -2) == LUA_TSTRING)
        {
            const std::string key = lower (lua_tostring (L, -2));
            if (lua_istable (L, -1))
            {
                std::vector<std::string> values;
                const auto n = (int) luaL_len (L, -1);
                for (int k = 1; k <= n; ++k)
                {
                    lua_rawgeti (L, -1, k);
                    if (lua_isstring (L, -1)) values.push_back (lua_tostring (L, -1));
                    lua_pop (L, 1);
                }
                tags[key] = values;
            }
            else if (lua_isstring (L, -1)) tags[key] = { lua_tostring (L, -1) };
            else if (lua_isboolean (L, -1) && ! lua_toboolean (L, -1)) tags[key] = {};
        }
        lua_pop (L, 1);
    }
    return tags;
}

std::optional<Track> trackArg (lua_State* L, int index, Api& api)
{
    if (lua_isnoneornil (L, index)) return api.nowPlaying();
    if (lua_isstring (L, index)) return api.readTrack (lua_tostring (L, index));
    luaL_checktype (L, index, LUA_TTABLE);
    Track t;
    lua_getfield (L, index, "location");
    if (lua_isstring (L, -1)) t.location = lua_tostring (L, -1);
    lua_pop (L, 1);
    lua_getfield (L, index, "tags");
    if (lua_istable (L, -1)) t.tags = readTags (L, -1);
    lua_pop (L, 1);
    lua_getfield (L, index, "length");
    t.length = lua_isnumber (L, -1) ? lua_tonumber (L, -1) : 0;
    lua_pop (L, 1);
    // A table with only a location: the real track's details.
    if (t.tags.empty() && ! t.location.empty())
        if (auto real = api.readTrack (t.location)) return real;
    return t;
}

std::vector<std::string> locationsArg (lua_State* L, int index)
{
    std::vector<std::string> out;
    if (lua_isstring (L, index)) { out.push_back (lua_tostring (L, index)); return out; }
    luaL_checktype (L, index, LUA_TTABLE);
    const auto n = (int) luaL_len (L, index);
    for (int k = 1; k <= n; ++k)
    {
        lua_rawgeti (L, index, k);
        if (lua_isstring (L, -1)) out.push_back (lua_tostring (L, -1));
        else if (lua_istable (L, -1))
        {
            lua_getfield (L, -1, "location");
            if (lua_isstring (L, -1)) out.push_back (lua_tostring (L, -1));
            lua_pop (L, 1);
        }
        lua_pop (L, 1);
    }
    return out;
}

Api& apiOf (lua_State* L);

int luaPrint (lua_State* L)
{
    std::ostringstream out;
    const int n = lua_gettop (L);
    for (int k = 1; k <= n; ++k)
    {
        if (k > 1) out << '\t';
        out << luaL_tolstring (L, k, nullptr);
        lua_pop (L, 1);
    }
    apiOf (L).print (stateOf (L).id, out.str());
    return 0;
}

int ref (lua_State* L, int index)
{
    luaL_checktype (L, index, LUA_TFUNCTION);
    lua_pushvalue (L, index);
    return luaL_ref (L, LUA_REGISTRYINDEX);
}

int luaOn (lua_State* L)
{
    const std::string event = luaL_checkstring (L, 1);
    stateOf (L).handlers[event].push_back (ref (L, 2));
    return 0;
}

int luaMenu (lua_State* L)
{
    const std::string title = luaL_checkstring (L, 1);
    stateOf (L).menu.push_back ({ title, ref (L, 2) });
    return 0;
}

int addTimer (lua_State* L, bool repeat)
{
    const double seconds = std::max (0.05, luaL_checknumber (L, 1));
    auto& s = stateOf (L);
    lua_getfield (L, LUA_REGISTRYINDEX, "kalibre.now");
    const double now = lua_tonumber (L, -1);
    lua_pop (L, 1);
    s.timers.push_back ({ seconds, now + seconds, ref (L, 2), repeat });
    lua_pushinteger (L, (lua_Integer) s.timers.size());
    return 1;
}
int luaEvery (lua_State* L) { return addTimer (L, true); }
int luaAfter (lua_State* L) { return addTimer (L, false); }
int luaCancel (lua_State* L)
{
    auto& s = stateOf (L);
    const auto k = (size_t) luaL_checkinteger (L, 1);
    if (k >= 1 && k <= s.timers.size()) s.timers[k - 1].live = false;
    return 0;
}

#define API apiOf (L)
int luaPlay (lua_State* L) { API.play(); return 0; }
int luaPause (lua_State* L) { API.pause(); return 0; }
int luaPlayPause (lua_State* L) { API.playPause(); return 0; }
int luaStop (lua_State* L) { API.stop(); return 0; }
int luaNext (lua_State* L) { API.next(); return 0; }
int luaPrevious (lua_State* L) { API.previous(); return 0; }
int luaSeek (lua_State* L) { API.seek (luaL_checknumber (L, 1)); return 0; }
int luaPosition (lua_State* L) { lua_pushnumber (L, API.position()); return 1; }
int luaIsPlaying (lua_State* L) { lua_pushboolean (L, API.isPlaying()); return 1; }
int luaIsPaused (lua_State* L) { lua_pushboolean (L, API.isPaused()); return 1; }
int luaVolume (lua_State* L)
{
    if (lua_isnumber (L, 1)) API.setVolumeDb (lua_tonumber (L, 1));
    lua_pushnumber (L, API.volumeDb());
    return 1;
}
int luaNowPlaying (lua_State* L)
{
    if (auto t = API.nowPlaying()) pushTrack (L, *t);
    else lua_pushnil (L);
    return 1;
}
int luaFormat (lua_State* L)
{
    const std::string pattern = luaL_checkstring (L, 1);
    auto t = trackArg (L, 2, API);
    if (! t) { lua_pushstring (L, ""); return 1; }
    lua_pushstring (L, tf::format (pattern, *t).c_str());
    return 1;
}
int luaMessage (lua_State* L) { API.message (luaL_checkstring (L, 1)); return 0; }
int luaQuery (lua_State* L) { pushTracks (L, API.query (luaL_optstring (L, 1, ""))); return 1; }
int luaCount (lua_State* L) { lua_pushinteger (L, API.libraryCount()); return 1; }
int luaPlaylists (lua_State* L)
{
    auto names = API.playlistNames();
    lua_createtable (L, (int) names.size(), 0);
    int i = 1;
    for (auto& n : names) { lua_pushstring (L, n.c_str()); lua_rawseti (L, -2, i++); }
    return 1;
}
// Playlists are counted from 1 in Lua, as Lua counts.
int luaActive (lua_State* L)
{
    if (lua_isinteger (L, 1)) API.setActivePlaylist ((int) lua_tointeger (L, 1) - 1);
    lua_pushinteger (L, API.activePlaylist() + 1);
    return 1;
}
int luaItems (lua_State* L) { pushTracks (L, API.playlistItems ((int) luaL_optinteger (L, 1, API.activePlaylist() + 1) - 1)); return 1; }
int luaCreate (lua_State* L) { lua_pushinteger (L, API.createPlaylist (luaL_optstring (L, 1, "")) + 1); return 1; }
int luaAdd (lua_State* L)
{
    // add(locations) to the active list, or add(playlist, locations).
    if (lua_isinteger (L, 1)) API.addToPlaylist ((int) lua_tointeger (L, 1) - 1, locationsArg (L, 2));
    else API.addToPlaylist (API.activePlaylist(), locationsArg (L, 1));
    return 0;
}
int luaClear (lua_State* L) { API.clearPlaylist ((int) luaL_optinteger (L, 1, API.activePlaylist() + 1) - 1); return 0; }
int luaPlayItem (lua_State* L) { API.playItem ((int) luaL_checkinteger (L, 1) - 1, (int) luaL_checkinteger (L, 2) - 1); return 0; }
int luaQueue (lua_State* L) { for (auto& l : locationsArg (L, 1)) API.queue (l); return 0; }
int luaSelection (lua_State* L) { pushTracks (L, API.selection()); return 1; }
int luaTagsGet (lua_State* L)
{
    if (auto t = API.readTrack (luaL_checkstring (L, 1))) pushTrack (L, *t);
    else lua_pushnil (L);
    return 1;
}
int luaTagsSet (lua_State* L)
{
    const std::string where = luaL_checkstring (L, 1);
    lua_pushboolean (L, API.writeTags (where, readTags (L, 2)));
    return 1;
}
int luaSettingGet (lua_State* L)
{
    auto v = API.settingsGet (stateOf (L).id, luaL_checkstring (L, 1));
    if (v.empty() && ! lua_isnoneornil (L, 2)) lua_pushvalue (L, 2);
    else lua_pushstring (L, v.c_str());
    return 1;
}
int luaSettingSet (lua_State* L)
{
    API.settingsSet (stateOf (L).id, luaL_checkstring (L, 1), luaL_tolstring (L, 2, nullptr));
    return 0;
}
int luaTime (lua_State* L) { lua_pushstring (L, formatTime (luaL_checknumber (L, 1)).c_str()); return 1; }
#undef API

Api& apiOf (lua_State* L)
{
    lua_getfield (L, LUA_REGISTRYINDEX, "kalibre.api");
    auto* a = static_cast<Api*> (lua_touserdata (L, -1));
    lua_pop (L, 1);
    return *a;
}

void setFunctions (lua_State* L, std::initializer_list<std::pair<const char*, lua_CFunction>> fns)
{
    for (auto& [name, fn] : fns) { lua_pushcfunction (L, fn); lua_setfield (L, -2, name); }
}

void install (lua_State* L)
{
    lua_newtable (L);
    setFunctions (L, { { "on", luaOn }, { "menu", luaMenu }, { "every", luaEvery }, { "after", luaAfter }, { "cancel", luaCancel },
                       { "play", luaPlay }, { "pause", luaPause }, { "play_pause", luaPlayPause }, { "stop", luaStop },
                       { "next", luaNext }, { "previous", luaPrevious }, { "seek", luaSeek }, { "position", luaPosition },
                       { "is_playing", luaIsPlaying }, { "is_paused", luaIsPaused }, { "volume", luaVolume },
                       { "now_playing", luaNowPlaying }, { "format", luaFormat }, { "message", luaMessage },
                       { "format_time", luaTime }, { "queue", luaQueue }, { "selection", luaSelection } });
    lua_pushstring (L, "1");
    lua_setfield (L, -2, "api_version");

    lua_newtable (L);
    setFunctions (L, { { "query", luaQuery }, { "count", luaCount } });
    lua_setfield (L, -2, "library");

    lua_newtable (L);
    setFunctions (L, { { "names", luaPlaylists }, { "active", luaActive }, { "items", luaItems }, { "create", luaCreate },
                       { "add", luaAdd }, { "clear", luaClear }, { "play", luaPlayItem } });
    lua_setfield (L, -2, "playlist");

    lua_newtable (L);
    setFunctions (L, { { "get", luaTagsGet }, { "set", luaTagsSet } });
    lua_setfield (L, -2, "tags");

    lua_newtable (L);
    setFunctions (L, { { "get", luaSettingGet }, { "set", luaSettingSet } });
    lua_setfield (L, -2, "settings");

    lua_setglobal (L, "kalibre");
    lua_pushcfunction (L, luaPrint);
    lua_setglobal (L, "print");
}

std::string trimmedError (lua_State* L)
{
    std::string e = lua_isstring (L, -1) ? lua_tostring (L, -1) : "an error";
    lua_pop (L, 1);
    return e;
}
} // namespace

//==============================================================================
Info readInfo (const std::string& id, const std::string& code)
{
    Info i;
    i.id = id;
    i.name = id;
    auto dot = i.name.find_last_of ('.');
    if (dot != std::string::npos && dot > 0) i.name = i.name.substr (0, dot);
    std::istringstream in (code);
    std::string line;
    while (std::getline (in, line))
    {
        auto t = trim (line);
        if (t.empty()) continue;
        if (t.rfind ("--", 0) != 0) break;
        t = trim (t.substr (2));
        auto colon = t.find (':');
        if (colon == std::string::npos) continue;
        const auto key = lower (trim (t.substr (0, colon)));
        const auto value = trim (t.substr (colon + 1));
        if (key == "name") i.name = value;
        else if (key == "version") i.version = value;
        else if (key == "author") i.author = value;
        else if (key == "description") i.description = value;
    }
    return i;
}

Host::Host (Api& a) : api (a) {}
Host::~Host() { unloadAll(); }

Info* Host::info (const std::string& id)
{
    for (auto& i : infos) if (i.id == id) return &i;
    return nullptr;
}

void Host::failed (const std::string& id, const std::string& error)
{
    if (auto* i = info (id)) i->error = error;
    api.print (id, "Error: " + error);
}

bool Host::load (const std::string& id, const std::string& code)
{
    unload (id);
    auto fresh = readInfo (id, code);
    if (auto* i = info (id)) { fresh.enabled = i->enabled; *i = fresh; }
    else infos.push_back (fresh);
    if (! info (id)->enabled) return true;

    auto s = std::make_unique<State>();
    s->id = id;
    s->host = this;
    s->L = luaL_newstate();
    auto* L = s->L;
    luaL_openlibs (L);
    // No os.exit, and no reaching outside through io.popen.
    lua_getglobal (L, "os");
    lua_pushnil (L); lua_setfield (L, -2, "exit");
    lua_pop (L, 1);
    lua_getglobal (L, "io");
    if (lua_istable (L, -1)) { lua_pushnil (L); lua_setfield (L, -2, "popen"); }
    lua_pop (L, 1);
    lua_pushlightuserdata (L, s.get());
    lua_setfield (L, LUA_REGISTRYINDEX, "kalibre.state");
    lua_pushlightuserdata (L, &api);
    lua_setfield (L, LUA_REGISTRYINDEX, "kalibre.api");
    lua_pushnumber (L, now);
    lua_setfield (L, LUA_REGISTRYINDEX, "kalibre.now");
    install (L);
    lua_sethook (L, hook, LUA_MASKCOUNT, 10000);
    s->deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds ((long long) (timeLimitSeconds * 1000));

    if (luaL_loadbuffer (L, code.data(), code.size(), ("@" + id).c_str()) != LUA_OK || lua_pcall (L, 0, 0, 0) != LUA_OK)
    {
        failed (id, trimmedError (L));
        return false;
    }
    info (id)->loaded = true;
    states[id] = std::move (s);
    return true;
}

void Host::unload (const std::string& id)
{
    states.erase (id);
    if (auto* i = info (id)) i->loaded = false;
}

void Host::unloadAll()
{
    states.clear();
    for (auto& i : infos) i.loaded = false;
}

namespace
{
// Calls the function at ref with `push` adding its arguments; errors go to the add-on's info.
template <typename Push>
bool call (State& s, int fnRef, int nargs, Push&& push, double limit, std::string& error)
{
    auto* L = s.L;
    s.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds ((long long) (limit * 1000));
    lua_rawgeti (L, LUA_REGISTRYINDEX, fnRef);
    push (L);
    if (lua_pcall (L, nargs, 0, 0) != LUA_OK) { error = trimmedError (L); return false; }
    return true;
}
} // namespace

void Host::fire (const std::string& event)
{
    for (auto& [id, s] : states)
    {
        auto it = s->handlers.find (event);
        if (it == s->handlers.end()) continue;
        for (int r : it->second)
        {
            std::string e;
            if (! call (*s, r, 0, [] (lua_State*) {}, timeLimitSeconds, e)) failed (id, e);
        }
    }
}

void Host::fire (const std::string& event, const Track& t)
{
    for (auto& [id, s] : states)
    {
        auto it = s->handlers.find (event);
        if (it == s->handlers.end()) continue;
        for (int r : it->second)
        {
            std::string e;
            if (! call (*s, r, 1, [&t] (lua_State* L) { pushTrack (L, t); }, timeLimitSeconds, e)) failed (id, e);
        }
    }
}

void Host::fire (const std::string& event, double v)
{
    for (auto& [id, s] : states)
    {
        auto it = s->handlers.find (event);
        if (it == s->handlers.end()) continue;
        for (int r : it->second)
        {
            std::string e;
            if (! call (*s, r, 1, [v] (lua_State* L) { lua_pushnumber (L, v); }, timeLimitSeconds, e)) failed (id, e);
        }
    }
}

std::vector<MenuItem> Host::menuItems() const
{
    std::vector<MenuItem> out;
    for (auto& [id, s] : states)
        for (auto& m : s->menu) out.push_back ({ id, m.title, m.ref });
    return out;
}

void Host::runMenu (const MenuItem& m)
{
    auto it = states.find (m.addon);
    if (it == states.end()) return;
    std::string e;
    if (! call (*it->second, m.ref, 0, [] (lua_State*) {}, timeLimitSeconds, e)) failed (m.addon, e);
}

void Host::tick (double nowSeconds)
{
    now = nowSeconds;
    for (auto& [id, s] : states)
    {
        lua_pushnumber (s->L, now);
        lua_setfield (s->L, LUA_REGISTRYINDEX, "kalibre.now");
        // Timers may add timers: by index, over those there were.
        const size_t count = s->timers.size();
        for (size_t k = 0; k < count; ++k)
        {
            auto t = s->timers[k];
            if (! t.live || t.due > now) continue;
            if (t.repeat) s->timers[k].due = now + t.interval;
            else s->timers[k].live = false;
            std::string e;
            if (! call (*s, t.ref, 0, [] (lua_State*) {}, timeLimitSeconds, e))
            {
                failed (id, e);
                s->timers[k].live = false;   // a timer that fails stops
            }
        }
    }
}

} // namespace kp::addons
