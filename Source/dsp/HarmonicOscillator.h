#pragma once

#include <cmath>
#include <juce_core/juce_core.h>

namespace srd
{
    //==============================================================================
    /**
     * Sine oscillator with a blend of 2nd and 3rd harmonics and a settable start phase.
     * Harmonics above 0.45 × sample rate are dropped, so it never aliases even when
     * a pitch envelope starts very high.
     */
    class HarmonicOscillator
    {
    public:
        void prepare (double newSampleRate) noexcept    { sampleRate = (float) newSampleRate; }

        /** Restarts the cycle. startPhase is 0..1 of a cycle (0.25 starts at the peak). */
        void reset (float startPhase) noexcept          { phase = startPhase - std::floor (startPhase); }

        /** 0 = pure sine, 1 = full harmonic blend. */
        void setHarmonics (float amount) noexcept       { harmonics = juce::jlimit (0.0f, 1.0f, amount); }

        float next (float frequencyHz) noexcept
        {
            const auto nyquistLimit = 0.45f * sampleRate;
            frequencyHz = juce::jlimit (0.0f, nyquistLimit, frequencyHz);

            constexpr auto twoPi = juce::MathConstants<float>::twoPi;
            auto out = std::sin (twoPi * phase);

            if (harmonics > 0.0f)
            {
                const auto h2 = 2.0f * frequencyHz < nyquistLimit ? 0.5f * std::sin (2.0f * twoPi * phase) : 0.0f;
                const auto h3 = 3.0f * frequencyHz < nyquistLimit ? 0.3f * std::sin (3.0f * twoPi * phase) : 0.0f;
                out = (out + harmonics * (h2 + h3)) / (1.0f + 0.5f * harmonics);
            }

            phase += frequencyHz / sampleRate;
            phase -= std::floor (phase);
            return out;
        }

    private:
        float sampleRate = 44100.0f;
        float phase = 0.0f;
        float harmonics = 0.0f;
    };
}
