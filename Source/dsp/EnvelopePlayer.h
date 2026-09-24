#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "BreakpointEnvelope.h"

namespace srd
{
    //==============================================================================
    /**
     * Plays a breakpoint envelope in real time, stretched by a length scale. Past the
     * last node it holds the final value and ramps down, so an envelope that doesn't
     * end at zero still finishes without a click.
     */
    class EnvelopePlayer
    {
    public:
        void prepare (double newSampleRate, float releaseSeconds)
        {
            sampleRate = newSampleRate;
            release.reset (newSampleRate, releaseSeconds);
            active = false;
        }

        /** gain scales the output; at 0 or below the envelope doesn't play. */
        void start (const EnvelopeData& envelope, float lengthScale, float newGain = 1.0f) noexcept
        {
            data = envelope;
            duration = data.getDuration();
            gain = newGain;
            cursor.reset();
            sampleIndex = 0;
            secondsPerSample = 1.0f / ((float) sampleRate * juce::jmax (0.01f, lengthScale));
            release.setCurrentAndTargetValue (1.0f);
            active = data.numNodes > 0 && gain > 0.0f;
        }

        void stop() noexcept                    { active = false; }
        bool isActive() const noexcept          { return active; }

        /** Where the next sample falls on the envelope as drawn, in seconds; Length changes how fast this advances. */
        float getTime() const noexcept          { return (float) sampleIndex * secondsPerSample; }

        float next() noexcept
        {
            if (! active)
                return 0.0f;

            const auto t = getTime();

            if (t > duration)
                release.setTargetValue (0.0f);

            const auto out = cursor.getValue (data, t) * release.getNextValue() * gain;
            ++sampleIndex;
            active = release.isSmoothing() || release.getTargetValue() > 0.0f;
            return out;
        }

    private:
        EnvelopeData data;
        EnvelopeCursor cursor;
        juce::SmoothedValue<float> release { 1.0f };

        double sampleRate = 44100.0;
        juce::int64 sampleIndex = 0;
        float secondsPerSample = 0.0f, duration = 0.0f, gain = 1.0f;
        bool active = false;
    };
}
