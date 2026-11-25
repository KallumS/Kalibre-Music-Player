#include "Visualisers.h"
#include "Panels.h"

namespace kp
{
using namespace juce;

namespace
{
float toDb (float v) { return v > 1e-6f ? 20.0f * std::log10 (v) : -120.0f; }
}

void Visualiser::paint (Graphics& g)
{
    paintPanel (g, *this, title);
    auto area = getLocalBounds().toFloat().reduced (8).withTrimmedTop ((float) headingHeight - 6);
    g.setColour (theme::roll);
    g.fillRoundedRectangle (area, 4);
    draw (g, area.reduced (4));
}

void Visualiser::mouseDown (const MouseEvent& e)
{
    if (! e.mods.isPopupMenu()) return;
    PopupMenu m;
    addToMenu (m);
    if (m.getNumItems() > 0) m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMousePosition());
}

//==============================================================================
void Oscilloscope::update()
{
    const int n = jlimit (64, ScopeTap::size / 2, (int) (player.sampleRate() * span) * 2);
    l.resize ((size_t) n);
    r.resize ((size_t) n);
    player.scope.read (l.data(), r.data(), n);
}

void Oscilloscope::draw (Graphics& g, Rectangle<float> a)
{
    g.setColour (theme::rule);
    g.drawHorizontalLine ((int) a.getCentreY(), a.getX(), a.getRight());
    if (l.empty()) return;
    const int show = (int) l.size() / 2;
    // Steadied: start where the left channel rises through zero, in the older half.
    int start = 0;
    for (int i = 1; i < show; ++i) if (l[(size_t) i - 1] <= 0 && l[(size_t) i] > 0) { start = i; break; }
    auto trace = [&] (const std::vector<float>& s, Colour c, float offset)
    {
        Path p;
        const float h = a.getHeight() * 0.25f;
        for (int x = 0; x < (int) a.getWidth(); ++x)
        {
            const int i = start + (int) ((double) x / a.getWidth() * show);
            const float y = a.getY() + a.getHeight() * offset - jlimit (-1.0f, 1.0f, s[(size_t) i]) * h;
            if (x == 0) p.startNewSubPath (a.getX(), y);
            else p.lineTo (a.getX() + (float) x, y);
        }
        g.setColour (c);
        g.strokePath (p, PathStrokeType (1.4f));
    };
    trace (l, theme::meterGreen, 0.27f);
    trace (r, theme::armMidi, 0.73f);
    g.setColour (theme::textSecondary);
    g.setFont (theme::font (theme::smallPoints));
    g.drawText ("L", a.withHeight (16), Justification::topLeft);
    g.drawText ("R", a.withTrimmedTop (a.getHeight() / 2).withHeight (16), Justification::topLeft);
    g.drawText (String ((int) (span * 1000)) + " ms", a.withHeight (16), Justification::topRight);
}

void Oscilloscope::addToMenu (PopupMenu& m)
{
    for (float s : { 0.01f, 0.02f, 0.05f, 0.1f, 0.25f })
        m.addItem (String ((int) (s * 1000)) + " ms Across", true, std::abs (span - s) < 1e-4f, [this, s] { span = s; });
}

//==============================================================================
void Goniometer::update()
{
    const int n = 2048;
    l.resize ((size_t) n);
    r.resize ((size_t) n);
    player.scope.read (l.data(), r.data(), n);
    double lr = 0, ll = 0, rr = 0;
    for (int i = 0; i < n; ++i) { lr += l[(size_t) i] * r[(size_t) i]; ll += l[(size_t) i] * l[(size_t) i]; rr += r[(size_t) i] * r[(size_t) i]; }
    const double c = ll > 1e-9 && rr > 1e-9 ? lr / std::sqrt (ll * rr) : 0;
    correlation = correlation * 0.8 + c * 0.2;
}

