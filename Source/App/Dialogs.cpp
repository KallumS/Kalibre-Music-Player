#include "Dialogs.h"
#include "Jobs.h"
#include "Sources.h"
#include "TagIO.h"
#include "TitleFormat.h"

#include <map>

namespace kp::dialogs
{
using namespace juce;

namespace
{
String u8 (const std::string& s) { return String::fromUTF8 (s.c_str()); }

struct Window : public DocumentWindow
{
    Window (const String& title) : DocumentWindow (title, theme::ground, DocumentWindow::closeButton) {}
    void closeButtonPressed() override;
    bool keyPressed (const KeyPress& k) override
    {
        if (k == KeyPress::escapeKey) { closeButtonPressed(); return true; }
        return DocumentWindow::keyPressed (k);
    }
};

OwnedArray<Window>& windows() { static OwnedArray<Window> w; return w; }

void Window::closeButtonPressed()
{
    // Deleted after this call returns: a button inside it may be what called.
    setVisible (false);
    MessageManager::callAsync ([this] { windows().removeObject (this); });
}
} // namespace

void open (const String& title, std::unique_ptr<Component> content, int width, int height, bool resizable)
{
    auto* w = new Window (title);
    w->setUsingNativeTitleBar (true);
    content->setSize (theme::px (width), theme::px (height));
    w->setContentOwned (content.release(), true);
    w->setResizable (resizable, false);
    if (resizable) w->setResizeLimits (theme::px (360), theme::px (240), 4000, 4000);
    w->centreWithSize (w->getWidth(), w->getHeight());
    w->setVisible (true);
    w->toFront (true);
    windows().add (w);
}

void closeAll() { windows().clear(); }

void text (const String& title, const String& body)
{
    auto t = std::make_unique<TextEditor>();
    t->setMultiLine (true);
    t->setReadOnly (true);
    t->setFont (theme::font());
    t->setText (body, false);
    open (title, std::move (t), 640, 600);
}

//==============================================================================
// Renaming, moving and copying files by a pattern.
namespace
{
class FileOperation : public Component
{
public:
    FileOperation (Context& x, std::vector<std::string> locs, FileOp o) : ctx (x), op (o)
    {
        for (auto& l : locs) if (! location::isArchive (l)) locations.push_back (l);
        skipped = (int) (locs.size() - locations.size());
        const auto key = op == FileOp::rename ? "renamePattern" : "movePattern";
        pattern.setText (ctx.c.settings.get (key, op == FileOp::rename ? "%tracknumber% - %title%" : "%album artist%/%album%/%tracknumber% - %title%"), false);
        pattern.onTextChange = [this] { preview(); };
        addAndMakeVisible (pattern);
        if (op != FileOp::rename)
        {
            folder = File (ctx.c.settings.get ("moveFolder", ctx.c.library.folders.empty() ? File::getSpecialLocation (File::userMusicDirectory).getFullPathName()
                                                                                             : u8 (ctx.c.library.folders.front())));
            chooseFolder.onClick = [this]
            {
                chooser = std::make_unique<FileChooser> ("Where the files go", folder);
                chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this] (const FileChooser& fc)
                {
                    if (fc.getResult() != File()) { folder = fc.getResult(); preview(); repaint(); }
                });
            };
            addAndMakeVisible (chooseFolder);
        }
        list.setMultiLine (true);
        list.setReadOnly (true);
        addAndMakeVisible (list);
        for (auto* b : { &go, &cancel }) addAndMakeVisible (b);
        go.setButtonText (op == FileOp::rename ? "Rename" : op == FileOp::move ? "Move" : "Copy");
        go.onClick = [this] { run(); };
        cancel.onClick = [this] { if (auto* w = findParentComponentOfClass<DocumentWindow>()) w->closeButtonPressed(); };
        preview();
    }

