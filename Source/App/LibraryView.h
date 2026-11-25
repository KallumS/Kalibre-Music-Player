/*
    LibraryView - the library, searched and browsed (decision 0006). A
    search field takes words or a query ("genre IS jazz AND date AFTER
    1965"); under it, columns of values (by default Genre, Album Artist and
    Album, each changeable to any field) narrow it further, as foobar2000's
    Facets do. What's chosen fills the "Library Selection" playlist;
    double-click plays it.
*/

#pragma once

#include "Panels.h"

namespace kp
{

class LibraryView : public juce::Component, private Controller::Listener, private juce::Timer
{
public:
    explicit LibraryView (Context&);
    ~LibraryView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void focusSearch();
    void setSearch (const juce::String&);   // tests

    struct Column;

private:
    Context& ctx;
    juce::TextEditor searchField;
    theme::HouseButton addFolder { juce::String::fromUTF8 ("Add a Folder\xe2\x80\xa6"), "Choose a folder of music for the library to watch." };
    juce::Label count;
    std::vector<std::unique_ptr<Column>> facetColumns;
    std::vector<size_t> found;                 // the search's tracks (indices in the library)
    bool dirty = true;

    void libraryChanged() override { dirty = true; startTimer (150); }
    void timerCallback() override;
    void refilter (int fromColumn);
    std::vector<std::string> chosenLocations();
    void send (bool play);
    friend struct Column;
};

} // namespace kp
