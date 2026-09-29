#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/PeakMeter.h"

namespace srd
{
    //==============================================================================
    /** A narrow peak meter with a clip light on top. It rises at once and falls smoothly,
        and the clip light holds for a moment. */
    class LevelMeter : public juce::Component,
                       private juce::Timer
    {
    public:
        enum ColourIds
        {
            backgroundColourId = 0x5d10300,
            outlineColourId,
            barColourId,
            clipColourId
        };

        explicit LevelMeter (PeakMeter& source) : meter (source)
        {
            meter.readAndReset();   // drop anything played while no meter was showing
            startTimerHz (updateHz);
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced (0.5f);
            const auto light = bounds.removeFromTop (6.0f);
            bounds.removeFromTop (3.0f);

            g.setColour (findColour (clipTicks > 0 ? clipColourId : outlineColourId));
            g.fillRoundedRectangle (light, 2.0f);

            g.setColour (findColour (backgroundColourId));
            g.fillRoundedRectangle (bounds, 2.0f);

            const auto fraction = juce::jmap (levelDb, floorDb, 0.0f, 0.0f, 1.0f);
            auto bar = bounds.reduced (1.0f);
            g.setColour (findColour (barColourId));
            g.fillRoundedRectangle (bar.removeFromBottom (bar.getHeight() * fraction), 1.5f);

            g.setColour (findColour (outlineColourId));
            g.drawRoundedRectangle (bounds, 2.0f, 1.0f);
        }

    private:
        static constexpr int updateHz = 30;
        static constexpr int clipHoldTicks = 2 * updateHz;
        static constexpr float floorDb = -60.0f;
        static constexpr float fallDbPerTick = 20.0f / updateHz;

        PeakMeter& meter;
        float levelDb = floorDb;
        int clipTicks = 0;

        void timerCallback() override
        {
            const auto peak = meter.readAndReset();
            const auto wasClipping = clipTicks > 0;

            if (peak > 1.0f)
                clipTicks = clipHoldTicks;
            else if (clipTicks > 0)
                --clipTicks;

            // Capped at the top of the scale, so even an infinite peak can still fall away
            const auto newLevel = juce::jmin (0.0f, std::max (juce::Decibels::gainToDecibels (peak, floorDb),
                                                              levelDb - fallDbPerTick));

            if (! juce::approximatelyEqual (newLevel, levelDb) || wasClipping != (clipTicks > 0))
            {
                levelDb = newLevel;
                repaint();
            }
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
    };
}
