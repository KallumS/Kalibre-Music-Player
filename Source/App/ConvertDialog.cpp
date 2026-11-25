// The converter and CD ripping (decision 0010): a format and its quality,
// a folder and a pattern for the files' names, then a job on every core.
#include "Dialogs.h"
#include "Jobs.h"
#include "TitleFormat.h"

namespace kp::dialogs
{
using namespace juce;

namespace
{
String u8 (const std::string& s) { return String::fromUTF8 (s.c_str()); }

// The settings both share, kept between uses.
class EncodeOptions : public Component
{
public:
    std::function<void()> onChange;

    explicit EncodeOptions (Controller& c) : ctl (c)
    {
        int id = 1;
        for (auto& f : encodeFormats()) format.addItem (u8 (f.name), id++);
        const auto saved = ctl.settings.get ("convertFormat", "flac").toStdString();
        for (size_t k = 0; k < encodeFormats().size(); ++k) if (encodeFormats()[k].id == saved) format.setSelectedId ((int) k + 1, dontSendNotification);
        if (format.getSelectedId() == 0) format.setSelectedId (1, dontSendNotification);
        format.onChange = [this] { refreshQuality(); changed(); };
        quality.onChange = [this] { changed(); };
        folder = File (ctl.settings.get ("convertFolder", File::getSpecialLocation (File::userMusicDirectory).getChildFile ("Converted").getFullPathName()));
        pattern.setText (ctl.settings.get ("convertPattern", "%album artist%/%album%/[%discnumber%-]%tracknumber% - %title%"), false);
        pattern.onTextChange = [this] { changed(); };
        chooseFolder.onClick = [this]
        {
            chooser = std::make_unique<FileChooser> ("Where the files go", folder);
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this] (const FileChooser& fc)
            {
                if (fc.getResult() != File()) { folder = fc.getResult(); changed(); }
            });
        };
        applyGain.setToggleState (ctl.settings.getBool ("convertApplyGain", false), dontSendNotification);
        applyGain.setTooltip ("The ReplayGain is applied to the sound itself, and left out of the tags.");
        applyGain.onClick = [this] { changed(); };
        overwrite.setToggleState (ctl.settings.getBool ("convertOverwrite", false), dontSendNotification);
        overwrite.onClick = [this] { changed(); };
        for (auto* c : std::initializer_list<Component*> { &format, &quality, &chooseFolder, &pattern, &applyGain, &overwrite }) addAndMakeVisible (c);
        refreshQuality();
    }

    ConvertOptions options() const
    {
        ConvertOptions o;
        const auto& f = encodeFormats()[(size_t) jmax (0, format.getSelectedId() - 1)];
        o.encode.format = f.id;
        const int q = quality.getSelectedId();
        if (f.hasBitDepth) o.encode.bitDepth = q == 2 ? 24 : 16;
        else if (f.id == "mp3") { if (q <= 10) o.encode.quality = q - 1; else o.encode.bitrate = q; }
        else if (f.id == "vorbis") o.encode.quality = q - 1;
        else o.encode.bitrate = q;
        o.folder = folder;
        o.pattern = pattern.getText().toStdString();
        o.applyReplayGain = applyGain.getToggleState();
        o.replayGain = ctl.replayGain;
        o.overwrite = overwrite.getToggleState();
        return o;
    }

    void paint (Graphics& g) override
    {
        g.setColour (theme::textSecondary);
        g.setFont (theme::font (theme::smallPoints));
        auto r = getLocalBounds();
        for (auto* label : { "Format", "Quality", "Folder", "File Names" })
            g.drawText (label, r.removeFromTop (theme::px (32)).removeFromLeft (theme::px (90)), Justification::centredLeft);
        g.setColour (theme::text);
        g.drawText (folder.getFullPathName(), Rectangle<int> (theme::px (90) + theme::px (110), theme::px (64), getWidth() - theme::px (210), theme::px (32)),
                    Justification::centredLeft, true);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto row = [&] { auto x = r.removeFromTop (theme::px (32)); x.removeFromLeft (theme::px (90)); return x.reduced (0, 2); };
        format.setBounds (row().removeFromLeft (theme::px (260)));
        quality.setBounds (row().removeFromLeft (theme::px (260)));
        chooseFolder.setBounds (row().removeFromLeft (theme::px (100)));
        pattern.setBounds (row());
        applyGain.setBounds (r.removeFromTop (theme::px (26)).withTrimmedLeft (theme::px (90)));
        overwrite.setBounds (r.removeFromTop (theme::px (26)).withTrimmedLeft (theme::px (90)));
    }

