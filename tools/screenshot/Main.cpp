// Renders the real plugin editor to a PNG file, off screen.
//
// The tool creates the processor and its editor, runs a synthetic test signal
// (no recorded audio) through the processor so the displays show real data,
// then saves a snapshot of the editor component. Nothing else is captured:
// no desktop, no window frame, no personal data.
//
// Usage: de-essr-screenshot <output.png> [uiScale] [pixelScale] [logicalWidth logicalHeight] [presetIndex]

#include "TestSignals.h"
#include "dsp/FactoryPresets.h"
#include "plugin/PluginProcessor.h"
#include "ui/PluginEditor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>
#include <memory>

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::printf ("Usage: de-essr-screenshot <output.png> [uiScale] [pixelScale] [logicalWidth logicalHeight] [presetIndex]\n");
        return 2;
    }

    const juce::ScopedJuceInitialiser_GUI gui;

    const auto output = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1]));
    const float uiScale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 1.25f;
    const float pixelScale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 1.0f;

    const double sampleRate = 48000.0;
    const int blockSize = 800; // one display refresh at 60 Hz, only used to pace this tool

    DeEssrAudioProcessor processor;
    processor.setUiScale (uiScale);

    if (argc > 5)
        processor.setUiSize (juce::String (argv[4]).getIntValue(), juce::String (argv[5]).getIntValue());

    // Round trip of every factory preset through the real parameters.
    {
        const auto presets = deessr::factoryPresets();
        int recognised = 0;

        for (size_t i = 0; i < presets.size(); ++i)
        {
            applyFactoryPreset (processor.apvts, presets[i]);
            recognised += findMatchingFactoryPreset (processor.apvts) == (int) i ? 1 : 0;
        }

        std::printf ("Presets applied and recognised: %d of %d\n", recognised, (int) presets.size());
        const int chosen = argc > 6 ? juce::String (argv[6]).getIntValue() : 0;
        applyFactoryPreset (processor.apvts, presets[(size_t) juce::jlimit (0, (int) presets.size() - 1, chosen)]);

        if (recognised != (int) presets.size())
            return 1;
    }

    processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    auto* deEsserEditor = dynamic_cast<DeEssrAudioProcessorEditor*> (editor.get());

    if (deEsserEditor == nullptr)
        return 1;

    // Synthetic speech-like signal; stop in the middle of the last sibilant burst.
    const auto sequence = testsig::speechLike (sampleRate, 9, -8.0f, 0.0f);
    const auto& lastBurst = sequence.sibilant.back();
    const int stopAt = lastBurst.start + (lastBurst.length * 3) / 4;

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;

    for (int offset = 0; offset + blockSize <= stopAt; offset += blockSize)
    {
        for (int channel = 0; channel < 2; ++channel)
            buffer.copyFrom (channel, 0, sequence.samples.data() + offset, blockSize);

        processor.processBlock (buffer, midi);
        deEsserEditor->advanceDisplays ((double) blockSize / sampleRate);
    }

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, pixelScale);

    output.getParentDirectory().createDirectory();
    output.deleteFile();
    juce::FileOutputStream stream (output);

    if (! stream.openedOk() || ! juce::PNGImageFormat().writeImageToStream (image, stream))
    {
        std::printf ("Could not write %s\n", output.getFullPathName().toRawUTF8());
        return 1;
    }

    std::printf ("Wrote %s (%d x %d)\n", output.getFileName().toRawUTF8(), image.getWidth(), image.getHeight());
    return 0;
}
