#pragma once

#include "CustomLookAndFeel.h"

namespace srd
{
    //==============================================================================
    /**
     * The header's title, small above large, with a switch to its right. Given to the shared HeaderBar
     * as its title. Mirrored by HeaderTitle in webui/src/Editor.tsx.
     */
    class HeaderTitle : public juce::Component
    {
    public:
        enum ColourIds
        {
            titleColourId = 0x5d10700,
            subtitleColourId
        };

        /** As TITLE_LAYOUT in webui/src/Editor.tsx. */
        struct Layout
        {
            int textWidth;
            float largeHeight;  // the large line's font
            int inset;          // above and below the text
            int switchWidth;
            int switchHeight;

            constexpr int width() const   { return textWidth + switchWidth; }
        };

        HeaderTitle (const juce::String& smallText, const juce::String& largeText, const Layout& l, juce::Component& switchToShow)
            : small (smallText), large (largeText), layout (l), switchControl (switchToShow)
        {
            // For screen readers, as the title is painted
            setTitle (small + " " + large);

            addAndMakeVisible (switchControl);
        }

        void paint (juce::Graphics& g) override
        {
            auto text = getLocalBounds().withWidth (layout.textWidth).reduced (0, layout.inset);

            // Painted, so the colours come from the LookAndFeel
            g.setColour (findColour (subtitleColourId));
            g.setFont (CustomLookAndFeel::font (smallFontHeight, true));
            g.drawText (small, text.removeFromTop (smallLineHeight), juce::Justification::bottomLeft);
            g.setColour (findColour (titleColourId));
            g.setFont (CustomLookAndFeel::font (layout.largeHeight, true));
            g.drawText (large, text, juce::Justification::centredLeft);
        }

        void resized() override
        {
            switchControl.setBounds (getLocalBounds().withTrimmedLeft (layout.textWidth)
                                         .withSizeKeepingCentre (layout.switchWidth, layout.switchHeight));
        }

    private:
        static constexpr int smallLineHeight = 16;
        static constexpr float smallFontHeight = 11.0f;

        const juce::String small, large;
        const Layout layout;
        juce::Component& switchControl;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderTitle)
    };
}
