#include "TitleFormat.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <mutex>
#include <set>
#include <unordered_map>

namespace kp::tf
{

//==============================================================================
// Fields

namespace
{
std::string tagJoined (const Track& t, const std::string& n) { return t.joined (n); }

std::string firstOf (const Track& t, std::initializer_list<const char*> names)
{
    for (auto* n : names)
        if (t.has (n)) return tagJoined (t, n);
    return {};
}

// "3/12" -> "3"; "03" stays.
std::string numberPart (const std::string& s)
{
    auto slash = s.find ('/');
    return trim (slash == std::string::npos ? s : s.substr (0, slash));
}

std::string totalPart (const std::string& s)
{
    auto slash = s.find ('/');
    return slash == std::string::npos ? std::string() : trim (s.substr (slash + 1));
}

std::string twoDigits (const std::string& n)
{
    if (n.empty()) return n;
    if (std::all_of (n.begin(), n.end(), [] (char c) { return std::isdigit ((unsigned char) c); }) && n.size() < 2)
        return std::string (2 - n.size(), '0') + n;
    return n;
}

std::string naturalSize (std::int64_t bytes)
{
    char buf[64];
    const double b = (double) bytes;
    if (b >= 1024.0 * 1024 * 1024) std::snprintf (buf, sizeof buf, "%.2f GB", b / (1024.0 * 1024 * 1024));
    else if (b >= 1024.0 * 1024) std::snprintf (buf, sizeof buf, "%.2f MB", b / (1024.0 * 1024));
    else if (b >= 1024.0) std::snprintf (buf, sizeof buf, "%.0f KB", b / 1024.0);
    else std::snprintf (buf, sizeof buf, "%lld B", (long long) bytes);
    return buf;
}

std::string withoutExtension (const std::string& name)
{
    auto dot = name.find_last_of ('.');
    return dot == std::string::npos || dot == 0 ? name : name.substr (0, dot);
}

std::string lastFolder (const std::string& dir)
{
    auto slash = dir.find_last_of ("/\\");
    return slash == std::string::npos ? dir : dir.substr (slash + 1);
}
} // namespace

bool TrackFields::field (std::string_view nameView, std::string& out) const
{
    const std::string name = lower (nameView);
    const auto& t = track;
    auto give = [&out] (std::string v) { out = std::move (v); return ! out.empty(); };

    if (name == "title") { if (t.has ("title")) return give (t.joined ("title")); return give (withoutExtension (location::fileName (t.location))); }
    if (name == "artist") return give (firstOf (t, { "artist", "album artist", "composer", "performer" }));
    if (name == "album artist") return give (firstOf (t, { "album artist", "albumartist", "artist", "composer", "performer" }));
    if (name == "track artist")
    {
        const auto a = firstOf (t, { "artist" });
        const auto aa = firstOf (t, { "album artist", "albumartist" });
        return give (aa.empty() || a == aa ? std::string() : a);
    }
    if (name == "tracknumber" || name == "track") return give (twoDigits (numberPart (t.first ("tracknumber"))));
    if (name == "totaltracks")
    {
        if (t.has ("totaltracks")) return give (t.first ("totaltracks"));
        if (t.has ("tracktotal")) return give (t.first ("tracktotal"));
        return give (totalPart (t.first ("tracknumber")));
    }
    if (name == "discnumber" || name == "disc") return give (numberPart (t.first ("discnumber")));
    if (name == "totaldiscs")
    {
        if (t.has ("totaldiscs")) return give (t.first ("totaldiscs"));
        if (t.has ("disctotal")) return give (t.first ("disctotal"));
        return give (totalPart (t.first ("discnumber")));
    }
    if (name == "year")
    {
        auto d = t.first ("date");
        if (d.empty()) d = t.first ("year");
        return give (d.size() >= 4 ? d.substr (0, 4) : d);
    }
    if (name == "date") { if (t.has ("date")) return give (t.joined ("date")); return give (t.joined ("year")); }
    if (name == "length") return t.length > 0 && give (formatTime (t.length));
    if (name == "length_seconds") return t.length > 0 && give (std::to_string ((long long) std::lround (t.length)));
    if (name == "length_samples") return t.length > 0 && give (std::to_string ((long long) std::llround (t.length * t.sampleRate)));
    if (name == "bitrate") return t.bitrate > 0 && give (std::to_string (t.bitrate));
    if (name == "samplerate") return t.sampleRate > 0 && give (std::to_string (t.sampleRate));
    if (name == "bitspersample") return t.bitsPerSample > 0 && give (std::to_string (t.bitsPerSample));
    if (name == "channels")
    {
        if (t.channels == 1) return give ("mono");
        if (t.channels == 2) return give ("stereo");
        return t.channels > 0 && give (std::to_string (t.channels) + "ch");
    }
    if (name == "codec") return give (t.codec);
    if (name == "filesize") return t.fileSize > 0 && give (std::to_string (t.fileSize));
    if (name == "filesize_natural") return t.fileSize > 0 && give (naturalSize (t.fileSize));
    if (name == "path") return give (t.location);
    if (name == "filename") return give (withoutExtension (location::fileName (t.location)));
    if (name == "filename_ext") return give (location::fileName (t.location));
    if (name == "ext") return give (location::extension (t.location));
    if (name == "directoryname") return give (lastFolder (location::directory (t.location)));
    if (name == "list_index") return listIndex > 0 && give (std::to_string (listIndex));
    if (name == "list_total") return listTotal > 0 && give (std::to_string (listTotal));
    if (name == "archive") { auto s = location::split (t.location); return s && give (location::fileName (s->first)); }
    if (name == "albumartist") return field ("album artist", out);
    return give (t.joined (name));
}

bool TrackFields::tag (std::string_view name, std::vector<std::string>& out) const
{
    auto it = track.tags.find (lower (name));
    if (it == track.tags.end() || it->second.empty()) return false;
    out = it->second;
    return true;
}

//==============================================================================
// Parsing

namespace
{
struct Parser
{
    std::string_view s;
    size_t i = 0;
    std::string error;

