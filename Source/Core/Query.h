/*
    Query - finding music in the library by what its tags say (decision
    0006), in foobar2000's query syntax, so a search people already know
    works:

        beatles abbey                       words: every one somewhere in its tags
        artist HAS beatles                  a field holding the words
        genre IS jazz                       a field being exactly it (any one of its values)
        date AFTER 1969 AND date BEFORE 1980
        rating GREATER 3                    numbers: GREATER, LESS, EQUAL
        comment MISSING                     no value; PRESENT has one
        added DURING LAST 2 WEEKS           MINUTES, HOURS, DAYS, WEEKS
        NOT genre IS podcast                NOT, AND, OR, ( and )
        %album artist% IS %artist%          a pattern compared with a pattern
        * HAS live                          any field
        ALL SORT BY %date%                  everything, sorted (SORT DESCENDING BY)

    Keywords are in capitals; a word in capitals that isn't one is a word to
    look for. A query that can't be read is taken as words.

    No JUCE in here.
*/

#pragma once

#include "Track.h"

#include <memory>
#include <string>
#include <vector>

namespace kp
{

class Query
{
public:
    Query();
    explicit Query (const std::string& text, std::int64_t nowMs = 0);
    ~Query();
    Query (Query&&) noexcept;
    Query& operator= (Query&&) noexcept;

    bool matches (const Track&) const;
    bool empty() const;                         // matches everything
    const std::string& sortPattern() const { return sortBy; }
    bool sortDescending() const { return descending; }
    const std::string& error() const { return err; }   // why it was read as words instead

    struct Node;

private:
    std::unique_ptr<Node> root;
    std::string sortBy, err;
    bool descending = false;
};

// Sorts tracks (by index into `tracks`) by a title-formatting pattern, keeping the order of equals.
void sortByPattern (std::vector<size_t>& order, const std::vector<Track>& tracks, const std::string& pattern, bool descending);
void sortByPattern (std::vector<const Track*>& tracks, const std::string& pattern, bool descending);

// A natural comparison: "Track 2" before "Track 10", ignoring case.
int naturalCompare (const std::string&, const std::string&);

} // namespace kp
