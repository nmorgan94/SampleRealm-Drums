#pragma once

#include "CustomLookAndFeel.h"
#include "LevelMeter.h"

namespace srd
{
    //==============================================================================
    /**
     * The note and frequency a hit's pitch settles on, with the output meter to its right.
     */
    class TailMeter : public juce::Component
    {
    public:
        enum ColourIds
        {
            labelColourId = 0x5d10600,
            noteColourId
        };

        static constexpr int meterWidth = 14, gap = 10, readoutWidth = 150;
        static constexpr int preferredWidth = readoutWidth + gap + meterWidth;

        explicit TailMeter (PeakMeter& source) : meter (source)
        {
            addAndMakeVisible (meter);
        }

        /** Empty texts hide the readout. */
        void setTail (const juce::String& newNote, const juce::String& newHz)
        {
            if (newNote == note && newHz == hz)
                return;

            note = newNote;
            hz = newHz;
            repaint (readoutArea);
        }

        void paint (juce::Graphics& g) override
        {
            if (note.isEmpty() || ! g.clipRegionIntersects (readoutArea))
                return;

            auto readout = readoutArea;
            g.setColour (findColour (labelColourId));
            g.setFont (CustomLookAndFeel::font (11.0f, true));
            g.drawText ("TAIL", readout.removeFromTop (16), juce::Justification::centredRight);
            g.setColour (findColour (noteColourId));
            g.setFont (CustomLookAndFeel::font (18.0f, true));
            g.drawText (note, readout.removeFromTop (22), juce::Justification::centredRight);
            g.setColour (findColour (labelColourId));
            g.setFont (CustomLookAndFeel::font (11.0f));
            g.drawText (hz, readout, juce::Justification::centredRight);
        }

        void resized() override
        {
            auto bounds = getLocalBounds();
            meter.setBounds (bounds.removeFromRight (meterWidth).withSizeKeepingCentre (meterWidth, 40));
            bounds.removeFromRight (gap);
            readoutArea = bounds;
        }

    private:
        LevelMeter meter;
        juce::String note, hz;
        juce::Rectangle<int> readoutArea;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TailMeter)
    };
}
