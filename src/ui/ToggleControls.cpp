#include "ui/ToggleControls.h"

#include "ui/Theme.h"

namespace
{
    void drawButton (juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& text,
                     bool filled, bool hovered)
    {
        bounds = bounds.reduced (0.5f);

        g.setColour (filled ? theme::accent : (hovered ? theme::control : theme::panel));
        g.fillRoundedRectangle (bounds, theme::cornerRadius);

        g.setColour (filled ? theme::accent : theme::border);
        g.drawRoundedRectangle (bounds, theme::cornerRadius, 1.0f);

        g.setColour (filled ? theme::onAccent : theme::text);
        g.setFont (theme::font (theme::fontBody, filled));
        g.drawText (text, bounds, juce::Justification::centred, false);
    }
}

// --- SegmentedControl -------------------------------------------------------

SegmentedControl::SegmentedControl (juce::AudioProcessorValueTreeState& state, const juce::String& parameterId,
                                    const juce::String& tooltip)
    : parameter (*state.getParameter (parameterId)),
      attachment (parameter, [this] (float value)
                  {
                      selected = juce::roundToInt (value);
                      repaint();
                  }),
      items (parameter.getAllValueStrings())
{
    setTooltip (tooltip);
    attachment.sendInitialUpdate();
}

void SegmentedControl::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const int count = juce::jmax (1, items.size());
    const float segmentWidth = bounds.getWidth() / (float) count;

    g.setColour (theme::panel);
    g.fillRoundedRectangle (bounds, theme::cornerRadius);

    for (int i = 0; i < count; ++i)
    {
        const auto segment = juce::Rectangle<float> (bounds.getX() + segmentWidth * (float) i, bounds.getY(),
                                                     segmentWidth, bounds.getHeight());
        const bool isSelected = i == selected;

        if (isSelected)
        {
            g.setColour (theme::accent);
            g.fillRoundedRectangle (segment.reduced (2.0f), theme::cornerRadius - 1.0f);
        }

        g.setColour (isSelected ? theme::onAccent : theme::text);
        g.setFont (theme::font (theme::fontBody, isSelected));
        g.drawText (items[i], segment, juce::Justification::centred, false);
    }

    g.setColour (theme::border);
    g.drawRoundedRectangle (bounds, theme::cornerRadius, 1.0f);
}

void SegmentedControl::mouseUp (const juce::MouseEvent& event)
{
    if (! getLocalBounds().contains (event.getPosition()) || items.isEmpty())
        return;

    const int index = juce::jlimit (0, items.size() - 1, event.getPosition().x * items.size() / juce::jmax (1, getWidth()));

    if (index != selected)
        attachment.setValueAsCompleteGesture ((float) index);
}

// --- ParamToggle ------------------------------------------------------------

ParamToggle::ParamToggle (juce::AudioProcessorValueTreeState& state, const juce::String& parameterId,
                          const juce::String& tooltip)
    : parameter (*state.getParameter (parameterId)),
      attachment (parameter, [this] (float value)
                  {
                      on = value > 0.5f;
                      repaint();
                  })
{
    setTooltip (tooltip);
    attachment.sendInitialUpdate();
}

void ParamToggle::paint (juce::Graphics& g)
{
    drawButton (g, getLocalBounds().toFloat(), parameter.getName (32), on, isMouseOver());
}

void ParamToggle::mouseUp (const juce::MouseEvent& event)
{
    if (getLocalBounds().contains (event.getPosition()))
        attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}

// --- MenuButton -------------------------------------------------------------

MenuButton::MenuButton (const juce::String& tooltip)
{
    setTooltip (tooltip);
}

void MenuButton::setText (const juce::String& newText)
{
    text = newText;
    repaint();
}

void MenuButton::paint (juce::Graphics& g)
{
    drawButton (g, getLocalBounds().toFloat(), text, false, isMouseOver());
}

void MenuButton::mouseUp (const juce::MouseEvent& event)
{
    if (getLocalBounds().contains (event.getPosition()) && onClick != nullptr)
        onClick();
}