    // Reads until one of `stops` at this depth (or the end).
    Nodes parse (std::string_view stops)
    {
        Nodes out;
        std::string text;
        auto flush = [&] { if (! text.empty()) { out.push_back ({ Node::Kind::text, text, {} }); text.clear(); } };
        while (i < s.size())
        {
            const char c = s[i];
            if (stops.find (c) != std::string_view::npos) break;
            if (c == '\'')
            {
                ++i;
                if (i < s.size() && s[i] == '\'') { text += '\''; ++i; continue; }   // '' is one quote
                auto end = s.find ('\'', i);
                if (end == std::string_view::npos) { error = "A quote (') isn't closed."; end = s.size(); }
                text += s.substr (i, end - i);
                i = end < s.size() ? end + 1 : end;
                continue;
            }
            if (c == '%')
            {
                auto end = s.find ('%', i + 1);
                if (end == std::string_view::npos) { error = "A field (%) isn't closed."; text += s.substr (i); i = s.size(); break; }
                flush();
                out.push_back ({ Node::Kind::field, lower (s.substr (i + 1, end - i - 1)), {} });
                i = end + 1;
                continue;
            }
            if (c == '[')
            {
                flush();
                ++i;
                Node n { Node::Kind::optional, {}, {} };
                n.args.push_back (parse ("]"));
                if (i < s.size() && s[i] == ']') ++i;
                else if (error.empty()) error = "A [ isn't closed with ].";
                out.push_back (std::move (n));
                continue;
            }
            if (c == '$')
            {
                size_t j = i + 1;
                while (j < s.size() && (std::isalnum ((unsigned char) s[j]) || s[j] == '_')) ++j;
                if (j < s.size() && s[j] == '(' && j > i + 1)
                {
                    flush();
                    Node n { Node::Kind::function, lower (s.substr (i + 1, j - i - 1)), {} };
                    i = j + 1;
                    // $f() has no arguments; otherwise arguments split at commas.
                    if (i < s.size() && s[i] == ')') { ++i; out.push_back (std::move (n)); continue; }
                    for (;;)
                    {
                        n.args.push_back (parse (",)"));
                        if (i >= s.size()) { if (error.empty()) error = "$" + n.text + "( isn't closed with )."; break; }
                        if (s[i] == ')') { ++i; break; }
                        ++i;   // the comma
                    }
                    out.push_back (std::move (n));
                    continue;
                }
            }
            text += c;
            ++i;
        }
        flush();
        return out;
    }
};

//==============================================================================
// Evaluating

struct Value
{
    std::string s;
    bool truth = false;
};

long long toInt (const std::string& s)
{
    size_t k = 0;
    while (k < s.size() && std::isspace ((unsigned char) s[k])) ++k;
    bool neg = false;
    if (k < s.size() && (s[k] == '-' || s[k] == '+')) { neg = s[k] == '-'; ++k; }
    long long v = 0;
    while (k < s.size() && std::isdigit ((unsigned char) s[k])) v = v * 10 + (s[k++] - '0');
    return neg ? -v : v;
}

// UTF-8 aware lengths and cuts, so "Björk" is five characters.
std::vector<std::string> chars (const std::string& s)
{
    std::vector<std::string> out;
    for (size_t k = 0; k < s.size();)
    {
        const auto c = (unsigned char) s[k];
        size_t len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
        len = std::min (len, s.size() - k);
        out.push_back (s.substr (k, len));
        k += len;
    }
    return out;
}

std::string join (const std::vector<std::string>& c, size_t from, size_t to)
{
    std::string out;
    for (size_t k = from; k < std::min (to, c.size()); ++k) out += c[k];
    return out;
}

size_t charCount (const std::string& s) { return chars (s).size(); }

std::string capitalise (const std::string& s, bool lowerRest)
{
    std::string out = s;
    bool start = true;
    for (auto& ch : out)
    {
        const auto c = (unsigned char) ch;
        if (std::isalpha (c)) { ch = (char) (start ? std::toupper (c) : lowerRest ? std::tolower (c) : c); start = false; }
        else if (c < 0x80) start = std::isspace (c) || c == '(' || c == '[' || c == '"' || c == '-' || c == '/';
    }
    return out;
}

class Evaluator
{
public:
    explicit Evaluator (const Fields& f) : fields (f) {}

