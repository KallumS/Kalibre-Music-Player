#include "LibraryView.h"
#include "Commands.h"

namespace kp
{
using namespace juce;

namespace
{
String u8 (const std::string& s) { return String::fromUTF8 (s.c_str()); }
const char* fieldChoices[] = { "genre", "album artist", "artist", "album", "year", "composer", "date", "codec", "directoryname", "%album artist% - %album%" };
} // namespace

struct LibraryView::Column : public Component, private ListBoxModel
{
    LibraryView& owner;
    int index;
    String field;
    ComboBox chooser;
    ListBox list;
    std::vector<Facet> values;               // first: "All (n)"
    std::vector<const Track*> input;         // what this column chooses among

    Column (LibraryView& o, int i, const String& f) : owner (o), index (i), field (f)
    {
        int id = 1;
        for (auto* c : fieldChoices) chooser.addItem (fieldTitle (c), id++);
        chooser.addItem (String::fromUTF8 ("Other Field\xe2\x80\xa6"), 99);
        selectField();
        chooser.onChange = [this]
        {
            const int id = chooser.getSelectedId();
            if (id == 99)
            {
                auto* w = new AlertWindow ("Browse By", "A tag's name (\"label\") or a pattern (\"%album artist% - %album%\").", MessageBoxIconType::NoIcon);
                w->addTextEditor ("field", field);
                w->addButton ("Browse", 1, KeyPress (KeyPress::returnKey));
                w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
                ui::present (w, ModalCallbackFunction::create ([this, w] (int r)
                {
                    if (r == 1 && w->getTextEditorContents ("field").trim().isNotEmpty()) setField (w->getTextEditorContents ("field").trim().toLowerCase());
                    else selectField();
                }), true);
                return;
            }
            if (id > 0 && id <= (int) std::size (fieldChoices)) setField (fieldChoices[id - 1]);
        };
        addAndMakeVisible (chooser);
        list.setModel (this);
        list.setMultipleSelectionEnabled (true);
        list.setRowHeight (theme::px (21));
        list.setColour (ListBox::backgroundColourId, theme::roll);
        addAndMakeVisible (list);
    }

    static String fieldTitle (const String& f)
    {
        if (f.contains ("%")) return f;
        if (f == "directoryname") return "Folder";
        StringArray words;
        words.addTokens (f, " ", "");
        for (auto& w : words) w = w.substring (0, 1).toUpperCase() + w.substring (1);
        return words.joinIntoString (" ");
    }

    void selectField()
    {
        int id = 0;
        for (int k = 0; k < (int) std::size (fieldChoices); ++k) if (field == fieldChoices[k]) id = k + 1;
        if (id == 0) { chooser.changeItemText (99, fieldTitle (field)); id = 99; }
        chooser.setSelectedId (id, dontSendNotification);
    }

    void setField (const String& f)
    {
        field = f;
        owner.ctx.c.settings.set ("facet" + String (index), field);
        selectField();
        owner.refilter (index);
    }

    void fill (std::vector<const Track*> tracks)
    {
        // Keep what was chosen, where it's still there.
        StringArray was;
        auto rows = list.getSelectedRows();
        for (int k = 0; k < rows.size(); ++k) if (rows[k] > 0 && rows[k] <= (int) values.size() - 1) was.add (u8 (values[(size_t) rows[k]].value));
        input = std::move (tracks);
        values = facets (input, field.toStdString());
        values.insert (values.begin(), Facet { "All", (int) input.size() });
        list.updateContent();
        SparseSet<int> keep;
        for (size_t k = 1; k < values.size(); ++k) if (was.contains (u8 (values[k].value))) keep.addRange ({ (int) k, (int) k + 1 });
        if (keep.isEmpty()) list.selectRow (0, false, true);
        else list.setSelectedRows (keep, dontSendNotification);
        list.repaint();
    }