    void remember()
    {
        auto o = options();
        ctl.settings.set ("convertFormat", String (o.encode.format));
        ctl.settings.set ("convertQuality", quality.getSelectedId());
        ctl.settings.set ("convertFolder", folder.getFullPathName());
        ctl.settings.set ("convertPattern", pattern.getText());
        ctl.settings.set ("convertApplyGain", applyGain.getToggleState());
        ctl.settings.set ("convertOverwrite", overwrite.getToggleState());
    }

private:
    Controller& ctl;
    ComboBox format, quality;
    theme::HouseButton chooseFolder { String::fromUTF8 ("Choose\xe2\x80\xa6") };
    TextEditor pattern;
    ToggleButton applyGain { "Apply ReplayGain to the Sound" }, overwrite { "Replace Files Already There" };
    File folder;
    std::unique_ptr<FileChooser> chooser;

    void changed() { repaint(); if (onChange) onChange(); }

    void refreshQuality()
    {
        quality.clear (dontSendNotification);
        const auto& f = encodeFormats()[(size_t) jmax (0, format.getSelectedId() - 1)];
        if (f.hasBitDepth) { quality.addItem ("16-bit", 1); quality.addItem ("24-bit", 2); }
        else if (f.id == "mp3")
        {
            for (int v = 0; v <= 6; ++v) quality.addItem ("VBR V" + String (v) + (v == 0 ? " (best)" : v == 2 ? " (about 190 kb/s)" : ""), v + 1);
            for (int b : f.bitrates) quality.addItem (String (b) + " kb/s constant", b);
        }
        else if (f.id == "vorbis") for (int q = 0; q <= 10; ++q) quality.addItem ("Quality " + String (q), q + 1);
        else for (int b : f.bitrates) quality.addItem (String (b) + " kb/s", b);
        const int saved = ctl.settings.getInt ("convertQuality", 0);
        if (quality.indexOfItemId (saved) >= 0) quality.setSelectedId (saved, dontSendNotification);
        else quality.setSelectedItemIndex (f.id == "mp3" ? 0 : f.id == "vorbis" ? 6 : f.hasBitDepth ? 0 : jmin (2, quality.getNumItems() - 1), dontSendNotification);
    }
};

void runConversion (Context& ctx, std::vector<Track> tracks, ConvertOptions o, const String& title)
{
    auto made = std::make_shared<std::vector<std::string>> (tracks.size());
    auto shared = std::make_shared<std::vector<Track>> (std::move (tracks));
    ctx.c.jobs.run (title, (int) shared->size(),
        [shared, o, made] (int i, Job& job)
        {
            File out;
            String error;
            if (convert ((*shared)[(size_t) i], o, out, error, &job.cancelled)) (*made)[(size_t) i] = out.getFullPathName().toStdString();
            else if (error != "stopped") job.addError (String::fromUTF8 (location::fileName ((*shared)[(size_t) i].location).c_str()) + ": " + error);
        },
        [&ctl = ctx.c, made, title, o] (Job& job)
        {
            int n = 0;
            for (auto& m : *made) n += m.empty() ? 0 : 1;
            auto errors = job.errorsSoFar();
            if (errors.isEmpty()) ctl.say (title + ": " + String (n) + (n == 1 ? " file" : " files") + " made in " + o.folder.getFullPathName() + ".");
            else ctl.say (title + ": " + String (n) + " made, " + String (errors.size()) + " not: " + errors.joinIntoString ("; "), true);
        });
}

