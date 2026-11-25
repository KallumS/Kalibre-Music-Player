/*
    Layout - how the window is divided among its panels (decision 0011): a
    tree of rows and columns of panels, and tabs. Edit Layout (View menu)
    lets the window be rearranged by hand: any panel replaced, split beside
    another, put in tabs with others, or taken out; dividers dragged.

    Panels are named ("playlist", "library", "oscilloscope"); the app
    knows what each is. Kept as JSON beside the settings.

    No JUCE in here.
*/

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace kp::layout
{

struct Node
{
    enum class Kind { panel, row, column, tabs } kind = Kind::panel;
    std::string panel;              // a panel's name
    std::vector<Node> kids;         // a row's, column's or tabs' parts
    std::vector<double> sizes;      // each kid's share of a row or column (they add to 1); fixed sizes are negative points
    int tab = 0;                    // the tab shown

    static Node of (const std::string& panelName) { Node n; n.panel = panelName; return n; }
    bool isSplit() const { return kind == Kind::row || kind == Kind::column; }
};

using Path = std::vector<int>;      // from the root: which kid, at each level

Node* at (Node& root, const Path&);
const Node* at (const Node& root, const Path&);

enum class Side { left, right, above, below };
// A new panel beside the one at `path`.
bool split (Node& root, const Path&, Side, const std::string& panel);
bool replace (Node& root, const Path&, const std::string& panel);
// The panel at `path` put in tabs with a new one (or the new one added to its tabs).
bool addTab (Node& root, const Path&, const std::string& panel);
// Takes a part out; its neighbours share its room. The last panel can't go.
bool remove (Node& root, const Path&);
// Rows and columns with one part become that part; sizes add to 1.
void tidy (Node&);
// Every panel's name, in order.
std::vector<std::string> panels (const Node&);

std::string toJson (const Node&);
std::optional<Node> fromJson (const std::string&);

// The layouts offered by View > Layout.
struct Preset { std::string name; Node root; };
const std::vector<Preset>& presets();

} // namespace kp::layout