    // The tracks this column lets through.
    std::vector<const Track*> output()
    {
        auto rows = list.getSelectedRows();
        if (rows.isEmpty() || rows.contains (0)) return input;
        std::vector<std::string> chosen;
        for (int k = 0; k < rows.size(); ++k) if (rows[k] > 0 && rows[k] < (int) values.size()) chosen.push_back (values[(size_t) rows[k]].value);
        std::vector<const Track*> out;
        for (auto* t : input)
            for (auto& v : chosen) if (hasFacetValue (*t, field.toStdString(), v)) { out.push_back (t); break; }
        return out;
    }

    void resized() override
    {
        auto r = getLocalBounds();
        chooser.setBounds (r.removeFromTop (theme::px (26)).reduced (2));
        list.setBounds (r);
    }

    int getNumRows() override { return (int) values.size(); }
    void paintListBoxItem (int row, Graphics& g, int w, int h, bool selected) override
    {
        if (row < 0 || row >= (int) values.size()) return;
        if (selected) g.fillAll (theme::raised.brighter (0.15f));
        auto& v = values[(size_t) row];
        g.setFont (theme::font (theme::bodyPoints, row == 0));
        g.setColour (theme::text);
        const auto plural = field.contains ("%") ? String ("Values") : fieldTitle (field) + "s";
        const auto label = row == 0 ? "All (" + String (values.size() - 1) + " " + plural + ")" : u8 (v.value);
        g.drawText (label, 8, 0, w - 56, h, Justification::centredLeft, true);
        g.setColour (theme::textSecondary);
        g.setFont (theme::font (theme::smallPoints));
        g.drawText (String (v.count), w - 52, 0, 44, h, Justification::centredRight);
    }
    void selectedRowsChanged (int) override { owner.refilter (index + 1); }
    void listBoxItemDoubleClicked (int, const MouseEvent&) override { owner.send (true); }
    void listBoxItemClicked (int row, const MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu()) return;
        if (! list.isRowSelected (row)) list.selectRow (row);
        PopupMenu m;
        m.addItem ("Play", [this] { owner.send (true); });
        m.addItem ("Add to the Playlist Shown", [this]
        {
            auto locs = owner.chosenLocations();
            owner.ctx.c.addLocations (owner.ctx.c.playlists.active, locs);
            owner.ctx.c.say (String ((int) locs.size()) + " tracks added to " + u8 (owner.ctx.c.active().name) + ".");
        });
        m.addItem ("Send to a New Playlist", [this]
        {
            auto locs = owner.chosenLocations();
            auto rows = list.getSelectedRows();
            const auto name = rows.size() == 1 && rows[0] > 0 ? u8 (values[(size_t) rows[0]].value) : String ("Library");
            const int p = owner.ctx.c.newPlaylist (name);
            owner.ctx.c.addLocations (p, locs);
        });
        m.addItem ("Play Next", [this]
        {
            const int p = owner.ctx.c.librarySelectionPlaylist();
            for (int k = 0; k < (int) owner.ctx.c.playlists.lists[(size_t) p].items.size(); ++k) owner.ctx.c.addToQueue (p, k);
        });
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&list).withMousePosition());
    }
    bool keyPressed (const KeyPress& k) override
    {
        if (k == KeyPress::returnKey) { owner.send (true); return true; }
        return false;
    }
};

LibraryView::LibraryView (Context& x) : ctx (x)
{
    searchField.setTextToShowWhenEmpty (String::fromUTF8 ("Search: words, or a query (genre IS jazz)\xe2\x80\xa6"), theme::textSecondary);
    searchField.setText (ctx.c.settings.get ("librarySearch"), false);
    searchField.onTextChange = [this] { ctx.c.settings.set ("librarySearch", searchField.getText()); dirty = true; startTimer (200); };
    searchField.onReturnKey = [this] { send (true); };
    searchField.onEscapeKey = [this] { searchField.clear(); dirty = true; startTimer (1); };
    addAndMakeVisible (searchField);
    addChildComponent (addFolder);
    addFolder.onClick = [this] { ctx.commands.invokeDirectly (cmd::libraryFolders, true); };
    count.setColour (Label::textColourId, theme::textSecondary);
    count.setFont (theme::font (theme::smallPoints));
    addAndMakeVisible (count);
    const char* defaults[] = { "genre", "album artist", "album" };
    for (int k = 0; k < 3; ++k)
    {
        facetColumns.push_back (std::make_unique<Column> (*this, k, ctx.c.settings.get ("facet" + String (k), defaults[k])));
        addAndMakeVisible (*facetColumns.back());
    }
    ctx.c.addListener (this);
    timerCallback();
}

