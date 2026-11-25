/*
    Panels - the parts the window is made of (decision 0011): each has a
    name the layout uses and a title shown when editing the layout.

      playlist        the playlists, as tabs, and the chosen one's tracks
      library         search and browse the library by artist, album, year, genre
      album art       the cover of what's playing (or chosen)
      now playing     what's playing, in words (a title-formatting pattern)
      transport       play, pause, the seek bar, the volume and the order
      equalizer       the ten-band equaliser
      oscilloscope, goniometer, spectrum, peak meter, loudness
      add-on console  what add-ons print
*/

#pragma once

#include "Controller.h"
#include "Theme.h"

namespace kp
{

struct Context
{
    Controller& c;
    juce::ApplicationCommandManager& commands;
};

struct PanelType
{
    const char* name;
    const char* title;
};
const std::vector<PanelType>& panelTypes();
juce::String panelTitle (const std::string& name);
std::unique_ptr<juce::Component> makePanel (const std::string& name, Context&);

// A panel's ground and a heading, drawn the same everywhere.
void paintPanel (juce::Graphics&, juce::Component&, const juce::String& title = {});
inline constexpr int headingHeight = 22;

//==============================================================================
class AlbumArtView : public juce::Component, private Controller::Listener
{
public:
    explicit AlbumArtView (Context&);
    ~AlbumArtView() override;
    void paint (juce::Graphics&) override;

private:
    Context& ctx;
    juce::Image image;
    std::string shownFor;
    void playbackChanged() override { update(); }
    void selectionChanged() override { update(); }
    void update();
};

class NowPlayingView : public juce::Component, private Controller::Listener, private juce::Timer
{
public:
    explicit NowPlayingView (Context&);
    ~NowPlayingView() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    Context& ctx;
    void playbackChanged() override { repaint(); }
    void timerCallback() override { repaint(); }
};

class AddonConsoleView : public juce::Component, private Controller::Listener
{
public:
    explicit AddonConsoleView (Context&);
    ~AddonConsoleView() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Context& ctx;
    juce::TextEditor text;
    void messageChanged() override;
};

} // namespace kp
