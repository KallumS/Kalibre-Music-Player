#include "Check.h"
#include "Tagging.h"

using namespace kp;

TEST ("tagging: guessing tags from file names")
{
    auto t = tagging::guessFromPath ("%tracknumber% - %artist% - %title%", "/Music/x/03 - Daft Punk - One More Time.mp3");
    CHECK (t.has_value());
    CHECK_EQ ((*t)["tracknumber"].front(), std::string ("03"));
    CHECK_EQ ((*t)["artist"].front(), std::string ("Daft Punk"));
    CHECK_EQ ((*t)["title"].front(), std::string ("One More Time"));
    // A title holding the separator: the earlier fields take as little as they can.
    t = tagging::guessFromPath ("%artist% - %title%", "/m/Band - Song - Live.flac");
    CHECK_EQ ((*t)["title"].front(), std::string ("Song - Live"));
    // Folders.
    t = tagging::guessFromPath ("%artist%/%album%/%tracknumber% %title%", "/Music/Air/Moon Safari/01 La Femme d'Argent.flac");
    CHECK (t.has_value());
    CHECK_EQ ((*t)["album"].front(), std::string ("Moon Safari"));
    CHECK_EQ ((*t)["title"].front(), std::string ("La Femme d'Argent"));
    CHECK_EQ ((*t)["artist"].front(), std::string ("Air"));
    CHECK (! tagging::guessFromPath ("%artist% - %title%", "/m/NoSeparator.flac").has_value());
    // Inside an archive.
    t = tagging::guessFromPath ("%tracknumber%. %title%", "archive:/m/a.zip|CD/01. Intro.wav");
    CHECK_EQ ((*t)["title"].front(), std::string ("Intro"));
}

TEST ("tagging: rows across tracks, and changes")
{
    Track a, b;
    a.tags["artist"] = { "X" };
    a.tags["album"] = { "Same" };
    b.tags["artist"] = { "Y", "Z" };
    b.tags["album"] = { "Same" };
    b.tags["mood"] = { "calm" };
    auto rows = tagging::rowsFor ({ &a, &b });
    auto find = [&] (const std::string& f) { for (auto& r : rows) if (r.field == f) return r; return tagging::Row {}; };
    CHECK_EQ (find ("artist").value, std::string (tagging::multipleValues()));
    CHECK_EQ (find ("album").value, std::string ("Same"));
    CHECK (find ("mood").differs);

    tagging::apply (b.tags, { { "artist", tagging::multipleValues(), {} }, { "genre", "Rock; Pop", {} }, { "album", "", {} },
                              { "feeling", tagging::multipleValues(), "mood" } });
    CHECK_EQ (b.tags["artist"], (std::vector<std::string> { "Y", "Z" }));
    CHECK_EQ (b.tags["genre"], (std::vector<std::string> { "Rock", "Pop" }));
    CHECK (! b.tags.count ("album"));
    CHECK (! b.tags.count ("mood"));
    CHECK_EQ (b.tags["feeling"], (std::vector<std::string> { "calm" }));
}

TEST ("tagging: numbering tracks")
{
    Tags a, b, c;
    std::vector<Tags*> all { &a, &b, &c };
    tagging::numberTracks (all, true);
    CHECK_EQ (c["tracknumber"].front(), std::string ("3"));
    CHECK_EQ (a["totaltracks"].front(), std::string ("3"));
}
