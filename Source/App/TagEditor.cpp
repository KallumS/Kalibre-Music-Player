// The tag editor (decision 0009): every chosen track's fields at once; a
// field that differs between them shows <multiple values> and is left as
// each has it unless changed. "; " between values makes several.
#include "Dialogs.h"
#include "TagIO.h"
#include "Tagging.h"
#include "TitleFormat.h"

namespace kp::dialogs
{
using namespace juce;

namespace
{
String u8 (const std::string& s) { return String::fromUTF8 (s.c_str()); }

class TagEditor : public Component, private TableListBoxModel
{
public:
    TagEditor (Context& x, std::vector<std::string> locs) : ctx (x), locations (std::move (locs))
    {
        std::vector<const Track*> tracks;
        for (auto& l : locations) tracks.push_back (&ctx.c.track (l));
        for (auto& r : tagging::rowsFor (tracks)) rows.push_back ({ u8 (r.field), u8 (r.value), u8 (r.field), u8 (r.value), false });
        // Details of the files, for the one track or the first.
        if (! tracks.empty())
        {
            auto& t = *tracks.front();
            details = tracks.size() == 1
                ? u8 (tf::format ("%filename_ext%$crlf()%codec%[, %bitrate% kb/s][, %samplerate% Hz][, %bitspersample%-bit][, %channels%][, %length%][, %filesize_natural%]", t))
                : String ((int) tracks.size()) + " tracks";
            bool readOnly = false;
            for (auto& l : locations) readOnly = readOnly || ! tagio::canWrite (l);
            if (readOnly) details << String::fromUTF8 ("\nSome are inside archives or can't be changed: their tags are only read.");
        }
        table.setModel (this);
        table.getHeader().addColumn ("Field", 1, 160);
        table.getHeader().addColumn ("Value", 2, 400);
        table.setRowHeight (theme::px (24));
        table.setColour (ListBox::backgroundColourId, theme::roll);
        addAndMakeVisible (table);
        for (auto* b : { &add, &remove, &number, &apply, &cancel }) addAndMakeVisible (b);
        add.onClick = [this] { rows.push_back ({ "", "", "", "", true }); table.updateContent(); table.selectRow ((int) rows.size() - 1); editCell ((int) rows.size() - 1, 1); };
        remove.onClick = [this]
        {
            const int r = table.getSelectedRow();
            if (r >= 0 && r < (int) rows.size()) { rows[(size_t) r].value = {}; rows[(size_t) r].removed = true; table.repaint(); }
        };
        number.setEnabled (locations.size() > 1);
        number.onClick = [this] { autoNumber = true; ctx.c.say ("Track numbers are given 1 to " + String ((int) locations.size()) + " in the playlist's order when you click Apply."); };
        apply.onClick = [this] { save(); };
        cancel.onClick = [this] { if (auto* w = findParentComponentOfClass<DocumentWindow>()) w->closeButtonPressed(); };
    }

    void paint (Graphics& g) override
    {
        g.fillAll (theme::ground);
        g.setColour (theme::textSecondary);
        g.setFont (theme::font (theme::smallPoints));
        g.drawFittedText (details, getLocalBounds().reduced (12).removeFromTop (theme::px (36)), Justification::topLeft, 3);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromTop (theme::px (40));
        auto bottom = r.removeFromBottom (theme::px (30));
        cancel.setBounds (bottom.removeFromRight (90));
        bottom.removeFromRight (8);
        apply.setBounds (bottom.removeFromRight (90));
        add.setBounds (bottom.removeFromLeft (100));
        bottom.removeFromLeft (8);
        remove.setBounds (bottom.removeFromLeft (110));
        bottom.removeFromLeft (8);
        number.setBounds (bottom.removeFromLeft (150));
        r.removeFromBottom (8);
        table.setBounds (r);
        table.getHeader().setColumnWidth (2, r.getWidth() - 170);
    }

private:
    struct Row { String field, value, originalField, originalValue; bool added = false, removed = false; };
    Context& ctx;
    std::vector<std::string> locations;
    std::vector<Row> rows;
    String details;
    bool autoNumber = false;
    TableListBox table;
    theme::HouseButton add { "Add Field" }, remove { "Remove Field" }, number { "Number the Tracks" }, apply { "Apply" }, cancel { "Cancel" };

