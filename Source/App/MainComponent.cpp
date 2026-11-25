#include "MainComponent.h"
#include "CdDrive.h"
#include "Commands.h"
#include "Decoder.h"
#include "Dialogs.h"
#include "LibraryView.h"
#include "Sources.h"
#include "TitleFormat.h"

namespace kp
{
using namespace juce;

namespace
{
String u8 (const std::string& s) { return String::fromUTF8 (s.c_str()); }
const String dots = String::fromUTF8 ("\xe2\x80\xa6");
}

//==============================================================================
MessageLine::MessageLine (Controller& x) : c (x)
{
    addChildComponent (stopJobs);
    stopJobs.onClick = [this] { c.jobs.cancelAll(); c.say ("Stopped."); };
    c.addListener (this);
    startTimerHz (4);
}
MessageLine::~MessageLine() { c.removeListener (this); }

void MessageLine::timerCallback()
{
    const bool busy = ! c.jobs.running().empty();
    if (stopJobs.isVisible() != busy) stopJobs.setVisible (busy);
    repaint();
}

void MessageLine::paint (Graphics& g)
{
    g.fillAll (theme::raised);
    g.setColour (theme::rule);
    g.drawHorizontalLine (getHeight() - 1, 0, (float) getWidth());
    auto r = getLocalBounds().reduced (theme::px (10), 0);
    if (stopJobs.isVisible()) r.removeFromRight (stopJobs.getWidth() + 8);
    // Work going on, on the right; the message on the left.
    String work;
    for (auto& j : c.jobs.running())
        if (j->total > 1 || j->title != "Measuring loudness")
            work << (work.isEmpty() ? "" : "   ") << j->title << " " << j->done.load() << " of " << j->total;
    if (c.scanStatus.isNotEmpty()) work << (work.isEmpty() ? "" : "   ") << c.scanStatus;
    g.setFont (theme::font (theme::smallPoints));
    if (work.isNotEmpty())
    {
        g.setColour (theme::textSecondary);
        const int w = jmin (r.getWidth() / 2, GlyphArrangement::getStringWidthInt (g.getCurrentFont(), work) + 12);
        g.drawText (work, r.removeFromRight (w), Justification::centredRight, true);
    }
    g.setColour (c.messageImportant ? theme::warnText : theme::text);
    g.setFont (theme::font (theme::bodyPoints));
    g.drawText (c.messageText, r, Justification::centredLeft, true);
}

void MessageLine::resized()
{
    stopJobs.setBounds (getLocalBounds().removeFromRight (theme::px (70)).reduced (4, 3));
}

void MessageLine::mouseDown (const MouseEvent&) { c.say ({}); }   // dismissed with a click

//==============================================================================
MainComponent::MainComponent (Controller& x) : ctx { x, commands }, c (x), messageLine (x), view (ctx), mini (ctx)
{
    ui::setHost (this);
    commands.registerAllCommandsForTarget (this);
    commands.setFirstCommandTarget (this);   // whatever has the focus, the window does the work
    if (auto keys = c.settings.getXml ("keys")) commands.getKeyMappings()->restoreFromXml (*keys);
    addKeyListener (commands.getKeyMappings());
    setWantsKeyboardFocus (true);
   #if ! JUCE_MAC
    menuBar = std::make_unique<MenuBarComponent> (this);
    addAndMakeVisible (*menuBar);
   #endif
    addAndMakeVisible (messageLine);
    addAndMakeVisible (view);
    addChildComponent (mini);
    mini.onExpand = [this] { setMini (false); };
    mini.onKeepInFront = [this] (bool on) { if (onKeepInFront) onKeepInFront (on); };
    auto saved = layout::fromJson (c.settings.get ("layout").toStdString());
    view.setLayout (saved ? *saved : layout::presets().front().root);
    view.onChanged = [this] { saveLayout(); };
    c.addListener (this);
    setSize (1280, 800);
}

MainComponent::~MainComponent()
{
    c.removeListener (this);
    dialogs::closeAll();
    ui::setHost (nullptr);
   #if JUCE_MAC
    MenuBarModel::setMacMainMenu (nullptr);
   #endif
}

void MainComponent::saveLayout() { c.settings.set ("layout", u8 (layout::toJson (view.current()))); }

void MainComponent::paint (Graphics& g) { g.fillAll (theme::ground); }

void MainComponent::setMini (bool on)
{
    if (on == miniMode) return;
    miniMode = on;
    if (menuBar) menuBar->setVisible (! on);
    messageLine.setVisible (! on);
    view.setVisible (! on);
    mini.setVisible (on);
    c.settings.set ("mini", on);
    if (onMiniChanged) onMiniChanged (on);
    resized();
    commands.commandStatusChanged();
}

void MainComponent::resized()
{
    if (miniMode) { mini.setBounds (getLocalBounds()); return; }
    auto r = getLocalBounds();
    if (menuBar) menuBar->setBounds (r.removeFromTop (LookAndFeel::getDefaultLookAndFeel().getDefaultMenuBarHeight()));
    messageLine.setBounds (r.removeFromTop (theme::px (28)));
    view.setBounds (r);
}

void MainComponent::playbackChanged()
{
    commands.commandStatusChanged();
    if (! setTitle) return;
    if (auto t = c.nowPlayingTrack()) setTitle (u8 (tf::format ("[%artist% - ]%title%", *t)) + String::fromUTF8 (" \xe2\x80\x94 Kalibre Player"));
    else setTitle ("Kalibre Player");
}

std::vector<std::string> MainComponent::chosen()
{
    auto sel = c.selectedLocations();
    if (sel.empty())
        if (auto t = c.nowPlayingTrack()) sel.push_back (t->location);
    return sel;
}

void MainComponent::openFiles (const StringArray& paths, bool play)
{
    Array<File> files;
    for (auto& p : paths) files.add (File (p));
    if (files.isEmpty()) return;
    // A playlist file opens as a playlist of its own.
    if (files.size() == 1 && (files[0].hasFileExtension ("m3u;m3u8;pls")))
    {
        c.importPlaylist (files[0]);
        if (play) c.playItem (c.playlists.active, 0);
        return;
    }
    const int before = (int) c.active().items.size();
    const int n = c.addFiles (c.playlists.active, files);
    if (n == 0) { c.say ("Nothing there is music the player knows.", true); return; }
    if (play) c.playItem (c.playlists.active, before);
}

void MainComponent::filesDropped (const StringArray& files, int, int)
{
    openFiles (files, false);
}

void MainComponent::chooseFiles (bool replace)
{
    chooser = std::make_unique<FileChooser> (replace ? "Open" : "Add Files", File(), "*", true);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::canSelectMultipleItems,
                          [this, replace] (const FileChooser& fc)
    {
        auto results = fc.getResults();
        if (results.isEmpty()) return;
        if (replace) { c.stop(); c.clearPlaylist (c.playlists.active); }
        StringArray paths;
        for (auto& f : results) paths.add (f.getFullPathName());
        openFiles (paths, replace);
    });
}

