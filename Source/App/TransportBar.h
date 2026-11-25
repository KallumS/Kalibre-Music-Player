/*
    TransportBar - play, pause, stop, previous and next; the seek bar with
    the time played and left; the volume; the order (decision 0014). The
    musician keeps the transport at the bottom of the window, as in Kalibre.
*/

#pragma once

#include "Panels.h"

namespace kp
{

class SeekBar : public juce::Component
{
public:
    explicit SeekBar (Controller& c) : ctl (c) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    Controller& ctl;
    double dragTo = -1;
    double fractionAt (int x) const;
};

// A transport button with its symbol drawn, not set in type (the font's are tiny).
class IconButton : public theme::HouseButton
{
public:
    enum class Icon { previous, play, pause, stop, next };
    IconButton (Icon i, const juce::String& tip) : theme::HouseButton ({}, tip), icon (i) { setTitle (tip.upToFirstOccurrenceOf (" (", false, false)); }
    void setIcon (Icon i) { if (icon != i) { icon = i; repaint(); } }
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    Icon icon;
};

class TransportBar : public juce::Component, private Controller::Listener, private juce::Timer
{
public:
    explicit TransportBar (Context&);
    ~TransportBar() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Context& ctx;
    IconButton previous { IconButton::Icon::previous, "Previous (Command-Left)" };
    IconButton playPause { IconButton::Icon::play, "Play or pause (Space)" };
    IconButton stop { IconButton::Icon::stop, "Stop (Command-.)" };
    IconButton next { IconButton::Icon::next, "Next (Command-Right)" };
    SeekBar seek;
    juce::Label played, left, title;
    juce::Slider volume { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::ComboBox order;

    void playbackChanged() override { update(); }
    void timerCallback() override;
    void update();
};

} // namespace kp
