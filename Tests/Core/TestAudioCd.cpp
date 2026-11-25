#include "AudioCd.h"
#include "Check.h"

#include <cmath>

using namespace kp;

static std::string hex (const std::string& raw)
{
    static const char* d = "0123456789abcdef";
    std::string out;
    for (unsigned char c : raw) { out += d[c >> 4]; out += d[c & 15]; }
    return out;
}

TEST ("cd: SHA-1")
{
    CHECK_EQ (hex (cd::sha1 ("abc")), std::string ("a9993e364706816aba3e25717850c26c9cd0d89d"));
    CHECK_EQ (hex (cd::sha1 ("")), std::string ("da39a3ee5e6b4b0d3255bfef95601890afd80709"));
}

TEST ("cd: MusicBrainz's disc ID, from its own example")
{
    cd::Toc toc;
    toc.firstTrack = 1;
    toc.lastTrack = 6;
    toc.leadOut = 95462;
    toc.offsets = { 150, 15363, 32314, 46592, 63414, 80489 };
    CHECK_EQ (cd::discId (toc), std::string ("49HHV7Eb8UKF3aQiNmu1GR8vKTY-"));
    CHECK (cd::lookupUrl (toc).find ("toc=1+6+95462+150+15363") != std::string::npos);
    CHECK (std::abs (toc.trackSeconds (0) - (15363 - 150) / 75.0) < 1e-9);
}

TEST ("cd: an answer read into an album and its tags")
{
    const std::string answer = R"({"releases":[{"id":"r1","title":"Album","date":"2001-02-03",
        "artist-credit":[{"name":"Band","joinphrase":" & "},{"name":"Friend"}],
        "media":[{"position":1,"discs":[{"id":"other"}],"tracks":[{"title":"x"}]},
                 {"position":2,"discs":[{"id":"ID"}],"tracks":[{"title":"One"},{"title":"Two","artist-credit":[{"name":"Guest"}]}]}]}]})";
    auto albums = cd::parseLookup (answer, "ID");
    CHECK_EQ (albums.size(), (size_t) 1);
    auto& a = albums[0];
    CHECK_EQ (a.artist, std::string ("Band & Friend"));
    CHECK_EQ (a.discNumber, 2);
    CHECK_EQ (a.totalDiscs, 2);
    CHECK_EQ (a.trackTitles, (std::vector<std::string> { "One", "Two" }));
    auto tags = cd::tagsFor (a, 1, 2);
    CHECK_EQ (tags["artist"].front(), std::string ("Guest"));
    CHECK_EQ (tags["album artist"].front(), std::string ("Band & Friend"));
    CHECK_EQ (tags["tracknumber"].front(), std::string ("2"));
    CHECK_EQ (tags["discnumber"].front(), std::string ("2"));
}
