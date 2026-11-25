#include "Check.h"
#include "Library.h"
#include "Query.h"

using namespace kp;

static std::vector<Track> tracks()
{
    auto make = [] (std::string loc, std::string artist, std::string title, std::string album, std::string date, std::string genre, std::string rating = {})
    {
        Track t;
        t.location = loc;
        t.tags["artist"] = { artist };
        t.tags["title"] = { title };
        t.tags["album"] = { album };
        t.tags["date"] = { date };
        t.tags["genre"] = { genre };
        if (! rating.empty()) t.tags["rating"] = { rating };
        return t;
    };
    return {
        make ("/m/1.flac", "The Beatles", "Come Together", "Abbey Road", "1969", "Rock", "5"),
        make ("/m/2.mp3", "Miles Davis", "So What", "Kind of Blue", "1959", "Jazz", "4"),
        make ("/m/3.ogg", "Radiohead", "Airbag", "OK Computer", "1997", "Rock"),
        make ("/m/4.opus", "John Coltrane", "Giant Steps", "Giant Steps", "1960", "Jazz", "3"),
    };
}

static int count (const std::string& q)
{
    Query query (q, 1000000);
    int n = 0;
    for (auto& t : tracks()) n += query.matches (t) ? 1 : 0;
    return n;
}

TEST ("queries: plain words find any field")
{
    CHECK_EQ (count ("beatles"), 1);
    CHECK_EQ (count ("giant"), 1);
    CHECK_EQ (count ("jazz"), 2);
    CHECK_EQ (count ("jazz steps"), 1);
    CHECK_EQ (count ("opus"), 1);          // the file's name
    CHECK_EQ (count (""), 4);
}

TEST ("queries: operators")
{
    CHECK_EQ (count ("artist HAS miles"), 1);
    CHECK_EQ (count ("genre IS rock"), 2);
    CHECK_EQ (count ("genre IS roc"), 0);
    CHECK_EQ (count ("date AFTER 1960"), 2);
    CHECK_EQ (count ("date BEFORE 1960"), 1);
    CHECK_EQ (count ("rating GREATER 3"), 2);
    CHECK_EQ (count ("rating LESS 4"), 1);
    CHECK_EQ (count ("rating MISSING"), 1);
    CHECK_EQ (count ("rating PRESENT"), 3);
    CHECK_EQ (count ("genre IS jazz AND rating GREATER 3"), 1);
    CHECK_EQ (count ("genre IS jazz OR date AFTER 1990"), 3);
    CHECK_EQ (count ("NOT genre IS jazz"), 2);
    CHECK_EQ (count ("(genre IS jazz OR artist HAS beatles) AND NOT rating EQUAL 3"), 2);
    CHECK_EQ (count ("%album% IS %title%"), 1);
    CHECK_EQ (count ("* HAS computer"), 1);
    CHECK_EQ (count ("ALL"), 4);
}

TEST ("queries: a query that can't be read is taken as words")
{
    Query q ("artist HAS", 0);
    CHECK (! q.error().empty());
    CHECK_EQ (count ("(genre IS jazz"), 0);
}

TEST ("queries: added DURING LAST")
{
    Track t;
    t.location = "/x.flac";
    const std::int64_t day = 86400000;
    t.added = 100 * day;
    CHECK (Query ("added DURING LAST 2 WEEKS", 105 * day).matches (t));
    CHECK (! Query ("added DURING LAST 2 DAYS", 105 * day).matches (t));
    CHECK (Query ("added DURING LAST WEEK", 101 * day).matches (t));
}

TEST ("queries: SORT BY")
{
    Library lib;
    for (auto& t : tracks()) lib.put (t);
    auto found = lib.search (Query ("genre IS jazz SORT BY %date%"));
    CHECK_EQ (found.size(), (size_t) 2);
    CHECK_EQ (lib.tracks()[found[0]].first ("artist"), std::string ("Miles Davis"));
    found = lib.search (Query ("ALL SORT DESCENDING BY %date%"));
    CHECK_EQ (lib.tracks()[found[0]].first ("artist"), std::string ("Radiohead"));
}

TEST ("natural order: numbers as numbers")
{
    CHECK (naturalCompare ("Track 2", "Track 10") < 0);
    CHECK (naturalCompare ("track 02", "Track 2") == 0);
    CHECK (naturalCompare ("b", "A") > 0);
}