void MainComponent::ask (const String& title, const String& message, const String& value, std::function<void (String)> then)
{
    auto* w = new AlertWindow (title, message, MessageBoxIconType::NoIcon);
    w->addTextEditor ("v", value);
    w->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    ui::present (w, ModalCallbackFunction::create ([w, then] (int r) { if (r == 1) then (w->getTextEditorContents ("v")); }), true);
}

//==============================================================================
StringArray MainComponent::getMenuBarNames() { return { "File", "Edit", "Playback", "View", "Add-ons", "Help" }; }

PopupMenu MainComponent::appMenu()
{
    PopupMenu m;
    m.addCommandItem (&commands, cmd::about);
    m.addSeparator();
    m.addCommandItem (&commands, cmd::preferences);
    return m;
}

PopupMenu MainComponent::getMenuForIndex (int index, const String&)
{
    PopupMenu m;
    auto* cm = &commands;
    switch (index)
    {
        case 0:
        {
            m.addCommandItem (cm, cmd::openFiles);
            m.addCommandItem (cm, cmd::addFiles);
            m.addCommandItem (cm, cmd::addFolder);
            m.addSeparator();
            m.addCommandItem (cm, cmd::newPlaylist);
            m.addCommandItem (cm, cmd::newAutoPlaylist);
            m.addCommandItem (cm, cmd::loadPlaylist);
            m.addCommandItem (cm, cmd::savePlaylist);
            m.addCommandItem (cm, cmd::renamePlaylist);
            m.addCommandItem (cm, cmd::removePlaylist);
            m.addSeparator();
            m.addCommandItem (cm, cmd::libraryFolders);
            m.addCommandItem (cm, cmd::rescanLibrary);
            m.addSeparator();
            m.addCommandItem (cm, cmd::playCd);
            m.addCommandItem (cm, cmd::ripCd);
            m.addSeparator();
            m.addCommandItem (cm, cmd::importAddon);
           #if ! JUCE_MAC
            m.addCommandItem (cm, cmd::preferences);
           #endif
            break;
        }
        case 1:
        {
            m.addCommandItem (cm, cmd::selectAll);
            m.addCommandItem (cm, cmd::removeSelected);
            m.addCommandItem (cm, cmd::cropSelected);
            m.addCommandItem (cm, cmd::removeDuplicates);
            m.addCommandItem (cm, cmd::removeDead);
            PopupMenu sort;
            for (auto id : { cmd::sortByArtist, cmd::sortByAlbum, cmd::sortByTitle, cmd::sortByPath, cmd::sortRandom }) sort.addCommandItem (cm, id);
            m.addSubMenu ("Sort", sort);
            m.addSeparator();
            m.addCommandItem (cm, cmd::search);
            m.addCommandItem (cm, cmd::queueSelected);
            m.addSeparator();
            m.addCommandItem (cm, cmd::properties);
            m.addCommandItem (cm, cmd::guessTags);
            m.addCommandItem (cm, cmd::renameFiles);
            m.addCommandItem (cm, cmd::moveFiles);
            m.addCommandItem (cm, cmd::copyFiles);
            m.addCommandItem (cm, cmd::convert);
            PopupMenu rgm;
            for (auto id : { cmd::rgTrack, cmd::rgAlbum, cmd::rgRemove }) rgm.addCommandItem (cm, id);
            m.addSubMenu ("ReplayGain", rgm);
            m.addSeparator();
            m.addCommandItem (cm, cmd::showInFinder);
            m.addCommandItem (cm, cmd::copyLocations);
            break;
        }
        case 2:
        {
            for (auto id : { cmd::playPause, cmd::stop, cmd::previous, cmd::next, cmd::playRandom }) m.addCommandItem (cm, id);
            m.addCommandItem (cm, cmd::stopAfterCurrent);
            m.addSeparator();
            for (auto id : { cmd::seekBack, cmd::seekForward, cmd::volumeUp, cmd::volumeDown, cmd::mute }) m.addCommandItem (cm, id);
            m.addSeparator();
            PopupMenu order;
            for (auto id : { cmd::orderNormal, cmd::orderRepeatPlaylist, cmd::orderRepeatTrack, cmd::orderShuffleTracks, cmd::orderShuffleAlbums, cmd::orderRandom })
                order.addCommandItem (cm, id);
            m.addSubMenu ("Order", order);
            PopupMenu rgm;
            for (auto id : { cmd::rgOff, cmd::rgTrackMode, cmd::rgAlbumMode, cmd::rgSmartMode }) rgm.addCommandItem (cm, id);
            m.addSubMenu ("ReplayGain", rgm);
            m.addCommandItem (cm, cmd::equalizerOn);
            break;
        }
        case 3:
        {
            m.addCommandItem (cm, cmd::miniPlayer);
            m.addSeparator();
            m.addCommandItem (cm, cmd::editLayout);
            PopupMenu presets;
            for (auto& p : layout::presets())
            {
                auto root = p.root;
                presets.addItem (u8 (p.name), [this, root] { view.setLayout (root); saveLayout(); });
            }
            m.addSubMenu ("Layout", presets);
            PopupMenu themes;
            for (int k = 0; k < (int) theme::palettes().size(); ++k)
                themes.addItem (theme::palettes()[(size_t) k].name, true, k == theme::currentPalette(), [this, k]
                {
                    theme::usePalette (k);
                    c.settings.set ("theme", k);
                    view.setLayout (view.current());
                });
            m.addSubMenu ("Theme", themes);
            m.addCommandItem (cm, cmd::textBigger);
            m.addCommandItem (cm, cmd::textSmaller);
            m.addSeparator();
            m.addCommandItem (cm, cmd::keyboardShortcuts);
            m.addCommandItem (cm, cmd::addonManager);
            m.addCommandItem (cm, cmd::addonConsole);
            break;
        }
        case 4:
        {
            if (c.addons)
                for (auto& it : c.addons->menuItems())
                    m.addItem (u8 (it.title), [this, it] { c.addons->runMenu (it); });
            if (m.getNumItems() > 0) m.addSeparator();
            m.addCommandItem (cm, cmd::importAddon);
            m.addCommandItem (cm, cmd::addonManager);
            m.addCommandItem (cm, cmd::addonsHelp);
            break;
        }
        case 5:
        {
            m.addCommandItem (cm, cmd::titleFormatHelp);
            m.addCommandItem (cm, cmd::queryHelp);
            m.addCommandItem (cm, cmd::addonsHelp);
           #if ! JUCE_MAC
            m.addSeparator();
            m.addCommandItem (cm, cmd::about);
           #endif
            break;
        }
        default: break;
    }
    return m;
}

