#pragma once

#include "dsp/DeEsserEngine.h"
#include "plugin/DisplayData.h"
#include "plugin/Parameters.h"

#include <juce_audio_processors/juce_audio_processors.h>

class DeEssrAudioProcessor final : public juce::AudioProcessor
{
public:
    DeEssrAudioProcessor();
    ~DeEssrAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void reset() override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- editor state, saved with the plugin state --------------------------
    static constexpr float defaultUiScale = 1.25f;
    static constexpr int defaultUiWidth = 880;   // logical size, at 100 %
    static constexpr int defaultUiHeight = 560;

    float getUiScale() const;
    void setUiScale (float scale);
    juce::Point<int> getUiSize() const;
    void setUiSize (int width, int height);
    bool getUiDiagnostics() const;
    void setUiDiagnostics (bool enabled);

    static juce::String getVersionString();

    juce::AudioProcessorValueTreeState apvts;
    DisplayData display;

private:
    void process (juce::AudioBuffer<float>& buffer, bool forceBypass) noexcept;

    ParameterReader parameters;
    juce::AudioProcessorParameter* bypassParameter = nullptr;
    deessr::DeEsserEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeEssrAudioProcessor)
};
