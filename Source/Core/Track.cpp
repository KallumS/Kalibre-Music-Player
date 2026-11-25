#include "Track.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace kp
{

std::string Track::first (const std::string& name) const
{
    auto it = tags.find (name);
    return it == tags.end() || it->second.empty() ? std::string() : it->second.front();
}

std::string Track::joined (const std::string& name) const
{
    auto it = tags.find (name);
    if (it == tags.end()) return {};
    std::string out;
    for (auto& v : it->second)
    {
        if (! out.empty()) out += ", ";
        out += v;
    }
    return out;
}

bool Track::has (const std::string& name) const
{
    auto it = tags.find (name);
    return it != tags.end() && ! it->second.empty();
}

void Track::set (const std::string& name, const std::string& value)
{
    if (value.empty()) tags.erase (name);
    else tags[name] = { value };
}

std::string lower (std::string_view s)
{
    std::string out (s);
    for (auto& c : out) c = (char) std::tolower ((unsigned char) c);
    return out;
}

std::string upper (std::string_view s)
{
    std::string out (s);
    for (auto& c : out) c = (char) std::toupper ((unsigned char) c);
    return out;
}

std::string trim (std::string_view s)
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace ((unsigned char) s[a])) ++a;
    while (b > a && std::isspace ((unsigned char) s[b - 1])) --b;
    return std::string (s.substr (a, b - a));
}

bool containsIgnoringCase (std::string_view h, std::string_view n)
{
    if (n.empty()) return true;
    auto it = std::search (h.begin(), h.end(), n.begin(), n.end(),
                           [] (char a, char b) { return std::tolower ((unsigned char) a) == std::tolower ((unsigned char) b); });
    return it != h.end();
}

bool equalsIgnoringCase (std::string_view a, std::string_view b)
{
    return a.size() == b.size()
        && std::equal (a.begin(), a.end(), b.begin(),
                       [] (char x, char y) { return std::tolower ((unsigned char) x) == std::tolower ((unsigned char) y); });
}

namespace location
{
bool isArchive (std::string_view l) { return l.substr (0, archivePrefix.size()) == archivePrefix; }

std::string archive (const std::string& archivePath, const std::string& entry)
{
    return std::string (archivePrefix) + archivePath + "|" + entry;
}

std::optional<std::pair<std::string, std::string>> split (std::string_view l)
{
    if (! isArchive (l)) return std::nullopt;
    auto rest = l.substr (archivePrefix.size());
    // The archive's own name can't hold a "|" we made; its entry might, so the first one splits.
    auto bar = rest.find ('|');
    if (bar == std::string_view::npos) return std::nullopt;
    return std::make_pair (std::string (rest.substr (0, bar)), std::string (rest.substr (bar + 1)));
}

std::string container (std::string_view l)
{
    if (auto s = split (l)) return s->first;
    return std::string (l);
}

static std::string_view lastPart (std::string_view p)
{
    auto slash = p.find_last_of ("/\\");
    return slash == std::string_view::npos ? p : p.substr (slash + 1);
}

std::string fileName (std::string_view l)
{
    // A CD track ("cdda:D:|3") reads as the WAV it's made into.
    if (l.substr (0, 5) == "cdda:")
    {
        const auto bar = l.find_last_of ('|');
        char buf[32];
        std::snprintf (buf, sizeof buf, "Track %02d.wav", bar == std::string_view::npos ? 0 : std::atoi (std::string (l.substr (bar + 1)).c_str()));
        return buf;
    }
    if (auto s = split (l)) return std::string (lastPart (s->second));
    return std::string (lastPart (l));
}

std::string extension (std::string_view l)
{
    auto name = fileName (l);
    auto dot = name.find_last_of ('.');
    return dot == std::string::npos || dot == 0 ? std::string() : lower (name.substr (dot + 1));
}

std::string directory (std::string_view l)
{
    std::string full = std::string (l);
    if (auto s = split (l)) full = s->first + "/" + s->second;
    auto slash = full.find_last_of ("/\\");
    return slash == std::string::npos ? std::string() : full.substr (0, slash);
}
} // namespace location

const std::vector<std::string>& audioExtensions()
{
    static const std::vector<std::string> e {
        "mp1", "mp2", "mp3", "mpa", "mpc", "mp+", "mpp", "aac", "m4a", "m4b", "mp4", "alac", "wma", "asf", "ogg", "oga",
        "flac", "fla", "wv", "wav", "wave", "w64", "aif", "aiff", "aifc", "au", "snd", "spx", "opus", "ape", "tta", "tak",
        "mka", "webm", "caf", "dsf", "dff", "ac3", "eac3", "dts", "mlp", "thd", "amr", "voc",
    };
    return e;
}

bool isAudioExtension (std::string_view ext)
{
    auto e = lower (ext);
    auto& all = audioExtensions();
    return std::find (all.begin(), all.end(), e) != all.end();
}

const std::vector<std::string>& archiveExtensions()
{
    static const std::vector<std::string> e { "zip", "gz", "gzip", "rar", "7z", "tar", "tgz" };
    return e;
}

bool isArchiveExtension (std::string_view s)
{
    auto l = lower (s);
    auto dot = l.find_last_of ('.');
    auto ext = dot == std::string::npos ? l : l.substr (dot + 1);
    auto& all = archiveExtensions();
    return std::find (all.begin(), all.end(), ext) != all.end();
}

std::string formatTime (double seconds)
{
    if (! std::isfinite (seconds) || seconds < 0) seconds = 0;
    const auto total = (long long) std::floor (seconds);
    const auto h = total / 3600, m = (total / 60) % 60, s = total % 60;
    char buf[32];
    if (h > 0) std::snprintf (buf, sizeof buf, "%lld:%02lld:%02lld", h, m, s);
    else std::snprintf (buf, sizeof buf, "%lld:%02lld", m, s);
    return buf;
}

} // namespace kp
