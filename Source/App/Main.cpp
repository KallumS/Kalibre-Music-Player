// Kalibre Player: the app, its one window, and files opened from the Finder.

#include "FilePanels.h"
#include "MainComponent.h"
#include "MediaKeys.h"

namespace kp
{
using namespace juce;

class MainWindow : public DocumentWindow
{
public:
    explicit MainWindow (Controller& c) : DocumentWindow ("Kalibre Player", theme::ground, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        content = new MainComponent (c);
        content->setTitle = [this] (const String& t) { setName (t); };
        setContentOwned (content, true);
        setResizable (true, true);
        setResizeLimits (640, 420, 10000, 10000);
        if (! restoreWindowStateFromString (c.settings.get ("window"))) centreWithSize (1280, 800);
        setVisible (true);
    }
    void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }
    MainComponent* content = nullptr;
};

class Application : public JUCEApplication
{
public:
    const String getApplicationName() override { return "Kalibre Player"; }
    const String getApplicationVersion() override { return KP_VERSION; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const String& commandLine) override
    {
        LookAndFeel::setDefaultLookAndFeel (&look);
        tips = std::make_unique<TooltipWindow> (nullptr, 600);
        controller = std::make_unique<Controller>();
        theme::usePalette (controller->settings.getInt ("theme", 0));
        theme::setScale (controller->settings.getInt ("textSize", 100) / 100.0f);
        window = std::make_unique<MainWindow> (*controller);
        ui::placeFilePanelsAtTop();
       #if JUCE_MAC
        extraAppMenu = window->content->appMenu();
        MenuBarModel::setMacMainMenu (window->content, &extraAppMenu);
       #endif
        media::install (*controller);
        if (controller->library.size() == 0 && controller->library.folders.empty())
            controller->say ("Welcome. Add a folder of music to the library (the library panel's Add a Folder), or drop files on the playlist.");
        anotherInstanceStarted (commandLine);
    }

    void shutdown() override
    {
        media::uninstall();
        if (window && controller) controller->settings.set ("window", window->getWindowStateAsString());
        window.reset();
        controller.reset();
        tips.reset();
        LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }

    // Files opened from the Finder (or dropped on the Dock's icon) play.
    void anotherInstanceStarted (const String& commandLine) override
    {
        if (! window || commandLine.trim().isEmpty()) return;
        StringArray args;
        args.addTokens (commandLine, true);
        StringArray files;
        for (auto& a : args)
        {
            const File f (File::getCurrentWorkingDirectory().getChildFile (a.unquoted()));
            if (f.exists()) files.add (f.getFullPathName());
        }
        if (! files.isEmpty()) window->content->openFiles (files, true);
    }

private:
    theme::LookAndFeel look;
    std::unique_ptr<TooltipWindow> tips;
    std::unique_ptr<Controller> controller;
    std::unique_ptr<MainWindow> window;
    PopupMenu extraAppMenu;
};

} // namespace kp

START_JUCE_APPLICATION (kp::Application)
