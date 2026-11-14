#include "ui/PluginEditor.h"

#include "dsp/FactoryPresets.h"

#include <algorithm>

namespace
{
    constexpr int topBarHeight = theme::controlHeight;
    constexpr int controlRowHeight = 56;
    constexpr int meterWidth = 64;
    constexpr int diagnosticsItemId = 1000;
}

// --- Content ----------------------------------------------------------------

DeEssrAudioProcessorEditor::Content::Content (DeEssrAudioProcessor& p)
    : mode (p.apvts, ids::mode,
            "Detection mode. Vocal: one voice, uses voiced / unvoiced criteria. "
            "Bus: a group or a full mix, reacts to what stands out from the average."),
      listen (p.apvts, ids::listen, "Listen to what is being removed instead of the processed signal."),
      bypass (p.apvts, ids::bypass, "Bypass the processing. The signal stays aligned with the reported latency."),
      presetButton ("Factory presets: starting points for common sources. Listen and Bypass are not changed."),
      scaleButton ("Interface scale and display diagnostics."),
      spectrum (p.apvts),
      tooltips (this, 700)
{
    const auto add = [this, &p] (size_t index, const char* id, const char* tooltip, bool bipolar = false)
    {
        controls[index] = std::make_unique<ParamControl> (p.apvts, id, tooltip, bipolar);
        addAndMakeVisible (*controls[index]);
    };

    add (0, ids::amount, "Sensitivity, relative to the level of the source. Higher values also treat softer sibilance.");
    add (1, ids::range, "Maximum reduction. The processing never removes more than this.");
    add (2, ids::focus, "0%: the whole sibilant band goes down together. 100%: only the peaks that stick out are shaved.");
    add (3, ids::bandLow, "Low edge of the sibilant band.");
    add (4, ids::bandHigh, "High edge of the sibilant band.");
    add (5, ids::release, "How quickly the reduction lets go after a sibilant.");
    add (6, ids::stereoLink, "100%: both channels get the same gain, the stereo image does not move. 0%: independent channels.");
    add (7, ids::outputGain, "Level of the processed signal.", true);

    diagnostics.setFont (theme::font (theme::fontSmall));
    diagnostics.setColour (juce::Label::textColourId, theme::text);
    diagnostics.setColour (juce::Label::backgroundColourId, theme::background.withAlpha (0.85f));
    diagnostics.setJustificationType (juce::Justification::centredLeft);
    diagnostics.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (mode);
    addAndMakeVisible (listen);
    addAndMakeVisible (bypass);
    addAndMakeVisible (presetButton);
    addAndMakeVisible (scaleButton);
    addAndMakeVisible (spectrum);
    addAndMakeVisible (history);
    addAndMakeVisible (meter);
    addChildComponent (diagnostics);
}

void DeEssrAudioProcessorEditor::Content::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    juce::AttributedString title;
    title.setJustification (juce::Justification::centredLeft);
    title.append (DEESSR_PRODUCT_NAME, theme::font (theme::fontTitle, true), theme::text);
    title.append ("   v" + DeEssrAudioProcessor::getVersionString(), theme::font (theme::fontSmall), theme::textDim);
    title.draw (g, titleArea.toFloat());
}

void DeEssrAudioProcessorEditor::Content::resized()
{
    auto area = getLocalBounds().reduced (theme::padding);

    // Top bar
    auto top = area.removeFromTop (topBarHeight);
    scaleButton.setBounds (top.removeFromRight (72));
    top.removeFromRight (theme::gap);
    bypass.setBounds (top.removeFromRight (84));
    top.removeFromRight (theme::gap);
    listen.setBounds (top.removeFromRight (84));
    top.removeFromRight (2 * theme::gap);
    mode.setBounds (top.removeFromRight (152));
    top.removeFromRight (2 * theme::gap);
    presetButton.setBounds (top.removeFromRight (96));
    top.removeFromRight (theme::gap);
    titleArea = top;

    area.removeFromTop (theme::gap);

    // Control row
    auto row = area.removeFromBottom (controlRowHeight);
    const int count = (int) controls.size();
    const int spacing = 16;
    const int width = (row.getWidth() - spacing * (count - 1)) / count;

    for (int i = 0; i < count; ++i)
    {
        controls[(size_t) i]->setBounds (row.removeFromLeft (width));
        row.removeFromLeft (spacing);
    }

    area.removeFromBottom (theme::gap);

    // Centre: spectrum above the history, meter on the right
    meter.setBounds (area.removeFromRight (meterWidth));
    area.removeFromRight (theme::gap);
    history.setBounds (area.removeFromBottom (juce::roundToInt ((float) area.getHeight() * 0.28f)));
    area.removeFromBottom (theme::gap);
    spectrum.setBounds (area);

    diagnostics.setBounds (area.getX() + 8, area.getY() + 8, juce::jmin (470, area.getWidth() - 16), 22);
}