    int getNumRows() override { return (int) rows.size(); }
    void paintRowBackground (Graphics& g, int row, int, int, bool selected) override
    {
        if (selected) g.fillAll (theme::raised.brighter (0.15f));
        else if (row % 2) g.fillAll (theme::lane);
    }
    void paintCell (Graphics& g, int row, int col, int w, int h, bool) override
    {
        if (row < 0 || row >= (int) rows.size()) return;
        auto& r = rows[(size_t) row];
        const bool multi = r.value == tagging::multipleValues();
        g.setColour (r.removed ? theme::textSecondary : multi ? theme::textSecondary : theme::text);
        g.setFont (theme::font (theme::bodyPoints, col == 1));
        g.drawText (col == 1 ? r.field.toUpperCase() : (r.removed ? String ("(removed)") : r.value), 6, 0, w - 12, h, Justification::centredLeft, true);
    }
    void cellDoubleClicked (int row, int col, const MouseEvent&) override { editCell (row, col); }
    void returnKeyPressed (int row) override { editCell (row, 2); }

    void editCell (int row, int col)
    {
        if (row < 0 || row >= (int) rows.size()) return;
        auto* w = new AlertWindow (col == 1 ? "Field" : rows[(size_t) row].field.toUpperCase(),
                                   col == 1 ? "The field's name (\"artist\", \"album artist\", \"mood\")." : "Several values: put \"; \" between them.",
                                   MessageBoxIconType::NoIcon);
        w->addTextEditor ("v", col == 1 ? rows[(size_t) row].field : rows[(size_t) row].value);
        w->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
        w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        w->enterModalState (true, ModalCallbackFunction::create ([this, w, row, col] (int r)
        {
            if (r != 1) return;
            auto& x = rows[(size_t) row];
            if (col == 1) x.field = w->getTextEditorContents ("v").trim().toLowerCase();
            else { x.value = w->getTextEditorContents ("v"); x.removed = false; }
            table.repaint();
        }), true);
        if (auto* ed = w->getTextEditor ("v")) { ed->grabKeyboardFocus(); ed->selectAll(); }
    }

