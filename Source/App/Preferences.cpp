// Preferences (decision 0002 amended): the library's folders, playback
// (the output, ReplayGain, crossfade), plug-ins, keyboard shortcuts, and
// how the window looks.
#include "Dialogs.h"

#include <juce_audio_utils/juce_audio_utils.h>

namespace kp::dialogs
{
using namespace juce;

namespace
{
String u8 (const std::string& s) { return String::fromUTF8 (s.c_str()); }

void heading (Graphics& g, Rectangle<int> r, const String& text)
{
    g.setColour (theme::textSecondary);
    g.setFont (theme::font (theme::smallPoints, true));
    g.drawText (text.toUpperCase(), r, Justification::bottomLeft);
}

//==============================================================================
class LibraryPage : public Component, private ListBoxModel, private Controller::Listener
{
public:
    explicit LibraryPage (Context& x) : ctx (x)
    {
        list.setModel (this);
        list.setColour (ListBox::backgroundColourId, theme::roll);
        list.setRowHeight (theme::px (24));
        addAndMakeVisible (list);
        for (auto* b : { &add, &remove, &again }) addAndMakeVisible (b);
        add.onClick = [this]
        {
            chooser = std::make_unique<FileChooser> ("A Folder for the Library", File::getSpecialLocation (File::userMusicDirectory));
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this] (const FileChooser& fc)
            {
                if (fc.getResult().isDirectory()) { ctx.c.addFolder (fc.getResult()); list.updateContent(); }
            });
        };
        remove.onClick = [this]
        {
            const int r = list.getSelectedRow();
            if (r >= 0 && r < (int) ctx.c.library.folders.size()) { ctx.c.removeFolder (u8 (ctx.c.library.folders[(size_t) r])); list.updateContent(); }
        };
        again.onClick = [this] { ctx.c.rescan(); };
        sends.setToggleState (ctx.c.settings.getBool ("librarySendsSelection", true), dontSendNotification);
        sends.onClick = [this] { ctx.c.settings.set ("librarySendsSelection", sends.getToggleState()); };
        addAndMakeVisible (sends);
        ctx.c.addListener (this);
    }
    ~LibraryPage() override { ctx.c.removeListener (this); }
    void paint (Graphics& g) override
    {
        g.fillAll (theme::ground);
        heading (g, getLocalBounds().reduced (12).removeFromTop (theme::px (22)), "Folders the library watches");
        g.setColour (theme::textSecondary);
        g.setFont (theme::font (theme::smallPoints));
        String status = String ((int) ctx.c.library.size()) + " tracks.";
        if (ctx.c.scanStatus.isNotEmpty()) status << " " << ctx.c.scanStatus;
        g.drawText (status, getLocalBounds().reduced (12).removeFromBottom (theme::px (60)).removeFromTop (theme::px (22)), Justification::centredLeft);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromTop (theme::px (28));
        sends.setBounds (r.removeFromBottom (theme::px (26)));
        r.removeFromBottom (theme::px (24));
        auto b = r.removeFromBottom (theme::px (30));
        for (auto* x : { &add, &remove, &again }) { x->setBounds (b.removeFromLeft (x->idealWidth())); b.removeFromLeft (8); }
        r.removeFromBottom (8);
        list.setBounds (r);
    }

private:
    Context& ctx;
    ListBox list;
    theme::HouseButton add { String::fromUTF8 ("Add a Folder\xe2\x80\xa6") }, remove { "Remove" }, again { "Look Through Again" };
    ToggleButton sends { "Choosing in the library fills the Library Selection playlist" };
    std::unique_ptr<FileChooser> chooser;
    int getNumRows() override { return (int) ctx.c.library.folders.size(); }
    void paintListBoxItem (int row, Graphics& g, int w, int h, bool selected) override
    {
        if (selected) g.fillAll (theme::raised);
        g.setColour (theme::text);
        g.setFont (theme::font());
        if (row >= 0 && row < getNumRows()) g.drawText (u8 (ctx.c.library.folders[(size_t) row]), 8, 0, w - 16, h, Justification::centredLeft, true);
    }
    void libraryChanged() override { list.updateContent(); repaint(); }
    void messageChanged() override { repaint(); }
};

