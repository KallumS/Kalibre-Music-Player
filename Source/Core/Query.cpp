#include "Query.h"
#include "TitleFormat.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <set>

namespace kp
{

struct Query::Node
{
    enum class Kind { all, words, has, is, missing, present, greater, less, equal, after, before, during, notOp, andOp, orOp } kind = Kind::all;
    std::string field;                    // "artist", "%artist% - %title%", "*"
    std::string value;
    std::vector<std::string> words;
    double number = 0;                    // GREATER/LESS/EQUAL; DURING's span in ms
    std::int64_t now = 0;
    std::vector<std::unique_ptr<Node>> kids;
};

namespace
{
using Node = Query::Node;
using Kind = Node::Kind;

struct Token
{
    std::string text;
    bool quoted = false;
};

std::vector<Token> tokenise (const std::string& s)
{
    std::vector<Token> out;
    size_t i = 0;
    while (i < s.size())
    {
        const char c = s[i];
        if (std::isspace ((unsigned char) c)) { ++i; continue; }
        if (c == '(' || c == ')') { out.push_back ({ std::string (1, c), false }); ++i; continue; }
        if (c == '"')
        {
            auto end = s.find ('"', i + 1);
            if (end == std::string::npos) end = s.size();
            out.push_back ({ s.substr (i + 1, end - i - 1), true });
            i = end < s.size() ? end + 1 : end;
            continue;
        }
        // A %pattern% may hold spaces: up to its closing %.
        size_t j = i;
        bool inField = false;
        while (j < s.size())
        {
            if (s[j] == '%') inField = ! inField;
            else if (! inField && (std::isspace ((unsigned char) s[j]) || s[j] == '(' || s[j] == ')' || s[j] == '"')) break;
            ++j;
        }
        out.push_back ({ s.substr (i, j - i), false });
        i = j;
    }
    return out;
}

const std::set<std::string>& operators()
{
    static const std::set<std::string> o { "HAS", "IS", "MISSING", "PRESENT", "GREATER", "LESS", "EQUAL", "AFTER", "BEFORE", "DURING" };
    return o;
}

bool isConnective (const Token& t) { return ! t.quoted && (t.text == "AND" || t.text == "OR" || t.text == ")"); }
bool isOperator (const Token& t) { return ! t.quoted && operators().count (t.text); }

struct Parser
{
    std::vector<Token> t;
    size_t i = 0;
    std::int64_t now = 0;
    std::string error;

    bool at (const char* word) const { return i < t.size() && ! t[i].quoted && t[i].text == word; }

    std::unique_ptr<Node> orExpr()
    {
        auto left = andExpr();
        while (left && at ("OR"))
        {
            ++i;
            auto right = andExpr();
            if (! right) return nullptr;
            auto n = std::make_unique<Node>();
            n->kind = Kind::orOp;
            n->kids.push_back (std::move (left));
            n->kids.push_back (std::move (right));
            left = std::move (n);
        }
        return left;
    }

    std::unique_ptr<Node> andExpr()
    {
        auto left = notExpr();
        while (left && i < t.size() && ! at ("OR") && ! at (")") && ! at ("SORT"))
        {
            if (at ("AND")) ++i;
            auto right = notExpr();
            if (! right) return nullptr;
            auto n = std::make_unique<Node>();
            n->kind = Kind::andOp;
            n->kids.push_back (std::move (left));
            n->kids.push_back (std::move (right));
            left = std::move (n);
        }
        return left;
    }

    std::unique_ptr<Node> notExpr()
    {
        if (at ("NOT"))
        {
            ++i;
            auto inner = notExpr();
            if (! inner) return nullptr;
            auto n = std::make_unique<Node>();
            n->kind = Kind::notOp;
            n->kids.push_back (std::move (inner));
            return n;
        }
        return primary();
    }

