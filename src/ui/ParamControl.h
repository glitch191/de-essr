#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/** One continuous parameter: name, value with unit, and a horizontal bar.

    - Drag anywhere on the control to change the value; hold Shift for fine steps.
    - Double-click to return to the default value.
    - Click the value to type it.
    - The mouse wheel moves the value in small steps.
*/
class ParamControl final : public juce::Component,
                           public juce::SettableTooltipClient
{
public:
    ParamControl (juce::AudioProcessorValueTreeState& state, const juce::String& parameterId,
                  const juce::String& tooltip, bool bipolar = false);

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    void setNormalised (float newValue);
    void refreshText();
    juce::Rectangle<int> barBounds() const;

    juce::RangedAudioParameter& parameter;
    juce::ParameterAttachment attachment;
    juce::Label valueLabel;
    const bool bipolar;

    float normalised = 0.0f;
    float dragValue = 0.0f;
    int lastDragX = 0;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamControl)
};
