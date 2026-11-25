#include "Library.h"
#include "TitleFormat.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <map>

namespace kp
{
using json = nlohmann::json;

const Track* Library::find (const std::string& l) const
{
    auto it = byLocation.find (l);
    return it == byLocation.end() ? nullptr : &all[it->second];
}

void Library::put (Track t)
{
    ++rev;
    auto it = byLocation.find (t.location);
    if (it != byLocation.end())
    {
        auto& old = all[it->second];
        if (t.added == 0) t.added = old.added;
        old = std::move (t);
        return;
    }
    byLocation[t.location] = all.size();
    all.push_back (std::move (t));
}

bool Library::remove (const std::string& l)
{
    auto it = byLocation.find (l);
    if (it == byLocation.end()) return false;
    all.erase (all.begin() + (std::ptrdiff_t) it->second);
    reindex();
    ++rev;
    return true;
}

int Library::removeUnder (const std::string& folderIn)
{
    std::string folder = folderIn;
    while (! folder.empty() && (folder.back() == '/' || folder.back() == '\\')) folder.pop_back();
    const auto before = all.size();
    all.erase (std::remove_if (all.begin(), all.end(), [&] (const Track& t)
    {
        const auto c = location::container (t.location);
        return c.size() > folder.size() && c.compare (0, folder.size(), folder) == 0 && (c[folder.size()] == '/' || c[folder.size()] == '\\');
    }), all.end());
    const int removed = (int) (before - all.size());
    if (removed > 0) { reindex(); ++rev; }
    return removed;
}

bool Library::move (const std::string& from, const std::string& to)
{
    auto it = byLocation.find (from);
    if (it == byLocation.end() || from == to) return false;
    const auto index = it->second;
    if (auto other = byLocation.find (to); other != byLocation.end())
    {
        all.erase (all.begin() + (std::ptrdiff_t) other->second);
        reindex();
        return move (from, to);
    }
    all[index].location = to;
    byLocation.erase (it);
    byLocation[to] = index;
    ++rev;
    return true;
}

void Library::reindex()
{
    byLocation.clear();
    for (size_t k = 0; k < all.size(); ++k) byLocation[all[k].location] = k;
}

const std::string& Library::defaultSortPattern()
{
    static const std::string p = "%album artist%|%date%|%album%|%discnumber%|%tracknumber%|%title%";
    return p;
}

std::vector<size_t> Library::search (const Query& q, const std::string& defaultSort) const
{
    std::vector<size_t> out;
    for (size_t k = 0; k < all.size(); ++k)
        if (q.matches (all[k])) out.push_back (k);
    const auto& pattern = q.sortPattern().empty() ? defaultSort : q.sortPattern();
    if (! pattern.empty()) sortByPattern (out, all, pattern, q.sortDescending());
    return out;
}

//==============================================================================
namespace
{
json toJ (const Track& t)
{
    json j;
    j["l"] = t.location;
    json tags = json::object();
    for (auto& [k, v] : t.tags) tags[k] = v;
    j["t"] = tags;
    j["len"] = t.length;
    j["sr"] = t.sampleRate;
    j["ch"] = t.channels;
    if (t.bitsPerSample) j["bps"] = t.bitsPerSample;
    if (t.bitrate) j["br"] = t.bitrate;
    j["c"] = t.codec;
    j["size"] = t.fileSize;
    j["mod"] = t.modified;
    j["add"] = t.added;
    return j;
}

bool fromJ (const json& j, Track& t)
{
    if (! j.is_object() || ! j.contains ("l")) return false;
    t.location = j.value ("l", "");
    t.tags.clear();
    if (auto it = j.find ("t"); it != j.end() && it->is_object())
        for (auto& [k, v] : it->items())
            if (v.is_array())
                for (auto& x : v) if (x.is_string()) t.tags[k].push_back (x.get<std::string>());
    t.length = j.value ("len", 0.0);
    t.sampleRate = j.value ("sr", 0);
    t.channels = j.value ("ch", 0);
    t.bitsPerSample = j.value ("bps", 0);
    t.bitrate = j.value ("br", 0);
    t.codec = j.value ("c", "");
    t.fileSize = j.value ("size", (std::int64_t) 0);
    t.modified = j.value ("mod", (std::int64_t) 0);
    t.added = j.value ("add", (std::int64_t) 0);
    return ! t.location.empty();
}
} // namespace

std::string trackToJson (const Track& t) { return toJ (t).dump(); }

bool trackFromJson (const std::string& s, Track& t)
{
    auto j = json::parse (s, nullptr, false);
    return ! j.is_discarded() && fromJ (j, t);
}

std::string Library::toJson() const
{
    json j;
    j["version"] = 1;
    j["folders"] = folders;
    json list = json::array();
    for (auto& t : all) list.push_back (toJ (t));
    j["tracks"] = std::move (list);
    return j.dump();
}

bool Library::fromJson (const std::string& s)
{
    auto j = json::parse (s, nullptr, false);
    if (j.is_discarded() || ! j.is_object()) return false;
    folders.clear();
    if (auto it = j.find ("folders"); it != j.end() && it->is_array())
        for (auto& f : *it) if (f.is_string()) folders.push_back (f.get<std::string>());
    all.clear();
    if (auto it = j.find ("tracks"); it != j.end() && it->is_array())
        for (auto& x : *it)
        {
            Track t;
            if (fromJ (x, t)) all.push_back (std::move (t));
        }
    reindex();
    ++rev;
    return true;
}

//==============================================================================
std::vector<std::string> fieldValues (const Track& t, const std::string& field)
{
    if (field.find ('%') != std::string::npos || field.find ('$') != std::string::npos)
    {
        auto [v, truth] = tf::compiled (field)->evalWithTruth (tf::TrackFields (t));
        if (! truth || v.empty()) return {};
        return { v };
    }
    const auto f = lower (field);
    // Fields made from others (album artist falls back to artist, year from the date) through title formatting.
    if (f == "album artist" || f == "year" || f == "artist" || f == "tracknumber" || f == "discnumber" || f == "codec"
        || f == "directoryname" || f == "samplerate" || f == "bitspersample" || f == "channels")
    {
        if (f == "artist" && t.has ("artist")) return t.tags.at ("artist");
        std::string v;
        if (tf::TrackFields (t).field (f, v)) return { v };
        return {};
    }
    auto it = t.tags.find (f);
    if (it == t.tags.end()) return {};
    return it->second;
}

std::vector<Facet> facets (const std::vector<const Track*>& tracks, const std::string& field)
{
    std::map<std::string, std::pair<std::string, int>> counts;   // lower-case key -> first spelling, count
    int none = 0;
    for (auto* t : tracks)
    {
        auto values = fieldValues (*t, field);
        if (values.empty()) { ++none; continue; }
        std::sort (values.begin(), values.end());
        values.erase (std::unique (values.begin(), values.end()), values.end());
        for (auto& v : values)
        {
            auto& c = counts[lower (v)];
            if (c.first.empty()) c.first = v;
            ++c.second;
        }
    }
    std::vector<Facet> out;
    for (auto& [k, c] : counts) out.push_back ({ c.first, c.second });
    std::sort (out.begin(), out.end(), [] (const Facet& a, const Facet& b) { return naturalCompare (a.value, b.value) < 0; });
    if (none > 0) out.insert (out.begin(), Facet { noValue(), none });
    return out;
}

bool hasFacetValue (const Track& t, const std::string& field, const std::string& value)
{
    auto values = fieldValues (t, field);
    if (value == noValue()) return values.empty();
    for (auto& v : values) if (equalsIgnoringCase (v, value)) return true;
    return false;
}

} // namespace kp
