#include "PlaylistView.h"
#include "Commands.h"
#include "TitleFormat.h"

namespace kp
{
using namespace juce;

namespace
{
const String dragTag = "kalibre-player-rows";
String u8 (const std::string& s) { return String::fromUTF8 (s.c_str()); }

} // namespace

std::vector<PlaylistView::Column> PlaylistView::defaultColumns()
{
    return {
        { String::fromUTF8 ("\xe2\x96\xb6"), "", 28 },
        { "#", "%tracknumber%", 40 },
        { "Title", "%title%", 240 },
        { "Artist", "%artist%", 180 },
        { "Album", "%album%", 200 },
        { "Year", "%year%", 54 },
        { "Length", "%length%", 64 },
        { "Codec", "%codec%[ %bitrate%]", 90 },
    };
}

PlaylistView::PlaylistView (Context& x) : ctx (x)
{
    addAndMakeVisible (tabs);
    tabs.addChangeListener (this);
    tabs.onMenu = [this] (int i) { tabMenu (i); };
    addAndMakeVisible (addTab);
    addTab.onClick = [this] { ctx.c.newPlaylist(); };
    table.setModel (this);
    table.setMultipleSelectionEnabled (true);
    table.setColour (ListBox::backgroundColourId, theme::roll);
    table.setColour (ListBox::outlineColourId, Colours::transparentBlack);
    table.setRowHeight (theme::px (22));
    table.getHeader().setColour (TableHeaderComponent::backgroundColourId, theme::sunken);
    table.getHeader().setColour (TableHeaderComponent::textColourId, theme::textSecondary);
    table.getHeader().setColour (TableHeaderComponent::outlineColourId, theme::rule);
    table.getHeader().setPopupMenuActive (false);
    table.getHeader().addMouseListener (this, false);
    addAndMakeVisible (table);
    rebuildColumns();
    rebuildTabs();
    ctx.c.addListener (this);
    startTimerHz (4);
}

PlaylistView::~PlaylistView() { ctx.c.removeListener (this); }

void PlaylistView::rebuildColumns()
{
    columns.clear();
    if (auto xml = ctx.c.settings.getXml ("columns"))
        for (auto* e : xml->getChildIterator())
            columns.push_back ({ e->getStringAttribute ("name"), e->getStringAttribute ("pattern"), e->getIntAttribute ("width", 100) });
    if (columns.empty()) columns = defaultColumns();
    auto& h = table.getHeader();
    h.removeAllColumns();
    for (size_t k = 0; k < columns.size(); ++k)
    {
        const bool marker = columns[k].pattern.isEmpty();
        h.addColumn (columns[k].name, (int) k + 1, columns[k].width, 20, -1,
                     TableHeaderComponent::visible | TableHeaderComponent::resizable | TableHeaderComponent::draggable
                         | (marker ? 0 : TableHeaderComponent::sortable));
    }
}

void PlaylistView::saveColumns()
{
    XmlElement xml ("Columns");
    auto& h = table.getHeader();
    for (int i = 0; i < h.getNumColumns (true); ++i)
    {
        const int id = h.getColumnIdOfIndex (i, true);
        auto& col = columns[(size_t) id - 1];
        auto* e = xml.createNewChildElement ("Column");
        e->setAttribute ("name", col.name);
        e->setAttribute ("pattern", col.pattern);
        e->setAttribute ("width", h.getColumnWidth (id));
    }
    ctx.c.settings.setXml ("columns", &xml);
}

void PlaylistView::rebuildTabs()
{
    updatingTabs = true;
    tabs.clearTabs();
    auto& lists = ctx.c.playlists.lists;
    for (size_t k = 0; k < lists.size(); ++k)
    {
        auto name = u8 (lists[k].name);
        if ((int) k == ctx.c.playlists.playing && ctx.c.playing()) name = String::fromUTF8 ("\xe2\x96\xb6 ") + name;
        tabs.addTab (name, theme::sunken, -1);
    }
    tabs.setCurrentTabIndex (ctx.c.playlists.active, false);
    updatingTabs = false;
}

void PlaylistView::playlistsChanged()
{
    rebuildTabs();
    table.updateContent();
    SparseSet<int> rows;
    for (int i : ctx.c.selected) rows.addRange ({ i, i + 1 });
    table.setSelectedRows (rows, dontSendNotification);
    table.repaint();
}

void PlaylistView::changeListenerCallback (ChangeBroadcaster*)
{
    if (! updatingTabs) ctx.c.choosePlaylist (tabs.getCurrentTabIndex());
}

void PlaylistView::timerCallback()
{
    // The ▶ on the tab playing follows playback.
    const auto want = ctx.c.playing() ? ctx.c.playlists.playing : -1;
    static int shown = -2;
    if (want != shown) { shown = want; rebuildTabs(); }
}

void PlaylistView::paint (Graphics& g) { g.fillAll (theme::sunken); }

void PlaylistView::paintOverChildren (Graphics& g)
{
    if (dropAt >= 0)
    {
        const int y = table.getY() + table.getRowPosition (jmin (dropAt, getNumRows()), true).getY();
        g.setColour (theme::accent);
        g.fillRect (table.getX(), y - 1, table.getWidth(), 2);
    }
    if (getNumRows() == 0)
    {
        g.setColour (theme::textSecondary);
        g.setFont (theme::font());
        g.drawFittedText ("Drop music here, or choose File > Add Files, or pick from the library.",
                          table.getBounds().reduced (24).withTrimmedTop (table.getHeaderHeight() + 24), Justification::centredTop, 3);
    }
}

void PlaylistView::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (theme::px (28));
    addTab.setBounds (top.removeFromRight (theme::px (32)).reduced (2));
    tabs.setBounds (top);
    table.setBounds (r);
}