//==============================================================================
class PlaybackPage : public Component
{
public:
    explicit PlaybackPage (Context& x) : ctx (x), device (x.c.devices, 0, 0, 2, 2, false, false, true, false)
    {
        addAndMakeVisible (device);
        for (int m = 0; m < 4; ++m) mode.addItem (rg::modeName ((rg::Mode) m), m + 1);
        mode.setSelectedId ((int) ctx.c.replayGain.mode + 1, dontSendNotification);
        auto setupDb = [] (Slider& s, double v)
        {
            s.setSliderStyle (Slider::LinearHorizontal);
            s.setTextBoxStyle (Slider::TextBoxRight, false, 70, 20);
            s.setRange (-15.0, 15.0, 0.5);
            s.setTextValueSuffix (" dB");
            s.setValue (v, dontSendNotification);
        };
        setupDb (withTags, ctx.c.replayGain.preampWith);
        setupDb (withoutTags, ctx.c.replayGain.preampWithout);
        clip.setToggleState (ctx.c.replayGain.preventClipping, dontSendNotification);
        scan.setToggleState (ctx.c.replayGain.scanMissing, dontSendNotification);
        for (auto* c : std::initializer_list<Component*> { &mode, &withTags, &withoutTags, &clip, &scan }) addAndMakeVisible (c);
        auto apply = [this]
        {
            rg::Settings s;
            s.mode = (rg::Mode) (mode.getSelectedId() - 1);
            s.preampWith = withTags.getValue();
            s.preampWithout = withoutTags.getValue();
            s.preventClipping = clip.getToggleState();
            s.scanMissing = scan.getToggleState();
            ctx.c.setReplayGain (s);
        };
        mode.onChange = apply;
        withTags.onValueChange = apply;
        withoutTags.onValueChange = apply;
        clip.onClick = apply;
        scan.onClick = apply;
        crossfade.setRange (0.0, 12.0, 0.5);
        crossfade.setTextBoxStyle (Slider::TextBoxRight, false, 70, 20);
        crossfade.setTextValueSuffix (" s");
        crossfade.setValue (ctx.c.player.crossfadeSeconds.load(), dontSendNotification);
        crossfade.onValueChange = [this] { ctx.c.setCrossfade (crossfade.getValue()); };
        addAndMakeVisible (crossfade);
    }
    void paint (Graphics& g) override
    {
        g.fillAll (theme::ground);
        auto r = getLocalBounds().reduced (12);
        heading (g, r.removeFromTop (theme::px (22)), "Output");
        r.removeFromTop (theme::px (210));
        heading (g, r.removeFromTop (theme::px (22)), "ReplayGain");
        g.setColour (theme::textSecondary);
        g.setFont (theme::font (theme::smallPoints));
        for (auto* l : { "Use", "Preamp with ReplayGain", "Preamp without" })
            g.drawText (l, r.removeFromTop (theme::px (30)).removeFromLeft (theme::px (170)), Justification::centredLeft);
        r.removeFromTop (theme::px (60));
        heading (g, r.removeFromTop (theme::px (22)), "Crossfade");
        g.setColour (theme::textSecondary);
        g.drawText ("Between tracks (0: gapless)", r.removeFromTop (theme::px (30)).removeFromLeft (theme::px (170)), Justification::centredLeft);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromTop (theme::px (22));
        device.setBounds (r.removeFromTop (theme::px (210)));
        r.removeFromTop (theme::px (22));
        auto row = [&] { return r.removeFromTop (theme::px (30)).withTrimmedLeft (theme::px (170)).reduced (0, 3); };
        mode.setBounds (row().removeFromLeft (theme::px (300)));
        withTags.setBounds (row());
        withoutTags.setBounds (row());
        clip.setBounds (r.removeFromTop (theme::px (28)));
        scan.setBounds (r.removeFromTop (theme::px (28)));
        r.removeFromTop (4);
        r.removeFromTop (theme::px (22));
        crossfade.setBounds (row());
    }

private:
    Context& ctx;
    AudioDeviceSelectorComponent device;
    ComboBox mode;
    Slider withTags, withoutTags, crossfade;
    ToggleButton clip { "Turn down a track whose peak would clip" }, scan { "Measure tracks without ReplayGain as they're queued" };
};

//==============================================================================
struct PluginWindow : public DocumentWindow
{
    explicit PluginWindow (AudioPluginInstance& p) : DocumentWindow (p.getName(), theme::ground, DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        if (auto* e = p.createEditorAndMakeActive()) setContentOwned (e, true);
        else setContentOwned (new GenericAudioProcessorEditor (p), true);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }
    void closeButtonPressed() override { setVisible (false); MessageManager::callAsync ([this] { delete this; }); }
};