//==============================================================================
void MainComponent::getAllCommands (Array<CommandID>& ids) { ids.addArray (cmd::all()); }

void MainComponent::getCommandInfo (CommandID id, ApplicationCommandInfo& info)
{
    cmd::describe (id, info);
    const bool hasSelection = ! c.selected.empty();
    const bool hasChosen = hasSelection || c.nowPlayingTrack().has_value();
    switch (id)
    {
        case cmd::removeSelected: case cmd::cropSelected: case cmd::queueSelected: case cmd::copyLocations:
            info.setActive (hasSelection); break;
        case cmd::properties: case cmd::guessTags: case cmd::renameFiles: case cmd::moveFiles: case cmd::copyFiles: case cmd::convert:
        case cmd::rgTrack: case cmd::rgAlbum: case cmd::rgRemove: case cmd::showInFinder:
            info.setActive (hasChosen); break;
        case cmd::stop: case cmd::next: case cmd::previous: case cmd::seekForward: case cmd::seekBack:
            info.setActive (c.playing()); break;
        case cmd::stopAfterCurrent: info.setTicked (c.stopAfterCurrent); break;
        case cmd::mute: info.setTicked (c.player.muted.load()); break;
        case cmd::orderNormal: case cmd::orderRepeatPlaylist: case cmd::orderRepeatTrack: case cmd::orderShuffleTracks: case cmd::orderShuffleAlbums: case cmd::orderRandom:
            info.setTicked ((int) c.order == (int) (id - cmd::orderNormal)); break;
        case cmd::rgOff: case cmd::rgTrackMode: case cmd::rgAlbumMode: case cmd::rgSmartMode:
            info.setTicked ((int) c.replayGain.mode == (int) (id - cmd::rgOff)); break;
        case cmd::equalizerOn: info.setTicked (c.player.eq.enabled.load()); break;
        case cmd::editLayout: info.setTicked (view.editing()); info.setActive (! miniMode); break;
        case cmd::miniPlayer: info.setTicked (miniMode); break;
        case cmd::playPause: info.shortName = c.playing() && ! c.paused() ? "Pause" : "Play"; break;
        default: break;
    }
}

