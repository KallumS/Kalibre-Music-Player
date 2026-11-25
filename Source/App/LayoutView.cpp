#include "LayoutView.h"

namespace kp
{
using namespace juce;

//==============================================================================
// A panel, and over it when editing, its name and a menu.
struct Holder : public Component
{
    LayoutView& owner;
    layout::Path path;
    std::string name;
    std::unique_ptr<Component> panel;

    struct Overlay : public Component
    {
        Holder& h;
        theme::HouseButton menu { String::fromUTF8 ("Change\xe2\x80\xa6"), "Replace this panel, put another beside it, or take it out." };
        explicit Overlay (Holder& o) : h (o)
        {
            addAndMakeVisible (menu);
            menu.onClick = [this] { h.owner.edit (h.path); };
        }
        void paint (Graphics& g) override
        {
            g.fillAll (theme::ground.withAlpha (0.82f));
            g.setColour (theme::accent);
            g.drawRect (getLocalBounds(), 2);
            g.setColour (theme::text);
            g.setFont (theme::font (16.0f, true));
            g.drawText (panelTitle (h.name), getLocalBounds().withTrimmedBottom (getHeight() / 2), Justification::centredBottom);
        }
        void resized() override
        {
            menu.setBounds (getLocalBounds().withTrimmedTop (getHeight() / 2 + 6).withSizeKeepingCentre (jmin (getWidth() - 8, theme::px (120)), theme::px (28))
                                .withY (getHeight() / 2 + 6));
        }
    };
    std::unique_ptr<Overlay> overlay;

    Holder (LayoutView& o, layout::Path p, const std::string& n, std::unique_ptr<Component> c)
        : owner (o), path (std::move (p)), name (n), panel (std::move (c))
    {
        addAndMakeVisible (*panel);
        overlay = std::make_unique<Overlay> (*this);
        addChildComponent (*overlay);
        overlay->setVisible (o.editMode);
    }
    void resized() override
    {
        panel->setBounds (getLocalBounds());
        overlay->setBounds (getLocalBounds());
    }
};

//==============================================================================
// A row or a column, its parts divided by bars to drag.
struct Split : public Component
{
    LayoutView& owner;
    layout::Path path;
    bool across;                         // a row: parts side by side
    std::vector<double> sizes;           // shares, or fixed points (negative)
    std::vector<std::unique_ptr<Component>> parts;
    static constexpr int bar = 5;
    int dragging = -1;
    std::vector<double> atStart;
    int startPos = 0;

    Split (LayoutView& o, layout::Path p, bool row, std::vector<double> s) : owner (o), path (std::move (p)), across (row), sizes (std::move (s)) {}

    void add (std::unique_ptr<Component> c) { addAndMakeVisible (*c); parts.push_back (std::move (c)); }

    // Each part's length along the split.
    std::vector<int> lengths() const
    {
        const int total = (across ? getWidth() : getHeight()) - bar * (int) jmax<size_t> (0, parts.size() - 1);
        int fixed = 0;
        double shares = 0;
        for (size_t k = 0; k < parts.size(); ++k)
        {
            const double s = k < sizes.size() ? sizes[k] : 1.0;
            if (s < 0) fixed += theme::px ((int) -s);
            else shares += s;
        }
        std::vector<int> out;
        int used = 0;
        for (size_t k = 0; k < parts.size(); ++k)
        {
            const double s = k < sizes.size() ? sizes[k] : 1.0;
            int len = s < 0 ? theme::px ((int) -s) : (shares > 0 ? (int) std::round ((total - fixed) * s / shares) : 0);
            if (k + 1 == parts.size()) len = total - used;   // the last takes what rounding left
            out.push_back (jmax (0, len));
            used += len;
        }
        return out;
    }

    void resized() override
    {
        auto lens = lengths();
        int at = 0;
        for (size_t k = 0; k < parts.size(); ++k)
        {
            if (across) parts[k]->setBounds (at, 0, lens[k], getHeight());
            else parts[k]->setBounds (0, at, getWidth(), lens[k]);
            at += lens[k] + bar;
        }
    }

    int barAt (Point<int> p) const
    {
        auto lens = lengths();
        int at = 0;
        for (size_t k = 0; k + 1 < parts.size(); ++k)
        {
            at += lens[k];
            const int pos = across ? p.x : p.y;
            if (pos >= at - 2 && pos < at + bar + 2) return (int) k;
            at += bar;
        }
        return -1;
    }

