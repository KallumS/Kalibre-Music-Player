#include "Theme.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace kal::theme
{
using namespace juce;

namespace
{
float textScale = 1.0f;
}

float scale() { return textScale; }

//==============================================================================
// Themes (decision 0132).
namespace
{
int paletteNow = 0;

// A colour moved lighter or darker, its hue kept, until it reads at `ratio` on `bg`.
Colour reach (Colour fg, Colour bg, double ratio)
{
    const bool up = luminance (fg) >= luminance (bg);
    float h = fg.getHue(), s = fg.getSaturation(), l = fg.getLightness();
    for (int i = 0; i < 200 && contrast (fg, bg) < ratio; ++i)
    {
        l = jlimit (0.0f, 1.0f, l + (up ? 0.01f : -0.01f));
        fg = Colour::fromHSL (h, s, l, 1.0f);
    }
    return fg;
}

struct Spec
{
    const char* name;
    float hue, saturation;          // the greys' lean, and how strongly
    Colour accent, warnText;
    bool light = false;
    float depth = 1.0f;             // how dark the darkest greys are (lower: darker)
};

Palette build (const Spec& t)
{
    Palette p {};
    p.name = t.name;
    p.light = t.light;
    auto grey = [&t] (float l, float satShare) { return Colour::fromHSL (t.hue, jlimit (0.0f, 1.0f, t.saturation * satShare), jlimit (0.0f, 1.0f, l), 1.0f); };
    const float accentHue = t.accent.getHue();
    if (! t.light)
    {
        const float d = t.depth;
        p.roll = grey (0.075f * d, 1.0f);   p.rowBlack = grey (0.085f * d, 1.0f);   p.rowWhite = grey (0.105f * d, 1.0f);
        p.sunken = grey (0.115f * d, 1.0f); p.lane = grey (0.135f * d, 1.0f);       p.frameHover = grey (0.15f * d, 1.0f);
        p.ground = grey (0.16f * d, 1.0f);  p.raised = grey (0.195f * d, 0.9f);     p.rule = grey (0.26f * d, 0.8f);
        p.scrollGrab = grey (0.47f, 0.5f);  p.edge = grey (0.58f, 0.4f);
        p.controlHeld = grey (0.60f, 0.25f); p.control = grey (0.70f, 0.25f);       p.controlHover = grey (0.78f, 0.25f);
        p.ink = grey (0.09f, 0.5f);
        p.textSecondary = grey (0.73f, 0.2f); p.textOnRaised = grey (0.78f, 0.2f);  p.text = grey (0.88f, 0.2f);
        p.playhead = grey (0.955f, 0.2f);
        p.rowInScale = Colour::fromHSL (accentHue, 0.25f, 0.15f * d + 0.02f, 1.0f);
        p.rowRoot = Colour::fromHSL (accentHue, 0.25f, 0.19f * d + 0.03f, 1.0f);
        p.warn = Colour (0xffD2483F);
        p.meterGreen = Colour (0xff7FD29A);
        p.meterAmber = Colour (0xffF2C46D);
        p.armAudio = Colour (0xffFF8577);
        p.armMidi = Colour (0xff7DB4E6);
    }
    else
    {
        p.roll = grey (0.985f, 1.0f);  p.rowWhite = grey (0.975f, 1.0f); p.rowBlack = grey (0.935f, 1.0f);
        p.sunken = grey (0.945f, 1.0f); p.lane = grey (0.925f, 1.0f);    p.frameHover = grey (0.915f, 1.0f);
        p.ground = grey (0.96f, 1.0f); p.raised = grey (0.995f, 0.5f);   p.rule = grey (0.80f, 0.8f);
        p.scrollGrab = grey (0.55f, 0.5f); p.edge = grey (0.50f, 0.4f);
        p.controlHeld = grey (0.72f, 0.4f); p.control = grey (0.82f, 0.4f); p.controlHover = grey (0.88f, 0.4f);
        p.ink = grey (0.10f, 0.5f);
        p.textSecondary = grey (0.32f, 0.3f); p.textOnRaised = grey (0.32f, 0.3f); p.text = grey (0.12f, 0.3f);
        p.playhead = grey (0.18f, 0.3f);
        p.rowInScale = Colour::fromHSL (accentHue, 0.45f, 0.90f, 1.0f);
        p.rowRoot = Colour::fromHSL (accentHue, 0.45f, 0.85f, 1.0f);
        p.warn = Colour (0xffD2483F);
        p.meterGreen = Colour (0xff2E9E52);
        p.meterAmber = Colour (0xffC98A10);
        p.armAudio = Colour (0xffFF8577);
        p.armMidi = Colour (0xff7DB4E6);
    }
    p.accent = t.accent;
    p.accentHover = t.accent.interpolatedWith (Colours::white, 0.15f);
    p.accentHeld = t.accent.darker (0.25f);
    p.warnText = t.warnText;
    // Every word 7:1 on what it sits on; things to see and grab 3:1 (0023's rule, every theme).
    for (auto bg : { p.ground, p.sunken, p.roll, p.raised, p.lane }) p.text = reach (p.text, bg, 7.2);
    for (auto bg : { p.ground, p.sunken, p.lane }) p.textSecondary = reach (p.textSecondary, bg, 7.1);
    p.textOnRaised = reach (p.textOnRaised, p.raised, 7.1);
    p.warnText = reach (p.warnText, p.ground, 7.1);
    for (auto bg : { p.control, p.accent, p.accentHover, p.armAudio, p.armMidi }) p.ink = reach (p.ink, bg, 7.1);
    // Still short (a dark accent): the fill moves, not the ink.
    for (auto* fill : { &p.control, &p.accent, &p.accentHover, &p.armAudio, &p.armMidi }) *fill = reach (*fill, p.ink, 7.1);
    p.controlHeld = reach (p.controlHeld, p.ink, 4.6);
    p.edge = reach (p.edge, p.ground, 3.2);
    p.scrollGrab = reach (p.scrollGrab, p.sunken, 3.2);
    p.focusRing = p.playhead;
    return p;
}

// The first, as Kalibre has always looked: these very values.
Palette house()
{
    return { "Midnight Yellow", accent, control, ground, ink, controlHover, controlHeld, accentHover, accentHeld, armAudio, armMidi,
             roll, sunken, lane, frameHover, raised, rule, scrollGrab, edge, playhead,
             text, textSecondary, textOnRaised, warnText, focusRing, warn, meterGreen, meterAmber,
             rowWhite, rowBlack, rowInScale, rowRoot, false };
}
} // namespace

const std::vector<Palette>& palettes()
{
    static const std::vector<Palette> all = []
    {
        std::vector<Palette> v { house() };
        const Spec specs[] {
            { "Violet Lime",   0.77f, 0.32f, Colour (0xff9BFF5A), Colour (0xffFFB08F) },
            { "Crimson Night", 0.99f, 0.40f, Colour (0xffFF7E6E), Colour (0xffFFD27A) },
            { "Obsidian",      0.70f, 0.08f, Colour (0xffC9B6FF), Colour (0xffFF9E96), false, 0.45f },
            { "Porcelain",     0.60f, 0.10f, Colour (0xff7CC6FF), Colour (0xffA3261C), true },
            { "Cobalt Flare",  0.62f, 0.42f, Colour (0xffFF8A70), Colour (0xffFFD86E) },
            { "Emerald Glow",  0.38f, 0.32f, Colour (0xff6CFF8E), Colour (0xffFFA796) },
            { "Ember Gold",    0.03f, 0.34f, Colour (0xffFFCB45), Colour (0xffFF9E96) },
            { "Frostbite",     0.55f, 0.34f, Colour (0xff8FF3FF), Colour (0xffFFA99E) },
            { "Copper Tide",   0.47f, 0.38f, Colour (0xffFFA04D), Colour (0xffFFA9C9) },
            { "Plum Gold",     0.86f, 0.24f, Colour (0xffFFD166), Colour (0xffFFA99E) },
        };
        for (auto& s : specs) v.push_back (build (s));
        return v;
    }();
    return all;
}

int currentPalette() { return paletteNow; }

Colour fade (Colour c, float depth)
{
    return palettes()[(size_t) paletteNow].light ? c.interpolatedWith (ground, 0.9f * (1.0f - depth)) : c.withMultipliedBrightness (depth);
}

Colour veil()
{
    return palettes()[(size_t) paletteNow].light ? textSecondary.withAlpha (0.22f) : ink.withAlpha (0.45f);
}

Colour underWords (Colour c, float depth, Colour words)
{
    return reach (fade (c, depth), words, 7.1);
}

void usePalette (int index)
{
    const auto& all = palettes();
    index = jlimit (0, (int) all.size() - 1, index);
    paletteNow = index;
    const auto& p = all[(size_t) index];
    accent = p.accent; control = p.control; ground = p.ground; ink = p.ink;
    controlHover = p.controlHover; controlHeld = p.controlHeld; accentHover = p.accentHover; accentHeld = p.accentHeld;
    armAudio = p.armAudio; armMidi = p.armMidi;
    roll = p.roll; sunken = p.sunken; lane = p.lane; frameHover = p.frameHover; raised = p.raised; rule = p.rule;
    scrollGrab = p.scrollGrab; edge = p.edge; playhead = p.playhead;
    text = p.text; textSecondary = p.textSecondary; textOnRaised = p.textOnRaised; warnText = p.warnText; focusRing = p.focusRing;
    warn = p.warn; meterGreen = p.meterGreen; meterAmber = p.meterAmber;
    rowWhite = p.rowWhite; rowBlack = p.rowBlack; rowInScale = p.rowInScale; rowRoot = p.rowRoot;
    // What JUCE draws, and every window, drawn again.
    if (auto* look = dynamic_cast<LookAndFeel*> (&juce::LookAndFeel::getDefaultLookAndFeel())) look->useThemeColours();
    auto& desktop = Desktop::getInstance();
    for (int i = 0; i < desktop.getNumComponents(); ++i)
        if (auto* comp = desktop.getComponent (i))
        {
            if (auto* w = dynamic_cast<ResizableWindow*> (comp)) w->setBackgroundColour (ground);
            comp->sendLookAndFeelChange();
            comp->repaint();
        }
}
void setScale (float s) { textScale = jlimit (textSizes().front() / 100.0f, textSizes().back() / 100.0f, s); }

double luminance (Colour c)
{
    auto lin = [] (uint8 v)
    {
        const auto s = v / 255.0;
        return s <= 0.04045 ? s / 12.92 : std::pow ((s + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * lin (c.getRed()) + 0.7152 * lin (c.getGreen()) + 0.0722 * lin (c.getBlue());
}

double contrast (Colour a, Colour b)
{
    auto la = luminance (a), lb = luminance (b);
    if (la < lb) std::swap (la, lb);
    return (la + 0.05) / (lb + 0.05);
}

Font font (float points, bool bold)
{
    auto f = Font (FontOptions().withPointHeight (points * textScale));
    return bold ? f.boldened() : f;
}

void drawFocusRing (Graphics& g, Rectangle<float> area, float radius)
{
    g.setColour (focusRing);
    const auto w = (float) focusRingWidth;
    g.drawRoundedRectangle (area.reduced (w * 0.5f), radius, w);
}

String wrapTip (const String& text)
{
    StringArray lines;
    for (auto& paragraph : StringArray::fromLines (text))
    {
        String line;
        for (auto& word : StringArray::fromTokens (paragraph, " ", ""))
        {
            if (line.isNotEmpty() && line.length() + 1 + word.length() > tooltipColumns)
            {
                lines.add (line);
                line.clear();
            }
            line << (line.isEmpty() ? "" : " ") << word;
        }
        lines.add (line);
    }
    return lines.joinIntoString ("\n");
}

//==============================================================================
LookAndFeel::LookAndFeel() { useThemeColours(); }

void LookAndFeel::useThemeColours()
{
    auto scheme = palettes()[(size_t) currentPalette()].light ? getLightColourScheme() : getDarkColourScheme();
    scheme.setUIColour (ColourScheme::windowBackground, ground);
    scheme.setUIColour (ColourScheme::widgetBackground, sunken);
    scheme.setUIColour (ColourScheme::menuBackground, raised);
    scheme.setUIColour (ColourScheme::outline, rule);
    scheme.setUIColour (ColourScheme::defaultText, text);
    scheme.setUIColour (ColourScheme::defaultFill, control);
    scheme.setUIColour (ColourScheme::highlightedText, text);
    scheme.setUIColour (ColourScheme::highlightedFill, rule);
    scheme.setUIColour (ColourScheme::menuText, text);
    setColourScheme (scheme);

    setColour (ResizableWindow::backgroundColourId, ground);
    setColour (DocumentWindow::textColourId, text);
    setColour (TextButton::buttonColourId, control);
    setColour (TextButton::buttonOnColourId, accent);
    setColour (TextButton::textColourOffId, ink);
    setColour (TextButton::textColourOnId, ink);
    setColour (ToggleButton::textColourId, text);
    setColour (ToggleButton::tickColourId, ink);
    setColour (ToggleButton::tickDisabledColourId, edge);
    setColour (Label::textColourId, text);
    setColour (PopupMenu::backgroundColourId, raised);
    setColour (PopupMenu::textColourId, text);
    setColour (PopupMenu::headerTextColourId, textOnRaised);
    setColour (PopupMenu::highlightedBackgroundColourId, rule);
    setColour (PopupMenu::highlightedTextColourId, text);
    setColour (TooltipWindow::backgroundColourId, raised);
    setColour (TooltipWindow::textColourId, text);
    setColour (TooltipWindow::outlineColourId, rule);
    setColour (ScrollBar::thumbColourId, scrollGrab);
    setColour (ScrollBar::trackColourId, sunken);
    setColour (AlertWindow::backgroundColourId, raised);
    setColour (AlertWindow::textColourId, text);
    setColour (AlertWindow::outlineColourId, rule);
    setColour (ComboBox::backgroundColourId, control);
    setColour (ComboBox::textColourId, ink);
    setColour (ComboBox::arrowColourId, ink);
    setColour (ComboBox::outlineColourId, edge);
    setColour (TextEditor::backgroundColourId, sunken);
    setColour (TextEditor::textColourId, text);
    setColour (TextEditor::outlineColourId, edge);
    setColour (TextEditor::focusedOutlineColourId, focusRing);
    setColour (TextEditor::highlightColourId, rule);
    setColour (CaretComponent::caretColourId, text);
    setColour (Slider::thumbColourId, control);
    setColour (Slider::trackColourId, rule);
    setColour (Slider::backgroundColourId, sunken);
    setColour (Slider::textBoxTextColourId, text);
    setColour (Slider::textBoxBackgroundColourId, sunken);
    setColour (Slider::textBoxOutlineColourId, rule);
    setColour (Slider::rotarySliderFillColourId, control);
    setColour (Slider::rotarySliderOutlineColourId, rule);
    setColour (ListBox::backgroundColourId, sunken);
    setColour (ListBox::outlineColourId, rule);
    setColour (ListBox::textColourId, text);
    setColour (TreeView::backgroundColourId, sunken);
    setColour (TreeView::selectedItemBackgroundColourId, rule);
    setColour (TableHeaderComponent::backgroundColourId, raised);
    setColour (TableHeaderComponent::textColourId, text);
    setColour (TableHeaderComponent::outlineColourId, rule);
    setColour (TabbedButtonBar::tabOutlineColourId, rule);
    setColour (TabbedButtonBar::frontOutlineColourId, rule);
    setColour (FileTreeComponent::highlightColourId, rule);
    setColour (DirectoryContentsDisplayComponent::highlightColourId, rule);
    setColour (DirectoryContentsDisplayComponent::textColourId, text);
    setColour (ProgressBar::backgroundColourId, sunken);
    setColour (ProgressBar::foregroundColourId, control);
}

Font LookAndFeel::getTextButtonFont (TextButton&, int) { return font(); }
Font LookAndFeel::getLabelFont (Label& l) { return l.getFont().withHeight (l.getFont().getHeight()); }

int LookAndFeel::getTextButtonWidthToFitText (TextButton& b, int buttonHeight)
{
    const auto w = GlyphArrangement::getStringWidthInt (font(), b.getButtonText());
    return jmax (buttonHeight, w + 2 * px (buttonPadding));
}

void LookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour& base, bool highlighted, bool down)
{
    const auto on = b.getToggleState();
    // A button may have an "on" colour of its own (an armed track's, decision 0064).
    const auto own = b.getProperties()["onColour"];
    const bool ownOn = on && ! own.isVoid();
    const auto onFill = ownOn ? Colour ((uint32) (int64) own) : accent;
    auto fill = on ? onFill : (base == findColour (TextButton::buttonColourId) ? control : base);
    if (! b.isEnabled()) fill = rule;
    else if (down) fill = on ? (ownOn ? onFill.darker (0.2f) : accentHeld) : controlHeld;
    else if (highlighted) fill = on ? (ownOn ? onFill.brighter (0.08f) : accentHover) : (fill == control ? controlHover : fill.brighter (0.1f));

    const auto area = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto radius = jmin (cornerRadius, area.getHeight() * 0.3f);
    g.setColour (fill);
    g.fillRoundedRectangle (area, radius);
    // A second "on" colour fills the right half (armed for audio and MIDI, decision 0065).
    if (const auto second = b.getProperties()["onColour2"]; ownOn && ! second.isVoid() && b.isEnabled())
    {
        Colour two ((uint32) (int64) second);
        if (down) two = two.darker (0.2f); else if (highlighted) two = two.brighter (0.08f);
        g.saveState();
        g.reduceClipRegion (area.withLeft (area.getCentreX()).toNearestInt());
        g.setColour (two);
        g.fillRoundedRectangle (area, radius);
        g.restoreState();
    }
    if (b.hasKeyboardFocus (false)) drawFocusRing (g, area, radius);
}

void LookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const auto f = font (b.getHeight() < px (24) ? smallPoints : bodyPoints);
    g.setFont (f);
    g.setColour (b.isEnabled() ? ink : edge);
    const auto area = b.getLocalBounds();
    g.drawText (b.getButtonText(), area.reduced (2, 0), Justification::centred, true);
    // No bar under a chosen button's label: the user didn't want it (16 November).
}

void LookAndFeel::drawTickBox (Graphics& g, Component& c, float x, float y, float w, float h, bool ticked, bool enabled,
                               bool highlighted, bool)
{
    Rectangle<float> box (x, y, w, h);
    g.setColour (ticked ? accent : (highlighted ? frameHover : sunken));
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (ticked ? accentHeld : edge);
    g.drawRoundedRectangle (box.reduced (0.5f), 3.0f, 1.0f);
    if (ticked)
    {
        Path p;
        p.startNewSubPath (box.getX() + w * 0.22f, box.getCentreY());
        p.lineTo (box.getX() + w * 0.43f, box.getBottom() - h * 0.25f);
        p.lineTo (box.getRight() - w * 0.2f, box.getY() + h * 0.25f);
        g.setColour (enabled ? ink : edge);
        g.strokePath (p, PathStrokeType (2.0f));
    }
    if (c.hasKeyboardFocus (false)) drawFocusRing (g, box.expanded (3.0f), 4.0f);
}

void LookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool down)
{
    const auto boxSize = (float) jmin (px (16), b.getHeight() - 4);
    drawTickBox (g, b, 2.0f, ((float) b.getHeight() - boxSize) * 0.5f, boxSize, boxSize, b.getToggleState(), b.isEnabled(),
                 highlighted, down);
    g.setColour (b.isEnabled() ? text : textSecondary);
    g.setFont (font());
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (roundToInt (boxSize) + 8), Justification::centredLeft, 1);
}

