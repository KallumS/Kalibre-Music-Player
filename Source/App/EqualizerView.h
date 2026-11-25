/*
    EqualizerView - the equaliser's ten bands and preamp, with presets
    (decision 0013). Double-click a band to set it back to 0 dB.
*/

#pragma once

#include "Panels.h"

namespace kp
{

class EqualizerView : public juce::Component
{
public:
    explicit EqualizerView (Context&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Context& ctx;
    juce::ToggleButton on { "On" };
    juce::ComboBox presets;
    juce::Slider preamp;
    std::array<juce::Slider, Equalizer::bands> bands;
    void changed();
};

} // namespace kp