    Value run (const Nodes& nodes) const
    {
        Value out;
        for (auto& n : nodes)
        {
            auto v = one (n);
            out.s += v.s;
            out.truth = out.truth || v.truth;
        }
        return out;
    }

private:
    const Fields& fields;

    Value one (const Node& n) const
    {
        switch (n.kind)
        {
            case Node::Kind::text: return { n.text, false };
            case Node::Kind::field:
            {
                std::string v;
                const bool has = fields.field (n.text, v);
                if (! has) return { "?", false };
                return { v, true };
            }
            case Node::Kind::optional:
            {
                auto v = run (n.args.front());
                if (! v.truth) return {};
                return v;
            }
            case Node::Kind::function: return call (n.text, n.args);
        }
        return {};
    }

    Value arg (const std::vector<Nodes>& a, size_t k) const { return k < a.size() ? run (a[k]) : Value {}; }
    static Value boolean (bool b) { return { {}, b }; }
    static Value str (std::string s) { bool t = ! s.empty(); return { std::move (s), t }; }
    static Value num (long long v) { return { std::to_string (v), true }; }

    Value call (const std::string& f, const std::vector<Nodes>& a) const
    {
        const size_t n = a.size();
        // Conditions: only the branch chosen is evaluated.
        if (f == "if" && (n == 2 || n == 3)) { if (arg (a, 0).truth) return arg (a, 1); return n == 3 ? arg (a, 2) : Value {}; }
        if (f == "if2" && n == 2) { auto v = arg (a, 0); return v.truth ? v : arg (a, 1); }
        if (f == "if3" && n >= 2)
        {
            for (size_t k = 0; k + 1 < n; ++k) { auto v = arg (a, k); if (v.truth) return v; }
            return arg (a, n - 1);
        }
        if (f == "ifequal" && n == 4) return toInt (arg (a, 0).s) == toInt (arg (a, 1).s) ? arg (a, 2) : arg (a, 3);
        if (f == "ifgreater" && n == 4) return toInt (arg (a, 0).s) > toInt (arg (a, 1).s) ? arg (a, 2) : arg (a, 3);
        if (f == "iflonger" && n == 4) return (long long) charCount (arg (a, 0).s) > toInt (arg (a, 1).s) ? arg (a, 2) : arg (a, 3);
        if (f == "select" && n >= 2)
        {
            auto k = toInt (arg (a, 0).s);
            return k >= 1 && (size_t) k < n ? arg (a, (size_t) k) : Value {};
        }

        std::vector<Value> v;
        for (auto& x : a) v.push_back (run (x));
        auto s = [&] (size_t k) -> const std::string& { static const std::string none; return k < v.size() ? v[k].s : none; };
        auto i = [&] (size_t k) { return toInt (s (k)); };

        if (f == "and") { bool r = true; for (auto& x : v) r = r && x.truth; return boolean (r); }
        if (f == "or") { bool r = false; for (auto& x : v) r = r || x.truth; return boolean (r); }
        if (f == "not" && n == 1) return boolean (! v[0].truth);
        if (f == "xor") { bool r = false; for (auto& x : v) r = r != x.truth; return boolean (r); }
        if (f == "greater" && n == 2) return boolean (i (0) > i (1));
        if (f == "longer" && n == 2) return boolean (charCount (s (0)) > charCount (s (1)));
        if (f == "strcmp" && n == 2) return boolean (s (0) == s (1));
        if (f == "stricmp" && n == 2) return boolean (equalsIgnoringCase (s (0), s (1)));
        if (f == "strstr" && n == 2) { auto p = s (0).find (s (1)); return p == std::string::npos ? Value {} : num ((long long) charCount (s (0).substr (0, p)) + 1); }

        if (f == "add") { long long r = 0; for (size_t k = 0; k < n; ++k) r += i (k); return num (r); }
        if (f == "sub" && n >= 1) { long long r = i (0); for (size_t k = 1; k < n; ++k) r -= i (k); return num (r); }
        if (f == "mul") { long long r = 1; for (size_t k = 0; k < n; ++k) r *= i (k); return num (r); }
        if (f == "div" && n >= 1) { long long r = i (0); for (size_t k = 1; k < n; ++k) { auto d = i (k); if (d == 0) return {}; r /= d; } return num (r); }
        if (f == "mod" && n == 2) { auto d = i (1); return d == 0 ? Value {} : num (i (0) % d); }
        if (f == "min" && n >= 1) { auto r = i (0); for (size_t k = 1; k < n; ++k) r = std::min (r, i (k)); return num (r); }
        if (f == "max" && n >= 1) { auto r = i (0); for (size_t k = 1; k < n; ++k) r = std::max (r, i (k)); return num (r); }

        auto keep = [&] (std::string out) { return Value { std::move (out), n > 0 && v[0].truth }; };
        if (f == "upper" && n == 1) return keep (upper (s (0)));
        if (f == "lower" && n == 1) return keep (lower (s (0)));
        if (f == "caps" && n == 1) return keep (capitalise (s (0), true));
        if (f == "caps2" && n == 1) return keep (capitalise (s (0), false));
        if (f == "trim" && n == 1) return keep (trim (s (0)));
        if (f == "len" && n == 1) return num ((long long) charCount (s (0)));
        if ((f == "left" || f == "cut") && n == 2) { auto c = chars (s (0)); return keep (join (c, 0, (size_t) std::max (0LL, i (1)))); }
        if (f == "right" && n == 2)
        {
            auto c = chars (s (0));
            const auto k = (size_t) std::max (0LL, i (1));
            return keep (join (c, c.size() > k ? c.size() - k : 0, c.size()));
        }
        if (f == "substr" && n == 3)
        {
            auto c = chars (s (0));
            const auto from = (size_t) std::max (1LL, i (1)) - 1, to = (size_t) std::max (0LL, i (2));
            return keep (join (c, from, to));
        }
        if ((f == "pad" || f == "pad_right") && (n == 2 || n == 3))
        {
            const auto len = (size_t) std::max (0LL, i (1));
            const std::string fill = n == 3 && ! s (2).empty() ? chars (s (2)).front() : " ";
            const auto have = charCount (s (0));
            std::string padding;
            for (size_t k = have; k < len; ++k) padding += fill;
            return keep (f == "pad" ? s (0) + padding : padding + s (0));
        }
        if (f == "padcut" && n == 2)
        {
            auto c = chars (s (0));
            const auto len = (size_t) std::max (0LL, i (1));
            auto out = join (c, 0, len);
            for (size_t k = c.size(); k < len; ++k) out += ' ';
            return keep (out);
        }
        if (f == "num" && n == 2)
        {
            auto out = std::to_string (std::llabs (i (0)));
            const auto len = (size_t) std::max (0LL, i (1));
            if (out.size() < len) out = std::string (len - out.size(), '0') + out;
            if (i (0) < 0) out = "-" + out;
            return keep (out);
        }
        if (f == "replace" && n >= 3 && n % 2 == 1)
        {
            std::string out = s (0);
            for (size_t k = 1; k + 1 < n; k += 2)
            {
                const auto& from = s (k);
                const auto& to = s (k + 1);
                if (from.empty()) continue;
                std::string next;
                size_t p = 0;
                for (;;)
                {
                    auto q = out.find (from, p);
                    if (q == std::string::npos) { next += out.substr (p); break; }
                    next += out.substr (p, q - p) + to;
                    p = q + from.size();
                }
                out = next;
            }
            return keep (out);
        }
        if (f == "repeat" && n == 2) { std::string out; for (long long k = 0; k < std::min (i (1), 1000LL); ++k) out += s (0); return keep (out); }
        if (f == "insert" && n == 3)
        {
            auto c = chars (s (0));
            const auto at = std::min ((size_t) std::max (0LL, i (2)), c.size());
            return keep (join (c, 0, at) + s (1) + join (c, at, c.size()));
        }
        if (f == "stripprefix" && n >= 1)
        {
            std::vector<std::string> prefixes;
            for (size_t k = 1; k < n; ++k) prefixes.push_back (s (k));
            if (n == 1) prefixes = { "A", "The" };
            for (auto& p : prefixes)
                if (s (0).size() > p.size() + 1 && equalsIgnoringCase (s (0).substr (0, p.size()), p) && s (0)[p.size()] == ' ')
                    return keep (s (0).substr (p.size() + 1));
            return keep (s (0));
        }
        if (f == "abbr" && n >= 1)
        {
            std::string out;
            bool start = true;
            for (char ch : s (0))
            {
                if (std::isspace ((unsigned char) ch)) { start = true; continue; }
                if (start && (std::isalnum ((unsigned char) ch) || (unsigned char) ch >= 0x80)) out += ch;
                start = false;
            }
            return keep (out);
        }
        if (f == "directory" && (n == 1 || n == 2))
        {
            std::string p = s (0);
            long long up = n == 2 ? std::max (1LL, i (1)) : 1;
            for (long long k = 0; k < up; ++k)
            {
                auto slash = p.find_last_of ("/\\");
                if (slash == std::string::npos) return {};
                p = p.substr (0, slash);
            }
            auto slash = p.find_last_of ("/\\");
            return keep (slash == std::string::npos ? p : p.substr (slash + 1));
        }
        if (f == "directory_path" && n == 1) { auto slash = s (0).find_last_of ("/\\"); return keep (slash == std::string::npos ? std::string() : s (0).substr (0, slash)); }
        if (f == "ext" && n == 1) return keep (location::extension (s (0)));
        if (f == "filename" && n == 1) { auto name = location::fileName (s (0)); auto dot = name.find_last_of ('.'); return keep (dot == std::string::npos ? name : name.substr (0, dot)); }
        if (f == "crlf" && n == 0) return { "\n", false };
        if (f == "tab" && n <= 1) { std::string out ((size_t) std::max (1LL, n == 1 ? i (0) : 1), '\t'); return { out, false }; }
        if (f == "char" && n == 1)
        {
            const auto c = (unsigned long) std::max (0LL, i (0));
            std::string out;
            if (c < 0x80) out += (char) c;
            else if (c < 0x800) { out += (char) (0xC0 | (c >> 6)); out += (char) (0x80 | (c & 0x3F)); }
            else if (c < 0x10000) { out += (char) (0xE0 | (c >> 12)); out += (char) (0x80 | ((c >> 6) & 0x3F)); out += (char) (0x80 | (c & 0x3F)); }
            return { out, false };
        }
        if (f == "meta" && (n == 1 || n == 2))
        {
            std::vector<std::string> vals;
            if (! fields.tag (s (0), vals)) return {};
            if (n == 2) { auto k = i (1); return k >= 0 && (size_t) k < vals.size() ? str (vals[(size_t) k]) : Value {}; }
            std::string out;
            for (auto& x : vals) { if (! out.empty()) out += ", "; out += x; }
            return str (out);
        }
        if (f == "meta_sep" && n >= 2)
        {
            std::vector<std::string> vals;
            if (! fields.tag (s (0), vals)) return {};
            std::string out;
            for (size_t k = 0; k < vals.size(); ++k)
            {
                if (k > 0) out += (n == 3 && k + 1 == vals.size()) ? s (2) : s (1);
                out += vals[k];
            }
            return str (out);
        }
        if (f == "meta_num" && n == 1) { std::vector<std::string> vals; return num (fields.tag (s (0), vals) ? (long long) vals.size() : 0); }
        if (f == "meta_test") { std::vector<std::string> vals; for (size_t k = 0; k < n; ++k) if (! fields.tag (s (k), vals)) return {}; return boolean (true); }
        if (f == "year" && n == 1) return keep (s (0).size() >= 4 ? s (0).substr (0, 4) : s (0));
        return { "[UNKNOWN FUNCTION]", false };
    }
};
} // namespace

const std::vector<std::string>& functionNames()
{
    static const std::vector<std::string> names {
        "if", "if2", "if3", "ifequal", "ifgreater", "iflonger", "select", "and", "or", "not", "xor", "greater", "longer",
        "strcmp", "stricmp", "strstr", "add", "sub", "mul", "div", "mod", "min", "max", "upper", "lower", "caps", "caps2",
        "trim", "len", "left", "cut", "right", "substr", "pad", "pad_right", "padcut", "num", "replace", "repeat", "insert",
        "stripprefix", "abbr", "directory", "directory_path", "ext", "filename", "crlf", "tab", "char", "meta", "meta_sep",
        "meta_num", "meta_test", "year",
    };
    return names;
}

namespace
{
void checkFunctions (const Nodes& nodes, std::string& err)
{
    static const std::set<std::string> known (functionNames().begin(), functionNames().end());
    for (auto& n : nodes)
    {
        if (n.kind == Node::Kind::function && ! known.count (n.text) && err.empty()) err = "There's no function $" + n.text + "().";
        for (auto& a : n.args) checkFunctions (a, err);
    }
}
} // namespace

Script::Script (std::string_view source) : src (source)
{
    Parser p { source, 0, {} };
    nodes = p.parse ("");
    err = p.error;
    if (err.empty()) checkFunctions (nodes, err);
}

std::pair<std::string, bool> Script::evalWithTruth (const Fields& f) const
{
    auto v = Evaluator (f).run (nodes);
    return { v.s, v.truth };
}

std::string Script::eval (const Fields& f) const { return Evaluator (f).run (nodes).s; }

std::shared_ptr<const Script> compiled (const std::string& pattern)
{
    static std::mutex lock;
    static std::unordered_map<std::string, std::shared_ptr<const Script>> cache;
    std::lock_guard<std::mutex> g (lock);
    auto it = cache.find (pattern);
    if (it != cache.end()) return it->second;
    if (cache.size() > 512) cache.clear();
    auto s = std::make_shared<const Script> (pattern);
    cache.emplace (pattern, s);
    return s;
}

std::string format (const std::string& pattern, const Track& t, int index, int total)
{
    return compiled (pattern)->eval (TrackFields (t, index, total));
}

std::string format (const std::string& pattern, const Fields& f) { return compiled (pattern)->eval (f); }

std::string safeFileName (std::string_view in)
{
    std::string out;
    for (char c : in)
    {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || (unsigned char) c < 32)
            out += '_';
        else out += c;
    }
    // No name of dots alone, none ending in a space or a dot (Windows' rule, and a Finder surprise).
    while (! out.empty() && (out.back() == ' ' || out.back() == '.')) out.pop_back();
    while (! out.empty() && out.front() == ' ') out.erase (out.begin());
    return out;
}

