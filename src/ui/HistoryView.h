#pragma once

#include "plugin/DisplayData.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

/** Scrolling history of the sibilance value and of the gain reduction.

    The scroll speed is defined in seconds, not in frames: the view advances by
    the elapsed time it is given, so it moves at the same speed at any refresh rate.
*/
class HistoryView final : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    HistoryView();

    /** Returns true when a repaint is needed. */
    bool advance (double elapsedSeconds, const DisplayData::HistoryPoint* points, int numPoints);

    void paint (juce::Graphics&) override;

private:
    static constexpr int numColumns = 400;
    static constexpr double columnSeconds = 0.02; // 8 seconds across the view

    std::array<float, numColumns> sibilance {};
    std::array<float, numColumns> reduction {};
    int head = 0;              // index of the next column to write
    int activeColumns = 0;     // columns that still hold a non-zero value
    double accumulated = 0.0;
    float pendingSibilance = 0.0f;
    float pendingReduction = 0.0f;
    bool active = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HistoryView)
};
