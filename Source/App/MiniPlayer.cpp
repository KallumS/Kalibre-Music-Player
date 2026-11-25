#include "MiniPlayer.h"
#include "TagIO.h"
#include "TitleFormat.h"

namespace kp
{
using namespace juce;

MiniPlayer::MiniPlayer (Context& x) : ctx (x), seek (x.c)
{
    for (auto* b : { &previous, &playPause, &next, &expand }) addAndMakeVisible (b);
    previous.onClick = [this] { ctx.c.previous(); };
    playPause.onClick = [this] { ctx.c.playPause(); };
    next.onClick = [this] { ctx.c.next(); };
    expand.onClick = [this] { if (onExpand) onExpand(); };
    addAndMakeVisible (seek);
    ctx.c.addListener (this);
    playbackChanged();
    startTimerHz (4);
}

MiniPlayer::~MiniPlayer() { ctx.c.removeListener (this); }

void MiniPlayer::playbackChanged()
{
    playPause.setIcon (ctx.c.playing() && ! ctx.c.paused() ? IconButton::Icon::pause : IconButton::Icon::play);
    auto t = ctx.c.nowPlayingTrack();
    const auto loc = t ? t->location : std::string();
    if (loc != coverFor)
    {
        coverFor = loc;
        cover = loc.empty() ? Image() : tagio::cover (loc);
    }
    repaint();
}

void MiniPlayer::paint (Graphics& g)
{
    g.fillAll (theme::raised);
    g.setColour (theme::rule);
    g.drawRect (getLocalBounds());

    auto art = coverArea.toFloat();
    if (cover.isValid())
    {
        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (cover, art, RectanglePlacement::centred | RectanglePlacement::fillDestination);
    }
    else
    {
        g.setColour (theme::lane);
        g.fillRoundedRectangle (art, 4.0f);
        // A note, drawn, where a cover would be.
        g.setColour (theme::textSecondary);
        auto n = art.reduced (art.getWidth() * 0.3f);
        g.fillEllipse (n.getX(), n.getBottom() - n.getHeight() * 0.35f, n.getWidth() * 0.5f, n.getHeight() * 0.35f);
        g.fillRect (n.getX() + n.getWidth() * 0.42f, n.getY(), 2.0f, n.getHeight() * 0.8f);
    }

    auto text = textArea;
    auto t = ctx.c.nowPlayingTrack();
    g.setColour (theme::text);
    g.setFont (theme::font (theme::bodyPoints, true));
    g.drawText (t ? String::fromUTF8 (tf::format ("%title%", *t).c_str()) : String ("Nothing Playing"),
                text.removeFromTop (text.getHeight() / 2), Justification::bottomLeft, true);
    g.setColour (theme::textSecondary);
    g.setFont (theme::font (theme::smallPoints));
    g.drawText (t ? String::fromUTF8 (tf::format ("[%artist%][  \xc2\xb7  %album%]", *t).c_str()) : String ("Double-click for the full window"),
                text, Justification::topLeft, true);

    if (t)
    {
        const double pos = ctx.c.player.position(), len = ctx.c.player.length();
        g.drawText (String (formatTime (pos)) + (len > 0 ? " / " + String (formatTime (len)) : String()), timeArea, Justification::centredRight);
    }
}

void MiniPlayer::resized()
{
    auto r = getLocalBounds().reduced (theme::px (6));
    coverArea = r.removeFromLeft (r.getHeight());
    r.removeFromLeft (theme::px (10));
    const int b = theme::px (30);
    auto buttons = r.removeFromRight (b * 4 + theme::px (14));
    expand.setBounds (buttons.removeFromRight (b - theme::px (4)).withSizeKeepingCentre (b - theme::px (4), b - theme::px (4)));
    buttons.removeFromRight (theme::px (8));
    for (auto* btn : { &next, &playPause, &previous })
    {
        btn->setBounds (buttons.removeFromRight (b).withSizeKeepingCentre (b, b));
        buttons.removeFromRight (2);
    }
    r.removeFromRight (theme::px (6));
    auto bottom = r.removeFromBottom (theme::px (16));
    timeArea = bottom.removeFromRight (theme::px (84));
    seek.setBounds (bottom.translated (-theme::px (6), 0));
    textArea = r;
}

void MiniPlayer::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        const bool inFront = ctx.c.settings.getBool ("miniInFront", true);
        PopupMenu m;
        m.addItem ("Keep in Front", true, inFront, [this, inFront]
        {
            ctx.c.settings.set ("miniInFront", ! inFront);
            if (onKeepInFront) onKeepInFront (! inFront);
        });
        m.addItem ("Full Window", [this] { if (onExpand) onExpand(); });
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMousePosition());
        return;
    }
    if (auto* top = getTopLevelComponent(); top && top != this) dragger.startDraggingComponent (top, e.getEventRelativeTo (top));
}

void MiniPlayer::mouseDrag (const MouseEvent& e)
{
    if (auto* top = getTopLevelComponent(); top && top != this && ! e.mods.isPopupMenu()) dragger.dragComponent (top, e.getEventRelativeTo (top), nullptr);
}

void MiniPlayer::mouseDoubleClick (const MouseEvent&) { if (onExpand) onExpand(); }

} // namespace kp