namespace
{
class SafeFields : public Fields
{
public:
    explicit SafeFields (const Fields& f) : inner (f) {}
    bool field (std::string_view n, std::string& out) const override
    {
        // %path% and %directoryname% are wanted whole; everything else is one name.
        const bool ok = inner.field (n, out);
        if (n != "path") out = safeFileName (out);
        return ok && ! out.empty();
    }
    bool tag (std::string_view n, std::vector<std::string>& out) const override
    {
        if (! inner.tag (n, out)) return false;
        for (auto& v : out) v = safeFileName (v);
        return true;
    }

private:
    const Fields& inner;
};
} // namespace

std::string formatPath (const std::string& pattern, const Track& t)
{
    TrackFields f (t);
    SafeFields safe (f);
    auto out = compiled (pattern)->eval (safe);
    // Each folder in the result made safe too (a pattern's own text may hold a ":").
    std::string clean, part;
    for (size_t k = 0; k <= out.size(); ++k)
    {
        if (k == out.size() || out[k] == '/' || out[k] == '\\')
        {
            auto p = trim (part);
            if (! p.empty() && p != "." && p != "..") { if (! clean.empty()) clean += '/'; clean += safeFileName (p); }
            part.clear();
        }
        else part += out[k];
    }
    return clean;
}

} // namespace kp::tf
