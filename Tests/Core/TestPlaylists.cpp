#include "Check.h"
#include "Playlists.h"

#include <set>

using namespace kp;

TEST ("playlists: moving items")
{
    Playlist p { "x", { "a", "b", "c", "d", "e" }, {} };
    auto now = Playlists::moveItems (p, { 0, 1 }, 4);
    CHECK_EQ (p.items, (std::vector<std::string> { "c", "d", "a", "b", "e" }));
    CHECK_EQ (now, (std::vector<int> { 2, 3 }));
    Playlists::moveItems (p, { 4 }, 0);
    CHECK_EQ (p.items, (std::vector<std::string> { "e", "c", "d", "a", "b" }));
    Playlists::removeItems (p, { 0, 4 });
    CHECK_EQ (p.items, (std::vector<std::string> { "c", "d", "a" }));
    Playlists::insertItems (p, 1, { "x", "c" });
    CHECK_EQ (Playlists::removeDuplicates (p), 1);
    CHECK_EQ (p.items, (std::vector<std::string> { "c", "x", "d", "a" }));
}

TEST ("playlists: kept and read back, names unique")
{
    Playlists all;
    all.create ("Jazz");
    CHECK_EQ (all.lists[(size_t) all.create ("Jazz")].name, std::string ("Jazz (2)"));
    all.lists[1].items = { "/a.flac" };
    all.lists[2].query = "genre IS jazz";
    all.active = 2;
    Playlists back;
    CHECK (back.fromJson (all.toJson()));
    CHECK_EQ (back.lists.size(), (size_t) 3);
    CHECK_EQ (back.lists[1].items, (std::vector<std::string> { "/a.flac" }));
    CHECK (back.lists[2].isAuto());
    CHECK_EQ (back.active, 2);
    back.removeList (2);
    CHECK_EQ (back.active, 1);
}

TEST ("order: in order, repeat, shuffle hears each once")
{
    Navigator n;
    n.seed (7);
    CHECK_EQ (n.next (Order::normal, 2, 3), -1);
    CHECK_EQ (n.next (Order::repeatPlaylist, 2, 3), 0);
    CHECK_EQ (n.next (Order::repeatTrack, 1, 3), 1);
    CHECK_EQ (n.next (Order::repeatTrack, 1, 3, {}, true), 2);
    std::set<int> heard { 3 };
    int cur = 3;
    for (int k = 0; k < 9; ++k) { cur = n.next (Order::shuffleTracks, cur, 10); heard.insert (cur); }
    CHECK_EQ (heard.size(), (size_t) 10);
    // Previous goes back through the same shuffle.
    int a = n.next (Order::shuffleTracks, cur, 10);
    CHECK_EQ (n.previous (Order::shuffleTracks, a, 10), cur);
}

TEST ("order: shuffled albums keep each album's order")
{
    Navigator n;
    n.seed (3);
    std::vector<std::string> albums { "A", "A", "A", "B", "B", "C" };
    int cur = 0;
    std::vector<int> seen { 0 };
    for (int k = 0; k < 5; ++k) { cur = n.next (Order::shuffleAlbums, cur, 6, albums); seen.push_back (cur); }
    // Each album's tracks one after the other, in order.
    for (size_t k = 1; k < seen.size(); ++k)
        if (albums[(size_t) seen[k]] == albums[(size_t) seen[k - 1]]) CHECK_EQ (seen[k], seen[k - 1] + 1);
    std::set<int> all (seen.begin(), seen.end());
    CHECK_EQ (all.size(), (size_t) 6);
}

TEST ("playlist files: M3U and PLS")
{
    auto text = writeM3u ({ "/a/1.flac", "/a/2.mp3" }, { "A - One", "B - Two" }, { 61, 120 });
    CHECK (text.find ("#EXTINF:61,A - One") != std::string::npos);
    CHECK_EQ (readM3u (text, "/x"), (std::vector<std::string> { "/a/1.flac", "/a/2.mp3" }));
    CHECK_EQ (readM3u ("\xEF\xBB\xBFsub\\3.flac\r\nfile:///b/My%20Song.mp3\n", "/m"),
              (std::vector<std::string> { "/m/sub/3.flac", "/b/My Song.mp3" }));
    CHECK_EQ (readPls ("[playlist]\nFile2=/b.mp3\nFile1=a.flac\nNumberOfEntries=2\n", "/m"),
              (std::vector<std::string> { "/m/a.flac", "/b.mp3" }));
}
