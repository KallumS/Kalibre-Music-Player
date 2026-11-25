/*
    Tagging - batch tag tools that need no files (decision 0009):

      - Guess Tags from File Names: a pattern like
        "%tracknumber% - %artist% - %title%" read against each file's name
        (and folders, with "/": "%album%/%tracknumber% %title%").
      - Edits to many tracks at once: a field set, cleared or made from a
        pattern for every one; the values shown as "<multiple values>" when
        they differ.

    No JUCE in here.
*/

#pragma once

#include "Track.h"

#include <optional>
#include <string>
#include <vector>

namespace kp::tagging
{

// The fields a pattern reads from a location, or nothing if it doesn't fit.
std::optional<Tags> guessFromPath (const std::string& pattern, const std::string& location);

inline const char* multipleValues() { return "<multiple values>"; }

// One row of the tag editor: a field and its value across the tracks.
struct Row
{
    std::string field;
    std::string value;        // joined with "; " for several values, or multipleValues()
    bool differs = false;
};
std::vector<Row> rowsFor (const std::vector<const Track*>&);

// An edit the tag editor makes: set a field ("" removes it) on every track.
// A value of multipleValues() leaves each track's as it was. "; " splits values.
struct Change
{
    std::string field;
    std::string value;
    std::string renamedFrom;  // the field was renamed from this
};
void apply (Tags&, const std::vector<Change>&);
std::vector<std::string> splitValues (const std::string&);

// The fields the tag editor always shows, in order.
const std::vector<std::string>& standardFields();

// Auto track numbers: 1..n in the given order, with the total.
void numberTracks (std::vector<Tags*>&, bool withTotal);

} // namespace kp::tagging