    std::unique_ptr<Node> primary()
    {
        if (i >= t.size()) { error = "Something's missing at the end."; return nullptr; }
        if (at ("("))
        {
            ++i;
            auto inner = orExpr();
            if (! inner) return nullptr;
            if (! at (")")) { error = "A ( isn't closed with )."; return nullptr; }
            ++i;
            return inner;
        }
        if (at ("ALL")) { ++i; return std::make_unique<Node>(); }

        // Words up to an operator (then they're a field), or up to a connective (then they're words to find).
        std::vector<Token> words;
        while (i < t.size() && ! isOperator (t[i]) && ! isConnective (t[i]) && ! at ("(") && ! at ("NOT") && ! at ("SORT"))
            words.push_back (t[i++]);
        if (i < t.size() && isOperator (t[i]))
        {
            if (words.empty()) { error = t[i].text + " needs a field before it."; return nullptr; }
            auto n = std::make_unique<Node>();
            std::string field;
            for (auto& w : words) { if (! field.empty()) field += ' '; field += w.text; }
            n->field = field;
            const auto op = t[i++].text;
            if (op == "MISSING") { n->kind = Kind::missing; return n; }
            if (op == "PRESENT") { n->kind = Kind::present; return n; }
            std::string value;
            while (i < t.size() && ! isConnective (t[i]) && ! at ("SORT") && ! (t[i].text == "(" && ! t[i].quoted))
            {
                if (! value.empty()) value += ' ';
                value += t[i++].text;
            }
            if (value.empty()) { error = op + " needs something after it."; return nullptr; }
            n->value = value;
            n->now = now;
            if (op == "HAS") n->kind = Kind::has;
            else if (op == "IS") n->kind = Kind::is;
            else if (op == "GREATER") n->kind = Kind::greater;
            else if (op == "LESS") n->kind = Kind::less;
            else if (op == "EQUAL") n->kind = Kind::equal;
            else if (op == "AFTER") n->kind = Kind::after;
            else if (op == "BEFORE") n->kind = Kind::before;
            else if (op == "DURING")
            {
                // DURING LAST 2 WEEKS / DURING LAST WEEK
                n->kind = Kind::during;
                std::vector<std::string> parts;
                {
                    std::string w;
                    for (char c : value + " ") { if (c == ' ') { if (! w.empty()) parts.push_back (w); w.clear(); } else w += c; }
                }
                if (parts.empty() || parts[0] != "LAST") { error = "DURING is written DURING LAST 2 WEEKS."; return nullptr; }
                double count = 1;
                size_t unitAt = 1;
                if (parts.size() > 2) { count = std::atof (parts[1].c_str()); unitAt = 2; }
                if (unitAt >= parts.size()) { error = "DURING LAST needs MINUTES, HOURS, DAYS or WEEKS."; return nullptr; }
                auto unit = upper (parts[unitAt]);
                if (! unit.empty() && unit.back() == 'S') unit.pop_back();
                double ms = unit == "MINUTE" ? 60e3 : unit == "HOUR" ? 3600e3 : unit == "DAY" ? 86400e3 : unit == "WEEK" ? 7 * 86400e3 : 0;
                if (ms == 0) { error = "DURING LAST needs MINUTES, HOURS, DAYS or WEEKS."; return nullptr; }
                n->number = count * ms;
            }
            if (n->kind == Kind::greater || n->kind == Kind::less || n->kind == Kind::equal) n->number = std::atof (value.c_str());
            return n;
        }
        if (words.empty()) { error = "Something's missing near " + (i < t.size() ? t[i].text : std::string ("the end")) + "."; return nullptr; }
        auto n = std::make_unique<Node>();
        n->kind = Kind::words;
        for (auto& w : words) n->words.push_back (w.text);
        return n;
    }
};

// Words to find, from any query: everything, as words.
std::unique_ptr<Node> asWords (const std::string& text)
{
    auto n = std::make_unique<Node>();
    n->kind = Kind::words;
    for (auto& tok : tokenise (text))
        if (tok.text != "(" && tok.text != ")") n->words.push_back (tok.text);
    if (n->words.empty()) n->kind = Kind::all;
    return n;
}

std::string isoTime (std::int64_t ms)
{
    if (ms <= 0) return {};
    std::time_t s = (std::time_t) (ms / 1000);
    std::tm tm {};
   #if defined (_WIN32)
    gmtime_s (&tm, &s);
   #else
    gmtime_r (&s, &tm);
   #endif
    char buf[32];
    std::strftime (buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tm);
    return buf;
}

// The values a field has: each of a tag's, or the pattern's one result.
std::vector<std::string> valuesOf (const Track& t, const std::string& field)
{
    const auto f = lower (field);
    if (f == "added") return { isoTime (t.added) };
    if (f == "modified" || f == "last_modified") return { isoTime (t.modified) };
    if (f.find ('%') != std::string::npos || f.find ('$') != std::string::npos)
    {
        auto [v, truth] = tf::compiled (field)->evalWithTruth (tf::TrackFields (t));
        if (! truth) return {};
        return { v };
    }
    if (f == "*")
    {
        std::vector<std::string> all;
        for (auto& [k, vals] : t.tags) all.insert (all.end(), vals.begin(), vals.end());
        all.push_back (location::fileName (t.location));
        return all;
    }
    auto it = t.tags.find (f);
    if (it != t.tags.end() && ! it->second.empty()) return it->second;
    std::string v;
    if (tf::TrackFields (t).field (f, v)) return { v };
    return {};
}

std::int64_t msOf (const Track& t, const std::string& field)
{
    const auto f = lower (field);
    if (f == "added") return t.added;
    if (f == "modified" || f == "last_modified") return t.modified;
    return 0;
}

bool eval (const Node& n, const Track& t)
{
    switch (n.kind)
    {
        case Kind::all: return true;
        case Kind::andOp: return eval (*n.kids[0], t) && eval (*n.kids[1], t);
        case Kind::orOp: return eval (*n.kids[0], t) || eval (*n.kids[1], t);
        case Kind::notOp: return ! eval (*n.kids[0], t);
        case Kind::words:
        {
            for (auto& w : n.words)
            {
                bool found = containsIgnoringCase (location::fileName (t.location), w);
                for (auto it = t.tags.begin(); ! found && it != t.tags.end(); ++it)
                    for (auto& v : it->second) if (containsIgnoringCase (v, w)) { found = true; break; }
                if (! found) return false;
            }
            return true;
        }
        case Kind::missing: return valuesOf (t, n.field).empty();
        case Kind::present: return ! valuesOf (t, n.field).empty();
        case Kind::during:
        {
            const auto ms = msOf (t, n.field);
            return ms > 0 && (double) (n.now - ms) <= n.number && ms <= n.now + 60000;
        }
        default: break;
    }
    // A pattern on the right is filled in from the same track.
    std::string value = n.value;
    if (value.find ('%') != std::string::npos) value = tf::format (value, t);
    for (auto& v : valuesOf (t, n.field))
    {
        switch (n.kind)
        {
            case Kind::has:
            {
                // Every word of the value in it, in any order.
                bool all = true;
                for (auto& tok : tokenise (value)) if (! containsIgnoringCase (v, tok.text)) { all = false; break; }
                if (all) return true;
                break;
            }
            case Kind::is: if (equalsIgnoringCase (v, value)) return true; break;
            case Kind::greater: if (std::atof (v.c_str()) > n.number) return true; break;
            case Kind::less: if (! v.empty() && std::atof (v.c_str()) < n.number) return true; break;
            case Kind::equal: if (! v.empty() && std::atof (v.c_str()) == n.number) return true; break;
            case Kind::after: if (naturalCompare (v, value) > 0) return true; break;
            case Kind::before: if (! v.empty() && naturalCompare (v, value) < 0) return true; break;
            default: break;
        }
    }
    return false;
}
} // namespace

Query::Query() = default;
Query::~Query() = default;
Query::Query (Query&&) noexcept = default;
Query& Query::operator= (Query&&) noexcept = default;

Query::Query (const std::string& textIn, std::int64_t nowMs)
{
    if (nowMs == 0)
        nowMs = std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::system_clock::now().time_since_epoch()).count();
    std::string text = textIn;
    auto tokens = tokenise (text);
    // SORT [ASCENDING|DESCENDING] BY pattern, at the end.
    for (size_t k = 0; k < tokens.size(); ++k)
    {
        if (tokens[k].quoted || tokens[k].text != "SORT") continue;
        size_t j = k + 1;
        if (j < tokens.size() && (tokens[j].text == "DESCENDING" || tokens[j].text == "ASCENDING")) { descending = tokens[j].text == "DESCENDING"; ++j; }
        if (j < tokens.size() && tokens[j].text == "BY")
        {
            std::string pattern;
            for (size_t m = j + 1; m < tokens.size(); ++m) { if (! pattern.empty()) pattern += ' '; pattern += tokens[m].text; }
            sortBy = pattern;
            tokens.resize (k);
            break;
        }
    }
    if (tokens.empty()) { root = std::make_unique<Node>(); return; }