LibraryView::~LibraryView() { ctx.c.removeListener (this); }

void LibraryView::focusSearch() { searchField.grabKeyboardFocus(); searchField.selectAll(); }
void LibraryView::setSearch (const String& s) { searchField.setText (s, false); dirty = true; timerCallback(); }

void LibraryView::paint (Graphics& g)
{
    paintPanel (g, *this);
    if (ctx.c.library.size() == 0 && ! ctx.c.scanning())
    {
        g.setColour (theme::textSecondary);
        g.setFont (theme::font());
        g.drawFittedText ("The library is empty. Add a folder of music, and the library watches it: what's added there appears here.",
                          getLocalBounds().reduced (16).withTrimmedTop (theme::px (110)), Justification::centredTop, 4);
    }
}

void LibraryView::resized()
{
    auto r = getLocalBounds().reduced (6);
    searchField.setBounds (r.removeFromTop (theme::px (28)));
    count.setBounds (r.removeFromTop (theme::px (20)));
    addFolder.setBounds (r.withHeight (theme::px (28)).withSizeKeepingCentre (jmin (r.getWidth(), theme::px (180)), theme::px (28)).translated (0, theme::px (40)));
    const bool empty = ctx.c.library.size() == 0;
    addFolder.setVisible (empty);
    // Side by side when there's room; one above another in a narrow panel.
    const bool across = r.getWidth() > theme::px (420);
    const int n = (int) facetColumns.size();
    for (int k = 0; k < n; ++k)
    {
        auto& c = *facetColumns[(size_t) k];
        c.setVisible (! empty);
        if (across) c.setBounds (r.removeFromLeft (r.getWidth() / (n - k)).reduced (2, 0));
        else c.setBounds (r.removeFromTop (r.getHeight() / (n - k)).reduced (0, 2));
    }
}

void LibraryView::timerCallback()
{
    stopTimer();
    if (! dirty) return;
    dirty = false;
    Query q (searchField.getText().toStdString());
    found = ctx.c.library.search (q);
    refilter (0);
    resized();
    repaint();
}

void LibraryView::refilter (int from)
{
    std::vector<const Track*> tracks;
    if (from == 0)
        for (auto i : found) tracks.push_back (&ctx.c.library.tracks()[i]);
    for (int k = from; k < (int) facetColumns.size(); ++k)
    {
        if (k > 0) tracks = facetColumns[(size_t) k - 1]->output();
        facetColumns[(size_t) k]->fill (tracks);
    }
    auto locs = chosenLocations();
    count.setText (String ((int) locs.size()) + " of " + String ((int) ctx.c.library.size()) + " tracks", dontSendNotification);
    // Shown in the Library Selection playlist, as it's chosen.
    if (ctx.c.library.size() > 0 && ctx.c.settings.getBool ("librarySendsSelection", true))
    {
        const int p = ctx.c.librarySelectionPlaylist();
        auto& list = ctx.c.playlists.lists[(size_t) p];
        if (list.items != locs)
        {
            list.items = locs;
            ctx.c.playlistEdited (p);
        }
    }
}

std::vector<std::string> LibraryView::chosenLocations()
{
    std::vector<std::string> out;
    if (facetColumns.empty()) return out;
    auto tracks = facetColumns.back()->output();
    // In the library's own order: album artist, date, album, disc, track.
    std::vector<const Track*> sorted (tracks);
    sortByPattern (sorted, Library::defaultSortPattern(), false);
    for (auto* t : sorted) out.push_back (t->location);
    return out;
}

void LibraryView::send (bool play)
{
    const int p = ctx.c.librarySelectionPlaylist();
    ctx.c.playlists.lists[(size_t) p].items = chosenLocations();
    ctx.c.playlistEdited (p);
    ctx.c.choosePlaylist (p);
    if (play) ctx.c.playItem (p, 0);
}

} // namespace kp
