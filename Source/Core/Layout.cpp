#include "Layout.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>

namespace kp::layout
{
using json = nlohmann::json;

Node* at (Node& root, const Path& p)
{
    Node* n = &root;
    for (int k : p)
    {
        if (k < 0 || k >= (int) n->kids.size()) return nullptr;
        n = &n->kids[(size_t) k];
    }
    return n;
}

const Node* at (const Node& root, const Path& p) { return at (const_cast<Node&> (root), p); }

static void normalise (Node& n)
{
    if (! n.isSplit()) { n.sizes.clear(); return; }
    n.sizes.resize (n.kids.size(), 0.0);
    double sum = 0;
    int unsized = 0;
    for (auto s : n.sizes) { if (s > 0) sum += s; else if (s == 0) ++unsized; }
    // Parts without a share get an equal one.
    const double each = n.kids.empty() ? 0 : (sum > 0 ? sum / (double) std::max<size_t> (1, n.kids.size() - (size_t) unsized) : 1.0);
    for (auto& s : n.sizes) if (s == 0) s = each;
    sum = 0;
    for (auto s : n.sizes) if (s > 0) sum += s;
    if (sum > 0) for (auto& s : n.sizes) if (s > 0) s /= sum;
}

void tidy (Node& n)
{
    for (auto& k : n.kids) tidy (k);
    if (n.isSplit() || n.kind == Node::Kind::tabs)
    {
        // A part of the same direction is merged into this one.
        if (n.isSplit())
        {
            normalise (n);
            std::vector<Node> kids;
            std::vector<double> sizes;
            for (size_t k = 0; k < n.kids.size(); ++k)
            {
                auto& kid = n.kids[k];
                if (kid.kind == n.kind && n.sizes[k] > 0)
                {
                    normalise (kid);
                    for (size_t j = 0; j < kid.kids.size(); ++j)
                    {
                        kids.push_back (std::move (kid.kids[j]));
                        sizes.push_back (kid.sizes[j] > 0 ? kid.sizes[j] * n.sizes[k] : kid.sizes[j]);
                    }
                }
                else { kids.push_back (std::move (kid)); sizes.push_back (n.sizes[k]); }
            }
            n.kids = std::move (kids);
            n.sizes = std::move (sizes);
        }
        if (n.kids.size() == 1)
        {
            Node only = std::move (n.kids[0]);
            n = std::move (only);
            return;
        }
        normalise (n);
        n.tab = std::clamp (n.tab, 0, std::max (0, (int) n.kids.size() - 1));
    }
}

bool split (Node& root, const Path& p, Side side, const std::string& panel)
{
    Node* target = at (root, p);
    if (! target) return false;
    const auto kind = (side == Side::left || side == Side::right) ? Node::Kind::row : Node::Kind::column;
    const bool before = side == Side::left || side == Side::above;
    // Inside a split of the same direction: a new part beside it.
    if (! p.empty())
    {
        Path parentPath (p.begin(), p.end() - 1);
        Node* parent = at (root, parentPath);
        if (parent && parent->kind == kind)
        {
            normalise (*parent);
            const int k = p.back();
            const auto share = parent->sizes[(size_t) k] > 0 ? parent->sizes[(size_t) k] / 2 : 0.0;
            if (share > 0) parent->sizes[(size_t) k] = share;
            const int at = before ? k : k + 1;
            parent->kids.insert (parent->kids.begin() + at, Node::of (panel));
            parent->sizes.insert (parent->sizes.begin() + at, share);
            normalise (*parent);
            return true;
        }
    }
    Node old = std::move (*target);
    Node s;
    s.kind = kind;
    if (before) { s.kids.push_back (Node::of (panel)); s.kids.push_back (std::move (old)); }
    else { s.kids.push_back (std::move (old)); s.kids.push_back (Node::of (panel)); }
    s.sizes = { 0.5, 0.5 };
    *target = std::move (s);
    return true;
}

bool replace (Node& root, const Path& p, const std::string& panel)
{
    Node* target = at (root, p);
    if (! target) return false;
    *target = Node::of (panel);
    return true;
}

bool addTab (Node& root, const Path& p, const std::string& panel)
{
    Node* target = at (root, p);
    if (! target) return false;
    if (target->kind == Node::Kind::tabs)
    {
        target->kids.push_back (Node::of (panel));
        target->tab = (int) target->kids.size() - 1;
        return true;
    }
    // A panel already in tabs: the new one joins them.
    if (! p.empty())
    {
        Path parentPath (p.begin(), p.end() - 1);
        if (Node* parent = at (root, parentPath); parent && parent->kind == Node::Kind::tabs)
        {
            parent->kids.push_back (Node::of (panel));
            parent->tab = (int) parent->kids.size() - 1;
            return true;
        }
    }
    Node old = std::move (*target);
    Node t;
    t.kind = Node::Kind::tabs;
    t.kids.push_back (std::move (old));
    t.kids.push_back (Node::of (panel));
    t.tab = 1;
    *target = std::move (t);
    return true;
}

bool remove (Node& root, const Path& p)
{
    if (p.empty()) return false;
    Path parentPath (p.begin(), p.end() - 1);
    Node* parent = at (root, parentPath);
    if (! parent || p.back() < 0 || p.back() >= (int) parent->kids.size()) return false;
    if (panels (root).size() <= 1) return false;
    parent->kids.erase (parent->kids.begin() + p.back());
    if (parent->isSplit() && p.back() < (int) parent->sizes.size()) parent->sizes.erase (parent->sizes.begin() + p.back());
    tidy (root);
    return true;
}

std::vector<std::string> panels (const Node& n)
{
    if (n.kind == Node::Kind::panel) return { n.panel };
    std::vector<std::string> out;
    for (auto& k : n.kids) { auto p = panels (k); out.insert (out.end(), p.begin(), p.end()); }
    return out;
}

static json toJ (const Node& n)
{
    json j;
    switch (n.kind)
    {
        case Node::Kind::panel: j["panel"] = n.panel; return j;
        case Node::Kind::row: j["row"] = json::array(); break;
        case Node::Kind::column: j["column"] = json::array(); break;
        case Node::Kind::tabs: j["tabs"] = json::array(); j["tab"] = n.tab; break;
    }
    auto& list = n.kind == Node::Kind::row ? j["row"] : n.kind == Node::Kind::column ? j["column"] : j["tabs"];
    for (auto& k : n.kids) list.push_back (toJ (k));
    if (n.isSplit()) j["sizes"] = n.sizes;
    return j;
}

static std::optional<Node> fromJ (const json& j)
{
    if (! j.is_object()) return std::nullopt;
    Node n;
    if (j.contains ("panel")) { n.panel = j.value ("panel", ""); return n.panel.empty() ? std::nullopt : std::optional<Node> (n); }
    const char* key = j.contains ("row") ? "row" : j.contains ("column") ? "column" : j.contains ("tabs") ? "tabs" : nullptr;
    if (! key || ! j[key].is_array()) return std::nullopt;
    n.kind = std::string (key) == "row" ? Node::Kind::row : std::string (key) == "column" ? Node::Kind::column : Node::Kind::tabs;
    for (auto& k : j[key]) if (auto c = fromJ (k)) n.kids.push_back (std::move (*c));
    if (n.kids.empty()) return std::nullopt;
    if (j.contains ("sizes") && j["sizes"].is_array()) for (auto& s : j["sizes"]) if (s.is_number()) n.sizes.push_back (s.get<double>());
    n.tab = j.value ("tab", 0);
    return n;
}

std::string toJson (const Node& n) { return toJ (n).dump (1); }

std::optional<Node> fromJson (const std::string& s)
{
    auto j = json::parse (s, nullptr, false);
    if (j.is_discarded()) return std::nullopt;
    auto n = fromJ (j);
    if (n) tidy (*n);
    return n;
}

static Node row (std::vector<Node> kids, std::vector<double> sizes) { Node n; n.kind = Node::Kind::row; n.kids = std::move (kids); n.sizes = std::move (sizes); return n; }
static Node column (std::vector<Node> kids, std::vector<double> sizes) { Node n; n.kind = Node::Kind::column; n.kids = std::move (kids); n.sizes = std::move (sizes); return n; }
static Node tabs (std::vector<Node> kids) { Node n; n.kind = Node::Kind::tabs; n.kids = std::move (kids); return n; }
static Node p (const char* name) { return Node::of (name); }

// Sizes below zero are fixed heights in points (the transport, the seek bar).
const std::vector<Preset>& presets()
{
    static const std::vector<Preset> all {
        { "Library and Playlist",
          column ({ row ({ column ({ p ("library") }, { 1.0 }),
                           p ("playlist"),
                           column ({ p ("album art"), p ("now playing"), tabs ({ p ("spectrum"), p ("oscilloscope"), p ("goniometer"), p ("peak meter"), p ("loudness") }) },
                                   { 0.38, 0.17, 0.45 }) },
                         { 0.24, 0.52, 0.24 }),
                    p ("transport") },
                  { 1.0, -96 }) },
        { "Playlist Only",
          column ({ p ("playlist"), p ("transport") }, { 1.0, -96 }) },
        { "Visualisers",
          column ({ row ({ p ("playlist"),
                           column ({ row ({ p ("oscilloscope"), p ("goniometer") }, { 0.5, 0.5 }), p ("spectrum"), row ({ p ("peak meter"), p ("loudness") }, { 0.5, 0.5 }) },
                                   { 0.4, 0.35, 0.25 }) },
                         { 0.45, 0.55 }),
                    p ("transport") },
                  { 1.0, -96 }) },
        { "Album Art",
          column ({ row ({ p ("album art"), column ({ p ("now playing"), p ("playlist") }, { 0.25, 0.75 }) }, { 0.45, 0.55 }), p ("transport") },
                  { 1.0, -96 }) },
        { "Mastering Desk",
          column ({ row ({ column ({ p ("library"), p ("equalizer") }, { 0.55, 0.45 }), p ("playlist"),
                           column ({ p ("loudness"), p ("peak meter"), p ("goniometer") }, { 0.35, 0.3, 0.35 }) },
                         { 0.3, 0.45, 0.25 }),
                    p ("transport") },
                  { 1.0, -96 }) },
    };
    return all;
}

} // namespace kp::layout
