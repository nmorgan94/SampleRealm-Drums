#pragma once

#include "PresetActions.h"
#include "PresetBar.h"

namespace srd
{
    //==============================================================================
    /**
     * The strip along the top of the editor: the title, a switch, the preset bar, the menu and a meter.
     * Mirrored by webui/src/HeaderBar.tsx.
     */
    class HeaderBar : public juce::Component
    {
    public:
        enum ColourIds
        {
            backgroundColourId = 0x5d10500,
            titleColourId,
            subtitleColourId
        };

        /** Small above large, as HeaderTitle in webui/src/HeaderBar.tsx. */
        struct Title
        {
            juce::String small, large;
        };

        /** Each plugin's sizes, as HeaderLayout in webui/src/HeaderBar.tsx. */
        struct Layout
        {
            int height;
            int edge;           // left of the title, right of the meter
            int titleWidth;
            float titleHeight;  // the large line
            int switchWidth;    // right after the title
            int gap;            // either side of the preset bar and menu
            int controlsHeight; // the switch too
            int menuWidth;
            int menuGap;        // between the preset bar and the menu
            int meterWidth;
            int meterInset;     // above and below the title and the meter
        };

        HeaderBar (const Title&, const Layout&, juce::Component& switchToShow, PresetManager&, PromptOverlay&,
                   std::function<void()> showManual, const juce::String& aboutText, juce::Component& meter);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        const Title title;
        const Layout layout;
        PromptOverlay& prompt;
        const std::function<void()> showManual;
        const juce::String aboutText;

        juce::Component& switchControl;
        PresetActions presetActions;
        PresetBar presetBar;
        juce::TextButton menuButton { "..." };
        juce::Component& meter;

        void showMenu();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
    };
}