Font LookAndFeel::getPopupMenuFont() { return font(); }

void LookAndFeel::drawPopupMenuBackgroundWithOptions (Graphics& g, int w, int h, const PopupMenu::Options&)
{
    g.fillAll (raised);
    g.setColour (rule);
    g.drawRect (0, 0, w, h, 1);
}

Font LookAndFeel::getMenuBarFont (MenuBarComponent&, int, const String&) { return font(); }

void LookAndFeel::drawMenuBarBackground (Graphics& g, int w, int h, bool, MenuBarComponent&)
{
    g.fillAll (ground);
    g.setColour (rule);
    g.fillRect (0, h - 1, w, 1);
}

void LookAndFeel::drawMenuBarItem (Graphics& g, int w, int h, int, const String& t, bool over, bool open, bool,
                                   MenuBarComponent& bar)
{
    if (open || over)
    {
        g.setColour (raised);
        g.fillRect (0, 0, w, h - 1);
    }
    g.setColour (text);
    g.setFont (getMenuBarFont (bar, 0, t));
    g.drawFittedText (t, 0, 0, w, h, Justification::centred, 1);
}

static TextLayout layoutTip (const String& t)
{
    AttributedString s;
    s.setJustification (Justification::topLeft);
    s.append (t, font(), text);
    TextLayout layout;
    layout.createLayoutWithBalancedLineLengths (s, 10000.0f);
    return layout;
}