    void paint (Graphics& g) override
    {
        g.fillAll (theme::ground);
        g.setColour (theme::textSecondary);
        g.setFont (theme::font (theme::smallPoints));
        String help = op == FileOp::rename ? "A pattern for each file's new name, in its own folder (the extension stays)."
                                           : "A pattern for each file's place in the folder below: \"/\" makes folders.";
        if (skipped > 0) help << " " << skipped << " inside archives are left as they are.";
        g.drawFittedText (help, getLocalBounds().reduced (12).removeFromTop (theme::px (34)), Justification::topLeft, 2);
        if (op != FileOp::rename)
        {
            g.setColour (theme::text);
            g.drawText (folder.getFullPathName(), getLocalBounds().reduced (12).withTrimmedTop (theme::px (76)).withHeight (theme::px (28)).withTrimmedLeft (theme::px (110)),
                        Justification::centredLeft, true);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromTop (theme::px (38));
        pattern.setBounds (r.removeFromTop (theme::px (28)));
        r.removeFromTop (8);
        if (op != FileOp::rename) chooseFolder.setBounds (r.removeFromTop (theme::px (28)).removeFromLeft (theme::px (100)));
        auto bottom = r.removeFromBottom (theme::px (30));
        cancel.setBounds (bottom.removeFromRight (90));
        bottom.removeFromRight (8);
        go.setBounds (bottom.removeFromRight (100));
        r.reduce (0, 8);
        list.setBounds (r);
    }

private:
    Context& ctx;
    FileOp op;
    std::vector<std::string> locations;
    int skipped = 0;
    File folder;
    TextEditor pattern, list;
    theme::HouseButton chooseFolder { String::fromUTF8 ("Folder\xe2\x80\xa6") }, go { "Rename" }, cancel { "Cancel" };
    std::unique_ptr<FileChooser> chooser;

    File target (const std::string& l)
    {
        const File from (u8 (l));
        auto rel = u8 (tf::formatPath (pattern.getText().toStdString(), ctx.c.track (l)));
        if (rel.isEmpty()) return from;
        if (op == FileOp::rename) rel = rel.replaceCharacter ('/', '_');
        rel = rel.replaceCharacter ('/', File::getSeparatorChar());
        return (op == FileOp::rename ? from.getParentDirectory() : folder).getChildFile (rel + from.getFileExtension());
    }

    void preview()
    {
        String out;
        for (size_t k = 0; k < locations.size() && k < 300; ++k)
            out << File (u8 (locations[k])).getFileName() << String::fromUTF8 ("  \xe2\x86\x92  ")
                << (op == FileOp::rename ? target (locations[k]).getFileName() : target (locations[k]).getRelativePathFrom (folder)) << "\n";
        list.setText (out, false);
    }

    void run()
    {
        ctx.c.settings.set (op == FileOp::rename ? "renamePattern" : "movePattern", pattern.getText());
        if (op != FileOp::rename) ctx.c.settings.set ("moveFolder", folder.getFullPathName());
        int done = 0;
        StringArray failed;
        // What's playing can't move under the decoder; it's stopped first if it's among them.
        if (auto t = ctx.c.nowPlayingTrack(); t && op == FileOp::move && std::find (locations.begin(), locations.end(), t->location) != locations.end()) ctx.c.stop();
        for (auto& l : locations)
        {
            const File from (u8 (l)), to = target (l);
            String error;
            if (moveFile (from, to, op == FileOp::copy, error))
            {
                ++done;
                if (op != FileOp::copy) ctx.c.fileMoved (l, to.getFullPathName().toStdString());
            }
            else failed.add (from.getFileName() + ": " + error);
        }
        ctx.c.playlistEdited (ctx.c.playlists.active);
        const String verb = op == FileOp::rename ? "renamed" : op == FileOp::move ? "moved" : "copied";
        if (failed.isEmpty()) ctx.c.say (String (done) + (done == 1 ? " file " : " files ") + verb + ".");
        else ctx.c.say (String (done) + " files " + verb + "; not " + failed.joinIntoString ("; ") + ".", true);
        if (auto* w = findParentComponentOfClass<DocumentWindow>()) w->closeButtonPressed();
    }
};
} // namespace

void fileOperation (Context& ctx, const std::vector<std::string>& locations, FileOp op)
{
    if (locations.empty()) { ctx.c.say ("Choose tracks in the playlist first."); return; }
    const String title = op == FileOp::rename ? "Rename Files" : op == FileOp::move ? "Move Files" : "Copy Files";
    open (title, std::make_unique<FileOperation> (ctx, locations, op), 680, 520);
}

//==============================================================================
// ReplayGain.

