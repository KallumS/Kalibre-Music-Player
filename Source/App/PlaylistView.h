/*
    PlaylistView - the playlists as tabs, and the shown one's tracks in
    columns (decision 0007). Each column is a title-formatting pattern, so
    any can be added (right-click the header). Click a column's header to
    sort by it; drag tracks to move them; drop files from the Finder in.
    Double-click (or Return) plays.
*/

#pragma once

#include "Panels.h"

namespace kp
{

struct PlaylistTabs : public juce::TabbedButtonBar
{
    PlaylistTabs() : juce::TabbedButtonBar (TabsAtTop) {}
    std::function<void (int)> onMenu;
    void popupMenuClickOnTab (int index, const juce::String&) override { if (onMenu) onMenu (index); }
};

class PlaylistView : public juce::Component,
                     public juce::DragAndDropTarget,
                     public juce::FileDragAndDropTarget,
                     private Controller::Listener,
                     private juce::TableListBoxModel,
                     private juce::ChangeListener,
                     private juce::Timer
{
public:
    explicit PlaylistView (Context&);
    ~PlaylistView() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;

    struct Column { juce::String name, pattern; int width; };
    static std::vector<Column> defaultColumns();

    // DragAndDropTarget: tracks dragged within the playlist.
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override { dropAt = -1; repaint(); }
    void itemDropped (const SourceDetails&) override;
    // FileDragAndDropTarget: files from the Finder.
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void fileDragMove (const juce::StringArray&, int, int y) override;
    void fileDragExit (const juce::StringArray&) override { dropAt = -1; repaint(); }
    void filesDropped (const juce::StringArray&, int, int y) override;

    juce::TableListBox& tableForTests() { return table; }

private:
    Context& ctx;
    PlaylistTabs tabs;
    theme::HouseButton addTab { "+", "A new playlist." };
    juce::TableListBox table;
    std::vector<Column> columns;
    int dropAt = -1;
    bool updatingTabs = false;
    int sortedColumn = 0;
    bool sortedForward = true;

    void playlistsChanged() override;
    void libraryChanged() override { table.repaint(); }
    void playbackChanged() override { table.repaint(); }
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void rebuildTabs();
    void rebuildColumns();
    void saveColumns();
    int rowAt (int y);

    int getNumRows() override;
    void paintRowBackground (juce::Graphics&, int row, int w, int h, bool selected) override;
    void paintCell (juce::Graphics&, int row, int column, int w, int h, bool selected) override;
    void cellDoubleClicked (int row, int column, const juce::MouseEvent&) override;
    void cellClicked (int row, int column, const juce::MouseEvent&) override;
    void backgroundClicked (const juce::MouseEvent&) override;
    void selectedRowsChanged (int) override;
    void deleteKeyPressed (int) override;
    void returnKeyPressed (int) override;
    void sortOrderChanged (int columnId, bool forwards) override;
    juce::var getDragSourceDescription (const juce::SparseSet<int>&) override;
    juce::String getCellTooltip (int row, int column) override;
    void columnMenu();
    void rowMenu();
    void tabMenu (int index);
    void editColumn (int index);
};

} // namespace kp