bool MainComponent::perform (const InvocationInfo& info)
{
    const int active = c.playlists.active;
    switch (info.commandID)
    {
        case cmd::openFiles: chooseFiles (true); return true;
        case cmd::addFiles: chooseFiles (false); return true;
        case cmd::addFolder:
            chooser = std::make_unique<FileChooser> ("Add a Folder", File::getSpecialLocation (File::userMusicDirectory));
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this] (const FileChooser& fc)
            {
                if (fc.getResult().isDirectory()) openFiles ({ fc.getResult().getFullPathName() }, false);
            });
            return true;
        case cmd::newPlaylist: c.newPlaylist(); return true;
        case cmd::newAutoPlaylist:
            ask ("New Autoplaylist", "A query: the playlist holds what it finds, and follows the library (Help > Search Queries).", "genre IS jazz", [this] (String q)
            {
                const int p = c.newPlaylist (q);
                c.playlists.lists[(size_t) p].query = q.toStdString();
                c.refreshAutoPlaylists();
            });
            return true;
        case cmd::loadPlaylist:
            chooser = std::make_unique<FileChooser> ("Load a Playlist", File(), "*.m3u;*.m3u8;*.pls");
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
            {
                if (fc.getResult().existsAsFile()) c.importPlaylist (fc.getResult());
            });
            return true;
        case cmd::savePlaylist:
            chooser = std::make_unique<FileChooser> ("Save the Playlist", File::getSpecialLocation (File::userMusicDirectory).getChildFile (u8 (c.active().name) + ".m3u8"), "*.m3u8");
            chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [this, active] (const FileChooser& fc)
            {
                if (fc.getResult() == File()) return;
                c.say (c.exportPlaylist (active, fc.getResult()) ? "Saved as " + fc.getResult().getFileName() + "." : String ("It couldn't be saved."));
            });
            return true;
        case cmd::removePlaylist: c.removePlaylist (active); return true;
        case cmd::renamePlaylist:
            ask ("Rename the Playlist", {}, u8 (c.active().name), [this, active] (String n) { c.renamePlaylist (active, n); });
            return true;
        case cmd::libraryFolders: dialogs::preferences (ctx, 0); return true;
        case cmd::rescanLibrary: c.rescan(); return true;
        case cmd::importAddon:
            chooser = std::make_unique<FileChooser> ("Import an Add-on", File(), "*.lua;*.zip");
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::canSelectDirectories,
                                  [this] (const FileChooser& fc)
            {
                if (fc.getResult() == File()) return;
                const auto e = c.importAddon (fc.getResult());
                c.say (e.isEmpty() ? fc.getResult().getFileName() + " added. Its items are in the Add-ons menu." : "Not added: " + e + ".", e.isNotEmpty());
                menuItemsChanged();
            });
            return true;
        case cmd::preferences: dialogs::preferences (ctx, 1); return true;
        case cmd::playCd:
        case cmd::ripCd:
        {
            auto discs = findAudioCds();
            if (discs.empty()) { c.say ("There's no audio CD in the drive.", true); return true; }
            if (info.commandID == cmd::ripCd) { dialogs::rip (ctx, discs.front()); return true; }
            const int p = c.newPlaylist (discs.front().name());
            c.addLocations (p, discs.front().tracks);
            c.playItem (p, 0);
            return true;
        }

        case cmd::selectAll:
        {
            std::vector<int> all ((size_t) c.active().items.size());
            for (size_t k = 0; k < all.size(); ++k) all[k] = (int) k;
            c.setSelection (all);
            c.playlistEdited (active);
            return true;
        }
        case cmd::removeSelected: c.removeItems (active, c.selected); return true;
        case cmd::cropSelected:
        {
            std::vector<int> others;
            for (int k = 0; k < (int) c.active().items.size(); ++k)
                if (std::find (c.selected.begin(), c.selected.end(), k) == c.selected.end()) others.push_back (k);
            c.removeItems (active, others);
            return true;
        }
        case cmd::removeDuplicates:
        {
            const int n = Playlists::removeDuplicates (c.active());
            c.playlistEdited (active);
            c.say (String (n) + (n == 1 ? " duplicate" : " duplicates") + " taken out.");
            return true;
        }
        case cmd::removeDead:
        {
            std::vector<int> dead;
            for (int k = 0; k < (int) c.active().items.size(); ++k)
                if (! sources::fileOf (c.active().items[(size_t) k]).existsAsFile()) dead.push_back (k);
            c.removeItems (active, dead);
            c.say (String ((int) dead.size()) + " missing files taken out.");
            return true;
        }
        case cmd::sortByArtist: c.sortPlaylist (active, "%album artist%|%date%|%album%|%discnumber%|%tracknumber%", false); return true;
        case cmd::sortByAlbum: c.sortPlaylist (active, "%album%|%discnumber%|%tracknumber%", false); return true;
        case cmd::sortByTitle: c.sortPlaylist (active, "%title%", false); return true;
        case cmd::sortByPath: c.sortPlaylist (active, "%path%", false); return true;
        case cmd::sortRandom: c.shufflePlaylist (active); return true;
        case cmd::search:
            if (auto* lib = dynamic_cast<LibraryView*> (view.find ("library"))) lib->focusSearch();
            else c.say ("The library isn't in the window: View > Edit Layout adds it.");
            return true;
        case cmd::properties: dialogs::tagEditor (ctx, chosen()); return true;
        case cmd::guessTags: dialogs::guessTags (ctx, chosen()); return true;
        case cmd::renameFiles: dialogs::fileOperation (ctx, chosen(), dialogs::FileOp::rename); return true;
        case cmd::moveFiles: dialogs::fileOperation (ctx, chosen(), dialogs::FileOp::move); return true;
        case cmd::copyFiles: dialogs::fileOperation (ctx, chosen(), dialogs::FileOp::copy); return true;
        case cmd::convert: dialogs::convert (ctx, chosen()); return true;
        case cmd::rgTrack: dialogs::scanReplayGain (ctx, chosen(), false); return true;
        case cmd::rgAlbum: dialogs::scanReplayGain (ctx, chosen(), true); return true;
        case cmd::rgRemove: dialogs::removeReplayGain (ctx, chosen()); return true;
        case cmd::queueSelected:
            for (int i : c.selected) c.addToQueue (active, i);
            c.say (String ((int) c.selected.size()) + (c.selected.size() == 1 ? " track plays next." : " tracks play next."));
            return true;
        case cmd::showInFinder: if (auto l = chosen(); ! l.empty()) sources::fileOf (l.front()).revealToUser(); return true;
        case cmd::copyLocations:
        {
            StringArray paths;
            for (auto& l : chosen()) paths.add (u8 (l));
            SystemClipboard::copyTextToClipboard (paths.joinIntoString ("\n"));
            return true;
        }

        case cmd::playPause: c.playPause(); return true;
        case cmd::play: c.play(); return true;
        case cmd::pause: c.pause(); return true;
        case cmd::stop: c.stop(); return true;
        case cmd::next: c.next(); return true;
        case cmd::previous: c.previous(); return true;
        case cmd::seekForward: c.seek (c.player.position() + 5); return true;
        case cmd::seekBack: c.seek (jmax (0.0, c.player.position() - 5)); return true;
        case cmd::volumeUp: c.setVolume (c.volume() + 2); return true;
        case cmd::volumeDown: c.setVolume (c.volume() - 2); return true;
        case cmd::mute: c.player.muted = ! c.player.muted.load(); commands.commandStatusChanged(); return true;
        case cmd::stopAfterCurrent: c.stopAfterCurrent = ! c.stopAfterCurrent; commands.commandStatusChanged(); return true;
        case cmd::playRandom: c.playRandom(); return true;
        case cmd::orderNormal: case cmd::orderRepeatPlaylist: case cmd::orderRepeatTrack: case cmd::orderShuffleTracks: case cmd::orderShuffleAlbums: case cmd::orderRandom:
            c.setOrder ((Order) (info.commandID - cmd::orderNormal)); return true;
        case cmd::rgOff: case cmd::rgTrackMode: case cmd::rgAlbumMode: case cmd::rgSmartMode:
        {
            auto s = c.replayGain;
            s.mode = (rg::Mode) (info.commandID - cmd::rgOff);
            c.setReplayGain (s);
            commands.commandStatusChanged();
            return true;
        }
        case cmd::equalizerOn: c.player.eq.enabled = ! c.player.eq.enabled.load(); commands.commandStatusChanged(); return true;

        case cmd::editLayout:
            view.setEditing (! view.editing());
            c.say (view.editing() ? "Editing the layout: click Change on any panel. " + ui::commandKey ("E") + " again when it's done." : String ("The layout is kept."));
            commands.commandStatusChanged();
            return true;
        case cmd::miniPlayer: setMini (! miniMode); return true;
        case cmd::resetLayout: view.setLayout (layout::presets().front().root); saveLayout(); return true;
        case cmd::textBigger: case cmd::textSmaller:
        {
            auto& sizes = theme::textSizes();
            const int now = roundToInt (theme::scale() * 100);
            int pick = now;
            if (info.commandID == cmd::textBigger) { for (int s : sizes) if (s > now) { pick = s; break; } }
            else for (auto it = sizes.rbegin(); it != sizes.rend(); ++it) if (*it < now) { pick = *it; break; }
            theme::setScale (pick / 100.0f);
            c.settings.set ("textSize", pick);
            view.setLayout (view.current());
            resized();
            c.say ("Text at " + String (pick) + "%.");
            return true;
        }
        case cmd::keyboardShortcuts: dialogs::preferences (ctx, 3); return true;
        case cmd::addonManager: dialogs::addons (ctx); return true;
        case cmd::addonConsole:
        {
            if (view.find ("add-on console") == nullptr)
            {
                auto root = view.current();
                layout::split (root, {}, layout::Side::below, "add-on console");
                view.setLayout (root);
                saveLayout();
            }
            return true;
        }
        case cmd::about:
        {
            String text;
            text << "Kalibre Player " << KP_VERSION << "\n\nA music player made from Kalibre, for Apple silicon Macs.\n\n"
                 << "It decodes with FFmpeg (LGPL), with LAME (LGPL) and libopus (BSD) for writing MP3 and Opus; reads and writes tags with TagLib (LGPL/MPL); "
                 << "opens archives with libarchive (BSD); and runs add-ons in Lua 5.4 (MIT). JUCE (AGPL/commercial) draws it and hosts plug-ins.\n\n"
                 << "Formats it decodes: " << decoderNames().joinIntoString (", ") << ".";
            dialogs::text ("About Kalibre Player", text);
            return true;
        }
        case cmd::titleFormatHelp: dialogs::text ("Title Formatting", dialogs::titleFormatHelp()); return true;
        case cmd::queryHelp: dialogs::text ("Search Queries", dialogs::queryHelp()); return true;
        case cmd::addonsHelp: dialogs::text ("Writing Add-ons", dialogs::addonsHelp()); return true;
        default: return false;
    }
}

} // namespace kp
