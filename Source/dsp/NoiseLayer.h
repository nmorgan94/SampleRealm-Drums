#pragma once

#include <juce_dsp/juce_dsp.h>

namespace srd
{
    //==============================================================================
    /**
     * White noise between a high-pass and a low-pass: snare wires, hats, air.
     */
    class NoiseLayer
    {
    public:
        void prepare (double sampleRate)
        {
            maxCutoffHz = 0.45f * (float) sampleRate;

            for (auto* filter : { &lowCut, &highCut })
                filter->prepare ({ sampleRate, 1, 1 });

            lowCut.setType (juce::dsp::StateVariableTPTFilterType::highpass);
            highCut.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
        }

        /** The high cut never goes below the low cut, so crossed settings leave a narrow band, not silence. */
        void setCutoffs (float lowCutHz, float highCutHz) noexcept
        {
            const auto low = juce::jlimit (20.0f, maxCutoffHz, lowCutHz);
            lowCut.setCutoffFrequency (low);
            highCut.setCutoffFrequency (juce::jlimit (low, maxCutoffHz, highCutHz));
        }

        /** Restarts the same noise each time, so every hit (and every preview) sounds alike. */
        void reset() noexcept
        {
            lowCut.reset();
            highCut.reset();
            random.setSeed (1);
        }

        float next() noexcept
        {
            return highCut.processSample (0, lowCut.processSample (0, random.nextFloat() * 2.0f - 1.0f));
        }

    private:
        juce::dsp::StateVariableTPTFilter<float> lowCut, highCut;
        juce::Random random;
        float maxCutoffHz = 19800.0f;
    };
}