int PlaylistView::getNumRows() { return (int) ctx.c.active().items.size(); }

void PlaylistView::paintRowBackground (Graphics& g, int row, int, int, bool selected)
{
    const bool playingRow = ctx.c.playing() && ctx.c.playlists.playing == ctx.c.playlists.active && ctx.c.playingIndex() == row;
    if (selected) g.fillAll (theme::raised.brighter (0.15f));
    else if (row % 2) g.fillAll (theme::lane);
    if (playingRow)
    {
        g.setColour (theme::accent);
        g.fillRect (0, 0, 3, g.getClipBounds().getHeight() + 2);
    }
}

void PlaylistView::paintCell (Graphics& g, int row, int column, int w, int h, bool)
{
    auto& items = ctx.c.active().items;
    if (row < 0 || row >= (int) items.size() || column < 1 || column > (int) columns.size()) return;
    const auto& col = columns[(size_t) column - 1];
    const bool playingRow = ctx.c.playing() && ctx.c.playlists.playing == ctx.c.playlists.active && ctx.c.playingIndex() == row;
    if (col.pattern.isEmpty())
    {
        // The marker column: playing, paused or queued.
        String mark;
        if (playingRow) mark = String::fromUTF8 (ctx.c.paused() ? "\xe2\x80\x96" : "\xe2\x96\xb6");
        else
            for (size_t q = 0; q < ctx.c.upNext.size(); ++q)
                if (ctx.c.upNext[q] == std::make_pair (ctx.c.playlists.active, row)) mark = String ((int) q + 1);
        g.setColour (playingRow ? theme::accent : theme::textSecondary);
        g.setFont (theme::font (theme::smallPoints, true));
        g.drawText (mark, 0, 0, w, h, Justification::centred);
        return;
    }
    const auto& t = ctx.c.track (items[(size_t) row]);
    const auto text = u8 (tf::format (col.pattern.toStdString(), t, row + 1, (int) items.size()));
    g.setColour (playingRow ? theme::text : theme::text.withMultipliedAlpha (0.92f));
    g.setFont (theme::font (theme::bodyPoints, playingRow));
    const bool right = col.pattern.contains ("length") || col.pattern.contains ("tracknumber") || col.pattern.contains ("bitrate");
    g.drawText (text, 6, 0, w - 12, h, right && ! col.pattern.contains ("codec") ? Justification::centredRight : Justification::centredLeft, true);
}

String PlaylistView::getCellTooltip (int row, int)
{
    auto& items = ctx.c.active().items;
    if (row < 0 || row >= (int) items.size()) return {};
    return u8 (items[(size_t) row]);
}

void PlaylistView::cellDoubleClicked (int row, int, const MouseEvent&) { ctx.c.playItem (ctx.c.playlists.active, row); }

