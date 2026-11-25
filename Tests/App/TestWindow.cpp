// The window: files added to a playlist, the library scanned and browsed,
// commands, the layout edited, add-ons run against the real controller.
#include "AppTestHelpers.h"
#include "Check.h"
#include "Commands.h"
#include "LibraryView.h"
#include "MainComponent.h"
#include "TagIO.h"

using namespace kp;

namespace
{
juce::File musicFolder()
{
    static juce::File dir;
    if (dir != juce::File()) return dir;
    dir = test::scratch().getChildFile ("Music");
    struct Song { const char* path; const char* artist; const char* album; const char* title; const char* genre; int track; };
    const Song songs[] = {
        { "Air/Moon Safari/01 La Femme.flac", "Air", "Moon Safari", "La Femme d'Argent", "Electronic", 1 },
        { "Air/Moon Safari/02 Sexy Boy.flac", "Air", "Moon Safari", "Sexy Boy", "Electronic", 2 },
        { "Miles Davis/Kind of Blue/01 So What.mp3", "Miles Davis", "Kind of Blue", "So What", "Jazz", 1 },
        { "Miles Davis/Kind of Blue/02 Freddie.m4a", "Miles Davis", "Kind of Blue", "Freddie Freeloader", "Jazz", 2 },
    };
    for (auto& s : songs)
    {
        const juce::String path (s.path);
        const auto ext = path.fromLastOccurrenceOf (".", false, false);
        auto f = test::write ("Music/" + path, test::fmt (ext == "m4a" ? "aac" : ext.toRawUTF8()), 44100, 22050, [] (int c, int i) { return test::sine (c, i); });
        juce::String e;
        tagio::write (f.getFullPathName().toStdString(),
                      { { "artist", { s.artist } }, { "album", { s.album } }, { "title", { s.title } }, { "genre", { s.genre } },
                        { "tracknumber", { std::to_string (s.track) } } }, e);
    }
    return dir;
}
} // namespace

TEST ("window: files dropped in a playlist, the library watched and browsed")
{
    auto settingsDir = test::scratch().getChildFile ("settings-window");
    Controller c (settingsDir, false);
    MainComponent main (c);
    main.setSize (1280, 800);

    // Added to the playlist, folders and all, in order.
    const int n = c.addFiles (c.playlists.active, { musicFolder() });
    CHECK_EQ (n, 4);
    CHECK (test::waitFor ([&] { return c.jobs.running().empty(); }));
    CHECK_EQ (c.track (c.active().items[0]).first ("title"), std::string ("La Femme d'Argent"));

    // The library: a folder added is scanned.
    c.addFolder (musicFolder());
    CHECK (test::waitFor ([&] { return c.library.size() == 4 && ! c.scanning(); }));
    CHECK_EQ ((int) c.library.size(), 4);

    // Browsing: a search narrows the Library Selection playlist.
    auto* lib = dynamic_cast<LibraryView*> (main.layoutView().find ("library"));
    CHECK (lib != nullptr);
    if (lib)
    {
        lib->setSearch ("genre IS jazz");
        const int p = c.librarySelectionPlaylist();
        CHECK_EQ (c.playlists.lists[(size_t) p].items.size(), (size_t) 2);
        lib->setSearch ("");
        CHECK_EQ (c.playlists.lists[(size_t) p].items.size(), (size_t) 4);
    }

    // An autoplaylist follows the library.
    const int ap = c.newPlaylist ("Air");
    c.playlists.lists[(size_t) ap].query = "artist IS air";
    c.refreshAutoPlaylists();
    CHECK_EQ (c.playlists.lists[(size_t) ap].items.size(), (size_t) 2);

    // Commands: sort, select all, remove duplicates.
    c.choosePlaylist (0);
    c.addFiles (0, { musicFolder().getChildFile ("Air") });
    CHECK_EQ (c.active().items.size(), (size_t) 6);
    main.commands.invokeDirectly (cmd::removeDuplicates, false);
    CHECK_EQ (c.active().items.size(), (size_t) 4);
    main.commands.invokeDirectly (cmd::sortByTitle, false);
    CHECK_EQ (c.track (c.active().items[0]).first ("title"), std::string ("Freddie Freeloader"));
    main.commands.invokeDirectly (cmd::selectAll, false);
    CHECK_EQ (c.selected.size(), (size_t) 4);

    // Kept and read back by a new controller.
    c.saveAll();
    Controller again (settingsDir, false);
    CHECK_EQ ((int) again.library.size(), 4);
    CHECK_EQ (again.playlists.lists.size(), c.playlists.lists.size());
}

