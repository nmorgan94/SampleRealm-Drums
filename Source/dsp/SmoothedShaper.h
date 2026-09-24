#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace srd
{
    //==============================================================================
    /**
     * Pushes a signal through a waveshaping function and mixes it with the dry signal.
     * Both the drive gain and the wet amount are smoothed; with no wet signal the loop
     * is skipped entirely.
     */
    class SmoothedShaper
    {
    public:
        void prepare (double sampleRate, double rampSeconds = 0.02) noexcept
        {
            gain.reset (sampleRate, rampSeconds);
            wet.reset (sampleRate, rampSeconds);
        }

        void reset() noexcept
        {
            gain.setCurrentAndTargetValue (gain.getTargetValue());
            wet.setCurrentAndTargetValue (wet.getTargetValue());
        }

        void setTargets (float driveGain, float wetAmount) noexcept
        {
            gain.setTargetValue (driveGain);
            wet.setTargetValue (wetAmount);
        }

        template <typename Index, typename Shape>
        void process (float* samples, Index numSamples, Shape&& shape) noexcept
        {
            if (wet.getTargetValue() <= 0.0f && ! wet.isSmoothing())
                return;

            for (Index i = 0; i < numSamples; ++i)
            {
                const auto dry = samples[i];
                const auto mix = wet.getNextValue();
                samples[i] = dry + mix * (shape (dry * gain.getNextValue()) - dry);
            }
        }

    private:
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> gain { 1.0f };
        juce::SmoothedValue<float> wet { 0.0f };
    };
}
