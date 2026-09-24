#pragma once

#include <juce_dsp/juce_dsp.h>

namespace srd
{
    //==============================================================================
    /**
     * Low shelf, sweepable peak and high shelf. Mono. Only the band whose setting changed
     * is recomputed, and coefficients are written in place, so updates never allocate.
     */
    class ThreeBandEQ
    {
    public:
        struct Settings
        {
            float lowGainDb  = 0.0f;
            float midFreqHz  = 400.0f;
            float midGainDb  = 0.0f;
            float highGainDb = 0.0f;
        };

        void prepare (double newSampleRate)
        {
            sampleRate = newSampleRate;

            for (auto* f : { &low, &mid, &high })
                f->coefficients = new juce::dsp::IIR::Coefficients<float> (1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);

            updateLow();
            updateMid();
            updateHigh();
            reset();
        }

        void reset() noexcept
        {
            low.reset();
            mid.reset();
            high.reset();
        }

        // Exact comparisons on purpose: they only detect whether a band's settings changed
        void setSettings (const Settings& s) noexcept
        {
            const auto lowChanged  = ! juce::exactlyEqual (s.lowGainDb, current.lowGainDb);
            const auto midChanged  = ! juce::exactlyEqual (s.midFreqHz, current.midFreqHz)
                                  || ! juce::exactlyEqual (s.midGainDb, current.midGainDb);
            const auto highChanged = ! juce::exactlyEqual (s.highGainDb, current.highGainDb);

            current = s;

            if (lowChanged)  updateLow();
            if (midChanged)  updateMid();
            if (highChanged) updateHigh();
        }

        // Always filters: at 0 dB the bands are unity, so there's no bypass to switch in and out of
        void process (float* samples, int numSamples) noexcept
        {
            for (int i = 0; i < numSamples; ++i)
                samples[i] = high.processSample (mid.processSample (low.processSample (samples[i])));
        }

    private:
        using Array = juce::dsp::IIR::ArrayCoefficients<float>;

        static constexpr float lowShelfHz  = 80.0f;
        static constexpr float highShelfHz = 6000.0f;
        static constexpr float shelfQ      = 0.707f;
        static constexpr float midQ        = 1.0f;

        double sampleRate = 44100.0;
        Settings current;
        juce::dsp::IIR::Filter<float> low, mid, high;

        float maxFrequency() const noexcept     { return (float) sampleRate * 0.45f; }
        static float gain (float db) noexcept   { return juce::Decibels::decibelsToGain (db); }

        void updateLow() noexcept
        {
            *low.coefficients = Array::makeLowShelf (sampleRate, lowShelfHz, shelfQ, gain (current.lowGainDb));
        }

        void updateMid() noexcept
        {
            const auto frequency = juce::jlimit (20.0f, maxFrequency(), current.midFreqHz);
            *mid.coefficients = Array::makePeakFilter (sampleRate, frequency, midQ, gain (current.midGainDb));
        }

        void updateHigh() noexcept
        {
            *high.coefficients = Array::makeHighShelf (sampleRate, juce::jmin (highShelfHz, maxFrequency()), shelfQ, gain (current.highGainDb));
        }
    };
}
