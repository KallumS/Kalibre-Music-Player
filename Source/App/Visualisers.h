/*
    Visualisers - the meters (decision 0016), each a panel of its own, read
    from what's heard (after the equaliser, plug-ins and volume):

      Oscilloscope  the waveform, both channels, steadied on a rising zero
      Goniometer    left against right, mid up and sides across, with the
                    correlation under it: mono is a vertical line, wide
                    sound a cloud, out-of-phase sound lies sideways
      Spectrum      20 Hz to 20 kHz on a log scale, with peaks held
      Peak Meter    each channel's peak and RMS, with the peak held, in dBFS
      Loudness      EBU R128: momentary, short-term, integrated, range, true peak
*/

#pragma once

#include "Player.h"
#include "Theme.h"

#include <juce_dsp/juce_dsp.h>

namespace kp
{

class Visualiser : public juce::Component, private juce::Timer
{
public:
    Visualiser (Player& p, const juce::String& t) : player (p), title (t) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

protected:
    Player& player;
    juce::String title;
    virtual void draw (juce::Graphics&, juce::Rectangle<float>) = 0;
    virtual void update() {}
    virtual void addToMenu (juce::PopupMenu&) {}
    void timerCallback() override { if (isShowing()) { update(); repaint(); } }
};

class Oscilloscope : public Visualiser
{
public:
    explicit Oscilloscope (Player& p) : Visualiser (p, "Oscilloscope") {}

private:
    std::vector<float> l, r;
    float span = 0.05f;   // seconds shown
    void update() override;
    void draw (juce::Graphics&, juce::Rectangle<float>) override;
    void addToMenu (juce::PopupMenu&) override;
};

class Goniometer : public Visualiser
{
public:
    explicit Goniometer (Player& p) : Visualiser (p, "Goniometer") {}

private:
    std::vector<float> l, r;
    double correlation = 0;
    juce::Image trail;
    void update() override;
    void draw (juce::Graphics&, juce::Rectangle<float>) override;
};

class Spectrum : public Visualiser
{
public:
    explicit Spectrum (Player& p);

private:
    static constexpr int order = 13, size = 1 << order;
    juce::dsp::FFT fft { order };
    juce::dsp::WindowingFunction<float> window { (size_t) size, juce::dsp::WindowingFunction<float>::blackmanHarris };
    std::vector<float> l, r, data, bars, peaks;
    std::vector<double> peakTimes;
    bool asBars = true;
    void update() override;
    void draw (juce::Graphics&, juce::Rectangle<float>) override;
    void addToMenu (juce::PopupMenu&) override;
};

class PeakMeter : public Visualiser
{
public:
    explicit PeakMeter (Player& p) : Visualiser (p, "Peak Meter") {}

private:
    float peak[2] {}, rms[2] {}, held[2] { -100, -100 };
    double heldAt[2] {};
    bool clipped[2] {};
    void update() override;
    void draw (juce::Graphics&, juce::Rectangle<float>) override;
    void mouseDown (const juce::MouseEvent&) override;
};

class LoudnessView : public Visualiser
{
public:
    explicit LoudnessView (Player& p) : Visualiser (p, "Loudness") {}

private:
    void draw (juce::Graphics&, juce::Rectangle<float>) override;
    void addToMenu (juce::PopupMenu&) override;
};

} // namespace kp