Rectangle<int> LookAndFeel::getTooltipBounds (const String& tip, Point<int> pos, Rectangle<int> parent)
{
    const auto layout = layoutTip (tip);
    const auto w = (int) std::ceil (layout.getWidth()) + 20;
    const auto h = (int) std::ceil (layout.getHeight()) + 12;
    return Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 24,
                           pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h).constrainedWithin (parent);
}

void LookAndFeel::drawTooltip (Graphics& g, const String& t, int w, int h)
{
    const auto bounds = Rectangle<int> (w, h).toFloat();
    g.setColour (raised);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (rule);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
    layoutTip (t).draw (g, bounds.reduced (10.0f, 6.0f));
}

void LookAndFeel::drawScrollbar (Graphics& g, ScrollBar&, int x, int y, int w, int h, bool vertical,
                                 int thumbStart, int thumbSize, bool over, bool down)
{
    g.setColour (sunken);
    g.fillRect (x, y, w, h);
    const auto thumb = vertical ? Rectangle<int> (x, thumbStart, w, thumbSize) : Rectangle<int> (thumbStart, y, thumbSize, h);
    g.setColour (down ? control : (over ? edge : scrollGrab));
    g.fillRoundedRectangle (thumb.toFloat().reduced (2.0f), 3.0f);
}