void Goniometer::draw (Graphics& g, Rectangle<float> a)
{
    auto bar = a.removeFromBottom (18);
    const float side = jmin (a.getWidth(), a.getHeight());
    auto sq = a.withSizeKeepingCentre (side, side);
    const auto c = sq.getCentre();
    const float radius = side * 0.5f;
    g.setColour (theme::rule);
    g.drawLine (c.x, sq.getY(), c.x, sq.getBottom());          // mono
    g.drawLine (sq.getX(), c.y, sq.getRight(), c.y);            // sides
    g.drawLine (c.x - radius * 0.7071f, c.y - radius * 0.7071f, c.x + radius * 0.7071f, c.y + radius * 0.7071f, 0.6f);   // L
    g.drawLine (c.x + radius * 0.7071f, c.y - radius * 0.7071f, c.x - radius * 0.7071f, c.y + radius * 0.7071f, 0.6f);   // R
    g.setColour (theme::textSecondary);
    g.setFont (theme::font (theme::smallPoints));
    g.drawText ("L", Rectangle<float> (c.x - radius * 0.72f - 14, c.y - radius * 0.72f - 14, 14, 14), Justification::centred);
    g.drawText ("R", Rectangle<float> (c.x + radius * 0.72f, c.y - radius * 0.72f - 14, 14, 14), Justification::centred);
    g.drawText ("M", Rectangle<float> (c.x + 2, sq.getY(), 14, 14), Justification::centredLeft);
    g.drawText ("S", Rectangle<float> (sq.getRight() - 14, c.y - 14, 14, 14), Justification::centred);

    // Each sample a dot: mid up, sides across (scaled so a full-scale signal reaches the edge).
    g.setColour (theme::meterGreen.withAlpha (0.55f));
    for (size_t i = 0; i < l.size(); ++i)
    {
        const float m = (l[i] + r[i]) * 0.7071f, s = (r[i] - l[i]) * 0.7071f;
        const float x = c.x + jlimit (-1.0f, 1.0f, s) * radius * 0.95f, y = c.y - jlimit (-1.0f, 1.0f, m) * radius * 0.95f;
        g.fillRect (x, y, 1.6f, 1.6f);
    }

    // The correlation, -1 to +1.
    g.setColour (theme::lane);
    auto track = bar.reduced (24, 6);
    g.fillRoundedRectangle (track, 2);
    const float x = track.getX() + (float) ((correlation + 1.0) * 0.5) * track.getWidth();
    g.setColour (correlation < 0 ? theme::warn : theme::meterGreen);
    g.fillRoundedRectangle (Rectangle<float> (x - 2, track.getY() - 3, 4, track.getHeight() + 6), 1);
    g.setColour (theme::textSecondary);
    g.drawText ("-1", bar.removeFromLeft (22), Justification::centred);
    g.drawText ("+1", bar.removeFromRight (22), Justification::centred);
}

//==============================================================================
Spectrum::Spectrum (Player& p) : Visualiser (p, "Spectrum")
{
    l.resize (size);
    r.resize (size);
    data.resize (size * 2);
}

void Spectrum::update()
{
    player.scope.read (l.data(), r.data(), size);
    for (int i = 0; i < size; ++i) data[(size_t) i] = 0.5f * (l[(size_t) i] + r[(size_t) i]);
    std::fill (data.begin() + size, data.end(), 0.0f);
    window.multiplyWithWindowingTable (data.data(), (size_t) size);
    fft.performFrequencyOnlyForwardTransform (data.data());
    const int columns = jmax (16, getWidth() / 6);
    bars.resize ((size_t) columns, -100.0f);
    peaks.resize ((size_t) columns, -100.0f);
    peakTimes.resize ((size_t) columns, 0.0);
    const double rate = player.sampleRate();
    const double now = Time::getMillisecondCounterHiRes() / 1000.0;
    for (int c = 0; c < columns; ++c)
    {
        // Log spaced, 20 Hz to 20 kHz.
        const double f0 = 20.0 * std::pow (1000.0, (double) c / columns), f1 = 20.0 * std::pow (1000.0, (double) (c + 1) / columns);
        int b0 = (int) (f0 / rate * size), b1 = jmax (b0 + 1, (int) (f1 / rate * size));
        b0 = jlimit (1, size / 2 - 1, b0);
        b1 = jlimit (b0 + 1, size / 2, b1);
        float m = 0;
        for (int b = b0; b < b1; ++b) m = jmax (m, data[(size_t) b]);
        // A full-scale sine reads 0 dB: the window's gain and the FFT's size taken out.
        const float db = toDb (m / (size * 0.25f) * 2.0f);
        auto& v = bars[(size_t) c];
        v = db > v ? db : v - 1.5f;   // falls back slowly
        if (v >= peaks[(size_t) c] || now - peakTimes[(size_t) c] > 1.2) { peaks[(size_t) c] = v; peakTimes[(size_t) c] = now; }
    }
}

