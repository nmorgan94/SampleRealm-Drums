#pragma once

#include <array>
#include <juce_dsp/juce_dsp.h>
#include "SquareOscillator.h"

namespace srd
{
    //==============================================================================
    /**
     * Six squares at the 808's inharmonic ratios through a band-pass: the metallic
     * core of hats, rides and cowbells.
     */
    class MetalOscillator
    {
    public:
        void prepare (double sampleRate)
        {
            for (auto& oscillator : oscillators)
                oscillator.prepare (sampleRate);

            maxToneHz = 0.45f * (float) sampleRate;
            bandPass.prepare ({ sampleRate, 1, 1 });
            bandPass.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        }

        /** Scales every oscillator, keeping the ratios between them. */
        void setFrequencyRatio (float newRatio) noexcept     { ratio = newRatio; }

        /** ring runs 0..1, from a broad band to a narrow, ringing one. */
        void setTone (float toneHz, float ring) noexcept
        {
            const auto resonance = juce::jmap (juce::jlimit (0.0f, 1.0f, ring), 0.5f, 6.0f);
            bandPass.setCutoffFrequency (juce::jlimit (20.0f, maxToneHz, toneHz));
            bandPass.setResonance (resonance);
            bandGain = makeUpGain / std::sqrt (resonance);
        }

        /** Restarts every oscillator from the same phases, so every hit sounds alike. */
        void reset() noexcept
        {
            for (std::size_t i = 0; i < oscillators.size(); ++i)
                oscillators[i].reset (startPhases[i]);

            bandPass.reset();
        }

        float next() noexcept
        {
            auto sum = 0.0f;

            for (std::size_t i = 0; i < oscillators.size(); ++i)
                sum += oscillators[i].next (frequencies[i] * ratio);

            return bandPass.processSample (0, sum) * bandGain;
        }

    private:
        static constexpr std::array frequencies { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
        static constexpr std::array startPhases { 0.0f, 0.37f, 0.71f, 0.13f, 0.52f, 0.89f };

        // Scales the six squares' sum (up to ±6). The band-pass removes most of their energy, so this is
        // well above 1/6: it puts the metal at 0 dB near the level of the noise layer at 0 dB
        static constexpr float makeUpGain = 0.4f;

        std::array<SquareOscillator, frequencies.size()> oscillators;
        juce::dsp::StateVariableTPTFilter<float> bandPass;
        float ratio = 1.0f, bandGain = 1.0f, maxToneHz = 19800.0f;
    };
}
