#include "ui/HistoryView.h"

#include "ui/Theme.h"

namespace
{
    constexpr float reductionScaleDb = 24.0f;

    bool isNonZero (float sibilance, float reduction)
    {
        return sibilance > 0.001f || reduction > 0.01f;
    }
}

HistoryView::HistoryView()
{
    setTooltip ("Last 8 seconds: detected sibilance (grey line) and applied gain reduction (filled, from the top).");
}

bool HistoryView::advance (double elapsedSeconds, const DisplayData::HistoryPoint* points, int numPoints)
{
    for (int i = 0; i < numPoints; ++i)
    {
        pendingSibilance = juce::jmax (pendingSibilance, points[i].sibilance);
        pendingReduction = juce::jmax (pendingReduction, points[i].reductionDb);
    }

    accumulated += elapsedSeconds;
    bool advanced = false;

    for (int guard = 0; accumulated >= columnSeconds && guard < numColumns; ++guard)
    {
        accumulated -= columnSeconds;
        const auto index = (size_t) head;

        if (isNonZero (sibilance[index], reduction[index]))
            --activeColumns;

        sibilance[index] = pendingSibilance;
        reduction[index] = pendingReduction;

        if (isNonZero (pendingSibilance, pendingReduction))
            ++activeColumns;

        head = (head + 1) % numColumns;
        advanced = true;
    }

    if (accumulated >= columnSeconds) // very long pause: do not try to catch up
        accumulated = 0.0;

    if (advanced)
    {
        pendingSibilance = 0.0f;
        pendingReduction = 0.0f;
    }

    // While something is visible the view scrolls smoothly, so it repaints on every
    // refresh. Once the last non-zero column has left, it stops.
    const bool nowActive = activeColumns > 0 || isNonZero (pendingSibilance, pendingReduction);
    const bool needsRepaint = nowActive || active;
    active = nowActive;
    return needsRepaint;
}

void HistoryView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto plot = bounds.reduced (1.0f);

    g.setColour (theme::panel);
    g.fillRect (bounds);

    // Faint lines every 6 dB of reduction.
    g.setColour (theme::grid);

    for (const float db : { 6.0f, 12.0f, 18.0f })
        g.drawHorizontalLine ((int) std::round (plot.getY() + plot.getHeight() * db / reductionScaleDb),
                              plot.getX(), plot.getRight());

    if (activeColumns > 0)
    {
        const float columnWidth = plot.getWidth() / (float) (numColumns - 1);
        const float fraction = (float) (accumulated / columnSeconds); // sub-column scroll

        juce::Path reductionPath, sibilancePath;
        reductionPath.startNewSubPath (plot.getRight(), plot.getY());

        for (int age = 0; age < numColumns; ++age)
        {
            const auto index = (size_t) ((head - 1 - age + 2 * numColumns) % numColumns);
            const float x = juce::jmax (plot.getX(), plot.getRight() - ((float) age + fraction) * columnWidth);
            const float yReduction = plot.getY() + plot.getHeight() * juce::jmin (reductionScaleDb, reduction[index]) / reductionScaleDb;
            const float ySibilance = plot.getBottom() - (plot.getHeight() - 2.0f) * juce::jlimit (0.0f, 1.0f, sibilance[index]) - 1.0f;

            reductionPath.lineTo (x, yReduction);

            if (age == 0)
                sibilancePath.startNewSubPath (x, ySibilance);
            else
                sibilancePath.lineTo (x, ySibilance);
        }

        reductionPath.lineTo (plot.getX(), plot.getY());
        reductionPath.closeSubPath();

        g.setColour (theme::accent.withAlpha (0.30f));
        g.fillPath (reductionPath);
        g.setColour (theme::accent);
        g.strokePath (reductionPath, juce::PathStrokeType (1.0f));

        g.setColour (theme::textDim);
        g.strokePath (sibilancePath, juce::PathStrokeType (1.0f));
    }

    // Legend
    g.setFont (theme::font (theme::fontSmall));
    auto legend = plot.reduced (8.0f, 4.0f).removeFromBottom (18.0f);
    g.setColour (theme::textDim);
    g.drawText ("Sibilance", legend.removeFromLeft (64.0f), juce::Justification::centredLeft, false);
    g.setColour (theme::accent);
    g.drawText ("Reduction", legend.removeFromLeft (72.0f), juce::Justification::centredLeft, false);

    g.setColour (theme::border);
    g.drawRect (bounds, 1.0f);
}
