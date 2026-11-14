#pragma once

#include "plugin/PluginProcessor.h"
#include "ui/HistoryView.h"
#include "ui/ParamControl.h"
#include "ui/ReductionMeter.h"
#include "ui/SpectrumView.h"
#include "ui/Theme.h"
#include "ui/ToggleControls.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

/** Editor window.

    Everything lives in one content component laid out in logical pixels (100 %).
    The interface scale (100 % to 200 %, 125 % by default) is a single transform
    on that component. It multiplies the Windows DPI scale handled by JUCE and is
    never applied twice. Scale and window size are stored in the plugin state.
*/
class DeEssrAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit DeEssrAudioProcessorEditor (DeEssrAudioProcessor&);
    ~DeEssrAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Moves every display forward by elapsedSeconds. Called on each vertical
        blank of the screen, and directly by the screenshot tool. */
    void advanceDisplays (double elapsedSeconds);

private:
    class Content final : public juce::Component
    {
    public:
        explicit Content (DeEssrAudioProcessor&);

        void paint (juce::Graphics&) override;
        void resized() override;

        SegmentedControl mode;
        ParamToggle listen;
        ParamToggle bypass;
        MenuButton scaleButton;
        SpectrumView spectrum;
        HistoryView history;
        ReductionMeter meter;
        std::array<std::unique_ptr<ParamControl>, 8> controls;
        juce::Label diagnostics;
        juce::TooltipWindow tooltips;

    private:
        juce::Rectangle<int> titleArea;
    };

    void applyScale();
    void showScaleMenu();
    void onVerticalBlank();
    void updateDiagnostics();

    static constexpr int minWidth = 760, minHeight = 480;     // logical pixels
    static constexpr int maxWidth = 1680, maxHeight = 1050;

    DeEssrAudioProcessor& deEsser;
    theme::LookAndFeel lookAndFeel;
    Content content;

    float scale = DeEssrAudioProcessor::defaultUiScale;
    int logicalWidth = DeEssrAudioProcessor::defaultUiWidth;
    int logicalHeight = DeEssrAudioProcessor::defaultUiHeight;
    bool applyingScale = false;

    // display clock and diagnostics
    double lastBlankSeconds = 0.0;
    bool diagnosticsEnabled = false;
    double diagnosticsAge = 0.0;
    std::array<float, 512> intervals {};
    int intervalCount = 0;
    int intervalIndex = 0;

    std::array<DisplayData::HistoryPoint, DisplayData::historyCapacity> historyBuffer {};

    juce::VBlankAttachment vblank;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeEssrAudioProcessorEditor)
};
