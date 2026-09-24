#pragma once

#include <juce_dsp/juce_dsp.h>
#include "HarmonicOscillator.h"

namespace srd
{
    //==============================================================================
    /**
     * Synthesised transient for the attack of a drum.
     *   Noise   - white noise through a band-pass at Tone
     *   Sweep   - a sine sweeping down from Pitch, three octaves over the decay
     *   Impulse - one square cycle at Tone: a short, bright tick
     * Each mode runs through a fast attack and an exponential decay to -60 dB.
     */
    class ClickGenerator
    {
    public:
        enum class Type { noise, sweep, impulse };

        /** Display names, in the same order as Type. */
        static juce::StringArray getTypeNames()     { return { "Noise", "Sweep", "Impulse" }; }

        /** Maps a choice-parameter index (in getTypeNames() order) to a Type. */
        static Type typeFromIndex (int index) noexcept
        {
            switch (index)
            {
                case 1:  return Type::sweep;
                case 2:  return Type::impulse;
                default: return Type::noise;
            }
        }

        struct Settings
        {
            Type  type     = Type::noise;
            float gain     = 0.25f;  // linear
            float toneHz   = 4000.0f;
            float decayMs  = 12.0f;  // time to -60 dB
            float pitchHz  = 2500.0f;
        };

        /** How long a click with these settings sounds before it goes silent. */
        static float getDurationSeconds (const Settings& s) noexcept
        {
            // decayMs is the time to -60 dB; the click runs on until it reaches `silence`
            return s.gain > 0.0f ? s.decayMs * 0.001f * std::log (silence) / std::log (0.001f) : 0.0f;
        }

        void prepare (double sampleRate);
        void trigger (const Settings&) noexcept;
        void stop() noexcept                { active = false; }

        bool isActive() const noexcept      { return active; }
        float next() noexcept;

    private:
        float sampleRate = 44100.0f;
        Settings settings;
        bool active = false;

        juce::dsp::StateVariableTPTFilter<float> filter;
        HarmonicOscillator sweep;
        juce::Random random;

        int   sampleIndex = 0;
        float envelope = 0.0f, decayCoefficient = 0.0f;
        float sweepHz = 0.0f, sweepCoefficient = 1.0f;
        int   impulseHalfPeriod = 1;

        static constexpr int attackSamples = 16;
        static constexpr float silence = 1.0e-4f;
    };
}