void LookAndFeel::drawCornerResizer (Graphics& g, int w, int h, bool over, bool dragging)
{
    g.setColour (dragging || over ? control : edge);
    for (float i = 0.3f; i < 1.0f; i += 0.3f)
        g.drawLine ((float) w * i, (float) h, (float) w, (float) h * i, 1.5f);
}

void LookAndFeel::drawStretchableLayoutResizerBar (Graphics& g, int w, int h, bool vertical, bool over, bool dragging)
{
    g.fillAll (ground);
    g.setColour (dragging ? control : (over ? edge : rule));
    if (vertical) g.fillRect (w / 2 - 1, 0, 2, h);
    else g.fillRect (0, h / 2 - 1, w, 2);
    // A grip in the middle, so it reads as something to drag.
    g.setColour (dragging || over ? control : edge);
    const auto c = Point<float> ((float) w * 0.5f, (float) h * 0.5f);
    for (int i = -1; i <= 1; ++i)
        g.fillEllipse (Rectangle<float> (3.0f, 3.0f).withCentre (vertical ? c.translated (0, (float) i * 6) : c.translated ((float) i * 6, 0)));
}

void LookAndFeel::drawComboBox (Graphics& g, int w, int h, bool down, int, int, int, int, ComboBox& box)
{
    const auto area = Rectangle<int> (w, h).toFloat().reduced (1.0f);
    const auto radius = jmin (cornerRadius, area.getHeight() * 0.3f);
    g.setColour (down ? controlHeld : (box.isMouseOver (true) ? controlHover : control));
    g.fillRoundedRectangle (area, radius);
    Path arrow;
    const auto ax = area.getRight() - 12.0f, ay = area.getCentreY();
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (ink);
    g.fillPath (arrow);
    if (box.hasKeyboardFocus (true)) drawFocusRing (g, area, radius);
}

