/*
    MainComponent - the one window: the message line at the top (as in
    Kalibre, decision 0025 there), the panels as the layout places them,
    the menus, and every command.
*/

#pragma once

#include "LayoutView.h"
#include "Panels.h"

namespace kp
{

class MessageLine : public juce::Component, private Controller::Listener, private juce::Timer
{
public:
    explicit MessageLine (Controller&);
    ~MessageLine() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    Controller& c;
    theme::HouseButton stopJobs { "Stop", "Stop the work going on." };
    void messageChanged() override { repaint(); }
    void timerCallback() override;
};

class MainComponent : public juce::Component,
                      public juce::MenuBarModel,
                      public juce::ApplicationCommandTarget,
                      public juce::DragAndDropContainer,
                      public juce::FileDragAndDropTarget,
                      private Controller::Listener
{
public:
    explicit MainComponent (Controller&);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::ApplicationCommandManager commands;
    Context ctx;
    LayoutView& layoutView() { return view; }
    void openFiles (const juce::StringArray&, bool play);   // from the Finder or the command line
    std::function<void (const juce::String&)> setTitle;    // the window's title

    // MenuBarModel
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int, const juce::String&) override;
    void menuItemSelected (int, int) override {}
    juce::PopupMenu appMenu();   // the Mac's application menu

    // ApplicationCommandTarget
    juce::ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
    void getAllCommands (juce::Array<juce::CommandID>&) override;
    void getCommandInfo (juce::CommandID, juce::ApplicationCommandInfo&) override;
    bool perform (const InvocationInfo&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    Controller& c;
    std::unique_ptr<juce::MenuBarComponent> menuBar;
    MessageLine messageLine;
    LayoutView view;
    std::unique_ptr<juce::FileChooser> chooser;

    void playbackChanged() override;
    void playlistsChanged() override { menuItemsChanged(); }
    void selectionChanged() override { commands.commandStatusChanged(); }
    void chooseFiles (bool replace);
    void ask (const juce::String& title, const juce::String& message, const juce::String& value, std::function<void (juce::String)>);
    void saveLayout();
    std::vector<std::string> chosen();
};

} // namespace kp
