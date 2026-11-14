#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/** Row of mutually exclusive segments bound to a choice parameter (for example Vocal | Bus). */
class SegmentedControl final : public juce::Component,
                               public juce::SettableTooltipClient
{
public:
    SegmentedControl (juce::AudioProcessorValueTreeState& state, const juce::String& parameterId,
                      const juce::String& tooltip);

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter& parameter;
    juce::ParameterAttachment attachment;
    juce::StringArray items;
    int selected = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedControl)
};

/** On / off button bound to a boolean parameter (Listen, Bypass). */
class ParamToggle final : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    ParamToggle (juce::AudioProcessorValueTreeState& state, const juce::String& parameterId,
                 const juce::String& tooltip);

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    juce::RangedAudioParameter& parameter;
    juce::ParameterAttachment attachment;
    bool on = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamToggle)
};

/** Plain text button that opens a menu (interface scale). */
class MenuButton final : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    explicit MenuButton (const juce::String& tooltip);

    void setText (const juce::String& newText);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    std::function<void()> onClick;

private:
    juce::String text;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MenuButton)
};