class PluginsPage : public Component, private ListBoxModel, private ChangeListener
{
public:
    explicit PluginsPage (Context& x)
        : ctx (x), scanner (x.c.plugins.formats, x.c.plugins.known, x.c.plugins.deadMansPedal(), nullptr, true)
    {
        ctx.c.plugins.known.addChangeListener (this);
        addAndMakeVisible (scanner);
        chain.setModel (this);
        chain.setColour (ListBox::backgroundColourId, theme::roll);
        chain.setRowHeight (theme::px (26));
        addAndMakeVisible (chain);
        for (auto* b : { &add, &remove, &up, &down, &bypass, &edit }) addAndMakeVisible (b);
        add.onClick = [this]
        {
            PopupMenu m;
            auto effects = ctx.c.plugins.effects();
            String maker;
            PopupMenu sub;
            for (auto& d : effects)
            {
                if (d.manufacturerName != maker && sub.getNumItems() > 0) { m.addSubMenu (maker.isEmpty() ? String ("Others") : maker, sub); sub = {}; }
                maker = d.manufacturerName;
                sub.addItem (d.name + " (" + d.pluginFormatName + ")", [this, d]
                {
                    ctx.c.plugins.capture (ctx.c.player);
                    ctx.c.plugins.slots.push_back ({ d, {}, false });
                    applyChain();
                });
            }
            if (sub.getNumItems() > 0) m.addSubMenu (maker.isEmpty() ? String ("Others") : maker, sub);
            if (m.getNumItems() == 0) m.addItem ("No effects yet: scan for plug-ins above", false, false, [] {});
            m.showMenuAsync (PopupMenu::Options().withTargetComponent (&add));
        };
        remove.onClick = [this] { change ([this] (int r) { ctx.c.plugins.slots.erase (ctx.c.plugins.slots.begin() + r); }); };
        up.onClick = [this] { change ([this] (int r) { if (r > 0) { std::swap (ctx.c.plugins.slots[(size_t) r], ctx.c.plugins.slots[(size_t) r - 1]); chain.selectRow (r - 1); } }); };
        down.onClick = [this] { change ([this] (int r) { if (r + 1 < (int) ctx.c.plugins.slots.size()) { std::swap (ctx.c.plugins.slots[(size_t) r], ctx.c.plugins.slots[(size_t) r + 1]); chain.selectRow (r + 1); } }); };
        bypass.onClick = [this] { change ([this] (int r) { ctx.c.plugins.slots[(size_t) r].bypassed = ! ctx.c.plugins.slots[(size_t) r].bypassed; }); };
        edit.onClick = [this] { openEditor (chain.getSelectedRow()); };
    }
    ~PluginsPage() override { ctx.c.plugins.known.removeChangeListener (this); }
    void paint (Graphics& g) override
    {
        g.fillAll (theme::ground);
        auto r = getLocalBounds().reduced (12);
        heading (g, r.removeFromTop (theme::px (22)), "The chain: the sound passes through these, top to bottom, after the equaliser");
        r.removeFromTop (getHeight() / 2 - theme::px (30));
        heading (g, r.removeFromTop (theme::px (22)), "Plug-ins found (AU and VST3 effects)");
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromTop (theme::px (22));
        auto top = r.removeFromTop (getHeight() / 2 - theme::px (30));
        auto b = top.removeFromBottom (theme::px (30));
        for (auto* x : { &add, &remove, &up, &down, &bypass, &edit }) { x->setBounds (b.removeFromLeft (x->idealWidth())); b.removeFromLeft (6); }
        top.removeFromBottom (6);
        chain.setBounds (top);
        r.removeFromTop (theme::px (22));
        scanner.setBounds (r);
    }

private:
    Context& ctx;
    PluginListComponent scanner;
    ListBox chain;
    theme::HouseButton add { String::fromUTF8 ("Add\xe2\x80\xa6") }, remove { "Remove" }, up { "Up" }, down { "Down" }, bypass { "Bypass" },
                       edit { String::fromUTF8 ("Open\xe2\x80\xa6") };

    void changeListenerCallback (ChangeBroadcaster*) override { ctx.c.plugins.saveKnown(); }
    void applyChain()
    {
        const auto e = ctx.c.plugins.apply (ctx.c.player);
        if (e.isNotEmpty()) ctx.c.say ("Plug-ins: " + e, true);
        chain.updateContent();
        chain.repaint();
    }
    void change (std::function<void (int)> f)
    {
        const int r = chain.getSelectedRow();
        if (r < 0 || r >= (int) ctx.c.plugins.slots.size()) return;
        ctx.c.plugins.capture (ctx.c.player);
        f (r);
        applyChain();
    }
    void openEditor (int r)
    {
        auto* c = ctx.c.player.chain();
        if (! c || r < 0 || r >= (int) c->plugins.size()) return;
        new PluginWindow (*c->plugins[(size_t) r]);
    }
    int getNumRows() override { return (int) ctx.c.plugins.slots.size(); }
    void paintListBoxItem (int row, Graphics& g, int w, int h, bool selected) override
    {
        if (row < 0 || row >= getNumRows()) return;
        auto& s = ctx.c.plugins.slots[(size_t) row];
        if (selected) g.fillAll (theme::raised);
        g.setColour (s.bypassed ? theme::textSecondary : theme::text);
        g.setFont (theme::font());
        g.drawText (String (row + 1) + ".  " + s.description.name + "  (" + s.description.manufacturerName + ")" + (s.bypassed ? "  bypassed" : ""),
                    8, 0, w - 16, h, Justification::centredLeft, true);
    }
    void listBoxItemDoubleClicked (int row, const MouseEvent&) override { openEditor (row); }
};