Font LookAndFeel::getComboBoxFont (ComboBox& b) { return font (b.getHeight() < px (24) ? smallPoints : bodyPoints); }

void LookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (4, 0, box.getWidth() - 22, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setColour (Label::textColourId, ink);
}

void LookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                    Slider::SliderStyle style, Slider& s)
{
    const bool vertical = style == Slider::LinearVertical;
    const auto r = Rectangle<int> (x, y, w, h).toFloat();
    const float track = 4.0f;
    auto rail = vertical ? Rectangle<float> (track, r.getHeight()).withCentre (r.getCentre())
                         : Rectangle<float> (r.getWidth(), track).withCentre (r.getCentre());
    g.setColour (sunken);
    g.fillRoundedRectangle (rail, 2.0f);
    g.setColour (edge);
    g.drawRoundedRectangle (rail, 2.0f, 1.0f);
    const float thumbLong = 10.0f, thumbShort = vertical ? jmin (r.getWidth(), 22.0f) : jmin (r.getHeight(), 18.0f);
    auto thumb = vertical ? Rectangle<float> (thumbShort, thumbLong).withCentre ({ r.getCentreX(), pos })
                          : Rectangle<float> (thumbLong, thumbShort).withCentre ({ pos, r.getCentreY() });
    const bool held = s.isMouseButtonDown();
    g.setColour (held ? accent : (s.isMouseOver() ? controlHover : control));
    g.fillRoundedRectangle (thumb, 3.0f);
    g.setColour (ink);
    if (vertical) g.fillRect (thumb.withSizeKeepingCentre (thumb.getWidth() - 6.0f, 1.5f));
    else g.fillRect (thumb.withSizeKeepingCentre (1.5f, thumb.getHeight() - 6.0f));
    if (s.hasKeyboardFocus (false)) drawFocusRing (g, thumb.expanded (3.0f), 4.0f);
}

void LookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto r = Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const auto size = jmin (r.getWidth(), r.getHeight());
    const auto c = r.getCentre();
    const auto radius = size * 0.5f;
    const auto angle = start + pos * (end - start);
    Path back;
    back.addCentredArc (c.x, c.y, radius - 2.0f, radius - 2.0f, 0.0f, start, end, true);
    g.setColour (rule);
    g.strokePath (back, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    // A pan knob fills from the centre.
    const bool centred = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = centred ? (start + end) * 0.5f : start;
    Path value;
    value.addCentredArc (c.x, c.y, radius - 2.0f, radius - 2.0f, 0.0f, jmin (from, angle), jmax (from, angle), true);
    g.setColour (s.isMouseButtonDown() ? accent : control);
    g.strokePath (value, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (text);
    g.drawLine (c.x, c.y, c.x + (radius - 5.0f) * std::sin (angle), c.y - (radius - 5.0f) * std::cos (angle), 2.0f);
    if (s.hasKeyboardFocus (false)) drawFocusRing (g, r.withSizeKeepingCentre (size + 4, size + 4), size * 0.5f);
}

Label* LookAndFeel::createSliderTextBox (Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (font (smallPoints));
    l->setColour (Label::textColourId, text);
    l->setColour (Label::backgroundColourId, Colours::transparentBlack);
    l->setColour (Label::outlineColourId, Colours::transparentBlack);
    return l;
}

void LookAndFeel::fillTextEditorBackground (Graphics& g, int w, int h, TextEditor&)
{
    g.setColour (sunken);
    g.fillRoundedRectangle (Rectangle<int> (w, h).toFloat(), 4.0f);
}

void LookAndFeel::drawTextEditorOutline (Graphics& g, int w, int h, TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? focusRing : edge);
    g.drawRoundedRectangle (Rectangle<int> (w, h).toFloat().reduced (0.5f), 4.0f, e.hasKeyboardFocus (true) ? 2.0f : 1.0f);
}