class ConvertDialog : public Component
{
public:
    ConvertDialog (Context& x, std::vector<std::string> locs) : ctx (x), locations (std::move (locs)), options (x.c)
    {
        options.onChange = [this] { preview(); };
        addAndMakeVisible (options);
        list.setMultiLine (true);
        list.setReadOnly (true);
        addAndMakeVisible (list);
        for (auto* b : { &start, &cancel }) addAndMakeVisible (b);
        start.onClick = [this]
        {
            options.remember();
            std::vector<Track> tracks;
            for (auto& l : locations) tracks.push_back (ctx.c.track (l));
            runConversion (ctx, std::move (tracks), options.options(), "Converting");
            if (auto* w = findParentComponentOfClass<DocumentWindow>()) w->closeButtonPressed();
        };
        cancel.onClick = [this] { if (auto* w = findParentComponentOfClass<DocumentWindow>()) w->closeButtonPressed(); };
        preview();
    }
    void paint (Graphics& g) override { g.fillAll (theme::ground); }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        options.setBounds (r.removeFromTop (theme::px (180)));
        auto bottom = r.removeFromBottom (theme::px (30));
        cancel.setBounds (bottom.removeFromRight (90));
        bottom.removeFromRight (8);
        start.setBounds (bottom.removeFromRight (120));
        r.reduce (0, 8);
        list.setBounds (r);
    }

private:
    Context& ctx;
    std::vector<std::string> locations;
    EncodeOptions options;
    TextEditor list;
    theme::HouseButton start { "Convert" }, cancel { "Cancel" };

    void preview()
    {
        auto o = options.options();
        String out;
        out << String ((int) locations.size()) << " tracks to " << u8 (o.encode.describe()) << ":\n";
        for (size_t k = 0; k < locations.size() && k < 100; ++k)
            out << convertTarget (ctx.c.track (locations[k]), o).getRelativePathFrom (o.folder) << "\n";
        list.setText (out, false);
    }
};

//==============================================================================
class RipDialog : public Component, private TableListBoxModel
{
public:
    RipDialog (Context& x, AudioCdDisc d) : ctx (x), disc (std::move (d)), options (x.c)
    {
        album.trackTitles.resize ((size_t) disc.tracks.size());
        album.trackArtists.resize ((size_t) disc.tracks.size());
        for (int k = 0; k < disc.tracks.size(); ++k) album.trackTitles[(size_t) k] = "Track " + std::to_string (k + 1);
        album.title = disc.name().toStdString();
        for (auto* e : { &artist, &title, &date }) { addAndMakeVisible (e); e->onTextChange = [this] { fromFields(); }; }
        addAndMakeVisible (options);
        table.setModel (this);
        table.getHeader().addColumn ("#", 1, 40);
        table.getHeader().addColumn ("Title", 2, 300);
        table.getHeader().addColumn ("Artist", 3, 200);
        table.getHeader().addColumn ("Length", 4, 70);
        table.setColour (ListBox::backgroundColourId, theme::roll);
        addAndMakeVisible (table);
        for (auto* b : { &start, &cancel }) addAndMakeVisible (b);
        status.setColour (Label::textColourId, theme::textSecondary);
        addAndMakeVisible (status);
        start.onClick = [this] { rip(); };
        cancel.onClick = [this] { if (auto* w = findParentComponentOfClass<DocumentWindow>()) w->closeButtonPressed(); };
        toFields();
        lookUpAlbum();
    }
    ~RipDialog() override { *alive = false; }

    void paint (Graphics& g) override
    {
        g.fillAll (theme::ground);
        g.setColour (theme::textSecondary);
        g.setFont (theme::font (theme::smallPoints));
        auto r = getLocalBounds().reduced (12);
        for (auto* l : { "Artist", "Album", "Year" }) g.drawText (l, r.removeFromTop (theme::px (30)).removeFromLeft (theme::px (90)), Justification::centredLeft);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        for (auto* e : { &artist, &title, &date }) e->setBounds (r.removeFromTop (theme::px (30)).withTrimmedLeft (theme::px (90)).reduced (0, 2));
        status.setBounds (r.removeFromTop (theme::px (22)));
        auto bottom = r.removeFromBottom (theme::px (30));
        cancel.setBounds (bottom.removeFromRight (90));
        bottom.removeFromRight (8);
        start.setBounds (bottom.removeFromRight (120));
        r.removeFromBottom (8);
        options.setBounds (r.removeFromBottom (theme::px (180)));
        r.removeFromBottom (8);
        table.setBounds (r);
    }

private:
    Context& ctx;
    AudioCdDisc disc;
    cd::Album album;
    EncodeOptions options;
    TextEditor artist, title, date;
    TableListBox table;
    Label status;
    theme::HouseButton start { "Rip" }, cancel { "Cancel" };
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    void toFields()
    {
        artist.setText (u8 (album.artist), false);
        title.setText (u8 (album.title), false);
        date.setText (u8 (album.date), false);
        table.updateContent();
        table.repaint();
    }
    void fromFields()
    {
        album.artist = artist.getText().toStdString();
        album.title = title.getText().toStdString();
        album.date = date.getText().toStdString();
    }