//==============================================================================
class AppearancePage : public Component
{
public:
    explicit AppearancePage (Context& x) : ctx (x)
    {
        int id = 1;
        for (auto& p : theme::palettes()) themes.addItem (p.name, id++);
        themes.setSelectedId (theme::currentPalette() + 1, dontSendNotification);
        themes.onChange = [this]
        {
            theme::usePalette (themes.getSelectedId() - 1);
            ctx.c.settings.set ("theme", themes.getSelectedId() - 1);
        };
        for (int s : theme::textSizes()) sizes.addItem (String (s) + "%", s);
        sizes.setSelectedId (roundToInt (theme::scale() * 100), dontSendNotification);
        sizes.onChange = [this]
        {
            ctx.c.settings.set ("textSize", sizes.getSelectedId());
            theme::setScale (sizes.getSelectedId() / 100.0f);
            if (auto* w = TopLevelWindow::getActiveTopLevelWindow()) w->repaint();
            ctx.c.say ("The new text size is used everywhere once the window is laid out again (View > Edit Layout twice, or reopen the player).");
        };
        addAndMakeVisible (themes);
        addAndMakeVisible (sizes);
    }
    void paint (Graphics& g) override
    {
        g.fillAll (theme::ground);
        auto r = getLocalBounds().reduced (12);
        heading (g, r.removeFromTop (theme::px (22)), "Theme");
        r.removeFromTop (theme::px (40));
        heading (g, r.removeFromTop (theme::px (22)), "Text size");
        r.removeFromTop (theme::px (40));
        g.setColour (theme::textSecondary);
        g.setFont (theme::font (theme::smallPoints));
        g.drawFittedText ("The window's panels are rearranged with View > Edit Layout, and View > Layout has ready-made layouts.",
                          r.removeFromTop (theme::px (40)), Justification::topLeft, 2);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromTop (theme::px (24));
        themes.setBounds (r.removeFromTop (theme::px (30)).removeFromLeft (theme::px (240)));
        r.removeFromTop (theme::px (34));
        sizes.setBounds (r.removeFromTop (theme::px (30)).removeFromLeft (theme::px (120)));
    }

private:
    Context& ctx;
    ComboBox themes, sizes;
};

class ShortcutsPage : public Component
{
public:
    explicit ShortcutsPage (Context& x) : ctx (x), editor (*x.commands.getKeyMappings(), true)
    {
        addAndMakeVisible (editor);
        editor.setColours (theme::raised, theme::text);
    }
    ~ShortcutsPage() override
    {
        if (auto xml = ctx.commands.getKeyMappings()->createXml (true)) ctx.c.settings.setXml ("keys", xml.get());
    }
    void paint (Graphics& g) override { g.fillAll (theme::ground); }
    void resized() override { editor.setBounds (getLocalBounds().reduced (12)); }

private:
    Context& ctx;
    KeyMappingEditorComponent editor;
};

class PreferencesView : public TabbedComponent
{
public:
    PreferencesView (Context& ctx, int tab) : TabbedComponent (TabbedButtonBar::TabsAtTop)
    {
        setTabBarDepth (theme::px (30));
        addTab ("Library", theme::ground, new LibraryPage (ctx), true);
        addTab ("Playback", theme::ground, new PlaybackPage (ctx), true);
        addTab ("Plug-ins", theme::ground, new PluginsPage (ctx), true);
        addTab ("Keyboard Shortcuts", theme::ground, new ShortcutsPage (ctx), true);
        addTab ("Appearance", theme::ground, new AppearancePage (ctx), true);
        setCurrentTabIndex (jlimit (0, 4, tab));
    }
    void paint (Graphics& g) override { g.fillAll (theme::ground); }   // behind the tabs, not the window's own
};
} // namespace

void preferences (Context& ctx, int tab)
{
    static Component::SafePointer<Component> shown;
    if (shown)
    {
        if (auto* t = dynamic_cast<TabbedComponent*> (shown.getComponent())) t->setCurrentTabIndex (tab);
        if (auto* w = shown->findParentComponentOfClass<DocumentWindow>()) w->toFront (true);
        return;
    }
    auto view = std::make_unique<PreferencesView> (ctx, tab);
    shown = view.get();
    open ("Preferences", std::move (view), 760, 680);
}

} // namespace kp::dialogs
