#include "Check.h"
#include "Library.h"

using namespace kp;

static Track track (std::string loc, std::vector<std::string> artists, std::string album, std::string date = {})
{
    Track t;
    t.location = loc;
    t.tags["artist"] = artists;
    t.tags["album"] = { album };
    if (! date.empty()) t.tags["date"] = { date };
    return t;
}

TEST ("library: put, replace, remove, move")
{
    Library lib;
    lib.put (track ("/a/1.flac", { "A" }, "X"));
    auto t = track ("/a/2.flac", { "B" }, "Y");
    t.added = 42;
    lib.put (t);
    CHECK_EQ (lib.size(), (size_t) 2);
    // Replaced, keeping when it was first added.
    auto again = track ("/a/2.flac", { "B2" }, "Y");
    lib.put (again);
    CHECK_EQ (lib.size(), (size_t) 2);
    CHECK_EQ (lib.find ("/a/2.flac")->first ("artist"), std::string ("B2"));
    CHECK_EQ (lib.find ("/a/2.flac")->added, (std::int64_t) 42);
    CHECK (lib.move ("/a/2.flac", "/b/2.flac"));
    CHECK (lib.find ("/a/2.flac") == nullptr);
    CHECK (lib.find ("/b/2.flac") != nullptr);
    CHECK (lib.remove ("/a/1.flac"));
    CHECK_EQ (lib.size(), (size_t) 1);
}

TEST ("library: a folder taken out takes its tracks, and only its")
{
    Library lib;
    lib.put (track ("/Music/a.flac", { "A" }, "X"));
    lib.put (track ("/Music/Sub/b.flac", { "A" }, "X"));
    lib.put (track ("/Music2/c.flac", { "A" }, "X"));
    lib.put (track ("archive:/Music/x.zip|d.flac", { "A" }, "X"));
    CHECK_EQ (lib.removeUnder ("/Music/"), 3);
    CHECK_EQ (lib.size(), (size_t) 1);
    CHECK (lib.find ("/Music2/c.flac") != nullptr);
}

TEST ("library: kept and read back")
{
    Library lib;
    lib.folders = { "/Music" };
    auto t = track ("/Music/a.flac", { "A", "B" }, "X", "2001");
    t.length = 12.5;
    t.codec = "FLAC";
    t.sampleRate = 96000;
    lib.put (t);
    Library back;
    CHECK (back.fromJson (lib.toJson()));
    CHECK_EQ (back.folders, lib.folders);
    CHECK_EQ (back.size(), (size_t) 1);
    auto* r = back.find ("/Music/a.flac");
    CHECK (r != nullptr);
    CHECK_EQ (r->tags.at ("artist"), (std::vector<std::string> { "A", "B" }));
    CHECK_EQ (r->length, 12.5);
    CHECK_EQ (r->sampleRate, 96000);
    CHECK_EQ (r->codec, std::string ("FLAC"));
}

TEST ("facets: values with counts, several values counted under each")
{
    std::vector<Track> all { track ("/1", { "A", "B" }, "X", "1999-01-01"), track ("/2", { "a" }, "Y", "1999"), track ("/3", { "C" }, "Y") };
    std::vector<const Track*> ptrs;
    for (auto& t : all) ptrs.push_back (&t);
    auto f = facets (ptrs, "artist");
    CHECK_EQ (f.size(), (size_t) 3);
    CHECK_EQ (f[0].value, std::string ("A"));
    CHECK_EQ (f[0].count, 2);
    auto years = facets (ptrs, "year");
    CHECK_EQ (years.size(), (size_t) 2);
    CHECK_EQ (years[0].value, std::string (noValue()));
    CHECK_EQ (years[1].value, std::string ("1999"));
    CHECK_EQ (years[1].count, 2);
    CHECK (hasFacetValue (all[1], "artist", "A"));
    CHECK (hasFacetValue (all[2], "year", noValue()));
    auto albums = facets (ptrs, "%album artist% - %album%");
    CHECK_EQ (albums.size(), (size_t) 3);
}

TEST ("locations: archives")
{
    auto l = location::archive ("/Music/Live.zip", "CD1/01 Intro.flac");
    CHECK (location::isArchive (l));
    CHECK_EQ (location::container (l), std::string ("/Music/Live.zip"));
    CHECK_EQ (location::fileName (l), std::string ("01 Intro.flac"));
    CHECK_EQ (location::extension (l), std::string ("flac"));
    CHECK_EQ (location::directory (l), std::string ("/Music/Live.zip/CD1"));
    CHECK (isArchiveExtension ("x.RAR"));
    CHECK (isAudioExtension ("WMA"));
    CHECK (! isAudioExtension ("txt"));
}
