#include "ui/SpectrumView.h"

#include "dsp/DspMath.h"
#include "dsp/Params.h"
#include "plugin/Parameters.h"
#include "ui/Theme.h"

namespace
{
    constexpr float minHz = 20.0f;
    constexpr float maxHz = 20000.0f;
    constexpr float topDb = -12.0f;          // per-bin power shown at the top of the plot
    constexpr float floorDb = -108.0f;       // and at the bottom
    constexpr float fallDbPerSecond = 48.0f; // spectrum decay
    constexpr float gainReleaseDbPerSecond = 60.0f;
    constexpr float reductionScaleDb = 24.0f;
    constexpr float reductionArea = 0.6f;    // share of the plot height used by 24 dB of reduction
    constexpr float edgeGrabDistance = 6.0f;
    constexpr int labelHeight = 20;

    juce::String formatFrequency (float hz)
    {
        return juce::String (hz / 1000.0f, 2) + " kHz";
    }
}

SpectrumView::SpectrumView (juce::AudioProcessorValueTreeState& state)
    : lowParameter (*state.getParameter (ids::bandLow)),
      highParameter (*state.getParameter (ids::bandHigh)),
      lowAttachment (lowParameter, [this] (float value) { lowHz = value; repaint(); }),
      highAttachment (highParameter, [this] (float value) { highHz = value; repaint(); })
{
    levelDb.fill (floorDb);
    gainDb.fill (0.0f);
    setTooltip ("Input spectrum, sibilant band and applied reduction. Drag the band edges to move them.");
    lowAttachment.sendInitialUpdate();
    highAttachment.sendInitialUpdate();
}

bool SpectrumView::advance (double elapsedSeconds, const DisplayData& data, bool newFrames)
{
    const auto elapsed = (float) elapsedSeconds;
    const float fall = fallDbPerSecond * elapsed;
    const float release = gainReleaseDbPerSecond * elapsed;
    bool nowActive = false;

    if (newFrames)
    {
        numBins = data.getNumBins();
        binHz = data.getBinHz();
    }

    for (int k = 0; k < numBins; ++k)
    {
        float level = juce::jmax (floorDb, levelDb[(size_t) k] - fall);
        float gain = juce::jmin (0.0f, gainDb[(size_t) k] + release);

        if (newFrames)
        {
            level = juce::jmax (level, deessr::powerToDb (data.getEnvelope (k)));
            gain = juce::jmin (gain, data.getGainDb (k));
        }

        levelDb[(size_t) k] = level;
        gainDb[(size_t) k] = gain;
        nowActive = nowActive || level > floorDb + 0.5f || gain < -0.05f;
    }

    // One last repaint when the display falls back to rest, then nothing.
    const bool needsRepaint = nowActive || active;
    active = nowActive;
    return needsRepaint;
}

juce::Rectangle<float> SpectrumView::plotBounds() const
{
    return getLocalBounds().toFloat().reduced (1.0f).withTrimmedBottom ((float) labelHeight);
}

float SpectrumView::frequencyToX (float frequencyHz) const
{
    const auto plot = plotBounds();
    const float position = std::log (juce::jlimit (minHz, maxHz, frequencyHz) / minHz) / std::log (maxHz / minHz);
    return plot.getX() + plot.getWidth() * position;
}

float SpectrumView::xToFrequency (float x) const
{
    const auto plot = plotBounds();
    const float position = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / juce::jmax (1.0f, plot.getWidth()));
    return minHz * std::pow (maxHz / minHz, position);
}

