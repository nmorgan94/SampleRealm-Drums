#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace srd
{
    //==============================================================================
    /**
     * A pad that fires on mouse-down, harder the higher it's pressed, and flashes on each hit.
     * A number shows the last hit's velocity. A bar up its left side fills to the hit's level, with a tick at the velocity.
     */
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

        /** velocity runs from 1/127 to 1. */
        std::function<void (float velocity)> onTrigger;

        void setFont (juce::Font newFont)   { font = std::move (newFont); repaint(); }

        /** Call on each hit, including ones from MIDI. Both run 0..1: a softer hit flashes dimmer. */
        void flash (float level, float velocity)
        {
            brightness = juce::jlimit (minFlash, 1.0f, level);
            shownLevel = juce::jlimit (0.0f, 1.0f, level);
            shownVelocity = juce::jlimit (0.0f, 1.0f, velocity);
            startTimerHz (60);
            repaint();
        }

        /** The velocity a press at this height plays. The top and bottom edges give the two ends. */
        static float velocityAt (float y, float height) noexcept
        {
            const auto span = height - 2.0f * velocityInset;
            return juce::jlimit (1.0f / 127.0f, 1.0f, (height - velocityInset - y) / span);
        }

        void mouseDown (const juce::MouseEvent& event) override
        {
            if (onTrigger != nullptr)
                onTrigger (velocityAt (event.position.y, (float) getHeight()));
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

            const auto textColour = findColour (textColourId);

            g.setColour (textColour);
            g.setFont (font);
            g.drawText (text, bounds, juce::Justification::centred);

            if (shownVelocity > 0.0f)
            {
                auto inner = getLocalBounds().toFloat().reduced (velocityInset);
                const auto track = inner.withWidth (barWidth);

                g.setColour (flashColour.withAlpha (0.25f));
                g.fillRoundedRectangle (track, barWidth / 2.0f);
                g.setColour (flashColour);
                g.fillRoundedRectangle (track.withTop (track.getBottom() - shownLevel * track.getHeight()), barWidth / 2.0f);

                g.setColour (textColour);
                g.fillRect (track.withY (track.getBottom() - shownVelocity * track.getHeight())
                                 .withHeight (tickHeight)
                                 .expanded (tickOverhang, 0.0f));

                g.setColour (textColour.withAlpha (0.7f));
                g.setFont (font.withHeight (numberHeight));
                g.drawText (juce::String (juce::roundToInt (shownVelocity * 127.0f)),
                            inner.removeFromBottom (numberHeight), juce::Justification::centred);
            }
        }

    private:
        juce::String text;
        juce::Font font { juce::FontOptions (16.0f, juce::Font::bold) };
        float brightness = 0.0f;

        // 0 until the first hit
        float shownVelocity = 0.0f, shownLevel = 0.0f;

        // The quietest hit still shows
        static constexpr float minFlash = 0.25f;

        static constexpr float velocityInset = 8.0f, barWidth = 3.0f, numberHeight = 11.0f;
        static constexpr float tickHeight = 1.5f, tickOverhang = 2.0f;

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
