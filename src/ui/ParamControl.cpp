#include "ui/ParamControl.h"

#include "ui/Theme.h"

namespace
{
    constexpr int nameHeight = 18;
    constexpr int valueHeight = 22;
    constexpr int barHeight = 4;
    constexpr float fineFactor = 0.1f;
}

ParamControl::ParamControl (juce::AudioProcessorValueTreeState& state, const juce::String& parameterId,
                            const juce::String& tooltip, bool isBipolar)
    : parameter (*state.getParameter (parameterId)),
      attachment (parameter, [this] (float value)
                  {
                      normalised = parameter.convertTo0to1 (value);
                      refreshText();
                      repaint();
                  }),
      bipolar (isBipolar)
{
    setTooltip (tooltip);
    setRepaintsOnMouseActivity (false);

    valueLabel.setFont (theme::font (theme::fontBody));
    valueLabel.setJustificationType (juce::Justification::centredLeft);
    valueLabel.setColour (juce::Label::textColourId, theme::text);
    valueLabel.setEditable (true, true, false);
    valueLabel.setTooltip (tooltip + " Click to type a value.");
    valueLabel.setBorderSize ({ 0, 0, 0, 0 });
    valueLabel.setMinimumHorizontalScale (0.9f);

    valueLabel.onEditorShow = [this]
    {
        if (auto* editor = valueLabel.getCurrentTextEditor())
        {
            editor->setFont (theme::font (theme::fontBody));
            editor->setText (parameter.getText (parameter.getValue(), 0), false);
            editor->selectAll();
        }
    };

    valueLabel.onTextChange = [this]
    {
        const float typed = parameter.getValueForText (valueLabel.getText());
        attachment.setValueAsCompleteGesture (parameter.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, typed)));
        refreshText();
    };

    addAndMakeVisible (valueLabel);
    attachment.sendInitialUpdate();
}

void ParamControl::refreshText()
{
    // English typography: no space before the percent sign, one before other units.
    const auto unit = parameter.getLabel();
    valueLabel.setText (parameter.getText (parameter.getValue(), 0) + (unit == "%" ? "" : " ") + unit,
                        juce::dontSendNotification);
}

juce::Rectangle<int> ParamControl::barBounds() const
{
    return getLocalBounds().removeFromBottom (12).withSizeKeepingCentre (getWidth(), barHeight);
}

void ParamControl::resized()
{
    valueLabel.setBounds (0, nameHeight, getWidth(), valueHeight);
}

void ParamControl::paint (juce::Graphics& g)
{
    const bool active = dragging || isMouseOver (true);

    g.setFont (theme::font (theme::fontSmall));
    g.setColour (active ? theme::text : theme::textDim);
    g.drawText (parameter.getName (64), 0, 0, getWidth(), nameHeight, juce::Justification::centredLeft, true);

    const auto bar = barBounds().toFloat();
    g.setColour (theme::border);
    g.fillRoundedRectangle (bar, 2.0f);

    const float x = bar.getX() + bar.getWidth() * normalised;
    const float origin = bipolar ? bar.getCentreX() : bar.getX();
    g.setColour (theme::accent);
    g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (origin, x), bar.getY(),
                                                    std::abs (x - origin), bar.getHeight()), 2.0f);

    g.setColour (theme::text);
    g.fillRect (juce::Rectangle<float> (x - 1.0f, bar.getCentreY() - 6.0f, 2.0f, 12.0f));
}

void ParamControl::setNormalised (float newValue)
{
    attachment.setValueAsPartOfGesture (parameter.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, newValue)));
}

void ParamControl::mouseDown (const juce::MouseEvent& event)
{
    dragging = true;
    dragValue = normalised;
    lastDragX = event.getPosition().x;
    attachment.beginGesture();
    repaint();
}

void ParamControl::mouseDrag (const juce::MouseEvent& event)
{
    if (! dragging)
        return;

    // Incremental, so Shift can be pressed or released in the middle of a drag.
    const int x = event.getPosition().x;
    const float step = (float) (x - lastDragX) / (float) juce::jmax (1, getWidth());
    lastDragX = x;
    dragValue = juce::jlimit (0.0f, 1.0f, dragValue + step * (event.mods.isShiftDown() ? fineFactor : 1.0f));
    setNormalised (dragValue);
}

void ParamControl::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
    {
        dragging = false;
        attachment.endGesture();
        repaint();
    }
}

void ParamControl::mouseDoubleClick (const juce::MouseEvent&)
{
    const float defaultValue = parameter.convertFrom0to1 (parameter.getDefaultValue());

    // The second press of the double-click has already opened a gesture.
    if (dragging)
    {
        dragValue = parameter.getDefaultValue();
        attachment.setValueAsPartOfGesture (defaultValue);
    }
    else
    {
        attachment.setValueAsCompleteGesture (defaultValue);
    }
}

void ParamControl::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (dragging || wheel.deltaY == 0.0f)
        return;

    const float step = (wheel.deltaY > 0.0f ? 0.02f : -0.02f) * (event.mods.isShiftDown() ? fineFactor : 1.0f);
    attachment.setValueAsCompleteGesture (parameter.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, normalised + step)));
}
