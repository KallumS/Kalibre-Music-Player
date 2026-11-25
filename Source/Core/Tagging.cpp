#include "Tagging.h"

#include <algorithm>
#include <set>

namespace kp::tagging
{

namespace
{
struct Piece
{
    bool isField = false;
    std::string text;   // the field's name, or literal text
};

std::vector<Piece> pieces (const std::string& pattern)
{
    std::vector<Piece> out;
    std::string lit;
    for (size_t i = 0; i < pattern.size();)
    {
        if (pattern[i] == '%')
        {
            auto end = pattern.find ('%', i + 1);
            if (end == std::string::npos) { lit += pattern.substr (i); break; }
            if (! lit.empty()) { out.push_back ({ false, lit }); lit.clear(); }
            out.push_back ({ true, lower (pattern.substr (i + 1, end - i - 1)) });
            i = end + 1;
        }
        else lit += pattern[i++];
    }
    if (! lit.empty()) out.push_back ({ false, lit });
    return out;
}

// Matches pieces against text from piece p, char t; fields take as little as they can (backtracking).
bool match (const std::vector<Piece>& ps, size_t p, const std::string& text, size_t t, Tags& out)
{
    if (p == ps.size()) return t == text.size();
    const auto& piece = ps[p];
    if (! piece.isField)
    {
        if (text.compare (t, piece.text.size(), piece.text) != 0)
        {
            // Literal text compared ignoring case.
            if (t + piece.text.size() > text.size() || ! equalsIgnoringCase (text.substr (t, piece.text.size()), piece.text)) return false;
        }
        return match (ps, p + 1, text, t + piece.text.size(), out);
    }
    // The last piece takes the rest; otherwise try each length from shortest.
    const bool last = p + 1 == ps.size();
    for (size_t end = last ? text.size() : t + 1; end <= text.size(); ++end)
    {
        auto value = text.substr (t, end - t);
        if (value.find ('/') != std::string::npos) break;   // a field never spans folders
        Tags attempt = out;
        if (piece.text != "dummy" && ! piece.text.empty()) attempt[piece.text] = { trim (value) };
        if (match (ps, p + 1, text, end, attempt)) { out = std::move (attempt); return true; }
    }
    return false;
}
} // namespace

std::optional<Tags> guessFromPath (const std::string& pattern, const std::string& locationIn)
{
    std::string path = locationIn;
    if (auto s = location::split (locationIn)) path = s->first + "/" + s->second;
    std::replace (path.begin(), path.end(), '\\', '/');
    // Without the extension.
    auto slash = path.find_last_of ('/');
    auto dot = path.find_last_of ('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) path = path.substr (0, dot);
    // As many of the path's last parts as the pattern has.
    const auto parts = (size_t) std::count (pattern.begin(), pattern.end(), '/') + 1;
    size_t start = path.size();
    for (size_t k = 0; k < parts; ++k)
    {
        auto s = start == 0 ? std::string::npos : path.find_last_of ('/', start - 1);
        if (s == std::string::npos) { start = 0; break; }
        start = s;
    }
    std::string tail = start == 0 ? path : path.substr (start + 1);
    if (! tail.empty() && tail[0] == '/') tail = tail.substr (1);
    Tags out;
    if (! match (pieces (pattern), 0, tail, 0, out)) return std::nullopt;
    for (auto it = out.begin(); it != out.end();)
        it = it->second.empty() || it->second.front().empty() ? out.erase (it) : std::next (it);
    return out;
}

const std::vector<std::string>& standardFields()
{
    static const std::vector<std::string> f { "artist", "title", "album", "date", "genre", "composer", "performer", "album artist",
                                              "tracknumber", "totaltracks", "discnumber", "totaldiscs", "comment" };
    return f;
}

std::vector<std::string> splitValues (const std::string& s)
{
    std::vector<std::string> out;
    size_t start = 0;
    for (;;)
    {
        auto at = s.find ("; ", start);
        auto v = trim (s.substr (start, at == std::string::npos ? std::string::npos : at - start));
        if (! v.empty()) out.push_back (v);
        if (at == std::string::npos) break;
        start = at + 2;
    }
    return out;
}

static std::string joinValues (const std::vector<std::string>& v)
{
    std::string out;
    for (auto& x : v) { if (! out.empty()) out += "; "; out += x; }
    return out;
}

std::vector<Row> rowsFor (const std::vector<const Track*>& tracks)
{
    std::vector<std::string> names = standardFields();
    std::set<std::string> seen (names.begin(), names.end());
    for (auto* t : tracks)
        for (auto& [k, v] : t->tags)
            if (seen.insert (k).second) names.push_back (k);
    std::vector<Row> rows;
    for (auto& n : names)
    {
        Row r { n, {}, false };
        bool first = true;
        for (auto* t : tracks)
        {
            auto it = t->tags.find (n);
            const auto v = it == t->tags.end() ? std::string() : joinValues (it->second);
            if (first) { r.value = v; first = false; }
            else if (v != r.value) { r.differs = true; }
        }
        if (r.differs) r.value = multipleValues();
        rows.push_back (r);
    }
    return rows;
}

void apply (Tags& tags, const std::vector<Change>& changes)
{
    for (auto& c : changes)
    {
        const auto field = lower (trim (c.field));
        if (! c.renamedFrom.empty() && lower (c.renamedFrom) != field)
        {
            auto it = tags.find (lower (c.renamedFrom));
            if (it != tags.end())
            {
                auto values = it->second;
                tags.erase (it);
                if (! field.empty() && c.value == multipleValues()) tags[field] = values;
            }
        }
        if (field.empty() || c.value == multipleValues()) continue;
        auto values = splitValues (c.value);
        if (values.empty()) tags.erase (field);
        else tags[field] = values;
    }
}

void numberTracks (std::vector<Tags*>& tracks, bool withTotal)
{
    const int n = (int) tracks.size();
    for (int k = 0; k < n; ++k)
    {
        (*tracks[(size_t) k])["tracknumber"] = { std::to_string (k + 1) };
        if (withTotal) (*tracks[(size_t) k])["totaltracks"] = { std::to_string (n) };
    }
}

} // namespace kp::tagging
