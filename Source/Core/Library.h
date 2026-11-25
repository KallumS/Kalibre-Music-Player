/*
    Library - every track in the folders the player watches (decision 0006),
    kept between runs as one file, and searched with a Query.

    The scanner (Source/Engine/LibraryScanner) reads files on a thread of its
    own and hands what it found to the window's thread, which applies it
    here: the library itself is only ever touched on one thread.

    Facets: the values one field has among some tracks ("artist": each
    artist, with how many tracks), for browsing by artist, album, year or
    genre. A field holding several values (two artists) counts under each.

    No JUCE in here.
*/

#pragma once

#include "Query.h"
#include "Track.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace kp
{

class Library
{
public:
    const std::vector<Track>& tracks() const { return all; }
    size_t size() const { return all.size(); }
    const Track* find (const std::string& location) const;

    // Adds or replaces a track (by location); keeps when it was first added.
    void put (Track);
    bool remove (const std::string& location);
    // Takes out every track in a folder (one no longer watched).
    int removeUnder (const std::string& folder);
    // A file renamed or moved: its track keeps its place and its added time.
    bool move (const std::string& from, const std::string& to);

    // The indices of the tracks a query matches, sorted as it says (or by the default pattern).
    std::vector<size_t> search (const Query&, const std::string& defaultSort = defaultSortPattern()) const;
    static const std::string& defaultSortPattern();

    std::vector<std::string> folders;       // the folders watched, as given
    std::uint64_t revision() const { return rev; }   // goes up on each change

    std::string toJson() const;
    bool fromJson (const std::string&);

private:
    std::vector<Track> all;
    std::unordered_map<std::string, size_t> byLocation;
    std::uint64_t rev = 0;
    void reindex();
};

// The values a field has for a track: a tag's values ("artist"), or a pattern's result ("%album artist% - %album%").
std::vector<std::string> fieldValues (const Track&, const std::string& field);

struct Facet
{
    std::string value;
    int count = 0;
};

// The values of `field` among `tracks`, sorted naturally, each with its count.
// "(no value)" counts the tracks without one; it comes first when there are any.
std::vector<Facet> facets (const std::vector<const Track*>& tracks, const std::string& field);
bool hasFacetValue (const Track&, const std::string& field, const std::string& value);
inline const char* noValue() { return "(No Value)"; }

// Serialising one track, for the library file and the playlists' caches.
std::string trackToJson (const Track&);
bool trackFromJson (const std::string&, Track&);

} // namespace kp