void scanReplayGain (Context& ctx, const std::vector<std::string>& locations, bool albums)
{
    if (locations.empty()) { ctx.c.say ("Choose tracks in the playlist first."); return; }
    auto locs = std::make_shared<std::vector<std::string>> (locations);
    auto results = std::make_shared<std::vector<std::optional<rg::Result>>> (locations.size());
    // The album each belongs to, by its tags.
    auto groups = std::make_shared<std::vector<std::string>>();
    for (auto& l : locations) groups->push_back (albums ? tf::format ("%album artist%|%album%|%discnumber%", ctx.c.track (l)) : l);
    ctx.c.jobs.run ("Scanning ReplayGain", (int) locations.size(),
        [locs, results] (int i, Job& job)
        {
            rg::Result r;
            String error;
            if (measure ((*locs)[(size_t) i], r, error, &job.cancelled)) (*results)[(size_t) i] = std::move (r);
            else if (error != "stopped") job.addError (u8 (location::fileName ((*locs)[(size_t) i])) + ": " + error);
        },
        [&ctl = ctx.c, locs, results, groups, albums] (Job& job)
        {
            if (job.cancelled) { ctl.say ("ReplayGain scan stopped."); return; }
            std::map<std::string, rg::Result> albumResults;
            if (albums)
            {
                std::map<std::string, std::vector<const rg::Result*>> byAlbum;
                for (size_t k = 0; k < locs->size(); ++k) if ((*results)[k]) byAlbum[(*groups)[k]].push_back (&*(*results)[k]);
                for (auto& [key, list] : byAlbum) albumResults[key] = rg::album (list);
            }
            int written = 0, readOnly = 0;
            std::vector<std::string> changed;
            for (size_t k = 0; k < locs->size(); ++k)
            {
                if (! (*results)[k]) continue;
                const auto& l = (*locs)[k];
                const auto* a = albums ? &albumResults[(*groups)[k]] : nullptr;
                auto add = rg::tagsFor (*(*results)[k], a);
                if (! tagio::canWrite (l)) { ++readOnly; continue; }
                Tags tags = ctl.track (l).tags;
                for (auto& [f, v] : add) tags[f] = v;
                String error;
                if (tagio::write (l, tags, error)) { ++written; changed.push_back (l); }
                else job.addError (u8 (location::fileName (l)) + ": " + error);
            }
            ctl.tracksChanged (changed);
            String m = "ReplayGain written to " + String (written) + (written == 1 ? " file." : " files.");
            if (readOnly > 0) m << " " << readOnly << " inside archives were measured but can't keep tags.";
            if (! job.errorsSoFar().isEmpty()) m << " Not: " << job.errorsSoFar().joinIntoString ("; ");
            ctl.say (m, ! job.errorsSoFar().isEmpty());
        });
}

void removeReplayGain (Context& ctx, const std::vector<std::string>& locations)
{
    int done = 0;
    std::vector<std::string> changed;
    for (auto& l : locations)
    {
        Tags tags = ctx.c.track (l).tags;
        bool had = false;
        for (auto it = tags.begin(); it != tags.end();)
        {
            if (it->first.rfind ("replaygain_", 0) == 0) { it = tags.erase (it); had = true; }
            else ++it;
        }
        String error;
        if (had && tagio::write (l, tags, error)) { ++done; changed.push_back (l); }
    }
    ctx.c.tracksChanged (changed);
    ctx.c.say ("ReplayGain tags taken out of " + String (done) + (done == 1 ? " file." : " files."));
}