// --- Editor -----------------------------------------------------------------

DeEssrAudioProcessorEditor::DeEssrAudioProcessorEditor (DeEssrAudioProcessor& p)
    : AudioProcessorEditor (p),
      deEsser (p),
      content (p),
      vblank (this, [this] { onVerticalBlank(); })
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    addAndMakeVisible (content);

    content.scaleButton.onClick = [this] { showScaleMenu(); };
    content.presetButton.onClick = [this] { showPresetMenu(); };
    content.presetButton.setText ("Presets");

    // Scale and size come from the plugin state and are applied before the first paint.
    scale = deEsser.getUiScale();
    const auto size = deEsser.getUiSize();
    logicalWidth = juce::jlimit (minWidth, maxWidth, size.x);
    logicalHeight = juce::jlimit (minHeight, maxHeight, size.y);
    diagnosticsEnabled = deEsser.getUiDiagnostics();
    content.diagnostics.setVisible (diagnosticsEnabled);

    setResizable (true, true);
    applyScale();
}

DeEssrAudioProcessorEditor::~DeEssrAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void DeEssrAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
}

void DeEssrAudioProcessorEditor::applyScale()
{
    const juce::ScopedValueSetter<bool> guard (applyingScale, true);

    setResizeLimits (juce::roundToInt ((float) minWidth * scale), juce::roundToInt ((float) minHeight * scale),
                     juce::roundToInt ((float) maxWidth * scale), juce::roundToInt ((float) maxHeight * scale));
    setSize (juce::roundToInt ((float) logicalWidth * scale), juce::roundToInt ((float) logicalHeight * scale));

    content.scaleButton.setText (juce::String (juce::roundToInt (scale * 100.0f)) + "%");
    resized();
}

void DeEssrAudioProcessorEditor::resized()
{
    const int width = juce::roundToInt ((float) getWidth() / scale);
    const int height = juce::roundToInt ((float) getHeight() / scale);

    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, width, height);

    if (! applyingScale && width > 0 && height > 0)
    {
        // The user dragged the corner: remember the new logical size.
        logicalWidth = juce::jlimit (minWidth, maxWidth, width);
        logicalHeight = juce::jlimit (minHeight, maxHeight, height);
        deEsser.setUiSize (logicalWidth, logicalHeight);
    }
}

void DeEssrAudioProcessorEditor::showScaleMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&lookAndFeel);
    const int current = juce::roundToInt (scale * 100.0f);

    for (const int percent : { 100, 125, 150, 175, 200 })
        menu.addItem (percent, juce::String (percent) + "%", true, percent == current);

    menu.addSeparator();
    menu.addItem (diagnosticsItemId, "Display diagnostics", true, diagnosticsEnabled);

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (content.scaleButton)
                            .withParentComponent (&content)
                            .withMinimumWidth (160),
                        [safe = juce::Component::SafePointer<DeEssrAudioProcessorEditor> (this)] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;

                            if (result == diagnosticsItemId)
                            {
                                safe->diagnosticsEnabled = ! safe->diagnosticsEnabled;
                                safe->deEsser.setUiDiagnostics (safe->diagnosticsEnabled);
                                safe->content.diagnostics.setVisible (safe->diagnosticsEnabled);
                                safe->updateDiagnostics();
                                return;
                            }

                            safe->scale = (float) result / 100.0f;
                            safe->deEsser.setUiScale (safe->scale);
                            safe->applyScale();
                        });
}

