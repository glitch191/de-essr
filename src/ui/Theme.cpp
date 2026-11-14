#include "ui/Theme.h"

namespace theme
{

namespace
{
    constexpr int tooltipMaxWidth = 320;
    constexpr int tooltipPadding = 8;

    juce::TextLayout layoutTooltip (const juce::String& tipText)
    {
        juce::AttributedString attributed;
        attributed.setJustification (juce::Justification::centredLeft);
        attributed.append (tipText, font (fontSmall), theme::text);

        juce::TextLayout layout;
        layout.createLayoutWithBalancedLineLengths (attributed, (float) tooltipMaxWidth);
        return layout;
    }
}

juce::Font font (float size, bool bold)
{
    return juce::Font (juce::FontOptions ("Segoe UI", size, bold ? juce::Font::bold : juce::Font::plain));
}

LookAndFeel::LookAndFeel()
{
    setDefaultSansSerifTypefaceName ("Segoe UI");

    setColour (juce::ResizableWindow::backgroundColourId, background);

    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, control);
    setColour (juce::PopupMenu::highlightedTextColourId, text);

    setColour (juce::Label::textColourId, text);
    setColour (juce::Label::textWhenEditingColourId, text);
    setColour (juce::Label::backgroundWhenEditingColourId, control);
    setColour (juce::Label::outlineWhenEditingColourId, accent);

    setColour (juce::TextEditor::backgroundColourId, control);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
    setColour (juce::TextEditor::highlightedTextColourId, text);
    setColour (juce::TextEditor::outlineColourId, border);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::CaretComponent::caretColourId, text);

    setColour (juce::TooltipWindow::backgroundColourId, panel);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, border);
}

juce::Font LookAndFeel::getPopupMenuFont()
{
    return font (fontBody);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (panel);
    g.setColour (border);
    g.drawRect (0, 0, width, height, 1);
}

juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                                    juce::Rectangle<int> parentArea)
{
    const auto layout = layoutTooltip (tipText);
    const int w = (int) std::ceil (layout.getWidth()) + 2 * tooltipPadding;
    const int h = (int) std::ceil (layout.getHeight()) + 2 * tooltipPadding;

    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 16,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 8) : screenPos.y + 16,
                                 w, h)
        .constrainedWithin (parentArea);
}

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& tipText, int width, int height)
{
    g.fillAll (panel);
    g.setColour (border);
    g.drawRect (0, 0, width, height, 1);

    layoutTooltip (tipText).draw (g, juce::Rectangle<float> ((float) tooltipPadding, (float) tooltipPadding,
                                                             (float) (width - 2 * tooltipPadding),
                                                             (float) (height - 2 * tooltipPadding)));
}

void LookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging)
{
    g.setColour (isMouseOver || isMouseDragging ? textDim : border);
    const auto fw = (float) w;
    const auto fh = (float) h;

    for (float offset : { 0.35f, 0.6f, 0.85f })
        g.drawLine (fw * offset, fh - 2.0f, fw - 2.0f, fh * offset, 1.0f);
}

void LookAndFeel::fillTextEditorBackground (juce::Graphics& g, int, int, juce::TextEditor&)
{
    g.fillAll (control);
}

void LookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    g.setColour (editor.hasKeyboardFocus (true) ? accent : border);
    g.drawRect (0, 0, width, height, 1);
}

} // namespace theme
