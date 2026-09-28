#pragma once

#include <cmath>
#include <juce_core/juce_core.h>

namespace srd
{
    //==============================================================================
    /**
     * Square wave with PolyBLEP-smoothed edges, so it stays clean at high pitches.
     * Frequencies are held below 0.45 × sample rate.
     */
    class SquareOscillator
    {
    public:
        void prepare (double newSampleRate) noexcept    { sampleRate = (float) newSampleRate; }

        /** Restarts the cycle. startPhase is 0..1 of a cycle. */
        void reset (float startPhase) noexcept          { phase = startPhase - std::floor (startPhase); }

        float next (float frequencyHz) noexcept
        {
            const auto increment = juce::jlimit (0.0f, 0.45f, frequencyHz / sampleRate);

            auto halfCycleLater = phase + 0.5f;
            halfCycleLater -= std::floor (halfCycleLater);

            const auto out = (phase < 0.5f ? 1.0f : -1.0f) + polyBlep (phase, increment) - polyBlep (halfCycleLater, increment);

            phase += increment;
            phase -= std::floor (phase);
            return out;
        }

    private:
        float sampleRate = 44100.0f;
        float phase = 0.0f;

        /** Correction for a unit step at phase 0, spread over the samples either side of it. */
        static float polyBlep (float t, float increment) noexcept
        {
            if (t < increment)
            {
                t /= increment;
                return t + t - t * t - 1.0f;
            }

            if (t > 1.0f - increment)
            {
                t = (t - 1.0f) / increment;
                return t * t + t + t + 1.0f;
            }

            return 0.0f;
        }
    };
}