void SpectrumView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto plot = plotBounds();

    g.setColour (theme::panel);
    g.fillRect (bounds);

    // --- frequency grid and labels ------------------------------------------
    g.setFont (theme::font (theme::fontSmall));

    for (const float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = std::round (frequencyToX (f)) + 0.5f;
        g.setColour (theme::grid);
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());

        const juce::String label = f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f);
        g.setColour (theme::textDim);
        g.drawText (label, juce::Rectangle<float> (x - 24.0f, plot.getBottom(), 48.0f, (float) labelHeight),
                    juce::Justification::centred, false);
    }

    // --- reduction scale (dB below the top edge) -----------------------------
    const float reductionHeight = plot.getHeight() * reductionArea;

    for (const float db : { 6.0f, 12.0f, 18.0f, 24.0f })
    {
        const float y = std::round (plot.getY() + reductionHeight * db / reductionScaleDb) + 0.5f;
        g.setColour (theme::grid);
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (theme::textDim);
        g.drawText ("-" + juce::String ((int) db) + " dB",
                    juce::Rectangle<float> (plot.getRight() - 64.0f, y - 18.0f, 58.0f, 18.0f),
                    juce::Justification::centredRight, false);
    }

    // --- sibilant band --------------------------------------------------------
    const float xLow = frequencyToX (lowHz);
    const float xHigh = frequencyToX (highHz);
    g.setColour (theme::accent.withAlpha (0.08f));
    g.fillRect (juce::Rectangle<float> (xLow, plot.getY(), xHigh - xLow, plot.getHeight()));

    // --- input spectrum -------------------------------------------------------
    if (numBins > 2)
    {
        juce::Path spectrum;
        const float step = 2.0f;
        bool started = false;

        for (float x = plot.getX(); x <= plot.getRight(); x += step)
        {
            const float binStart = xToFrequency (x) / binHz;
            const float binEnd = xToFrequency (x + step) / binHz;
            float db = floorDb;

            if (binStart >= (float) (numBins - 1))
                break;

            if (binEnd - binStart < 1.0f)
            {
                const int index = juce::jlimit (0, numBins - 2, (int) binStart);
                const float fraction = juce::jlimit (0.0f, 1.0f, binStart - (float) index);
                db = levelDb[(size_t) index] + (levelDb[(size_t) index + 1] - levelDb[(size_t) index]) * fraction;
            }
            else
            {
                const int first = juce::jlimit (0, numBins - 1, (int) std::ceil (binStart));
                const int last = juce::jlimit (first, numBins - 1, (int) binEnd);

                for (int k = first; k <= last; ++k)
                    db = juce::jmax (db, levelDb[(size_t) k]);
            }

            const float y = juce::jmap (juce::jlimit (floorDb, topDb, db), floorDb, topDb, plot.getBottom(), plot.getY());

            if (! started)
            {
                spectrum.startNewSubPath (x, y);
                started = true;
            }
            else
            {
                spectrum.lineTo (x, y);
            }
        }

        if (started)
        {
            juce::Path filled (spectrum);
            filled.lineTo (spectrum.getCurrentPosition().x, plot.getBottom());
            filled.lineTo (plot.getX(), plot.getBottom());
            filled.closeSubPath();

            g.setColour (theme::text.withAlpha (0.10f));
            g.fillPath (filled);
            g.setColour (theme::textDim);
            g.strokePath (spectrum, juce::PathStrokeType (1.0f));
        }

        // --- applied reduction ------------------------------------------------
        juce::Path reduction;
        bool anyReduction = false;
        reduction.startNewSubPath (plot.getX(), plot.getY());

        for (float x = plot.getX(); x <= plot.getRight(); x += step)
        {
            const float bin = xToFrequency (x) / binHz;
            const int index = juce::jlimit (0, numBins - 2, (int) bin);
            const float fraction = juce::jlimit (0.0f, 1.0f, bin - (float) index);
            const float db = bin >= (float) (numBins - 1)
                                 ? 0.0f
                                 : gainDb[(size_t) index] + (gainDb[(size_t) index + 1] - gainDb[(size_t) index]) * fraction;

            anyReduction = anyReduction || db < -0.05f;
            reduction.lineTo (x, plot.getY() + reductionHeight * juce::jmin (reductionScaleDb, -db) / reductionScaleDb);
        }

        if (anyReduction)
        {
            juce::Path filled (reduction);
            filled.lineTo (plot.getRight(), plot.getY());
            filled.closeSubPath();

            g.setColour (theme::accent.withAlpha (0.25f));
            g.fillPath (filled);
            g.setColour (theme::accent);
            g.strokePath (reduction, juce::PathStrokeType (1.5f));
        }
    }

    // --- band edges on top of everything ---------------------------------------
    for (const auto edge : { Edge::low, Edge::high })
    {
        const float x = std::round (edge == Edge::low ? xLow : xHigh);
        const bool highlighted = dragging == edge || hovered == edge;
        g.setColour (theme::accent.withAlpha (highlighted ? 1.0f : 0.75f));
        g.fillRect (juce::Rectangle<float> (x - (highlighted ? 1.0f : 0.5f), plot.getY(),
                                            highlighted ? 2.0f : 1.0f, plot.getHeight()));

        // Grab handle
        g.fillRoundedRectangle (juce::Rectangle<float> (x - 4.0f, plot.getCentreY() - 12.0f, 8.0f, 24.0f), 2.0f);
    }

    drawBandLabel (g, xLow, lowHz, true);
    drawBandLabel (g, xHigh, highHz, false);

    g.setColour (theme::border);
    g.drawRect (bounds, 1.0f);
    g.drawHorizontalLine ((int) plot.getBottom(), bounds.getX(), bounds.getRight());
}