void Spectrum::draw (Graphics& g, Rectangle<float> a)
{
    const float floor = -90.0f;
    auto yOf = [&] (float db) { return a.getBottom() - jlimit (0.0f, 1.0f, (db - floor) / -floor) * a.getHeight(); };
    g.setFont (theme::font (theme::smallPoints));
    for (float db : { -12.0f, -24.0f, -48.0f, -72.0f })
    {
        g.setColour (theme::lane);
        g.drawHorizontalLine ((int) yOf (db), a.getX(), a.getRight());
        g.setColour (theme::textSecondary);
        g.drawText (String ((int) db), Rectangle<float> (a.getRight() - 30, yOf (db) - 14, 30, 14), Justification::centredRight);
    }
    for (double f : { 100.0, 1000.0, 10000.0 })
    {
        const float x = a.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * a.getWidth();
        g.setColour (theme::lane);
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        g.setColour (theme::textSecondary);
        g.drawText (f >= 1000 ? String ((int) (f / 1000)) + "k" : String ((int) f), Rectangle<float> (x + 2, a.getBottom() - 14, 30, 14), Justification::centredLeft);
    }
    if (bars.empty()) return;
    const float w = a.getWidth() / (float) bars.size();
    if (asBars)
    {
        for (size_t c = 0; c < bars.size(); ++c)
        {
            const float x = a.getX() + (float) c * w;
            const float y = yOf (bars[c]);
            g.setGradientFill (ColourGradient (theme::meterAmber, x, a.getY(), theme::meterGreen, x, a.getY() + a.getHeight() * 0.4f, false));
            g.fillRect (x + 1, y, jmax (1.0f, w - 2), a.getBottom() - y);
            g.setColour (theme::text.withAlpha (0.7f));
            g.fillRect (x + 1, yOf (peaks[c]) - 1, jmax (1.0f, w - 2), 2.0f);
        }
    }
    else
    {
        Path p;
        for (size_t c = 0; c < bars.size(); ++c)
        {
            const float x = a.getX() + ((float) c + 0.5f) * w, y = yOf (bars[c]);
            if (c == 0) p.startNewSubPath (x, y);
            else p.lineTo (x, y);
        }
        g.setColour (theme::meterGreen);
        g.strokePath (p, PathStrokeType (1.6f));
    }
}

void Spectrum::addToMenu (PopupMenu& m)
{
    m.addItem ("Bars", true, asBars, [this] { asBars = true; });
    m.addItem ("Line", true, ! asBars, [this] { asBars = false; });
}

//==============================================================================
void PeakMeter::update()
{
    const float p[2] = { player.levels.peakL.exchange (0.0f), player.levels.peakR.exchange (0.0f) };
    const float s[2] = { player.levels.rmsL.load(), player.levels.rmsR.load() };
    const double now = Time::getMillisecondCounterHiRes() / 1000.0;
    for (int c = 0; c < 2; ++c)
    {
        const float db = toDb (p[c]);
        peak[c] = db > peak[c] ? db : jmax (db, peak[c] - 1.2f);   // falls 36 dB a second
        rms[c] = toDb (std::sqrt (s[c]));
        if (db >= held[c] || now - heldAt[c] > 2.0) { held[c] = db; heldAt[c] = now; }
        if (p[c] >= 1.0f) clipped[c] = true;
    }
}