    void lookUpAlbum()
    {
        if (disc.toc.offsets.empty()) { status.setText ("The disc's table of contents couldn't be read: name the tracks here.", dontSendNotification); return; }
        status.setText (String::fromUTF8 ("Asking MusicBrainz about this disc\xe2\x80\xa6"), dontSendNotification);
        auto toc = disc.toc;
        auto a = alive;
        Thread::launch ([this, toc, a]
        {
            String error;
            auto found = lookUp (toc, error);
            MessageManager::callAsync ([this, a, found, error]
            {
                if (! *a) return;
                if (found.empty()) { status.setText ("Not found (" + error + "): name the tracks here.", dontSendNotification); return; }
                album = found.front();
                album.trackTitles.resize ((size_t) disc.tracks.size());
                album.trackArtists.resize ((size_t) disc.tracks.size());
                status.setText ("Found on MusicBrainz" + String (found.size() > 1 ? " (the first of " + String ((int) found.size()) + " releases)" : "") + ".",
                                dontSendNotification);
                toFields();
            });
        });
    }

    int getNumRows() override { return disc.tracks.size(); }
    void paintRowBackground (Graphics& g, int row, int, int, bool selected) override { if (selected) g.fillAll (theme::raised); else if (row % 2) g.fillAll (theme::lane); }
    void paintCell (Graphics& g, int row, int col, int w, int h, bool) override
    {
        String text;
        if (col == 1) text = String (row + 1);
        else if (col == 2) text = u8 (album.trackTitles[(size_t) row]);
        else if (col == 3) text = u8 (album.trackArtists[(size_t) row].empty() ? album.artist : album.trackArtists[(size_t) row]);
        else text = u8 (formatTime (disc.toc.trackSeconds (row)));
        g.setColour (theme::text);
        g.setFont (theme::font());
        g.drawText (text, 6, 0, w - 12, h, Justification::centredLeft, true);
    }
    void cellDoubleClicked (int row, int col, const MouseEvent&) override
    {
        if (col != 2 && col != 3) return;
        auto* w = new AlertWindow (col == 2 ? "Title" : "Artist", {}, MessageBoxIconType::NoIcon);
        w->addTextEditor ("v", col == 2 ? u8 (album.trackTitles[(size_t) row]) : u8 (album.trackArtists[(size_t) row]));
        w->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
        w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        w->enterModalState (true, ModalCallbackFunction::create ([this, w, row, col] (int r)
        {
            if (r != 1) return;
            (col == 2 ? album.trackTitles : album.trackArtists)[(size_t) row] = w->getTextEditorContents ("v").toStdString();
            table.repaint();
        }), true);
    }

    void rip()
    {
        fromFields();
        options.remember();
        std::vector<Track> tracks;
        for (int k = 0; k < disc.tracks.size(); ++k)
        {
            Track t;
            t.location = disc.tracks[k].getFullPathName().toStdString();
            t.tags = cd::tagsFor (album, k, disc.tracks.size());
            tracks.push_back (std::move (t));
        }
        runConversion (ctx, std::move (tracks), options.options(), "Ripping " + u8 (album.title));
        if (auto* w = findParentComponentOfClass<DocumentWindow>()) w->closeButtonPressed();
    }
};
} // namespace

void convert (Context& ctx, const std::vector<std::string>& locations)
{
    if (locations.empty()) { ctx.c.say ("Choose tracks in the playlist first."); return; }
    open ("Convert " + String ((int) locations.size()) + (locations.size() == 1 ? " Track" : " Tracks"), std::make_unique<ConvertDialog> (ctx, locations), 680, 560);
}

void rip (Context& ctx, const AudioCdDisc& disc)
{
    open ("Rip " + disc.name(), std::make_unique<RipDialog> (ctx, disc), 700, 720);
}

} // namespace kp::dialogs