void DeEssrAudioProcessorEditor::onVerticalBlank()
{
    // Elapsed time comes from the clock, never from an assumed refresh rate.
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const double elapsed = lastBlankSeconds > 0.0 ? now - lastBlankSeconds : 0.0;
    lastBlankSeconds = now;

    if (elapsed > 0.0)
    {
        intervals[(size_t) intervalIndex] = (float) elapsed;
        intervalIndex = (intervalIndex + 1) % (int) intervals.size();
        intervalCount = juce::jmin (intervalCount + 1, (int) intervals.size());
    }

    advanceDisplays (juce::jmin (elapsed, 0.25));
}

void DeEssrAudioProcessorEditor::advanceDisplays (double elapsedSeconds)
{
    const int numPoints = deEsser.display.readHistory (historyBuffer.data(), (int) historyBuffer.size());
    float reduction = 0.0f;

    for (int i = 0; i < numPoints; ++i)
        reduction = juce::jmax (reduction, historyBuffer[(size_t) i].reductionDb);

    // Each view reports whether its picture changed: nothing is repainted at rest.
    if (content.spectrum.advance (elapsedSeconds, deEsser.display, numPoints > 0))
        content.spectrum.repaint();

    if (content.history.advance (elapsedSeconds, historyBuffer.data(), numPoints))
        content.history.repaint();

    if (content.meter.advance (elapsedSeconds, reduction))
        content.meter.repaint();

    if (diagnosticsEnabled)
    {
        diagnosticsAge += elapsedSeconds;

        if (diagnosticsAge >= 0.25)
        {
            diagnosticsAge = 0.0;
            updateDiagnostics();
        }
    }
}

void DeEssrAudioProcessorEditor::updateDiagnostics()
{
    if (! diagnosticsEnabled)
        return;

    if (intervalCount < 8)
    {
        content.diagnostics.setText (" Display: measuring", juce::dontSendNotification);
        return;
    }

    std::array<float, 512> sorted {};
    std::copy_n (intervals.begin(), (size_t) intervalCount, sorted.begin());
    std::sort (sorted.begin(), sorted.begin() + intervalCount);

    double sum = 0.0;

    for (int i = 0; i < intervalCount; ++i)
        sum += (double) sorted[(size_t) i];

    const double median = (double) sorted[(size_t) (intervalCount / 2)];
    const double average = sum / (double) intervalCount;
    const int worstIndex = juce::jlimit (0, intervalCount - 1, (int) std::ceil (0.99 * (double) intervalCount) - 1);
    const double worst = (double) sorted[(size_t) worstIndex];

    content.diagnostics.setText (" Display: " + juce::String (1.0 / juce::jmax (1.0e-6, median), 1) + " Hz"
                                     + "  |  frame time avg " + juce::String (average * 1000.0, 2) + " ms"
                                     + "  |  worst 1% " + juce::String (worst * 1000.0, 2) + " ms",
                                 juce::dontSendNotification);
}

void DeEssrAudioProcessorEditor::showPresetMenu()
{
    const auto presets = deessr::factoryPresets();
    const int current = findMatchingFactoryPreset (deEsser.apvts);

    juce::PopupMenu menu;
    menu.setLookAndFeel (&lookAndFeel);

    // One submenu per category; the preset matching the current settings is ticked.
    for (size_t first = 0; first < presets.size();)
    {
        const juce::String category (presets[first].category);
        juce::PopupMenu submenu;
        submenu.setLookAndFeel (&lookAndFeel);
        bool containsCurrent = false;
        size_t index = first;

        for (; index < presets.size() && category == presets[index].category; ++index)
        {
            const bool isCurrent = (int) index == current;
            containsCurrent = containsCurrent || isCurrent;
            submenu.addItem ((int) index + 1, presets[index].name, true, isCurrent);
        }

        menu.addSubMenu (category, submenu, true, nullptr, containsCurrent);
        first = index;
    }

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (content.presetButton)
                            .withParentComponent (&content)
                            .withMinimumWidth (180),
                        [safe = juce::Component::SafePointer<DeEssrAudioProcessorEditor> (this)] (int result)
                        {
                            const auto all = deessr::factoryPresets();

                            if (safe != nullptr && result > 0 && (size_t) result <= all.size())
                                applyFactoryPreset (safe->deEsser.apvts, all[(size_t) result - 1]);
                        });
}