TEST ("window: the layout is edited and kept")
{
    auto settingsDir = test::scratch().getChildFile ("settings-layout");
    {
        Controller c (settingsDir, false);
        MainComponent main (c);
        main.setSize (1200, 760);
        for (auto& preset : layout::presets())
        {
            main.layoutView().setLayout (preset.root);
            CHECK (main.layoutView().find (layout::panels (preset.root).front()) != nullptr);
        }
        auto root = layout::presets().front().root;
        layout::addTab (root, { 0, 0 }, "equalizer");
        main.layoutView().setLayout (root);
        main.layoutView().onChanged();
        main.layoutView().setEditing (true);
        main.layoutView().setEditing (false);
        CHECK (main.layoutView().find ("equalizer") != nullptr);
    }
    Controller c (settingsDir, false);
    MainComponent main (c);
    CHECK (main.layoutView().find ("equalizer") != nullptr);
}

TEST ("window: an add-on reaches the real player")
{
    auto settingsDir = test::scratch().getChildFile ("settings-addons");
    Controller c (settingsDir, false);
    c.addFiles (c.playlists.active, { musicFolder() });
    auto addon = test::scratch().getChildFile ("Counter.lua");
    addon.replaceWithText ("-- name: Counter\n"
                           "kalibre.menu('Count', function()\n"
                           "  kalibre.message(#kalibre.playlist.items() .. ' tracks, ' .. #kalibre.playlist.names() .. ' playlists')\n"
                           "end)\n");
    CHECK_EQ (c.importAddon (addon), juce::String());
    auto items = c.addons->menuItems();
    CHECK_EQ (items.size(), (size_t) 1);
    if (! items.empty()) c.addons->runMenu (items.front());
    CHECK_EQ (c.messageText, juce::String ("4 tracks, 1 playlists"));
    // A zip of a folder with main.lua.
    auto folder = test::scratch().getChildFile ("ZipAddon");
    folder.getChildFile ("main.lua").create();
    folder.getChildFile ("main.lua").replaceWithText ("-- name: Zipped\nkalibre.menu('Hello', function() kalibre.message('hi') end)\n");
    auto zip = test::scratch().getChildFile ("ZipAddon.zip");
    {
        juce::ZipFile::Builder b;
        b.addFile (folder.getChildFile ("main.lua"), 9, "ZipAddon/main.lua");
        juce::FileOutputStream out (zip);
        b.writeToStream (out, nullptr);
    }
    CHECK_EQ (c.importAddon (zip), juce::String());
    CHECK_EQ (c.addons->menuItems().size(), (size_t) 2);
}

TEST ("window: the example add-ons load without errors")
{
    Controller c (test::scratch().getChildFile ("settings-examples"), false);
    int loaded = 0;
    for (auto& f : juce::File (KP_SOURCE_DIR).getChildFile ("Addons").findChildFiles (juce::File::findFiles, false, "*.lua"))
    {
        const auto id = f.getFileName().toStdString();
        if (! c.addons->load (id, f.loadFileAsString().toStdString()))
            check::fail (__FILE__, __LINE__, id + ": " + c.addons->info (id)->error);
        ++loaded;
    }
    CHECK (loaded >= 4);
    CHECK (c.addons->menuItems().size() >= 10);
}
