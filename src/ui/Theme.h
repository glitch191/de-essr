#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Colours, fonts and metrics of the editor, all in one place.
    Sizes are logical pixels at 100 %; the editor scale is applied on top.
*/
namespace theme
{
    // Neutral dark palette with a single accent.
    inline const juce::Colour background { 0xff17191c };
    inline const juce::Colour panel { 0xff1f2226 };
    inline const juce::Colour control { 0xff2a2e33 };
    inline const juce::Colour border { 0xff3b4047 };
    inline const juce::Colour grid { 0xff2c3036 };
    inline const juce::Colour text { 0xffe7e9ec };
    inline const juce::Colour textDim { 0xffa6abb3 };
    inline const juce::Colour accent { 0xffdb9a3f };
    inline const juce::Colour onAccent { 0xff17191c };

    constexpr int padding = 12;
    constexpr int gap = 8;
    constexpr float cornerRadius = 4.0f;
    constexpr int controlHeight = 32;   // minimum height of a clickable target

    constexpr float fontSmall = 13.0f;  // secondary text, never smaller
    constexpr float fontBody = 15.0f;
    constexpr float fontTitle = 16.0f;

    juce::Font font (float size, bool bold = false);

    /** Flat look for the stock JUCE parts the editor still uses
        (popup menu, tooltip, text editor, corner resizer). */
    class LookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();

        juce::Font getPopupMenuFont() override;
        void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
        juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                               juce::Rectangle<int> parentArea) override;
        void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
        void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;
        void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
        void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    };
}
