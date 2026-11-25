/*
    Theme - the house colour scheme (docs/COLOUR.md) and the sizes from
    Apple's Human Interface Guidelines (docs/apple-hig-review.md).

    Three colours carry it: a dark cool-grey ground, a light grey for the
    controls raised off it, and one yellow (#FFF200) for whatever is switched
    on - a chosen button, chosen notes and clips, an armed state, the tune
    being played. Every grey is blue-shifted (R < G < B), and every button
    takes the dark ink, chosen or not. Colours are named by role; nothing
    else in the code writes a colour value. If you change one here, change
    COLOUR.md too.

    Text is at least 7:1 on what it sits on. Red is for marking (the record
    button, clipping) and never for words, which take warnText.

    Themes (View > Themes, decision 0132): the colours below are the first
    theme's; choosing another sets every one of them, the window's look with
    them, and everything is drawn again. Each theme keeps the same roles and
    the same contrast (every word 7:1); its greys lean towards its own hue.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

#include "Platform.h"

namespace kal::ui
{
// The words of the system Kalibre runs on (decision 0129): Option-click or
// Alt+click, the Finder or File Explorer, this Mac or this PC.
inline const platform::Words& words() { return platform::words(); }
inline juce::String optionClick (const char* what = "click") { return juce::String (platform::optionClick (what)); }
inline juce::String commandKey (const char* what) { return juce::String (platform::commandKey (what)); }

// Text beyond ASCII (the ellipsis, dashes, key symbols) goes through here, so
// juce::String reads it as UTF-8.
inline juce::String u8 (const char* utf8) { return juce::String::fromUTF8 (utf8); }
// A number with so many decimals; none is rounded to a whole number. (juce::String's
// own takes 0 to mean every decimal there is: the UAT read "502.246 Hz" and "27.9536%".)
inline juce::String number (double v, int places)
{
    return places <= 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, places);
}
} // namespace kal::ui

namespace kal::theme
{
using juce::Colour;

// The four that carry it.
inline Colour accent  { 0xffFFF200 };
inline Colour control { 0xffA9AFBA };
inline Colour ground  { 0xff23272E };
inline Colour ink     { 0xff14171C };

inline Colour controlHover { 0xffC0C6CF };
inline Colour controlHeld  { 0xff8F96A2 };
inline Colour accentHover  { 0xffFFF42E };
inline Colour accentHeld   { 0xffD1C600 };

// A track armed (decision 0064): for audio, red, as every DAW has it; for
// MIDI, blue. Light enough that the button's dark ink reads at 7:1 (7.6, 8.2).
inline Colour armAudio { 0xffFF8577 };
inline Colour armMidi  { 0xff7DB4E6 };

// The ramp.
inline Colour roll        { 0xff111419 };   // piano roll and arrangement ground
inline Colour sunken      { 0xff1A1D23 };   // frames, lanes, the lower zone's ground
inline Colour lane        { 0xff1E2228 };   // beat lines, alternate rows
inline Colour frameHover  { 0xff22262D };
inline Colour raised      { 0xff2A2F37 };   // menus, tooltips, panels raised off the window
inline Colour rule        { 0xff3A404A };   // separators, bar lines
inline Colour scrollGrab  { 0xff6D7581 };
inline Colour edge        { 0xff8A919C };   // edges of things to see and grab: 4.7:1
inline Colour playhead    { 0xffF2F4F7 };

// Words.
inline Colour text          { 0xffDDE1E7 };   // body text, 11.4:1 on the ground
inline Colour textSecondary { 0xffB0B6C0 };   // 7.4:1 on the ground
inline Colour textOnRaised  { 0xffBFC5CE };   // secondary words on menus: 7.8:1
inline Colour warnText      { 0xffFF9E96 };   // 7.6:1; never the accent's hue
inline Colour focusRing     { 0xffF2F4F7 };   // near white, never the accent

// Marks, never words.
inline Colour warn   { 0xffD2483F };          // record, clipping
inline Colour meterGreen { 0xff7FD29A };      // a meter below -6 dB
inline Colour meterAmber { 0xffF2C46D };      // -6 to 0 dB

// The piano roll's rows: in-scale rows lighter, the root's row lighter still.
inline Colour rowWhite   { 0xff181B21 };
inline Colour rowBlack   { 0xff14171C };
inline Colour rowInScale { 0xff1F2A2E };   // a cool teal wash: ScaleView's highlight, darkened for a ground
inline Colour rowRoot    { 0xff24343A };

//==============================================================================
// Themes (decision 0132): every colour above, set together.
struct Palette
{
    const char* name;
    Colour accent, control, ground, ink, controlHover, controlHeld, accentHover, accentHeld, armAudio, armMidi,
           roll, sunken, lane, frameHover, raised, rule, scrollGrab, edge, playhead,
           text, textSecondary, textOnRaised, warnText, focusRing, warn, meterGreen, meterAmber,
           rowWhite, rowBlack, rowInScale, rowRoot;
    bool light = false;   // dark words on a light ground
};
const std::vector<Palette>& palettes();
int currentPalette();
// Sets every colour, the window's look, and draws everything again. Not kept: the window keeps it.
void usePalette (int index);
// A track or section colour as a fill that `text` reads on at 7:1: `depth` of its brightness in dark
// themes, as far toward the ground in light ones (lower: further from the colour itself).
Colour underWords (Colour, float depth = 0.5f, Colour words = text);
// Laid over what's out of reach (outside a clip): it dims, darker in dark themes, greyer in light ones.
Colour veil();
// A colour faded by `depth` (1: itself): toward black in dark themes, toward the ground in light ones.
Colour fade (Colour, float depth);

// Track colours offered (all R < G < B except the warm ones, which are content).
inline const juce::Array<Colour>& trackColours()
{
    static const juce::Array<Colour> c {
        Colour (0xffA9AFBA), Colour (0xff6FA8DC), Colour (0xff7FC8B6), Colour (0xff9DCB7A),
        Colour (0xffE3C26F), Colour (0xffE89A6B), Colour (0xffE07A86), Colour (0xffB892D9),
    };
    return c;
}

//==============================================================================
// Sizes in points at 100%. View > Text Size scales all of them.
inline constexpr float bodyPoints    = 13.0f;
inline constexpr float smallPoints   = 11.0f;   // dense labels (bar numbers): Apple's Subheadline
inline constexpr int   controlHeight = 28;      // the Mac's default control
inline constexpr int   smallControl  = 20;      // the Mac's smallest: track headers, dense rows
inline constexpr int   controlGap    = 12;
inline constexpr int   margin        = 16;
inline constexpr int   buttonPadding = 12;
inline constexpr float cornerRadius  = 6.0f;
inline constexpr int   focusRingWidth = 2;
inline constexpr int   tooltipColumns = 64;

double luminance (Colour);
double contrast (Colour, Colour);

float scale();                 // the Text Size, 1.0 = 100%
// View > Text Size's choices, in percent, smallest first (50% to 200%).
inline const std::vector<int>& textSizes() { static const std::vector<int> s { 50, 75, 90, 100, 110, 125, 150, 200 }; return s; }
void setScale (float);
inline int px (int points) { return juce::roundToInt ((float) points * scale()); }

juce::Font font (float points = bodyPoints, bool bold = false);

// A control reached with Tab is ringed in focusRing, inside its bounds.
void drawFocusRing (juce::Graphics&, juce::Rectangle<float>, float radius = cornerRadius);

// Tooltips broken into lines of about 64 characters.
juce::String wrapTip (const juce::String&);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void useThemeColours();   // the theme's colours for everything JUCE draws (0132)

    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    int getTextButtonWidthToFitText (juce::TextButton&, int buttonHeight) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawTickBox (juce::Graphics&, juce::Component&, float x, float y, float w, float h, bool ticked, bool enabled,
                      bool highlighted, bool down) override;

    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackgroundWithOptions (juce::Graphics&, int w, int h, const juce::PopupMenu::Options&) override;
    juce::Font getMenuBarFont (juce::MenuBarComponent&, int, const juce::String&) override;
    void drawMenuBarBackground (juce::Graphics&, int w, int h, bool, juce::MenuBarComponent&) override;
    void drawMenuBarItem (juce::Graphics&, int w, int h, int, const juce::String&, bool over, bool open, bool,
                          juce::MenuBarComponent&) override;

    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
    void drawCornerResizer (juce::Graphics&, int w, int h, bool over, bool dragging) override;
    void drawStretchableLayoutResizerBar (juce::Graphics&, int w, int h, bool vertical, bool over, bool dragging) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end,
                           juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;

    juce::Font getLabelFont (juce::Label&) override;
};

//==============================================================================
// A house button: dark ink on light grey, yellow when on, a title for
// VoiceOver, its tip shown when the pointer rests on it. Space presses it
// only when the keyboard brought it the focus (Tab, as with the Mac's
// keyboard navigation): a button that was just given the focus back by the
// window lets Space play (the musician's report: Space pressed Devices).
class HouseButton : public juce::TextButton
{
public:
    HouseButton (const juce::String& label, const juce::String& tip = {}, bool toggles = false);
    void setTip (const juce::String&);
    int idealWidth (int height = controlHeight);
    bool keyPressed (const juce::KeyPress&) override;
    // A right-click calls this instead of pressing the button (a pad's name
    // opens the pad's menu, 0133). Unset, a right-click does nothing.
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void focusGained (FocusChangeType) override;
    void focusLost (FocusChangeType) override;

private:
    bool byKeyboard = false;   // the focus came by Tab
};

} // namespace kal::theme

namespace kal::ui
{
// Small questions (a tempo, a name) are asked inside the main window, over
// it, never in a window of their own (decision 0002). The main component
// registers itself as the host.
void setHost (juce::Component*);
juce::Component* host();
// Questions are shown near the top, just under this (the top bar), across the
// middle; plug-ins' windows step aside meanwhile, in the rack and their own.
void present (juce::AlertWindow*, juce::ModalComponentManager::Callback*, bool deleteWhenDismissed);
void setQuestionsBelow (juce::Component*);
juce::Rectangle<int> questionBounds (juce::Rectangle<int> host, juce::Rectangle<int> above, juce::Rectangle<int> question);
} // namespace kal::ui
