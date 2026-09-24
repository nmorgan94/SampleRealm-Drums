#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace srd
{
    //==============================================================================
    /** A rendered clip drawn as a filled min/max outline. */
    class Waveform
    {
    public:
        void setSamples (const float* data, int numSamples, double newSampleRate)
        {
            samples.clearQuick();
            samples.addArray (data, numSamples);
            sampleRate = newSampleRate;
            outlineSeconds = 0.0f; // rebuild on the next draw
        }

        /** Draws the first `seconds` of the clip across the area, one column per pixel, at a fixed ±1 scale. */
        void draw (juce::Graphics& g, juce::Rectangle<float> area, float seconds, juce::Colour colour) const
        {
            if (samples.isEmpty() || area.isEmpty() || seconds <= 0.0f)
                return;

            // Owners often repaint for other reasons (e.g. dragging over it), so the outline is cached
            if (area != outlineArea || ! juce::approximatelyEqual (seconds, outlineSeconds))
                buildOutline (area, seconds);

            g.setColour (colour);
            g.fillPath (outline);
        }

    private:
        juce::Array<float> samples;
        double sampleRate = 44100.0;

        mutable juce::Path outline;
        mutable juce::Rectangle<float> outlineArea;
        mutable float outlineSeconds = 0.0f;

        void buildOutline (juce::Rectangle<float> area, float seconds) const
        {
            outline.clear();
            outlineArea = area;
            outlineSeconds = seconds;

            const auto samplesPerPixel = seconds * sampleRate / area.getWidth();
            const auto centreY = area.getCentreY();
            const auto halfHeight = area.getHeight() * 0.5f;
            const auto toY = [&] (float sample) { return centreY - juce::jlimit (-1.0f, 1.0f, sample) * halfHeight; };

            juce::Array<juce::Point<float>> lower;

            for (auto x = 0.0f; x < area.getWidth(); x += 1.0f)
            {
                const auto start = juce::roundToInt (x * samplesPerPixel);
                const auto end   = std::min (samples.size(), std::max (start + 1, juce::roundToInt ((x + 1.0f) * samplesPerPixel)));

                if (start >= samples.size())
                    break;

                const auto range = juce::FloatVectorOperations::findMinAndMax (samples.begin() + start, end - start);
                const juce::Point<float> upper { area.getX() + x, toY (range.getEnd()) };

                if (lower.isEmpty())
                    outline.startNewSubPath (upper);
                else
                    outline.lineTo (upper);

                lower.add ({ upper.x, toY (range.getStart()) });
            }

            // Back along the lower edge to close the shape
            for (auto i = lower.size(); --i >= 0;)
                outline.lineTo (lower.getReference (i));

            outline.closeSubPath();
        }
    };
}
