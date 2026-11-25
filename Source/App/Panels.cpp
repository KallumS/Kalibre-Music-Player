#include "Panels.h"
#include "EqualizerView.h"
#include "LibraryView.h"
#include "PlaylistView.h"
#include "TagIO.h"
#include "TitleFormat.h"
#include "TransportBar.h"
#include "Visualisers.h"

namespace kp
{
using namespace juce;

const std::vector<PanelType>& panelTypes()
{
    static const std::vector<PanelType> t {
        { "playlist", "Playlist" }, { "library", "Library" }, { "album art", "Album Art" }, { "now playing", "Now Playing" },
        { "transport", "Transport" }, { "equalizer", "Equaliser" }, { "oscilloscope", "Oscilloscope" }, { "goniometer", "Goniometer" },
        { "spectrum", "Spectrum" }, { "peak meter", "Peak Meter" }, { "loudness", "Loudness" }, { "add-on console", "Add-on Console" },
    };
    return t;
}

String panelTitle (const std::string& name)
{
    for (auto& p : panelTypes()) if (name == p.name) return p.title;
    return String::fromUTF8 (name.c_str());
}

std::unique_ptr<Component> makePanel (const std::string& name, Context& ctx)
{
    if (name == "playlist") return std::make_unique<PlaylistView> (ctx);
    if (name == "library") return std::make_unique<LibraryView> (ctx);
    if (name == "album art") return std::make_unique<AlbumArtView> (ctx);
    if (name == "now playing") return std::make_unique<NowPlayingView> (ctx);
    if (name == "transport") return std::make_unique<TransportBar> (ctx);
    if (name == "equalizer") return std::make_unique<EqualizerView> (ctx);
    if (name == "oscilloscope") return std::make_unique<Oscilloscope> (ctx.c.player);
    if (name == "goniometer") return std::make_unique<Goniometer> (ctx.c.player);
    if (name == "spectrum") return std::make_unique<Spectrum> (ctx.c.player);
    if (name == "peak meter") return std::make_unique<PeakMeter> (ctx.c.player);
    if (name == "loudness") return std::make_unique<LoudnessView> (ctx.c.player);
    if (name == "add-on console") return std::make_unique<AddonConsoleView> (ctx);
    // A panel this version doesn't know (a layout from a newer one): an empty place.
    struct Unknown : Component { void paint (Graphics& g) override { paintPanel (g, *this, "Unknown Panel"); } };
    return std::make_unique<Unknown>();
}

void paintPanel (Graphics& g, Component& c, const String& title)
{
    g.fillAll (theme::sunken);
    if (title.isNotEmpty())
    {
        g.setColour (theme::textSecondary);
        g.setFont (theme::font (theme::smallPoints, true));
        g.drawText (title.toUpperCase(), c.getLocalBounds().removeFromTop (headingHeight).reduced (8, 0), Justification::centredLeft);
    }
}

//==============================================================================
AlbumArtView::AlbumArtView (Context& x) : ctx (x)
{
    ctx.c.addListener (this);
    update();
}
AlbumArtView::~AlbumArtView() { ctx.c.removeListener (this); }

void AlbumArtView::update()
{
    // What's playing; or, stopped, the first track chosen.
    std::string loc;
    if (auto t = ctx.c.nowPlayingTrack()) loc = t->location;
    else if (auto sel = ctx.c.selectedLocations(); ! sel.empty()) loc = sel.front();
    if (loc == shownFor) return;
    shownFor = loc;
    image = loc.empty() ? Image() : tagio::cover (loc);
    repaint();
}

void AlbumArtView::paint (Graphics& g)
{
    paintPanel (g, *this);
    auto area = getLocalBounds().reduced (8).toFloat();
    if (! image.isValid())
    {
        g.setColour (theme::lane);
        const auto side = jmin (area.getWidth(), area.getHeight());
        auto square = area.withSizeKeepingCentre (side, side);
        g.fillRoundedRectangle (square, theme::cornerRadius);
        g.setColour (theme::textSecondary);
        g.setFont (theme::font (theme::smallPoints));
        g.drawText (shownFor.empty() ? "No Cover" : "No Cover in the File or Its Folder", square.reduced (8), Justification::centred);
        return;
    }
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (image, area, RectanglePlacement::centred);
}

//==============================================================================
NowPlayingView::NowPlayingView (Context& x) : ctx (x)
{
    ctx.c.addListener (this);
    startTimerHz (4);
}
NowPlayingView::~NowPlayingView() { ctx.c.removeListener (this); }

void NowPlayingView::paint (Graphics& g)
{
    paintPanel (g, *this, "Now Playing");
    auto area = getLocalBounds().reduced (12, 0).withTrimmedTop (headingHeight);
    auto t = ctx.c.nowPlayingTrack();
    if (! t)
    {
        g.setColour (theme::textSecondary);
        g.setFont (theme::font());
        g.drawText ("Nothing is playing. Double-click a track to play it.", area, Justification::topLeft);
        return;
    }
    const auto pattern = ctx.c.settings.get ("nowPlayingPattern",
        "%title%$crlf()[%artist%]$crlf()[%album%][ (%year%)]$crlf()%codec%[ %bitrate% kb/s][, %samplerate% Hz][, %bitspersample%-bit]");
    const auto text = String::fromUTF8 (tf::format (pattern.toStdString(), *t).c_str());
    auto lines = StringArray::fromLines (text);
    for (int k = 0; k < lines.size() && area.getHeight() > 0; ++k)
    {
        const bool first = k == 0;
        g.setColour (first ? theme::text : theme::textSecondary);
        g.setFont (theme::font (first ? 18.0f : theme::bodyPoints, first));
        const int h = theme::px (first ? 26 : 19);
        g.drawFittedText (lines[k], area.removeFromTop (h), Justification::centredLeft, 1);
    }
    g.setColour (theme::textSecondary);
    g.setFont (theme::font (theme::smallPoints));
    g.drawText (String (formatTime (ctx.c.player.position())) + " / " + String (formatTime (ctx.c.player.length())), area.removeFromTop (theme::px (18)),
                Justification::centredLeft);
}

void NowPlayingView::mouseDown (const MouseEvent& e)
{
    if (! e.mods.isPopupMenu()) return;
    PopupMenu m;
    m.addItem (String::fromUTF8 ("Change What It Shows\xe2\x80\xa6"), [this]
    {
        auto* w = new AlertWindow ("Now Playing", "A title-formatting pattern; $crlf() starts a line.", MessageBoxIconType::NoIcon);
        w->addTextEditor ("pattern", ctx.c.settings.get ("nowPlayingPattern",
            "%title%$crlf()[%artist%]$crlf()[%album%][ (%year%)]$crlf()%codec%[ %bitrate% kb/s][, %samplerate% Hz][, %bitspersample%-bit]"));
        w->addButton ("Change", 1, KeyPress (KeyPress::returnKey));
        w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        ui::present (w, ModalCallbackFunction::create ([this, w] (int r)
        {
            if (r == 1) { ctx.c.settings.set ("nowPlayingPattern", w->getTextEditorContents ("pattern")); repaint(); }
        }), true);
    });
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMousePosition());
}

//==============================================================================
AddonConsoleView::AddonConsoleView (Context& x) : ctx (x)
{
    text.setMultiLine (true);
    text.setReadOnly (true);
    text.setFont (FontOptions (Font::getDefaultMonospacedFontName(), theme::px (11), Font::plain));
    addAndMakeVisible (text);
    ctx.c.addListener (this);
    messageChanged();
}
AddonConsoleView::~AddonConsoleView() { ctx.c.removeListener (this); }
void AddonConsoleView::paint (Graphics& g) { paintPanel (g, *this, "Add-on Console"); }
void AddonConsoleView::resized() { text.setBounds (getLocalBounds().withTrimmedTop (headingHeight).reduced (4)); }
void AddonConsoleView::messageChanged()
{
    const auto all = ctx.c.addonConsole.joinIntoString ("\n");
    if (all != text.getText()) { text.setText (all, false); text.moveCaretToEnd(); }
}

} // namespace kp
