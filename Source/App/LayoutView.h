/*
    LayoutView - the window's panels laid out as the layout says (decision
    0011): rows and columns with dividers to drag, and tabs. With View >
    Edit Layout on, each panel shows its name and a menu: replace it, put
    another beside it or in tabs with it, or take it out.
*/

#pragma once

#include "Layout.h"
#include "Panels.h"

namespace kp
{

class LayoutView : public juce::Component
{
public:
    explicit LayoutView (Context&);
    ~LayoutView() override;
    void setLayout (const layout::Node&);
    const layout::Node& current() const { return root; }
    void setEditing (bool);
    bool editing() const { return editMode; }
    std::function<void()> onChanged;     // the layout changed (to be kept)
    void resized() override;

    // The first panel of a kind, if it's shown (the library's search, for Command-F).
    juce::Component* find (const std::string& panel) const;

    struct Node;

private:
    Context& ctx;
    layout::Node root;
    std::unique_ptr<juce::Component> built;
    bool editMode = false;
    void rebuild();
    std::unique_ptr<juce::Component> build (const layout::Node&, layout::Path);
    void edit (layout::Path);
    void sizesChanged (const layout::Path&, const std::vector<double>&, bool finished);
    void tabChanged (const layout::Path&, int);
    friend struct Split;
    friend struct Holder;
    friend struct Tabs;
};

} // namespace kp