void SpectrumView::drawBandLabel (juce::Graphics& g, float x, float frequencyHz, bool alignRight) const
{
    const auto plot = plotBounds();
    const float width = 72.0f;
    auto area = juce::Rectangle<float> (alignRight ? x - width - 8.0f : x + 8.0f,
                                        plot.getBottom() - 26.0f, width, 20.0f);

    // Keep the label inside the plot when the edge is close to a border.
    if (area.getX() < plot.getX() + 2.0f)
        area.setX (x + 8.0f);

    if (area.getRight() > plot.getRight() - 2.0f)
        area.setX (x - width - 8.0f);

    g.setFont (theme::font (theme::fontSmall));
    g.setColour (theme::accent);
    g.drawText (formatFrequency (frequencyHz), area,
                area.getX() < x ? juce::Justification::centredRight : juce::Justification::centredLeft, false);
}

SpectrumView::Edge SpectrumView::edgeAt (juce::Point<float> position) const
{
    const float toLow = std::abs (position.x - frequencyToX (lowHz));
    const float toHigh = std::abs (position.x - frequencyToX (highHz));

    if (juce::jmin (toLow, toHigh) > edgeGrabDistance || ! plotBounds().contains (position))
        return Edge::none;

    return toLow <= toHigh ? Edge::low : Edge::high;
}

void SpectrumView::mouseMove (const juce::MouseEvent& event)
{
    const auto edge = edgeAt (event.position);

    if (edge != hovered)
    {
        hovered = edge;
        setMouseCursor (edge == Edge::none ? juce::MouseCursor::NormalCursor
                                           : juce::MouseCursor::LeftRightResizeCursor);
        repaint();
    }
}

void SpectrumView::mouseExit (const juce::MouseEvent&)
{
    if (hovered != Edge::none && dragging == Edge::none)
    {
        hovered = Edge::none;
        setMouseCursor (juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void SpectrumView::mouseDown (const juce::MouseEvent& event)
{
    dragging = edgeAt (event.position);

    if (dragging == Edge::low)
        lowAttachment.beginGesture();
    else if (dragging == Edge::high)
        highAttachment.beginGesture();
}

void SpectrumView::mouseDrag (const juce::MouseEvent& event)
{
    using namespace deessr;
    const float frequency = xToFrequency (event.position.x);

    if (dragging == Edge::low)
        lowAttachment.setValueAsPartOfGesture (juce::jlimit (limits::bandMinHz,
                                                             juce::jmax (limits::bandMinHz, highHz / limits::bandMinRatio),
                                                             frequency));
    else if (dragging == Edge::high)
        highAttachment.setValueAsPartOfGesture (juce::jlimit (juce::jmin (limits::bandMaxHz, lowHz * limits::bandMinRatio),
                                                              limits::bandMaxHz, frequency));
}

void SpectrumView::mouseUp (const juce::MouseEvent&)
{
    if (dragging == Edge::low)
        lowAttachment.endGesture();
    else if (dragging == Edge::high)
        highAttachment.endGesture();

    dragging = Edge::none;
    repaint();
}
