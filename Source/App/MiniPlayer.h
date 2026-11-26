/*
    MiniPlayer - the window as small as it can usefully be (decision 0018):
    the cover, what's playing, the seek bar, and Previous, Play and Next, in
    a strip that stays in front of other windows. View > Mini Player
    (Command-Shift-M), its expand button, or a double-click goes back to the
    full window, where it was. Drag it anywhere; right-click for Keep in
    Front.
*/

#pragma once

#include "TransportBar.h"

namespace kp
{

class MiniPlayer : public juce::Component, private Controller::Listener, private juce::Timer
{
public:
    explicit MiniPlayer (Context&);
    ~MiniPlayer() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    std::function<void()> onExpand;
    std::function<void (bool)> onKeepInFront;
    static juce::Point<int> size() { return { theme::px (440), theme::px (68) }; }

private:
    Context& ctx;
    IconButton previous { IconButton::Icon::previous, "Previous (" + ui::commandKey ("Left") + ")" };
    IconButton playPause { IconButton::Icon::play, "Play or pause (Space)" };
    IconButton next { IconButton::Icon::next, "Next (" + ui::commandKey ("Right") + ")" };
    IconButton expand { IconButton::Icon::expand, "Back to the full window (" + ui::commandKey (platform::with ("Shift", "M").c_str()) + ")" };
    SeekBar seek;
    juce::Image cover;
    std::string coverFor;
    juce::ComponentDragger dragger;
    juce::Rectangle<int> coverArea, textArea, timeArea;

    void playbackChanged() override;
    void timerCallback() override { repaint (timeArea); seek.repaint(); }
};

} // namespace kp
