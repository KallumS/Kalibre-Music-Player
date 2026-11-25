#include "EqualizerView.h"

namespace kp
{
using namespace juce;

EqualizerView::EqualizerView (Context& x) : ctx (x)
{
    auto& eq = ctx.c.player.eq;
    on.setToggleState (eq.enabled.load(), dontSendNotification);
    on.onClick = [this] { ctx.c.player.eq.enabled = on.getToggleState(); };
    addAndMakeVisible (on);
    presets.setTextWhenNothingSelected ("Presets");
    int id = 1;
    for (auto& p : Equalizer::presets()) presets.addItem (p.name, id++);
    presets.onChange = [this]
    {
        const int i = presets.getSelectedId() - 1;
        if (i < 0) return;
        auto& p = Equalizer::presets()[(size_t) i];
        for (int b = 0; b < Equalizer::bands; ++b) bands[(size_t) b].setValue (p.gains[(size_t) b], dontSendNotification);
        if (! on.getToggleState()) { on.setToggleState (true, dontSendNotification); ctx.c.player.eq.enabled = true; }
        changed();
        presets.setSelectedId (0, dontSendNotification);
    };
    addAndMakeVisible (presets);
    auto setup = [] (Slider& s, double v)
    {
        s.setSliderStyle (Slider::LinearVertical);
        s.setTextBoxStyle (Slider::TextBoxBelow, false, 44, 18);
        s.setRange (-12.0, 12.0, 0.5);
        s.setValue (v, dontSendNotification);
        s.setDoubleClickReturnValue (true, 0.0);
        s.setTextValueSuffix (" dB");
    };
    setup (preamp, eq.preampDb.load());
    preamp.onValueChange = [this] { changed(); };
    addAndMakeVisible (preamp);
    for (int b = 0; b < Equalizer::bands; ++b)
    {
        setup (bands[(size_t) b], eq.gainDb[(size_t) b].load());
        bands[(size_t) b].onValueChange = [this] { changed(); };
        addAndMakeVisible (bands[(size_t) b]);
    }
}

void EqualizerView::changed()
{
    auto& eq = ctx.c.player.eq;
    eq.preampDb = (float) preamp.getValue();
    for (int b = 0; b < Equalizer::bands; ++b) eq.gainDb[(size_t) b] = (float) bands[(size_t) b].getValue();
    eq.changed();
}

void EqualizerView::paint (Graphics& g)
{
    paintPanel (g, *this, "Equaliser");
    g.setColour (theme::textSecondary);
    g.setFont (theme::font (theme::smallPoints));
    auto labels = getLocalBounds().reduced (8).removeFromBottom (16);
    const int w = labels.getWidth() / (Equalizer::bands + 1);
    g.drawText ("Pre", labels.removeFromLeft (w), Justification::centred);
    for (auto f : Equalizer::frequencies)
        g.drawText (f >= 1000 ? String ((int) (f / 1000)) + "k" : String ((int) f), labels.removeFromLeft (w), Justification::centred);
}

void EqualizerView::resized()
{
    auto r = getLocalBounds().reduced (8).withTrimmedTop (headingHeight - 4);
    auto top = r.removeFromTop (theme::px (28));
    on.setBounds (top.removeFromLeft (theme::px (60)));
    presets.setBounds (top.removeFromLeft (theme::px (160)).reduced (0, 2));
    r.removeFromBottom (16);
    const int w = r.getWidth() / (Equalizer::bands + 1);
    preamp.setBounds (r.removeFromLeft (w));
    for (auto& s : bands) s.setBounds (r.removeFromLeft (w));
}

} // namespace kp
