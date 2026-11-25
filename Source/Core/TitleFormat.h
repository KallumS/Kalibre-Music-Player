/*
    TitleFormat - patterns that turn a track's tags into words (decision
    0005), in the syntax foobar2000 made familiar, so patterns people already
    have work here:

        %artist% - %title%                    fields
        [%album% ]                            shown only if a field inside has a value
        $if(%date%,%date%,no date)            functions
        'literal [text]'                      quoted, as written

    The playlist's columns, the window's title, Now Playing, renaming and
    moving files, the converter's file names and Format Field all use it.

    Fields are tags by name, with a few made from others as foobar has them:
    %artist% falls back to the album artist, %album artist% to the artist,
    %title% to the file's name, %tracknumber% has two digits; and the file's
    own (%length%, %codec%, %bitrate%, %filename%, %path%...). The full list
    is in docs/TITLE-FORMATTING.md.

    No JUCE in here.
*/

#pragma once

#include "Track.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace kp::tf
{

// What a pattern reads its fields from.
class Fields
{
public:
    virtual ~Fields() = default;
    // A field by name (lower-case), with foobar's fallbacks; false if it has no value.
    virtual bool field (std::string_view name, std::string& out) const = 0;
    // A tag's values as they are (for $meta), without fallbacks.
    virtual bool tag (std::string_view, std::vector<std::string>&) const { return false; }
};

// A track's fields; index and total are its place in the list it's shown in (%list_index%).
class TrackFields : public Fields
{
public:
    explicit TrackFields (const Track& t, int index = 0, int total = 0) : track (t), listIndex (index), listTotal (total) {}
    bool field (std::string_view name, std::string& out) const override;
    bool tag (std::string_view name, std::vector<std::string>& out) const override;

private:
    const Track& track;
    int listIndex, listTotal;
};

struct Node;
using Nodes = std::vector<Node>;

struct Node
{
    enum class Kind { text, field, function, optional } kind = Kind::text;
    std::string text;              // the words, the field's name or the function's
    std::vector<Nodes> args;       // a function's arguments; an optional section's one list
};

class Script
{
public:
    Script() = default;
    explicit Script (std::string_view source);
    std::string eval (const Fields&) const;
    // The result and whether any field in it had a value (what [ ] looks at).
    std::pair<std::string, bool> evalWithTruth (const Fields&) const;
    const std::string& source() const { return src; }
    // What's wrong with it, if anything (an unclosed bracket, an unknown function).
    const std::string& error() const { return err; }

private:
    std::string src, err;
    Nodes nodes;
};

// A compiled script for a pattern, kept for the next time it's asked for (thread-safe).
std::shared_ptr<const Script> compiled (const std::string& pattern);
std::string format (const std::string& pattern, const Track&, int index = 0, int total = 0);
std::string format (const std::string& pattern, const Fields&);

// The names of every function, for the docs and the help shown in pattern fields.
const std::vector<std::string>& functionNames();

// A file name made safe: the characters the Finder and other systems refuse become "_".
std::string safeFileName (std::string_view);

// A relative path for renaming, moving and converting: "/" in the pattern
// makes folders, and a "/" inside a field's value ("AC/DC") doesn't.
std::string formatPath (const std::string& pattern, const Track&);

} // namespace kp::tf