    void save()
    {
        std::vector<tagging::Change> changes;
        for (auto& r : rows)
        {
            if (r.field.isEmpty()) continue;
            const bool renamed = ! r.added && r.field != r.originalField;
            if (r.removed) { changes.push_back ({ r.originalField.toStdString(), "", {} }); continue; }
            if (r.added || renamed || r.value != r.originalValue)
                changes.push_back ({ r.field.toStdString(), r.value.toStdString(), renamed ? r.originalField.toStdString() : std::string() });
        }
        int written = 0;
        StringArray failed;
        for (size_t k = 0; k < locations.size(); ++k)
        {
            Tags tags = ctx.c.track (locations[k]).tags;
            tagging::apply (tags, changes);
            if (autoNumber)
            {
                tags["tracknumber"] = { std::to_string (k + 1) };
                tags["totaltracks"] = { std::to_string (locations.size()) };
            }
            String error;
            if (tagio::write (locations[k], tags, error)) ++written;
            else failed.add (u8 (location::fileName (locations[k])) + ": " + error);
        }
        ctx.c.tracksChanged (locations);
        if (failed.isEmpty()) ctx.c.say ("Tags written to " + String (written) + (written == 1 ? " file." : " files."));
        else ctx.c.say ("Tags written to " + String (written) + " files; not to " + failed.joinIntoString ("; ") + ".", true);
        if (auto* w = findParentComponentOfClass<DocumentWindow>()) w->closeButtonPressed();
    }
};

//==============================================================================
class GuessTags : public Component
{
public:
    GuessTags (Context& x, std::vector<std::string> locs) : ctx (x), locations (std::move (locs))
    {
        pattern.setText (ctx.c.settings.get ("guessPattern", "%tracknumber% - %artist% - %title%"));
        pattern.onTextChange = [this] { preview(); };
        addAndMakeVisible (pattern);
        result.setMultiLine (true);
        result.setReadOnly (true);
        addAndMakeVisible (result);
        for (auto* b : { &apply, &cancel }) addAndMakeVisible (b);
        apply.onClick = [this] { save(); };
        cancel.onClick = [this] { if (auto* w = findParentComponentOfClass<DocumentWindow>()) w->closeButtonPressed(); };
        preview();
    }
    void paint (Graphics& g) override
    {
        g.fillAll (theme::ground);
        g.setColour (theme::textSecondary);
        g.setFont (theme::font (theme::smallPoints));
        g.drawFittedText ("A pattern for the file names: fields between % signs, and \"/\" for a folder (\"%artist%/%album%/%tracknumber% %title%\"). "
                          "The fields it finds are written to each file's tags.", getLocalBounds().reduced (12).removeFromTop (theme::px (36)), Justification::topLeft, 3);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromTop (theme::px (40));
        pattern.setBounds (r.removeFromTop (theme::px (28)));
        auto bottom = r.removeFromBottom (theme::px (30));
        cancel.setBounds (bottom.removeFromRight (90));
        bottom.removeFromRight (8);
        apply.setBounds (bottom.removeFromRight (90));
        r.reduce (0, 8);
        result.setBounds (r);
    }

private:
    Context& ctx;
    std::vector<std::string> locations;
    TextEditor pattern, result;
    theme::HouseButton apply { "Apply" }, cancel { "Cancel" };

    void preview()
    {
        String out;
        int fits = 0;
        for (size_t k = 0; k < locations.size() && k < 200; ++k)
        {
            auto g = tagging::guessFromPath (pattern.getText().toStdString(), locations[k]);
            out << u8 (location::fileName (locations[k])) << "\n    ";
            if (! g) { out << "(doesn't fit the pattern)\n"; continue; }
            ++fits;
            StringArray parts;
            for (auto& [f, v] : *g) parts.add (u8 (f) + ": " + u8 (v.front()));
            out << parts.joinIntoString (",  ") << "\n";
        }
        result.setText (out, false);
        apply.setEnabled (fits > 0);
    }

    void save()
    {
        ctx.c.settings.set ("guessPattern", pattern.getText());
        int written = 0;
        for (auto& l : locations)
        {
            auto g = tagging::guessFromPath (pattern.getText().toStdString(), l);
            if (! g) continue;
            Tags tags = ctx.c.track (l).tags;
            for (auto& [f, v] : *g) tags[f] = v;
            String error;
            if (tagio::write (l, tags, error)) ++written;
        }
        ctx.c.tracksChanged (locations);
        ctx.c.say ("Tags from file names written to " + String (written) + " files.");
        if (auto* w = findParentComponentOfClass<DocumentWindow>()) w->closeButtonPressed();
    }
};
} // namespace

void tagEditor (Context& ctx, const std::vector<std::string>& locations)
{
    if (locations.empty()) { ctx.c.say ("Choose tracks in the playlist first."); return; }
    open (locations.size() == 1 ? String ("Tags: ") + u8 (location::fileName (locations.front())) : "Tags: " + String ((int) locations.size()) + " Tracks",
          std::make_unique<TagEditor> (ctx, locations), 640, 560);
}

void guessTags (Context& ctx, const std::vector<std::string>& locations)
{
    if (locations.empty()) { ctx.c.say ("Choose tracks in the playlist first."); return; }
    open ("Tags from File Names", std::make_unique<GuessTags> (ctx, locations), 640, 480);
}

} // namespace kp::dialogs
