#include "TransportBar.h"
#include "TitleFormat.h"

namespace kp
{
using namespace juce;

double SeekBar::fractionAt (int x) const { return jlimit (0.0, 1.0, (double) (x - 6) / jmax (1, getWidth() - 12)); }

void SeekBar::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (6, 0);
    auto track = r.withSizeKeepingCentre (r.getWidth(), 6);
    g.setColour (theme::lane);
    g.fillRoundedRectangle (track, 3);
    const double len = ctl.player.length();
    if (! ctl.playing() || len <= 0) return;
    const double f = dragTo >= 0 ? dragTo : jlimit (0.0, 1.0, ctl.player.position() / len);
    g.setColour (theme::control);
    g.fillRoundedRectangle (track.withWidth ((float) (track.getWidth() * f)), 3);
    g.setColour (theme::playhead);
    g.fillEllipse (Rectangle<float> (12, 12).withCentre ({ track.getX() + (float) (track.getWidth() * f), track.getCentreY() }));
}

void SeekBar::mouseDown (const MouseEvent& e) { if (ctl.playing()) { dragTo = fractionAt (e.x); repaint(); } }
void SeekBar::mouseDrag (const MouseEvent& e) { if (dragTo >= 0) { dragTo = fractionAt (e.x); repaint(); } }
void SeekBar::mouseUp (const MouseEvent&)
{
    if (dragTo >= 0 && ctl.playing()) ctl.seek (dragTo * ctl.player.length());
    dragTo = -1;
    repaint();
}

//==============================================================================
TransportBar::TransportBar (Context& x) : ctx (x), seek (x.c)
{
    for (auto* b : { &previous, &playPause, &stop, &next }) addAndMakeVisible (b);
    previous.onClick = [this] { ctx.c.previous(); };
    playPause.onClick = [this] { ctx.c.playPause(); };
    stop.onClick = [this] { ctx.c.stop(); };
    next.onClick = [this] { ctx.c.next(); };
    addAndMakeVisible (seek);
    for (auto* l : { &played, &left })
    {
        l->setFont (theme::font (theme::smallPoints));
        l->setColour (Label::textColourId, theme::textSecondary);
        addAndMakeVisible (l);
    }
    left.setJustificationType (Justification::centredRight);
    title.setFont (theme::font (theme::bodyPoints, true));
    title.setColour (Label::textColourId, theme::text);
    addAndMakeVisible (title);
    volume.setRange (-60.0, 0.0, 0.5);
    volume.setSkewFactorFromMidPoint (-12.0);
    volume.setValue (ctx.c.volume(), dontSendNotification);
    volume.setTooltip ("Volume (Command-Up, Command-Down)");
    volume.onValueChange = [this] { ctx.c.setVolume (volume.getValue()); };
    volume.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (volume);
    for (int k = 0; k < orderCount; ++k) order.addItem (orderName ((Order) k), k + 1);
    order.setSelectedId ((int) ctx.c.order + 1, dontSendNotification);
    order.setTooltip ("What plays after each track");
    order.onChange = [this] { ctx.c.setOrder ((Order) (order.getSelectedId() - 1)); };
    addAndMakeVisible (order);
    ctx.c.addListener (this);
    update();
    startTimerHz (10);
}

TransportBar::~TransportBar() { ctx.c.removeListener (this); }

void TransportBar::paint (Graphics& g)
{
    g.fillAll (theme::raised);
    g.setColour (theme::rule);
    g.drawHorizontalLine (0, 0, (float) getWidth());
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced (theme::px (10), theme::px (6));
    auto top = r.removeFromTop (r.getHeight() / 2);
    played.setBounds (top.removeFromLeft (theme::px (56)));
    left.setBounds (top.removeFromRight (theme::px (56)));
    seek.setBounds (top);
    r.removeFromTop (2);
    const int b = jmin (r.getHeight(), theme::px (32));
    for (auto* btn : { &previous, &playPause, &stop, &next })
    {
        btn->setBounds (r.removeFromLeft (b + theme::px (8)).withSizeKeepingCentre (b + theme::px (4), b));
        r.removeFromLeft (4);
    }
    order.setBounds (r.removeFromRight (theme::px (170)).withSizeKeepingCentre (theme::px (170), jmin (r.getHeight(), theme::px (28))));
    r.removeFromRight (theme::px (10));
    volume.setBounds (r.removeFromRight (theme::px (140)));
    r.removeFromRight (theme::px (10));
    title.setBounds (r.reduced (theme::px (8), 0));
}

void TransportBar::update()
{
    const bool on = ctx.c.playing() && ! ctx.c.paused();
    playPause.setButtonText (String::fromUTF8 (on ? "\xe2\x8f\xb8" : "\xe2\x96\xb6"));
    playPause.setToggleState (on, dontSendNotification);
    if (auto t = ctx.c.nowPlayingTrack()) title.setText (String::fromUTF8 (tf::format ("%title%[  \xc2\xb7  %artist%]", *t).c_str()), dontSendNotification);
    else title.setText ({}, dontSendNotification);
    order.setSelectedId ((int) ctx.c.order + 1, dontSendNotification);
    timerCallback();
}

void TransportBar::timerCallback()
{
    const double pos = ctx.c.player.position(), len = ctx.c.player.length();
    const bool on = ctx.c.playing();
    played.setText (on ? String (formatTime (pos)) : String(), dontSendNotification);
    left.setText (on && len > 0 ? String::fromUTF8 ("\xe2\x88\x92") + String (formatTime (len - pos)) : String(), dontSendNotification);
    if (std::abs (volume.getValue() - ctx.c.volume()) > 0.01 && ! volume.isMouseButtonDown()) volume.setValue (ctx.c.volume(), dontSendNotification);
    seek.repaint();
}

} // namespace kp
