#include "plugin/Parameters.h"

#include "dsp/FactoryPresets.h"

#include <array>
#include <cmath>

namespace
{
    using Attributes = juce::AudioParameterFloatAttributes;

    juce::NormalisableRange<float> skewedRange (float start, float end, float centre)
    {
        juce::NormalisableRange<float> range (start, end);
        range.setSkewForCentre (centre);
        return range;
    }

    Attributes numeric (const juce::String& unit, int decimals)
    {
        return Attributes()
            .withLabel (unit)
            .withStringFromValueFunction ([decimals] (float value, int) { return juce::String (value, decimals); })
            .withValueFromStringFunction ([] (const juce::String& text) { return (float) text.trim().getDoubleValue(); });
    }

    /** Stored in Hz, shown and typed in kHz. A typed value above 100 is taken as Hz. */
    Attributes frequency()
    {
        return Attributes()
            .withLabel ("kHz")
            .withStringFromValueFunction ([] (float value, int) { return juce::String (value / 1000.0f, 2); })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                const auto typed = (float) text.trim().getDoubleValue();
                return typed > 100.0f ? typed : typed * 1000.0f;
            });
    }

    Attributes signedDecibels()
    {
        return Attributes()
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float value, int)
            {
                return (value > 0.049f ? "+" : "") + juce::String (value, 1);
            })
            .withValueFromStringFunction ([] (const juce::String& text) { return (float) text.trim().getDoubleValue(); });
    }

    std::unique_ptr<juce::AudioParameterFloat> floatParam (const char* id, const juce::String& name,
                                                           juce::NormalisableRange<float> range,
                                                           float defaultValue, const Attributes& attributes)
    {
        return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range,
                                                            defaultValue, attributes);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace deessr;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::mode, 1 }, "Mode",
                                                              juce::StringArray { "Vocal", "Bus" }, 0));

    layout.add (floatParam (ids::amount, "Amount", { 0.0f, 100.0f }, 50.0f, numeric ("%", 0)));
    layout.add (floatParam (ids::range, "Range", { 0.0f, limits::rangeMaxDb }, 9.0f, numeric ("dB", 1)));
    layout.add (floatParam (ids::focus, "Focus", { 0.0f, 100.0f }, 50.0f, numeric ("%", 0)));

    layout.add (floatParam (ids::bandLow, "Band Low",
                            skewedRange (limits::bandMinHz, limits::bandMaxHz, 5657.0f), 4000.0f, frequency()));
    layout.add (floatParam (ids::bandHigh, "Band High",
                            skewedRange (limits::bandMinHz, limits::bandMaxHz, 5657.0f), 12000.0f, frequency()));

    layout.add (floatParam (ids::release, "Release",
                            skewedRange (limits::releaseMinMs, limits::releaseMaxMs, 80.0f), 60.0f, numeric ("ms", 0)));
    layout.add (floatParam (ids::stereoLink, "Stereo Link", { 0.0f, 100.0f }, 100.0f, numeric ("%", 0)));
    layout.add (floatParam (ids::outputGain, "Output Gain",
                            { limits::outputGainMinDb, limits::outputGainMaxDb }, 0.0f, signedDecibels()));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::listen, 1 }, "Listen", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ids::bypass, 1 }, "Bypass", false));

    return layout;
}

ParameterReader::ParameterReader (juce::AudioProcessorValueTreeState& state)
    : mode (state.getRawParameterValue (ids::mode)),
      amount (state.getRawParameterValue (ids::amount)),
      range (state.getRawParameterValue (ids::range)),
      focus (state.getRawParameterValue (ids::focus)),
      bandLow (state.getRawParameterValue (ids::bandLow)),
      bandHigh (state.getRawParameterValue (ids::bandHigh)),
      release (state.getRawParameterValue (ids::release)),
      stereoLink (state.getRawParameterValue (ids::stereoLink)),
      outputGain (state.getRawParameterValue (ids::outputGain)),
      listen (state.getRawParameterValue (ids::listen)),
      bypass (state.getRawParameterValue (ids::bypass))
{
}

deessr::Params ParameterReader::read() const noexcept
{
    const auto get = [] (const std::atomic<float>* value) { return value->load (std::memory_order_relaxed); };

    deessr::Params p;
    p.mode = get (mode) < 0.5f ? deessr::Mode::vocal : deessr::Mode::bus;
    p.amount = get (amount) * 0.01f;
    p.rangeDb = get (range);
    p.focus = get (focus) * 0.01f;
    p.bandLowHz = get (bandLow);
    p.bandHighHz = get (bandHigh);
    p.releaseMs = get (release);
    p.stereoLink = get (stereoLink) * 0.01f;
    p.outputGainDb = get (outputGain);
    p.listen = get (listen) > 0.5f;
    p.bypass = get (bypass) > 0.5f;
    return p;
}

// --- factory presets ---------------------------------------------------------

namespace
{
    struct PresetValue
    {
        const char* id;
        float value;      // in the units of the parameter
        float tolerance;  // used to recognise the preset from the current settings
    };

    std::array<PresetValue, 9> presetValues (const deessr::Params& p)
    {
        return { {
            { ids::mode, p.mode == deessr::Mode::bus ? 1.0f : 0.0f, 0.1f },
            { ids::amount, p.amount * 100.0f, 0.5f },
            { ids::range, p.rangeDb, 0.05f },
            { ids::focus, p.focus * 100.0f, 0.5f },
            { ids::bandLow, p.bandLowHz, 10.0f },
            { ids::bandHigh, p.bandHighHz, 10.0f },
            { ids::release, p.releaseMs, 0.5f },
            { ids::stereoLink, p.stereoLink * 100.0f, 0.5f },
            { ids::outputGain, p.outputGainDb, 0.05f },
        } };
    }
}

void applyFactoryPreset (juce::AudioProcessorValueTreeState& state, const deessr::FactoryPreset& preset)
{
    for (const auto& item : presetValues (preset.params))
    {
        if (auto* parameter = state.getParameter (item.id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (item.value));
            parameter->endChangeGesture();
        }
    }
}

int findMatchingFactoryPreset (const juce::AudioProcessorValueTreeState& state)
{
    const auto presets = deessr::factoryPresets();

    for (size_t index = 0; index < presets.size(); ++index)
    {
        bool matches = true;

        for (const auto& item : presetValues (presets[index].params))
        {
            const auto* current = state.getRawParameterValue (item.id);
            matches = matches && current != nullptr
                      && std::abs (current->load (std::memory_order_relaxed) - item.value) <= item.tolerance;
        }

        if (matches)
            return (int) index;
    }

    return -1;
}
