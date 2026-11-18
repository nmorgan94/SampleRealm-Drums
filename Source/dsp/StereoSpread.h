#pragma once

#include <array>
#include <juce_dsp/juce_dsp.h>

namespace srd
{
    //==============================================================================
    /**
     * Spreads a mono signal across two channels. The side signal is the input, high-passed
     * and turned in phase by a chain of all-passes: added to the left and taken from the
     * right, it leans each band of the spectrum to one side. The channels still sum to the input.
     */
    class StereoSpread
    {
    public:
        void prepare (double sampleRate)
        {
            const juce::dsp::ProcessSpec spec { sampleRate, 1, 1 };
            const auto maxHz = 0.45f * (float) sampleRate;

            lowCut.prepare (spec);
            lowCut.setType (juce::dsp::FirstOrderTPTFilterType::highpass);
            lowCut.setCutoffFrequency (lowCutHz);

            for (std::size_t i = 0; i < allPasses.size(); ++i)
            {
                allPasses[i].prepare (spec);
                allPasses[i].setType (juce::dsp::FirstOrderTPTFilterType::allpass);
                allPasses[i].setCutoffFrequency (juce::jmin (maxHz, turnHz[i]));
            }

            width.reset (sampleRate, 0.02);
        }

        void reset() noexcept
        {
            lowCut.reset();

            for (auto& allPass : allPasses)
                allPass.reset();

            width.setCurrentAndTargetValue (width.getTargetValue());
        }

        /** 0..1. At 0 both channels are the input; a change is smoothed. */
        void setWidth (float newWidth) noexcept
        {
            width.setTargetValue (juce::jlimit (0.0f, 1.0f, newWidth) * maxSide);
        }

        bool isSpreading() const noexcept   { return width.getTargetValue() > 0.0f || width.isSmoothing(); }

        /** Spreads left's mono signal over left and right. */
        void process (float* left, float* right, int numSamples) noexcept
        {
            if (! isSpreading())
            {
                juce::FloatVectorOperations::copy (right, left, numSamples);
                return;
            }

            for (int i = 0; i < numSamples; ++i)
            {
                const auto mono = left[i];
                auto side = lowCut.processSample (0, mono);

                for (auto& allPass : allPasses)
                    side = allPass.processSample (0, side);

                side *= width.getNextValue();
                left[i]  = mono + side;
                right[i] = mono - side;
            }
        }

    private:
        // Below this the signal stays in the middle
        static constexpr float lowCutHz = 300.0f;

        // The side signal's level against the input at full width
        static constexpr float maxSide = 0.7f;

        // Where each all-pass is a quarter turn: between them the bands alternate sides
        static constexpr std::array turnHz { 500.0f, 1100.0f, 2400.0f, 5000.0f, 9000.0f, 15000.0f };

        juce::dsp::FirstOrderTPTFilter<float> lowCut;
        std::array<juce::dsp::FirstOrderTPTFilter<float>, turnHz.size()> allPasses;
        juce::SmoothedValue<float> width { 0.0f };
    };
}
