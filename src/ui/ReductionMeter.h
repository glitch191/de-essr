#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Vertical gain reduction meter with a numeric peak readout. */
class ReductionMeter final : public juce::Component,
                             public juce::SettableTooltipClient
{
public:
    ReductionMeter();

    /** currentReductionDb is the largest reduction since the last call (0 when idle).
        Returns true when a repaint is needed. */
    bool advance (double elapsedSeconds, float currentReductionDb);

    void paint (juce::Graphics&) override;

private:
    float level = 0.0f;       // displayed reduction, dB
    float peak = 0.0f;        // held peak, dB
    double peakAge = 0.0;
    float shownLevel = -1.0f; // last painted values, to skip identical repaints
    float shownPeak = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReductionMeter)
};