void PlaylistView::cellClicked (int row, int, const MouseEvent& e)
{
    if (! e.mods.isPopupMenu()) return;
    if (! table.isRowSelected (row)) table.selectRow (row);
    rowMenu();
}

void PlaylistView::backgroundClicked (const MouseEvent& e)
{
    table.deselectAllRows();
    if (e.mods.isPopupMenu()) rowMenu();
}

void PlaylistView::selectedRowsChanged (int)
{
    std::vector<int> rows;
    auto set = table.getSelectedRows();
    for (int k = 0; k < set.size(); ++k) rows.push_back (set[k]);
    ctx.c.setSelection (rows);
}

void PlaylistView::deleteKeyPressed (int) { ctx.commands.invokeDirectly (cmd::removeSelected, false); }
void PlaylistView::returnKeyPressed (int row) { ctx.c.playItem (ctx.c.playlists.active, row); }

void PlaylistView::sortOrderChanged (int id, bool forwards)
{
    if (id < 1 || id > (int) columns.size() || columns[(size_t) id - 1].pattern.isEmpty()) return;
    ctx.c.sortPlaylist (ctx.c.playlists.active, columns[(size_t) id - 1].pattern.toStdString(), ! forwards);
}

bool PlaylistView::keyPressed (const KeyPress& k)
{
    if (k == KeyPress::returnKey && table.getSelectedRow() >= 0) { ctx.c.playItem (ctx.c.playlists.active, table.getSelectedRow()); return true; }
    return false;
}

void PlaylistView::mouseDown (const MouseEvent& e)
{
    if (e.eventComponent == &table.getHeader() && e.mods.isPopupMenu()) columnMenu();
}

var PlaylistView::getDragSourceDescription (const SparseSet<int>&) { return dragTag; }

int PlaylistView::rowAt (int y)
{
    const int local = y - table.getY();
    const int row = table.getRowContainingPosition (10, local);
    if (row < 0) return local < table.getHeaderHeight() + table.getRowHeight() ? 0 : getNumRows();
    const auto bounds = table.getRowPosition (row, true);
    return local - bounds.getY() > bounds.getHeight() / 2 ? row + 1 : row;
}

bool PlaylistView::isInterestedInDragSource (const SourceDetails& d) { return d.description.toString() == dragTag; }
void PlaylistView::itemDragMove (const SourceDetails& d) { dropAt = rowAt (d.localPosition.y); repaint(); }

void PlaylistView::itemDropped (const SourceDetails& d)
{
    const int to = rowAt (d.localPosition.y);
    dropAt = -1;
    ctx.c.moveItems (ctx.c.playlists.active, ctx.c.selected, to);
    repaint();
}

void PlaylistView::fileDragMove (const StringArray&, int, int y) { dropAt = rowAt (y); repaint(); }

void PlaylistView::filesDropped (const StringArray& files, int, int y)
{
    const int at = rowAt (y);
    dropAt = -1;
    Array<File> fs;
    for (auto& f : files) fs.add (File (f));
    const int n = ctx.c.addFiles (ctx.c.playlists.active, fs, at);
    ctx.c.say (n == 0 ? String ("Nothing there is music the player knows.") : String (n) + (n == 1 ? " track added." : " tracks added."));
    repaint();
}

void PlaylistView::rowMenu()
{
    PopupMenu m;
    auto* cm = &ctx.commands;
    m.addCommandItem (cm, cmd::play, "Play");
    m.addCommandItem (cm, cmd::queueSelected);
    m.addSeparator();
    m.addCommandItem (cm, cmd::properties);
    m.addCommandItem (cm, cmd::guessTags);
    PopupMenu files;
    files.addCommandItem (cm, cmd::renameFiles);
    files.addCommandItem (cm, cmd::moveFiles);
    files.addCommandItem (cm, cmd::copyFiles);
    files.addCommandItem (cm, cmd::showInFinder);
    files.addCommandItem (cm, cmd::copyLocations);
    m.addSubMenu ("Files", files);
    m.addCommandItem (cm, cmd::convert);
    PopupMenu rgm;
    rgm.addCommandItem (cm, cmd::rgTrack);
    rgm.addCommandItem (cm, cmd::rgAlbum);
    rgm.addCommandItem (cm, cmd::rgRemove);
    m.addSubMenu ("ReplayGain", rgm);
    // Add-ons' own items work on the chosen tracks too.
    if (ctx.c.addons)
    {
        auto items = ctx.c.addons->menuItems();
        if (! items.empty())
        {
            PopupMenu am;
            for (auto& it : items) am.addItem (u8 (it.title), [this, it] { ctx.c.addons->runMenu (it); });
            m.addSubMenu ("Add-ons", am);
        }
    }
    m.addSeparator();
    m.addCommandItem (cm, cmd::removeSelected);
    m.addCommandItem (cm, cmd::cropSelected);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&table).withMousePosition());
}

