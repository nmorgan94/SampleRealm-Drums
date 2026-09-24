#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace srd
{
    //==============================================================================
    /** A pad that fires on mouse-down and flashes on each hit. */
    class TriggerPad : public juce::Component,
                       private juce::Timer
    {
    public:
        enum ColourIds
        {
            backgroundColourId = 0x5d10200,
            flashColourId,
            textColourId
        };

        explicit TriggerPad (juce::String padText) : text (std::move (padText))
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        std::function<void()> onTrigger;

        void setFont (juce::Font newFont)   { font = std::move (newFont); repaint(); }

        /** Call on each hit, including ones from MIDI. */
        void flash()
        {
            brightness = 1.0f;
            startTimerHz (60);
            repaint();
        }

        void mouseDown (const juce::MouseEvent&) override
        {
            if (onTrigger != nullptr)
                onTrigger();
        }

        void paint (juce::Graphics& g) override
        {
            const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
            const auto background = findColour (backgroundColourId);
            const auto flashColour = findColour (flashColourId);

            g.setColour (background.interpolatedWith (flashColour, brightness * 0.8f));
            g.fillRoundedRectangle (bounds, 6.0f);
            g.setColour (flashColour.withAlpha (0.4f + 0.6f * brightness));
            g.drawRoundedRectangle (bounds, 6.0f, 1.5f);

            g.setColour (findColour (textColourId));
            g.setFont (font);
            g.drawText (text, bounds, juce::Justification::centred);
        }

    private:
        juce::String text;
        juce::Font font { juce::FontOptions (16.0f, juce::Font::bold) };
        float brightness = 0.0f;

        void timerCallback() override
        {
            brightness *= 0.85f;

            if (brightness < 0.01f)
            {
                brightness = 0.0f;
                stopTimer();
            }

            repaint();
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TriggerPad)
    };
}