void PeakMeter::draw (Graphics& g, Rectangle<float> a)
{
    const float floor = -60.0f;
    auto scale = a.removeFromBottom (16);
    auto xOf = [&] (float db) { return a.getX() + jlimit (0.0f, 1.0f, (db - floor) / -floor) * a.getWidth(); };
    const float h = jmin (28.0f, (a.getHeight() - 8) / 2);
    for (int c = 0; c < 2; ++c)
    {
        auto row = Rectangle<float> (a.getX(), a.getY() + (float) c * (h + 8), a.getWidth(), h);
        g.setColour (theme::lane);
        g.fillRect (row);
        // Green to -6, amber to 0.
        const float px = xOf (peak[c]), sx = xOf (rms[c]), six = xOf (-6.0f);
        g.setColour (theme::meterGreen.withAlpha (0.45f));
        g.fillRect (row.withRight (jmin (px, six)));
        if (px > six) { g.setColour (theme::meterAmber.withAlpha (0.6f)); g.fillRect (row.withLeft (six).withRight (px)); }
        g.setColour (theme::meterGreen);
        g.fillRect (row.withRight (jmin (sx, six)).reduced (0, h * 0.25f));
        if (sx > six) { g.setColour (theme::meterAmber); g.fillRect (row.withLeft (six).withRight (sx).reduced (0, h * 0.25f)); }
        g.setColour (theme::text);
        g.fillRect (xOf (held[c]) - 1, row.getY(), 2.0f, row.getHeight());
        if (clipped[c]) { g.setColour (theme::warn); g.fillRect (row.withLeft (row.getRight() - 6)); }
        g.setColour (theme::textSecondary);
        g.setFont (theme::font (theme::smallPoints));
        g.drawText ((c == 0 ? "L  " : "R  ") + (held[c] > -100 ? String (held[c], 1) : String::fromUTF8 ("\xe2\x88\x92\xe2\x88\x9e")),
                    row.reduced (4, 0), Justification::centredLeft);
    }
    g.setColour (theme::textSecondary);
    for (float db : { -48.0f, -36.0f, -24.0f, -12.0f, -6.0f, -3.0f, 0.0f })
        g.drawText (String ((int) db), Rectangle<float> (xOf (db) - 15, scale.getY(), 30, 14), Justification::centred);
}

void PeakMeter::mouseDown (const MouseEvent&) { clipped[0] = clipped[1] = false; held[0] = held[1] = -100; }

//==============================================================================
void LoudnessView::draw (Graphics& g, Rectangle<float> a)
{
    auto& m = player.loudness;
    auto value = [] (double v, const char* unit) { return v <= loudness::silence + 1 ? String::fromUTF8 ("\xe2\x88\x92\xe2\x88\x9e") : String (v, 1) + " " + unit; };
    struct Row { const char* name; String text; double v; };
    const Row rows[] = {
        { "Momentary", value (m.momentary(), "LUFS"), m.momentary() },
        { "Short-term", value (m.shortTerm(), "LUFS"), m.shortTerm() },
        { "Integrated", value (m.integrated(), "LUFS"), m.integrated() },
        { "Range", String (m.range(), 1) + " LU", -100 },
        { "True Peak", value (m.truePeakDb(), "dBTP"), -100 },
    };
    const float h = jmin (30.0f, a.getHeight() / 5.6f);
    for (auto& r : rows)
    {
        auto line = a.removeFromTop (h);
        g.setColour (theme::textSecondary);
        g.setFont (theme::font (theme::smallPoints));
        g.drawText (r.name, line.removeFromLeft (jmin (96.0f, line.getWidth() * 0.4f)), Justification::centredLeft);
        g.setColour (theme::text);
        g.setFont (theme::font (16.0f, true));
        g.drawText (r.text, line.removeFromLeft (110), Justification::centredLeft);
        if (r.v > -100 && line.getWidth() > 30)
        {
            // A bar from -40 to 0 LUFS, with -14 (streaming's level) marked.
            auto bar = line.reduced (4, h * 0.3f);
            g.setColour (theme::lane);
            g.fillRect (bar);
            const float f = (float) jlimit (0.0, 1.0, (r.v + 40.0) / 40.0);
            g.setColour (r.v > -9 ? theme::meterAmber : theme::meterGreen);
            g.fillRect (bar.withWidth (bar.getWidth() * f));
            g.setColour (theme::text);
            g.fillRect (bar.getX() + bar.getWidth() * (26.0f / 40.0f), bar.getY() - 2, 1.5f, bar.getHeight() + 4);
        }
    }
    g.setColour (theme::textSecondary);
    g.setFont (theme::font (theme::smallPoints));
    g.drawText ("Right-click to start again", a.removeFromTop (16), Justification::centredLeft);
}

void LoudnessView::addToMenu (PopupMenu& m) { m.addItem ("Start the Integrated Reading Again", [this] { player.resetLoudness = true; }); }

} // namespace kp