void PlaylistView::editColumn (int index)
{
    const bool adding = index < 0;
    auto* w = new AlertWindow (adding ? "Add a Column" : "Change the Column",
                               "Its name, and a title-formatting pattern for what it shows (Help > Title Formatting lists the fields).",
                               MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", adding ? String ("Genre") : columns[(size_t) index].name, "Name:");
    w->addTextEditor ("pattern", adding ? String ("%genre%") : columns[(size_t) index].pattern, "Pattern:");
    w->addButton (adding ? "Add" : "Change", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    ui::present (w, ModalCallbackFunction::create ([this, w, index, adding] (int r)
    {
        if (r != 1) return;
        auto name = w->getTextEditorContents ("name").trim();
        auto pattern = w->getTextEditorContents ("pattern").trim();
        tf::Script check (pattern.toStdString());
        if (! check.error().empty()) { ctx.c.say ("That pattern can't be read: " + u8 (check.error()), true); return; }
        saveColumns();
        rebuildColumns();
        if (adding) columns.push_back ({ name, pattern, 120 });
        else { columns[(size_t) index].name = name; columns[(size_t) index].pattern = pattern; }
        XmlElement xml ("Columns");
        for (auto& c : columns)
        {
            auto* e = xml.createNewChildElement ("Column");
            e->setAttribute ("name", c.name);
            e->setAttribute ("pattern", c.pattern);
            e->setAttribute ("width", c.width);
        }
        ctx.c.settings.setXml ("columns", &xml);
        rebuildColumns();
        table.updateContent();
    }), true);
}

void PlaylistView::columnMenu()
{
    auto& h = table.getHeader();
    const int id = h.getColumnIdAtX (h.getMouseXYRelative().x);
    PopupMenu m;
    m.addItem (String::fromUTF8 ("Add a Column\xe2\x80\xa6"), [this] { editColumn (-1); });
    if (id > 0)
    {
        m.addItem (String::fromUTF8 ("Change \xe2\x80\x9c") + columns[(size_t) id - 1].name + String::fromUTF8 ("\xe2\x80\x9d\xe2\x80\xa6"), [this, id] { editColumn (id - 1); });
        m.addItem ("Remove " + columns[(size_t) id - 1].name, [this, id]
        {
            saveColumns();
            rebuildColumns();
            columns.erase (columns.begin() + id - 1);
            XmlElement xml ("Columns");
            for (auto& c : columns)
            {
                auto* e = xml.createNewChildElement ("Column");
                e->setAttribute ("name", c.name);
                e->setAttribute ("pattern", c.pattern);
                e->setAttribute ("width", c.width);
            }
            ctx.c.settings.setXml ("columns", &xml);
            rebuildColumns();
        });
    }
    m.addSeparator();
    m.addItem ("Back to the First Columns", [this] { ctx.c.settings.set ("columns", var()); rebuildColumns(); });
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&h).withMousePosition());
}

void PlaylistView::tabMenu (int index)
{
    PopupMenu m;
    m.addItem (String::fromUTF8 ("Rename\xe2\x80\xa6"), [this, index] { ctx.c.choosePlaylist (index); ctx.commands.invokeDirectly (cmd::renamePlaylist, true); });
    m.addItem (String::fromUTF8 ("Save as M3U\xe2\x80\xa6"), [this, index] { ctx.c.choosePlaylist (index); ctx.commands.invokeDirectly (cmd::savePlaylist, true); });
    if (auto* p = ctx.c.playlists.get (index); p && p->isAuto())
        m.addItem ("Make It a Playlist of Its Own", [this, index] { if (auto* q = ctx.c.playlists.get (index)) { q->query.clear(); ctx.c.playlistEdited (index); } });
    m.addSeparator();
    m.addItem ("Remove", [this, index] { ctx.c.removePlaylist (index); });
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&tabs).withMousePosition());
}

} // namespace kp