    void paint (Graphics& g) override { g.fillAll (theme::ground); }
    MouseCursor getMouseCursor() override
    {
        return barAt (getMouseXYRelative()) >= 0 ? (across ? MouseCursor::LeftRightResizeCursor : MouseCursor::UpDownResizeCursor) : MouseCursor::NormalCursor;
    }
    void mouseMove (const MouseEvent&) override { updateMouseCursor(); }
    void mouseDown (const MouseEvent& e) override
    {
        dragging = barAt (e.getPosition());
        atStart = sizes;
        startPos = across ? e.x : e.y;
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (dragging < 0) return;
        const int delta = (across ? e.x : e.y) - startPos;
        auto& a = sizes[(size_t) dragging];
        auto& b = sizes[(size_t) dragging + 1];
        const double sa = atStart[(size_t) dragging], sb = atStart[(size_t) dragging + 1];
        // Moving a bar beside a fixed part resizes that part; between shares, moves share from one to the other.
        auto lens = lengths();
        double shares = 0;
        int shareLength = 0;
        for (size_t k = 0; k < sizes.size(); ++k) if (atStart[k] >= 0) { shares += atStart[k]; shareLength += lens[k]; }
        const double perPixel = shareLength > 0 ? shares / shareLength : 0;
        const int minLen = theme::px (60);
        if (sa < 0) a = -jmax (40.0, -sa + delta / theme::scale());
        else if (sb < 0) b = -jmax (40.0, -sb - delta / theme::scale());
        if (sa >= 0 && sb >= 0)
        {
            const double moved = jlimit (-(sa - minLen * perPixel), sb - minLen * perPixel, delta * perPixel);
            a = sa + moved;
            b = sb - moved;
        }
        resized();
        owner.sizesChanged (path, sizes, false);
    }
    void mouseUp (const MouseEvent&) override
    {
        if (dragging >= 0) owner.sizesChanged (path, sizes, true);
        dragging = -1;
    }
};

//==============================================================================
struct Tabs : public TabbedComponent
{
    LayoutView& owner;
    layout::Path path;
    bool ready = false;
    Tabs (LayoutView& o, layout::Path p) : TabbedComponent (TabbedButtonBar::TabsAtTop), owner (o), path (std::move (p))
    {
        setTabBarDepth (theme::px (26));
        setOutline (0);
    }
    void currentTabChanged (int index, const String&) override { if (ready) owner.tabChanged (path, index); }
};

//==============================================================================
LayoutView::LayoutView (Context& c) : ctx (c) {}
LayoutView::~LayoutView() = default;

void LayoutView::setLayout (const layout::Node& n)
{
    root = n;
    rebuild();
}

void LayoutView::setEditing (bool e)
{
    editMode = e;
    rebuild();
}

void LayoutView::rebuild()
{
    built.reset();
    built = build (root, {});
    addAndMakeVisible (*built);
    resized();
}

void LayoutView::resized() { if (built) built->setBounds (getLocalBounds()); }

std::unique_ptr<Component> LayoutView::build (const layout::Node& n, layout::Path path)
{
    using K = layout::Node::Kind;
    if (n.kind == K::panel) return std::make_unique<Holder> (*this, path, n.panel, makePanel (n.panel, ctx));
    if (n.kind == K::tabs)
    {
        auto t = std::make_unique<Tabs> (*this, path);
        for (size_t k = 0; k < n.kids.size(); ++k)
        {
            auto p = path;
            p.push_back ((int) k);
            auto child = build (n.kids[k], p);
            const auto title = n.kids[k].kind == K::panel ? panelTitle (n.kids[k].panel) : String ("Panels");
            t->addTab (title, theme::sunken, child.release(), true);
        }
        t->setCurrentTabIndex (jlimit (0, (int) n.kids.size() - 1, n.tab), false);
        t->ready = true;
        return t;
    }
    auto s = std::make_unique<Split> (*this, path, n.kind == K::row, n.sizes);
    for (size_t k = 0; k < n.kids.size(); ++k)
    {
        auto p = path;
        p.push_back ((int) k);
        s->add (build (n.kids[k], p));
    }
    return s;
}

void LayoutView::sizesChanged (const layout::Path& p, const std::vector<double>& sizes, bool finished)
{
    if (auto* n = layout::at (root, p)) n->sizes = sizes;
    if (finished && onChanged) onChanged();
}

void LayoutView::tabChanged (const layout::Path& p, int index)
{
    if (auto* n = layout::at (root, p)) n->tab = index;
    if (onChanged) onChanged();
}

void LayoutView::edit (layout::Path p)
{
    auto panels = [] (std::function<void (const std::string&)> pick)
    {
        PopupMenu m;
        for (auto& t : panelTypes())
        {
            std::string name = t.name;
            m.addItem (t.title, [pick, name] { pick (name); });
        }
        return m;
    };
    auto change = [this] (std::function<bool()> f)
    {
        if (! f()) return;
        layout::tidy (root);
        // After the change the menu's own panel may be gone: rebuilt after this call returns.
        MessageManager::callAsync ([safe = SafePointer<LayoutView> (this)] { if (safe) { safe->rebuild(); if (safe->onChanged) safe->onChanged(); } });
    };
    PopupMenu m;
    m.addSubMenu ("Replace With", panels ([this, p, change] (const std::string& n) { change ([this, p, n] { return layout::replace (root, p, n); }); }));
    m.addSeparator();
    const std::pair<const char*, layout::Side> sides[] = { { "Add to the Left", layout::Side::left }, { "Add to the Right", layout::Side::right },
                                                          { "Add Above", layout::Side::above }, { "Add Below", layout::Side::below } };
    for (auto& [label, side] : sides)
    {
        auto sd = side;
        m.addSubMenu (label, panels ([this, p, sd, change] (const std::string& n) { change ([this, p, sd, n] { return layout::split (root, p, sd, n); }); }));
    }
    m.addSubMenu ("Add as a Tab", panels ([this, p, change] (const std::string& n) { change ([this, p, n] { return layout::addTab (root, p, n); }); }));
    m.addSeparator();
    m.addItem ("Remove", layout::panels (root).size() > 1, false, [this, p, change] { change ([this, p] { return layout::remove (root, p); }); });
    m.showMenuAsync (PopupMenu::Options());
}

Component* LayoutView::find (const std::string& panel) const
{
    std::function<Component* (Component*)> search = [&] (Component* c) -> Component*
    {
        if (auto* h = dynamic_cast<Holder*> (c); h && h->name == panel) return h->panel.get();
        for (auto* k : c->getChildren()) if (auto* f = search (k)) return f;
        return nullptr;
    };
    return built ? search (built.get()) : nullptr;
}

} // namespace kp
