#pragma once

#include "plugin/DisplayData.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

/** Input spectrum, sibilant band with draggable edges, and the gain reduction
    curve currently applied (drawn hanging from the top edge).
*/
class SpectrumView final : public juce::Component,
                           public juce::SettableTooltipClient
{
public:
    explicit SpectrumView (juce::AudioProcessorValueTreeState& state);

    /** Moves the display forward by elapsedSeconds. Returns true when a repaint is needed. */
    bool advance (double elapsedSeconds, const DisplayData& data, bool newFrames);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    enum class Edge { none, low, high };

    juce::Rectangle<float> plotBounds() const;
    float frequencyToX (float frequencyHz) const;
    float xToFrequency (float x) const;
    Edge edgeAt (juce::Point<float> position) const;
    void drawBandLabel (juce::Graphics&, float x, float frequencyHz, bool alignRight) const;

    juce::RangedAudioParameter& lowParameter;
    juce::RangedAudioParameter& highParameter;
    juce::ParameterAttachment lowAttachment;
    juce::ParameterAttachment highAttachment;
    float lowHz = 4000.0f;
    float highHz = 12000.0f;

    std::array<float, DisplayData::maxBins> levelDb;
    std::array<float, DisplayData::maxBins> gainDb;
    int numBins = 0;
    float binHz = 93.75f;
    bool active = false;

    Edge dragging = Edge::none;
    Edge hovered = Edge::none;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumView)
};
