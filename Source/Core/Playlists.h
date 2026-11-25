/*
    Playlists - the tabs of tracks the player plays from (decision 0007),
    as foobar2000 keeps them: several lists, one shown, one playing, each a
    list of locations. A list can be filled by a query instead (an
    "autoplaylist"): it holds what the query finds, and follows the library.

    Order: how the next track is chosen - in order, repeating the list or the
    track, shuffling tracks or albums, or at random.

    No JUCE in here.
*/

#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace kp
{

struct Playlist
{
    std::string name;
    std::vector<std::string> items;   // locations
    std::string query;                // an autoplaylist's query; empty for a list of its own
    bool isAuto() const { return ! query.empty(); }
};

enum class Order { normal, repeatPlaylist, repeatTrack, shuffleTracks, shuffleAlbums, random };
const char* orderName (Order);
inline constexpr int orderCount = 6;

// Chooses the next and the previous track. Shuffles are remembered, so
// Previous goes back through the same order, and each track is heard once
// before any is heard again.
class Navigator
{
public:
    // `albums`: for each item, a key naming its album (only for shuffleAlbums).
    int next (Order, int current, int count, const std::vector<std::string>& albums = {}, bool manual = false);
    int previous (Order, int current, int count, const std::vector<std::string>& albums = {});
    void forget() { order.clear(); history.clear(); }
    void seed (std::uint32_t s) { rng.seed (s); }

private:
    std::vector<int> order;    // a shuffle: the items in the order they'll play
    std::vector<int> history;  // what a shuffle played, for Previous
    int orderCount = -1;
    Order madeFor = Order::normal;
    std::mt19937 rng { std::random_device {}() };
    void shuffle (Order, int count, const std::vector<std::string>& albums, int startWith);
    int placeOf (int item) const;
};

class Playlists
{
public:
    Playlists();
    std::vector<Playlist> lists;
    int active = 0;        // the one shown
    int playing = -1;      // the one playing from (-1: none)

    int create (const std::string& name);
    void removeList (int index);
    std::string uniqueName (const std::string& base) const;
    Playlist* get (int index) { return index >= 0 && index < (int) lists.size() ? &lists[(size_t) index] : nullptr; }

    // Items moved to `to` (the place before which they go, in the list before the move).
    static std::vector<int> moveItems (Playlist&, std::vector<int> indices, int to);
    static void removeItems (Playlist&, std::vector<int> indices);
    static void insertItems (Playlist&, int at, const std::vector<std::string>& locations);
    // Items with the same location after the first taken out; how many went.
    static int removeDuplicates (Playlist&);

    std::string toJson() const;
    bool fromJson (const std::string&);
};

// Playlist files: M3U (and M3U8) written and read; PLS read.
std::string writeM3u (const std::vector<std::string>& locations, const std::vector<std::string>& titles = {}, const std::vector<double>& lengths = {});
// Relative paths are made from `baseDir`.
std::vector<std::string> readM3u (const std::string& text, const std::string& baseDir);
std::vector<std::string> readPls (const std::string& text, const std::string& baseDir);

} // namespace kp
