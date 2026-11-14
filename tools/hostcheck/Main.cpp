// Loads a built de-essr.vst3 bundle the way a host does and checks the basics:
// it loads, accepts mono and stereo layouts, reports the expected latency,
// keeps that latency in practice, produces finite output with varying block
// sizes, restores its state, and opens its editor.
//
// This complements pluginval, it does not replace it.
//
// Usage: de-essr-hostcheck <path to de-essr.vst3>

#include "dsp/DeEsserEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>
#include <cstdio>

namespace
{
    int failures = 0;

    void check (bool condition, const juce::String& what)
    {
        std::printf ("  [%s] %s\n", condition ? "ok" : "FAILED", what.toRawUTF8());

        if (! condition)
            ++failures;
    }

    juce::AudioProcessorParameter* findParameter (juce::AudioProcessor& plugin, const juce::String& name)
    {
        for (auto* parameter : plugin.getParameters())
            if (parameter->getName (64) == name)
                return parameter;

        return nullptr;
    }

    bool isFinite (const juce::AudioBuffer<float>& buffer, int numSamples)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int i = 0; i < numSamples; ++i)
                if (! std::isfinite (buffer.getSample (channel, i)))
                    return false;

        return true;
    }

    void run (juce::AudioPluginFormatManager& formats, const juce::PluginDescription& description,
              double sampleRate, int numChannels)
    {
        std::printf ("%.1f kHz, %s\n", sampleRate / 1000.0, numChannels == 1 ? "mono" : "stereo");

        const int maxBlock = 512;
        juce::String error;
        auto plugin = formats.createPluginInstance (description, sampleRate, maxBlock, error);
        check (plugin != nullptr, "plugin instance created " + error);

        if (plugin == nullptr)
            return;

        const auto set = numChannels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (set);
        layout.outputBuses.add (set);
        check (plugin->setBusesLayout (layout), "layout accepted");

        plugin->setRateAndBufferSizeDetails (sampleRate, maxBlock);
        plugin->prepareToPlay (sampleRate, maxBlock);

        const int latency = plugin->getLatencySamples();
        check (latency == deessr::DeEsserEngine::latencyFor (sampleRate),
               "reported latency is " + juce::String (latency) + " samples ("
                   + juce::String (1000.0 * latency / sampleRate, 1) + " ms)");

        auto* amount = findParameter (*plugin, "Amount");
        auto* range = findParameter (*plugin, "Range");
        check (amount != nullptr && range != nullptr, "parameters are exposed to the host");
        check (plugin->getParameters().size() == 11, "11 automatable parameters");

        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buffer (numChannels, maxBlock);
        juce::Random random (1234);

        // Noise through blocks of varying sizes.
        bool finite = true;

        for (const int blockSize : { 512, 1, 64, 300, 17, 512, 128 })
        {
            for (int repeat = 0; repeat < 20; ++repeat)
            {
                juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(), numChannels, blockSize);

                for (int channel = 0; channel < numChannels; ++channel)
                    for (int i = 0; i < blockSize; ++i)
                        block.setSample (channel, i, random.nextFloat() * 0.5f - 0.25f);

                plugin->processBlock (block, midi);
                finite = finite && isFinite (block, blockSize);
            }
        }

        check (finite, "finite output with varying block sizes");

        // An impulse must come out exactly latency samples later (Amount at zero).
        if (amount != nullptr)
        {
            amount->setValueNotifyingHost (0.0f);
            plugin->reset();

            int position = 0, peakPosition = -1;
            float peakValue = 0.0f;

            for (int block = 0; block < 4 + 2 * latency / maxBlock; ++block)
            {
                buffer.clear();

                if (block == 1)
                    buffer.setSample (0, 100, 0.5f);

                plugin->processBlock (buffer, midi);

                for (int i = 0; i < maxBlock; ++i)
                {
                    if (std::abs (buffer.getSample (0, i)) > peakValue)
                    {
                        peakValue = std::abs (buffer.getSample (0, i));
                        peakPosition = position + i;
                    }
                }

                position += maxBlock;
            }

            check (peakPosition == maxBlock + 100 + latency && std::abs (peakValue - 0.5f) < 1.0e-4f,
                   "impulse delayed by exactly the reported latency");
        }

        // State round trip.
        if (range != nullptr)
        {
            range->setValueNotifyingHost (0.25f);
            juce::MemoryBlock state;
            plugin->getStateInformation (state);
            range->setValueNotifyingHost (0.9f);
            plugin->setStateInformation (state.getData(), (int) state.getSize());
            check (std::abs (range->getValue() - 0.25f) < 1.0e-3f, "state restored");
        }

        // Editor.
        if (auto* editor = plugin->createEditorAndMakeActive())
        {
            check (editor->getWidth() > 0 && editor->getHeight() > 0,
                   "editor opens at " + juce::String (editor->getWidth()) + " x " + juce::String (editor->getHeight()));
            plugin->editorBeingDeleted (editor);
            delete editor;
        }
        else
        {
            check (false, "editor opens");
        }

        plugin->releaseResources();
    }
}

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::printf ("Usage: de-essr-hostcheck <path to de-essr.vst3>\n");
        return 2;
    }

    const juce::ScopedJuceInitialiser_GUI gui;
    const auto file = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1]));

    juce::AudioPluginFormatManager formats;
    formats.addFormat (std::make_unique<juce::VST3PluginFormat>());

    juce::OwnedArray<juce::PluginDescription> found;
    juce::VST3PluginFormat().findAllTypesForFile (found, file.getFullPathName());

    if (found.isEmpty())
    {
        std::printf ("No VST3 plugin found in %s\n", file.getFileName().toRawUTF8());
        return 1;
    }

    std::printf ("%s %s, by %s, category %s\n", found[0]->name.toRawUTF8(), found[0]->version.toRawUTF8(),
                 found[0]->manufacturerName.toRawUTF8(), found[0]->category.toRawUTF8());

    for (const double sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (const int numChannels : { 1, 2 })
            run (formats, *found[0], sampleRate, numChannels);

    std::printf (failures == 0 ? "All host checks passed.\n" : "%d host check(s) FAILED.\n", failures);
    return failures == 0 ? 0 : 1;
}
