#include "plugin/PluginProcessor.h"

#include "ui/PluginEditor.h"

namespace
{
    const juce::Identifier uiScaleId { "uiScale" };
    const juce::Identifier uiWidthId { "uiWidth" };
    const juce::Identifier uiHeightId { "uiHeight" };
    const juce::Identifier uiDiagnosticsId { "uiDiagnostics" };
}

DeEssrAudioProcessor::DeEssrAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DeEssrState", createParameterLayout()),
      parameters (apvts),
      bypassParameter (apvts.getParameter (ids::bypass))
{
    engine.setListener (&display);
    engine.prepare (48000.0, 2);
    setLatencySamples (engine.latencySamples());
}

DeEssrAudioProcessor::~DeEssrAudioProcessor() = default;

const juce::String DeEssrAudioProcessor::getName() const
{
    return DEESSR_PRODUCT_NAME;
}

juce::String DeEssrAudioProcessor::getVersionString()
{
    return DEESSR_VERSION_STRING;
}

bool DeEssrAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& output = layouts.getMainOutputChannelSet();

    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == output;
}

void DeEssrAudioProcessor::prepareToPlay (double sampleRate, int)
{
    // All memory used by processBlock is allocated here.
    engine.prepare (sampleRate, juce::jlimit (1, deessr::DeEsserEngine::maxChannels, getTotalNumInputChannels()));
    engine.setParams (parameters.read());
    setLatencySamples (engine.latencySamples());
}

void DeEssrAudioProcessor::releaseResources()
{
}

void DeEssrAudioProcessor::reset()
{
    engine.reset();
}

double DeEssrAudioProcessor::getTailLengthSeconds() const
{
    return (double) engine.latencySamples() / engine.sampleRate();
}

void DeEssrAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    process (buffer, false);
}

void DeEssrAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // The bypassed signal is delayed by the reported latency, so it stays aligned.
    process (buffer, true);
}

void DeEssrAudioProcessor::process (juce::AudioBuffer<float>& buffer, bool forceBypass) noexcept
{
    juce::ScopedNoDenormals noDenormals;

    const int numInputs = getTotalNumInputChannels();
    const int numOutputs = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int channel = numInputs; channel < numOutputs; ++channel)
        buffer.clear (channel, 0, numSamples);

    auto params = parameters.read();
    params.bypass = params.bypass || forceBypass;
    engine.setParams (params);

    const int channels = juce::jmin (numInputs, buffer.getNumChannels(), deessr::DeEsserEngine::maxChannels);

    if (channels > 0 && numSamples > 0)
        engine.process (buffer.getArrayOfWritePointers(), channels, numSamples);
}

juce::AudioProcessorEditor* DeEssrAudioProcessor::createEditor()
{
    return new DeEssrAudioProcessorEditor (*this);
}

void DeEssrAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (const auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void DeEssrAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

float DeEssrAudioProcessor::getUiScale() const
{
    return juce::jlimit (1.0f, 2.0f, (float) apvts.state.getProperty (uiScaleId, defaultUiScale));
}

void DeEssrAudioProcessor::setUiScale (float scale)
{
    apvts.state.setProperty (uiScaleId, juce::jlimit (1.0f, 2.0f, scale), nullptr);
}

juce::Point<int> DeEssrAudioProcessor::getUiSize() const
{
    return { (int) apvts.state.getProperty (uiWidthId, defaultUiWidth),
             (int) apvts.state.getProperty (uiHeightId, defaultUiHeight) };
}

void DeEssrAudioProcessor::setUiSize (int width, int height)
{
    apvts.state.setProperty (uiWidthId, width, nullptr);
    apvts.state.setProperty (uiHeightId, height, nullptr);
}

bool DeEssrAudioProcessor::getUiDiagnostics() const
{
    return (bool) apvts.state.getProperty (uiDiagnosticsId, false);
}

void DeEssrAudioProcessor::setUiDiagnostics (bool enabled)
{
    apvts.state.setProperty (uiDiagnosticsId, enabled, nullptr);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DeEssrAudioProcessor();
}
