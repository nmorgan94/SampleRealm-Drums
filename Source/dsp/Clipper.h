#pragma once

#include <cmath>
#include "SmoothedShaper.h"

namespace srd
{
    //==============================================================================
    /**
     * Soft-knee clipper with a ceiling of ±1. Amount 0..1 pushes up to +18 dB into it.
     * At 0 it's bypassed; turning it on or off crossfades rather than switching.
     */
    class Clipper
    {
    public:
        void prepare (double sampleRate) noexcept   { shaper.prepare (sampleRate); }
        void reset() noexcept                       { shaper.reset(); }

        void setAmount (float amount) noexcept
        {
            amount = juce::jlimit (0.0f, 1.0f, amount);
            shaper.setTargets (juce::Decibels::decibelsToGain (amount * maxBoostDb), amount > 0.0f ? 1.0f : 0.0f);
        }

        static float clip (float x) noexcept
        {
            const auto magnitude = std::abs (x);

            if (magnitude <= knee)
                return x;

            const auto shaped = knee + (1.0f - knee) * std::tanh ((magnitude - knee) / (1.0f - knee));
            return std::copysign (shaped, x);
        }

        void process (float* samples, int numSamples) noexcept   { shaper.process (samples, numSamples, clip); }

    private:
        static constexpr float knee = 0.9f;
        static constexpr float maxBoostDb = 18.0f;

        SmoothedShaper shaper;
    };
}