    bool structured = false;
    for (auto& tok : tokens) if (! tok.quoted && (operators().count (tok.text) || tok.text == "ALL" || tok.text == "NOT")) structured = true;
    if (structured)
    {
        Parser p { tokens, 0, nowMs, {} };
        auto r = p.orExpr();
        if (r && p.i == tokens.size()) { root = std::move (r); return; }
        err = p.error.empty() ? "Part of it couldn't be read." : p.error;
    }
    std::string words;
    for (auto& tok : tokens) { if (! words.empty()) words += ' '; words += tok.quoted ? "\"" + tok.text + "\"" : tok.text; }
    root = asWords (words);
}

bool Query::matches (const Track& t) const { return ! root || eval (*root, t); }
bool Query::empty() const { return ! root || root->kind == Kind::all; }

int naturalCompare (const std::string& a, const std::string& b)
{
    size_t i = 0, j = 0;
    while (i < a.size() && j < b.size())
    {
        const auto ca = (unsigned char) a[i], cb = (unsigned char) b[j];
        if (std::isdigit (ca) && std::isdigit (cb))
        {
            size_t ie = i, je = j;
            while (ie < a.size() && std::isdigit ((unsigned char) a[ie])) ++ie;
            while (je < b.size() && std::isdigit ((unsigned char) b[je])) ++je;
            // Compare as numbers: skip leading zeros, then length, then digits.
            size_t is = i, js = j;
            while (is + 1 < ie && a[is] == '0') ++is;
            while (js + 1 < je && b[js] == '0') ++js;
            if (ie - is != je - js) return ie - is < je - js ? -1 : 1;
            for (; is < ie; ++is, ++js) if (a[is] != b[js]) return a[is] < b[js] ? -1 : 1;
            i = ie;
            j = je;
            continue;
        }
        const auto la = std::tolower (ca), lb = std::tolower (cb);
        if (la != lb) return la < lb ? -1 : 1;
        ++i;
        ++j;
    }
    if (i < a.size()) return 1;
    if (j < b.size()) return -1;
    return 0;
}

void sortByPattern (std::vector<size_t>& order, const std::vector<Track>& tracks, const std::string& pattern, bool descending)
{
    auto script = tf::compiled (pattern);
    std::vector<std::string> keys (tracks.size());
    for (auto k : order) keys[k] = script->eval (tf::TrackFields (tracks[k]));
    std::stable_sort (order.begin(), order.end(), [&] (size_t x, size_t y)
    {
        const int c = naturalCompare (keys[x], keys[y]);
        return descending ? c > 0 : c < 0;
    });
}

void sortByPattern (std::vector<const Track*>& tracks, const std::string& pattern, bool descending)
{
    auto script = tf::compiled (pattern);
    std::vector<std::pair<std::string, const Track*>> keyed;
    keyed.reserve (tracks.size());
    for (auto* t : tracks) keyed.emplace_back (script->eval (tf::TrackFields (*t)), t);
    std::stable_sort (keyed.begin(), keyed.end(), [&] (auto& x, auto& y)
    {
        const int c = naturalCompare (x.first, y.first);
        return descending ? c > 0 : c < 0;
    });
    for (size_t k = 0; k < keyed.size(); ++k) tracks[k] = keyed[k].second;
}

} // namespace kp