//==============================================================================
// Add-ons.
namespace
{
class AddonsView : public Component, private ListBoxModel, private Controller::Listener
{
public:
    explicit AddonsView (Context& x) : ctx (x)
    {
        list.setModel (this);
        list.setRowHeight (theme::px (44));
        list.setColour (ListBox::backgroundColourId, theme::roll);
        addAndMakeVisible (list);
        for (auto* b : { &import, &toggle, &reload, &reveal }) addAndMakeVisible (b);
        import.onClick = [this]
        {
            chooser = std::make_unique<FileChooser> ("Import an Add-on", File(), "*.lua;*.zip");
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::canSelectDirectories,
                                  [this] (const FileChooser& fc)
            {
                if (fc.getResult() == File()) return;
                const auto e = ctx.c.importAddon (fc.getResult());
                ctx.c.say (e.isEmpty() ? fc.getResult().getFileName() + " added." : "Not added: " + e + ".", e.isNotEmpty());
                list.updateContent();
                list.repaint();
            });
        };
        toggle.onClick = [this]
        {
            const int r = list.getSelectedRow();
            auto& all = ctx.c.addons->addons();
            if (r < 0 || r >= (int) all.size()) return;
            const auto id = u8 (all[(size_t) r].id);
            ctx.c.settings.set ("addon." + id + ".enabled", ! all[(size_t) r].enabled);
            ctx.c.loadAddons();
            list.updateContent();
            list.repaint();
        };
        reload.onClick = [this] { ctx.c.loadAddons(); list.updateContent(); list.repaint(); };
        reveal.onClick = [this] { ctx.c.settings.addonsFolder().createDirectory(); ctx.c.settings.addonsFolder().revealToUser(); };
        ctx.c.addListener (this);
    }
    ~AddonsView() override { ctx.c.removeListener (this); }
    void paint (Graphics& g) override
    {
        g.fillAll (theme::ground);
        g.setColour (theme::textSecondary);
        g.setFont (theme::font (theme::smallPoints));
        g.drawFittedText ("Add-ons are Lua scripts. Their menu items are in the Add-ons menu; Help > Writing Add-ons says how to make one.",
                          getLocalBounds().reduced (12).removeFromTop (theme::px (34)), Justification::topLeft, 2);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromTop (theme::px (38));
        auto bottom = r.removeFromBottom (theme::px (30));
        for (auto* b : { &import, &toggle, &reload, &reveal }) { b->setBounds (bottom.removeFromLeft (b->idealWidth())); bottom.removeFromLeft (8); }
        r.removeFromBottom (8);
        list.setBounds (r);
    }

private:
    Context& ctx;
    ListBox list;
    theme::HouseButton import { String::fromUTF8 ("Import\xe2\x80\xa6") }, toggle { "Turn On or Off" }, reload { "Load Again" },
                       reveal { "Show the Folder" };
    std::unique_ptr<FileChooser> chooser;

    int getNumRows() override { return ctx.c.addons ? (int) ctx.c.addons->addons().size() : 0; }
    void paintListBoxItem (int row, Graphics& g, int w, int h, bool selected) override
    {
        auto& all = ctx.c.addons->addons();
        if (row < 0 || row >= (int) all.size()) return;
        auto& a = all[(size_t) row];
        if (selected) g.fillAll (theme::raised);
        g.setColour (a.enabled ? theme::text : theme::textSecondary);
        g.setFont (theme::font (theme::bodyPoints, true));
        String head = u8 (a.name);
        if (! a.version.empty()) head << "  " << u8 (a.version);
        if (! a.enabled) head << "  (off)";
        g.drawText (head, 10, 2, w - 20, h / 2, Justification::bottomLeft, true);
        g.setFont (theme::font (theme::smallPoints));
        g.setColour (a.error.empty() ? theme::textSecondary : theme::warnText);
        g.drawText (a.error.empty() ? u8 (a.description.empty() ? a.id : a.description) : u8 (a.error), 10, h / 2, w - 20, h / 2 - 2, Justification::topLeft, true);
    }
    void messageChanged() override { list.repaint(); }
};
} // namespace

void addons (Context& ctx) { open ("Add-ons", std::make_unique<AddonsView> (ctx), 620, 460); }

//==============================================================================
String titleFormatHelp()
{
    return String::fromUTF8 (R"(Title Formatting

Patterns turn a track's tags into words, as foobar2000's do. The playlist's columns, Now Playing, renaming and moving files, the converter's file names and the library's browsing all use them.

FIELDS, between % signs:
  %artist%  %title%  %album%  %album artist%  %date%  %year%  %genre%  %composer%  %performer%  %comment%
  %tracknumber% (two digits)  %totaltracks%  %discnumber%  %totaldiscs%
  %length%  %length_seconds%  %bitrate%  %samplerate%  %bitspersample%  %channels%  %codec%
  %filename%  %filename_ext%  %ext%  %directoryname%  %path%  %filesize%  %filesize_natural%  %archive%
  %list_index%  %list_total%
  Any other tag by its name: %mood%, %label%, %replaygain_track_gain%.
  %artist% falls back to the album artist, composer or performer; %album artist% to the artist; %title% to the file's name.
  A field with no value shows "?".

[ ]  shows what's inside only if a field in it has a value:  [%album% - ]%title%
' '  shows what's inside as it is:  '[live]'

FUNCTIONS:
  $if(cond,then[,else])  $if2(a,else)  $if3(a,b,...,else)  $ifequal(a,b,then,else)  $ifgreater(a,b,then,else)  $iflonger(a,n,then,else)
  $select(n,a,b,...)  $and(...)  $or(...)  $not(x)  $xor(...)  $greater(a,b)  $longer(a,b)  $strcmp(a,b)  $stricmp(a,b)  $strstr(a,b)
  $add(a,b,...)  $sub  $mul  $div  $mod  $min  $max
  $upper(s)  $lower(s)  $caps(s)  $caps2(s)  $trim(s)  $len(s)  $left(s,n)  $cut(s,n)  $right(s,n)  $substr(s,from,to)
  $pad(s,n[,c])  $pad_right(s,n[,c])  $padcut(s,n)  $num(n,digits)  $replace(s,a,b,...)  $repeat(s,n)  $insert(s,t,n)
  $stripprefix(s[,prefixes])  $abbr(s)  $directory(path[,n])  $directory_path(path)  $ext(path)  $filename(path)
  $meta(field[,n])  $meta_sep(field,sep[,last])  $meta_num(field)  $meta_test(fields...)  $year(date)
  $crlf()  $tab([n])  $char(code)

EXAMPLES:
  %album artist%/[%date% - ]%album%/[%discnumber%-]%tracknumber% - %title%
  $if(%track artist%,%track artist% - )%title%
  $num(%tracknumber%,3). $caps(%title%)
)");
}