//==============================================================================
HouseButton::HouseButton (const String& label, const String& tip, bool toggles) : TextButton (label)
{
    setMouseClickGrabsKeyboardFocus (false);
    setWantsKeyboardFocus (true);
    setClickingTogglesState (toggles);
    setTip (tip.isEmpty() ? label : tip);
}

void HouseButton::setTip (const String& tip)
{
    setTooltip (wrapTip (tip));
    setDescription (tip);
    setTitle (getButtonText().length() <= 2 ? tip.upToFirstOccurrenceOf (".", false, false) : getButtonText());
}

int HouseButton::idealWidth (int height)
{
    return getLookAndFeel().getTextButtonWidthToFitText (*this, px (height));
}

void HouseButton::focusGained (FocusChangeType how)
{
    byKeyboard = how == focusChangedByTabKey;
    TextButton::focusGained (how);
}

void HouseButton::focusLost (FocusChangeType how)
{
    byKeyboard = false;
    TextButton::focusLost (how);
}

void HouseButton::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && onRightClick) { onRightClick(); return; }
    juce::TextButton::mouseDown (e);
}

void HouseButton::mouseUp (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && onRightClick) return;
    juce::TextButton::mouseUp (e);
}

bool HouseButton::keyPressed (const KeyPress& key)
{
    if (key == KeyPress::spaceKey || key == KeyPress::returnKey)
    {
        if (byKeyboard && isEnabled()) { triggerClick(); return true; }
        return false;   // the window's: Play, Go to the Start
    }
    if (key == KeyPress::escapeKey && hasKeyboardFocus (false)) { giveAwayKeyboardFocus(); return true; }
    return TextButton::keyPressed (key);
}

} // namespace kal::theme

