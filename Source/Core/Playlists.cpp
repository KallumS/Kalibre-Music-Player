#include "Playlists.h"
#include "Track.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

namespace kp
{
using json = nlohmann::json;

const char* orderName (Order o)
{
    switch (o)
    {
        case Order::normal: return "Default";
        case Order::repeatPlaylist: return "Repeat Playlist";
        case Order::repeatTrack: return "Repeat Track";
        case Order::shuffleTracks: return "Shuffle Tracks";
        case Order::shuffleAlbums: return "Shuffle Albums";
        case Order::random: return "Random";
    }
    return "";
}

//==============================================================================
void Navigator::shuffle (Order o, int count, const std::vector<std::string>& albums, int startWith)
{
    order.clear();
    orderCount = count;
    madeFor = o;
    if (count <= 0) return;
    if (o == Order::shuffleAlbums && (int) albums.size() == count)
    {
        // Albums in a random order, each album's tracks in their own order.
        std::vector<std::string> keys;
        std::map<std::string, std::vector<int>> groups;
        for (int k = 0; k < count; ++k)
        {
            auto& g = groups[albums[(size_t) k]];
            if (g.empty()) keys.push_back (albums[(size_t) k]);
            g.push_back (k);
        }
        std::shuffle (keys.begin(), keys.end(), rng);
        if (startWith >= 0)
        {
            auto it = std::find (keys.begin(), keys.end(), albums[(size_t) startWith]);
            if (it != keys.end()) std::rotate (keys.begin(), it, it + 1);
        }
        for (auto& key : keys) for (int k : groups[key]) order.push_back (k);
        return;
    }
    for (int k = 0; k < count; ++k) order.push_back (k);
    std::shuffle (order.begin(), order.end(), rng);
    if (startWith >= 0)
    {
        auto it = std::find (order.begin(), order.end(), startWith);
        if (it != order.end()) std::iter_swap (order.begin(), it);
    }
}

int Navigator::placeOf (int item) const
{
    auto it = std::find (order.begin(), order.end(), item);
    return it == order.end() ? -1 : (int) (it - order.begin());
}

int Navigator::next (Order o, int current, int count, const std::vector<std::string>& albums, bool manual)
{
    if (count <= 0) return -1;
    switch (o)
    {
        case Order::normal: return current + 1 < count ? current + 1 : -1;
        case Order::repeatPlaylist: return current + 1 < count ? current + 1 : 0;
        case Order::repeatTrack:
            // Next pressed by hand moves on; the end of the track repeats it.
            if (manual) return current + 1 < count ? current + 1 : 0;
            return current >= 0 && current < count ? current : 0;
        case Order::random:
        {
            if (count == 1) return 0;
            std::uniform_int_distribution<int> pick (0, count - 1);
            int k = pick (rng);
            if (k == current) k = (k + 1) % count;
            return k;
        }
        case Order::shuffleTracks:
        case Order::shuffleAlbums:
        {
            if (orderCount != count || madeFor != o || order.empty()) { shuffle (o, count, albums, current); history.clear(); }
            if (current >= 0) { history.push_back (current); if (history.size() > 10000) history.erase (history.begin()); }
            int at = placeOf (current);
            if (at < 0) return order.front();
            if (at + 1 < (int) order.size()) return order[(size_t) at + 1];
            // Each heard once: a new shuffle, not starting with what just played.
            shuffle (o, count, albums, -1);
            if (order.size() > 1 && order.front() == current) std::swap (order[0], order[1]);
            return order.front();
        }
    }
    return -1;
}

int Navigator::previous (Order o, int current, int count, const std::vector<std::string>& albums)
{
    if (count <= 0) return -1;
    switch (o)
    {
        case Order::normal: return current > 0 ? current - 1 : 0;
        case Order::repeatPlaylist:
        case Order::repeatTrack: return current > 0 ? current - 1 : count - 1;
        case Order::random: return current >= 0 ? current : 0;
        case Order::shuffleTracks:
        case Order::shuffleAlbums:
        {
            if (orderCount != count || madeFor != o || order.empty()) { shuffle (o, count, albums, current); history.clear(); }
            if (! history.empty()) { const int back = history.back(); history.pop_back(); if (back < count) return back; }
            int at = placeOf (current);
            return at > 0 ? order[(size_t) at - 1] : (current >= 0 ? current : order.front());
        }
    }
    return -1;
}

//==============================================================================
Playlists::Playlists() { lists.push_back ({ "Default", {}, {} }); }

std::string Playlists::uniqueName (const std::string& base) const
{
    auto taken = [this] (const std::string& n) { return std::any_of (lists.begin(), lists.end(), [&] (auto& p) { return p.name == n; }); };
    if (! taken (base)) return base;
    for (int k = 2;; ++k)
    {
        auto n = base + " (" + std::to_string (k) + ")";
        if (! taken (n)) return n;
    }
}

int Playlists::create (const std::string& name)
{
    lists.push_back ({ uniqueName (name.empty() ? "New Playlist" : name), {}, {} });
    return (int) lists.size() - 1;
}

void Playlists::removeList (int index)
{
    if (index < 0 || index >= (int) lists.size()) return;
    lists.erase (lists.begin() + index);
    if (lists.empty()) lists.push_back ({ "Default", {}, {} });
    if (playing == index) playing = -1;
    else if (playing > index) --playing;
    if (active >= (int) lists.size()) active = (int) lists.size() - 1;
    else if (active > index) --active;
}

std::vector<int> Playlists::moveItems (Playlist& p, std::vector<int> indices, int to)
{
    std::sort (indices.begin(), indices.end());
    indices.erase (std::unique (indices.begin(), indices.end()), indices.end());
    indices.erase (std::remove_if (indices.begin(), indices.end(), [&] (int k) { return k < 0 || k >= (int) p.items.size(); }), indices.end());
    std::vector<std::string> moving;
    int before = 0;
    for (int k : indices) { moving.push_back (p.items[(size_t) k]); if (k < to) ++before; }
    for (auto it = indices.rbegin(); it != indices.rend(); ++it) p.items.erase (p.items.begin() + *it);
    int at = std::clamp (to - before, 0, (int) p.items.size());
    p.items.insert (p.items.begin() + at, moving.begin(), moving.end());
    std::vector<int> now;
    for (int k = 0; k < (int) moving.size(); ++k) now.push_back (at + k);
    return now;
}

void Playlists::removeItems (Playlist& p, std::vector<int> indices)
{
    std::sort (indices.begin(), indices.end());
    indices.erase (std::unique (indices.begin(), indices.end()), indices.end());
    for (auto it = indices.rbegin(); it != indices.rend(); ++it)
        if (*it >= 0 && *it < (int) p.items.size()) p.items.erase (p.items.begin() + *it);
}

void Playlists::insertItems (Playlist& p, int at, const std::vector<std::string>& locations)
{
    at = std::clamp (at < 0 ? (int) p.items.size() : at, 0, (int) p.items.size());
    p.items.insert (p.items.begin() + at, locations.begin(), locations.end());
}

int Playlists::removeDuplicates (Playlist& p)
{
    std::set<std::string> seen;
    const auto before = p.items.size();
    p.items.erase (std::remove_if (p.items.begin(), p.items.end(), [&] (const std::string& l) { return ! seen.insert (l).second; }), p.items.end());
    return (int) (before - p.items.size());
}

std::string Playlists::toJson() const
{
    json j;
    j["active"] = active;
    json all = json::array();
    for (auto& p : lists)
    {
        json x;
        x["name"] = p.name;
        x["items"] = p.items;
        if (p.isAuto()) x["query"] = p.query;
        all.push_back (x);
    }
    j["lists"] = all;
    return j.dump();
}

bool Playlists::fromJson (const std::string& s)
{
    auto j = json::parse (s, nullptr, false);
    if (j.is_discarded() || ! j.is_object() || ! j.contains ("lists")) return false;
    lists.clear();
    for (auto& x : j["lists"])
    {
        Playlist p;
        p.name = x.value ("name", "Playlist");
        p.query = x.value ("query", "");
        if (x.contains ("items") && x["items"].is_array())
            for (auto& i : x["items"]) if (i.is_string()) p.items.push_back (i.get<std::string>());
        lists.push_back (std::move (p));
    }
    if (lists.empty()) lists.push_back ({ "Default", {}, {} });
    active = std::clamp (j.value ("active", 0), 0, (int) lists.size() - 1);
    playing = -1;
    return true;
}

//==============================================================================
std::string writeM3u (const std::vector<std::string>& locations, const std::vector<std::string>& titles, const std::vector<double>& lengths)
{
    std::ostringstream out;
    out << "#EXTM3U\n";
    for (size_t k = 0; k < locations.size(); ++k)
    {
        if (k < titles.size())
            out << "#EXTINF:" << (k < lengths.size() ? (long long) std::lround (lengths[k]) : -1) << "," << titles[k] << "\n";
        out << locations[k] << "\n";
    }
    return out.str();
}

namespace
{
std::string resolve (std::string line, const std::string& baseDir)
{
    line = trim (line);
    if (line.rfind ("file://", 0) == 0)
    {
        line = line.substr (7);
        // %20 and the like.
        std::string decoded;
        for (size_t k = 0; k < line.size(); ++k)
        {
            if (line[k] == '%' && k + 2 < line.size() && std::isxdigit ((unsigned char) line[k + 1]) && std::isxdigit ((unsigned char) line[k + 2]))
            {
                decoded += (char) std::stoi (line.substr (k + 1, 2), nullptr, 16);
                k += 2;
            }
            else decoded += line[k];
        }
        line = decoded;
    }
    if (line.empty()) return {};
    const bool absolute = line[0] == '/' || line[0] == '\\' || (line.size() > 2 && line[1] == ':') || location::isArchive (line);
    if (absolute || baseDir.empty()) return line;
    std::replace (line.begin(), line.end(), '\\', '/');
    return baseDir + "/" + line;
}

std::vector<std::string> lines (const std::string& text)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : text)
    {
        if (c == '\n' || c == '\r') { if (! cur.empty()) out.push_back (cur); cur.clear(); }
        else cur += c;
    }
    if (! cur.empty()) out.push_back (cur);
    // A byte order mark on the first.
    if (! out.empty() && out[0].rfind ("\xEF\xBB\xBF", 0) == 0) out[0] = out[0].substr (3);
    return out;
}
} // namespace

std::vector<std::string> readM3u (const std::string& text, const std::string& baseDir)
{
    std::vector<std::string> out;
    for (auto& l : lines (text))
    {
        auto t = trim (l);
        if (t.empty() || t[0] == '#') continue;
        auto r = resolve (t, baseDir);
        if (! r.empty()) out.push_back (r);
    }
    return out;
}

std::vector<std::string> readPls (const std::string& text, const std::string& baseDir)
{
    std::vector<std::pair<int, std::string>> files;
    for (auto& l : lines (text))
    {
        auto t = trim (l);
        if (lower (t.substr (0, 4)) != "file") continue;
        auto eq = t.find ('=');
        if (eq == std::string::npos) continue;
        files.emplace_back (std::atoi (t.substr (4, eq - 4).c_str()), resolve (t.substr (eq + 1), baseDir));
    }
    std::stable_sort (files.begin(), files.end(), [] (auto& a, auto& b) { return a.first < b.first; });
    std::vector<std::string> out;
    for (auto& f : files) if (! f.second.empty()) out.push_back (f.second);
    return out;
}

} // namespace kp
