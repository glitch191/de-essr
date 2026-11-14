#include "ui/ReductionMeter.h"

#include "ui/Theme.h"

namespace
{
    constexpr float scaleDb = 24.0f;
    constexpr float releaseDbPerSecond = 30.0f;
    constexpr double peakHoldSeconds = 1.5;
    constexpr int headerHeight = 22;
    constexpr int footerHeight = 24;
}

ReductionMeter::ReductionMeter()
{
    setTooltip ("Gain reduction applied right now, with the recent maximum below.");
}

bool ReductionMeter::advance (double elapsedSeconds, float currentReductionDb)
{
    level = juce::jmax (currentReductionDb, level - releaseDbPerSecond * (float) elapsedSeconds);
    level = juce::jmax (0.0f, level);

    if (currentReductionDb >= peak)
    {
        peak = currentReductionDb;
        peakAge = 0.0;
    }
    else
    {
        peakAge += elapsedSeconds;

        if (peakAge > peakHoldSeconds)
            peak = juce::jmax (level, peak - releaseDbPerSecond * (float) elapsedSeconds);
    }

    // Repaint only when the picture would actually differ.
    const bool changed = std::abs (level - shownLevel) > 0.02f || std::abs (peak - shownPeak) > 0.04f;

    if (changed)
    {
        shownLevel = level;
        shownPeak = peak;
    }

    return changed;
}

void ReductionMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.setColour (theme::panel);
    g.fillRect (bounds);
    g.setColour (theme::border);
    g.drawRect (bounds, 1);

    g.setFont (theme::font (theme::fontSmall));
    g.setColour (theme::textDim);
    g.drawText ("GR", bounds.removeFromTop (headerHeight), juce::Justification::centred, false);

    g.setColour (peak > 0.05f ? theme::accent : theme::textDim);
    g.drawText (peak > 0.05f ? "-" + juce::String (peak, 1) : juce::String ("0.0"),
                bounds.removeFromBottom (footerHeight), juce::Justification::centred, false);

    // Bar on the left, scale on the right.
    auto area = bounds.reduced (8, 2).toFloat();
    const auto bar = area.removeFromLeft (juce::jmin (14.0f, area.getWidth() * 0.4f));

    g.setColour (theme::control);
    g.fillRect (bar);

    const float filled = bar.getHeight() * juce::jmin (scaleDb, level) / scaleDb;
    g.setColour (theme::accent);
    g.fillRect (bar.withHeight (filled));

    g.setColour (theme::textDim);

    for (const int db : { 0, 6, 12, 18, 24 })
    {
        const float y = bar.getY() + bar.getHeight() * (float) db / scaleDb;
        g.drawText (juce::String (db), juce::Rectangle<float> (area.getX() + 4.0f, y - 9.0f, area.getWidth() - 4.0f, 18.0f)
                                           .constrainedWithin (area.withTrimmedLeft (4.0f).expanded (0.0f, 2.0f)),
                    juce::Justification::centredLeft, false);
    }
}