namespace kal::ui
{
static juce::Component::SafePointer<juce::Component> hostComponent, aboveQuestions;

void setHost (juce::Component* c) { hostComponent = c; }
juce::Component* host() { return hostComponent.getComponent(); }
void setQuestionsBelow (juce::Component* c) { aboveQuestions = c; }

juce::Rectangle<int> questionBounds (juce::Rectangle<int> host, juce::Rectangle<int> above, juce::Rectangle<int> question)
{
    // Near the top, across the middle (the musician's choice: a plug-in's window
    // lower down covered the Save question): just under the top bar.
    const int top = above.isEmpty() ? host.getY() + 24 : above.getBottom() + 12;
    auto r = question.withCentre ({ host.getCentreX(), 0 }).withY (top);
    if (r.getBottom() > host.getBottom()) r.setY (juce::jmax (host.getY(), host.getBottom() - r.getHeight()));
    return r;
}

namespace
{
// Plug-ins' windows step aside while a question is asked: on the Mac each is a
// native view, drawn over anything Kalibre draws, so it could cover the
// question ("Save the changes?" when quitting) with no way to bring it forward.
struct PluginsAside : public juce::ModalComponentManager::Callback
{
    std::unique_ptr<juce::ModalComponentManager::Callback> inner;
    std::vector<juce::Component::SafePointer<juce::Component>> hidden;

    void hideIn (juce::Component& c)
    {
        for (auto* child : c.getChildren())
        {
            if (! child->isVisible()) continue;
            // A plug-in's window, or what shows one in the rack (decision 0058:
            // on the Mac that holds it in a native view of its own, outside this tree).
            if (dynamic_cast<juce::AudioProcessorEditor*> (child) != nullptr)
            {
                child->setVisible (false);
                hidden.emplace_back (child);
            }
            else hideIn (*child);
        }
    }

    // Plug-ins moved into windows of their own (0056) step aside too: they're in
    // front of Kalibre's window, wherever on the screen the question is.
    void hideOwnWindows()
    {
        auto& desktop = juce::Desktop::getInstance();
        for (int i = 0; i < desktop.getNumComponents(); ++i)
            if (auto* w = dynamic_cast<juce::DocumentWindow*> (desktop.getComponent (i)))
                if (w->isVisible() && dynamic_cast<juce::AudioProcessorEditor*> (w->getContentComponent()) != nullptr)
                {
                    w->setVisible (false);
                    hidden.emplace_back (w);
                }
    }

    void modalStateFinished (int result) override
    {
        for (auto& h : hidden) if (h) h->setVisible (true);
        if (inner) inner->modalStateFinished (result);
    }
};
} // namespace

void present (juce::AlertWindow* w, juce::ModalComponentManager::Callback* cb, bool deleteWhenDismissed)
{
    auto aside = std::make_unique<PluginsAside>();
    aside->inner.reset (cb);
    if (auto* h = host())
    {
        aside->hideIn (*h);
        aside->hideOwnWindows();
        h->addAndMakeVisible (w);
        w->setAlwaysOnTop (true);
        auto* a = aboveQuestions.getComponent();
        const auto above = a != nullptr && a->isVisible() && h->isParentOf (a) ? h->getLocalArea (a, a->getLocalBounds()) : juce::Rectangle<int>();
        w->setBounds (questionBounds (h->getLocalBounds(), above, w->getBounds()));
    }
    w->enterModalState (true, aside.release(), deleteWhenDismissed);
    // A field to type in is ready, its text chosen, so typing replaces it:
    // otherwise the keys went to the window's shortcuts (the UAT typed a
    // section's name, and R started recording, decision 0133).
    for (auto* child : w->getChildren())
        if (auto* field = dynamic_cast<juce::TextEditor*> (child); field != nullptr && field->isVisible())
        {
            field->grabKeyboardFocus();
            field->selectAll();
            break;
        }
}
} // namespace kal::ui
