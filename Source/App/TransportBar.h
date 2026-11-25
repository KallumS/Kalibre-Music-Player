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

class TransportBar : public juce::Component, private Controller::Listener, private juce::Timer
{
public:
    explicit TransportBar (Context&);
    ~TransportBar() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Context& ctx;
    theme::HouseButton previous { juce::String::fromUTF8 ("\xe2\x8f\xae"), "Previous (Command-Left)" };
    theme::HouseButton playPause { juce::String::fromUTF8 ("\xe2\x96\xb6"), "Play or pause (Space)" };
    theme::HouseButton stop { juce::String::fromUTF8 ("\xe2\x96\xa0"), "Stop" };
    theme::HouseButton next { juce::String::fromUTF8 ("\xe2\x8f\xad"), "Next (Command-Right)" };
    SeekBar seek;
    juce::Label played, left, title;
    juce::Slider volume { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::ComboBox order;

    void playbackChanged() override { update(); }
    void timerCallback() override;
    void update();
};

} // namespace kp