String queryHelp()
{
    return String::fromUTF8 (R"(Search Queries

Type words to find tracks with every word somewhere in their tags or file name:
  beatles abbey

Or a query, with keywords in capitals, as foobar2000's:
  artist HAS beatles                    the field holds these words
  genre IS jazz                         the field is exactly this (any one of its values)
  date AFTER 1969  /  date BEFORE 1980
  rating GREATER 3  /  LESS  /  EQUAL
  comment MISSING  /  comment PRESENT
  added DURING LAST 2 WEEKS             MINUTES, HOURS, DAYS, WEEKS
  %album artist% IS %artist%            a pattern compared with a pattern
  * HAS live                            any field
  NOT, AND, OR and ( ) combine them:
  genre IS jazz AND (date BEFORE 1970 OR artist HAS davis)
  ALL                                   everything
  ... SORT BY %date%  /  SORT DESCENDING BY %rating%

A query that can't be read is taken as words. Autoplaylists (File > New Autoplaylist) hold what a query finds, and follow the library.
)");
}

String addonsHelp()
{
    return String::fromUTF8 (R"(Writing Add-ons

An add-on is a Lua 5.4 script: a .lua file, or a folder with main.lua in it (or a .zip of one). File > Import Add-on puts one in the Add-ons folder; View > Add-ons lists them. The first comment lines name it:

  -- name: Now Playing Notifier
  -- version: 1.0
  -- author: You
  -- description: Says what's playing.

It reaches the player through the table "kalibre":

EVENTS  kalibre.on(event, function(...) end)
  "startup", "shutdown", "track_started" (track), "track_finished" (track), "paused", "resumed", "stopped",
  "seeked" (seconds), "volume_changed" (dB), "library_changed", "selection_changed"

MENUS AND TIMERS
  kalibre.menu("Title", fn)          an item in the Add-ons menu (and the playlist's right-click menu)
  kalibre.every(seconds, fn)         again and again; returns an id
  kalibre.after(seconds, fn)         once; returns an id
  kalibre.cancel(id)

PLAYBACK
  kalibre.play()  pause()  play_pause()  stop()  next()  previous()  seek(seconds)
  kalibre.position()  is_playing()  is_paused()  volume([dB])  now_playing()  queue(location or tracks)

THE LIBRARY AND PLAYLISTS (counted from 1)
  kalibre.library.query("genre IS jazz")  library.count()
  kalibre.playlist.names()  active([n])  items([n])  create(name)  add([n,] locations or tracks)  clear([n])  play(n, item)
  kalibre.selection()                the tracks chosen in the playlist

TAGS
  kalibre.tags.get(location)         a track
  kalibre.tags.set(location, { mood = "calm", comment = false })   false takes a field out

WORDS AND SETTINGS
  kalibre.message(text)              in the message line at the top
  print(...)                         in the add-on console (View > Add-on Console)
  kalibre.format(pattern[, track])   title formatting
  kalibre.format_time(seconds)       "3:05"
  kalibre.settings.get(key[, default])  settings.set(key, value)   kept for this add-on

A track is a table: location, length, codec, samplerate, channels, bitrate, bitspersample, filesize, tags (each a list of values), and artist, title, album, album_artist, date, genre, tracknumber, discnumber for ease.

An add-on that runs for more than a few seconds at once is stopped, and its error shown in View > Add-ons.
)");
}

} // namespace kp::dialogs
