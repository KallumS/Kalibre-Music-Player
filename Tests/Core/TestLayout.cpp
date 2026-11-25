#include "Check.h"
#include "Layout.h"

#include <cmath>

using namespace kp::layout;

TEST ("layout: presets are whole, and kept and read back")
{
    for (auto& p : presets())
    {
        CHECK (! panels (p.root).empty());
        auto back = fromJson (toJson (p.root));
        CHECK (back.has_value());
        CHECK_EQ (panels (*back), panels (p.root));
    }
    CHECK (! fromJson ("nonsense").has_value());
}

TEST ("layout: splitting, tabs, replacing, removing")
{
    Node root = Node::of ("playlist");
    CHECK (split (root, {}, Side::left, "library"));
    CHECK (root.kind == Node::Kind::row);
    CHECK_EQ (panels (root), (std::vector<std::string> { "library", "playlist" }));
    // Beside one in a row: a third part, not a nested row.
    CHECK (split (root, { 1 }, Side::right, "album art"));
    CHECK_EQ (root.kids.size(), (size_t) 3);
    CHECK_EQ (panels (root), (std::vector<std::string> { "library", "playlist", "album art" }));
    double sum = 0;
    for (auto s : root.sizes) sum += s;
    CHECK (std::abs (sum - 1.0) < 1e-9);
    // Below one: a column inside the row.
    CHECK (split (root, { 2 }, Side::below, "spectrum"));
    CHECK (root.kids[2].kind == Node::Kind::column);
    CHECK (addTab (root, { 2, 1 }, "oscilloscope"));
    CHECK (root.kids[2].kids[1].kind == Node::Kind::tabs);
    CHECK (addTab (root, { 2, 1, 0 }, "goniometer"));
    CHECK_EQ (root.kids[2].kids[1].kids.size(), (size_t) 3);
    CHECK (replace (root, { 0 }, "equalizer"));
    CHECK_EQ (std::string (panels (root).front()), std::string ("equalizer"));
    // Removing the album art leaves the column of one tabs part: it becomes the tabs.
    CHECK (remove (root, { 2, 0 }));
    CHECK (root.kids[2].kind == Node::Kind::tabs);
    CHECK (remove (root, { 0 }));
    CHECK (remove (root, { 0 }));
    CHECK (root.kind == Node::Kind::tabs);
    CHECK (remove (root, { 0 }));
    CHECK (remove (root, { 0 }));
    CHECK (root.kind == Node::Kind::panel);
    CHECK (! remove (root, {}));
}
