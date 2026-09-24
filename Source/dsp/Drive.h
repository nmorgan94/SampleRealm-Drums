#pragma once

#include <juce_dsp/juce_dsp.h>
#include "SmoothedShaper.h"

namespace srd
{
    //==============================================================================
    /**
     * Mono waveshaping distortion, run at 4× oversampling (low-latency polyphase IIR)
     * so the harmonics it adds don't alias. Amount 0 is transparent; the wet signal
     * fades in over the first 20% of the Amount range.
     */
    class Drive
    {
    public:
        enum class Type { soft, hard, fold, tube };

        /** Display names, in the same order as Type. */
        static juce::StringArray getTypeNames()     { return { "Soft", "Hard", "Fold", "Tube" }; }

        /** Maps a choice-parameter index (in getTypeNames() order) to a Type. */
        static Type typeFromIndex (int index) noexcept
        {
            switch (index)
            {
                case 1:  return Type::hard;
                case 2:  return Type::fold;
                case 3:  return Type::tube;
                default: return Type::soft;
            }
        }

        Drive();

        void prepare (double sampleRate, int maxBlockSize);
        void reset() noexcept;

        /** Latency added by the oversampling filters, in samples at the base rate. */
        int getLatencySamples() const noexcept;

        /** amount and mix are 0..1. */
        void setParameters (Type, float amount, float mix) noexcept;

        /** Processes in place, in chunks no larger than the prepared block size. */
        void process (float* samples, int numSamples) noexcept;

        static float shape (Type, float x) noexcept;

    private:
        juce::dsp::Oversampling<float> oversampling;
        SmoothedShaper shaper;
        Type type = Type::soft;
        int maxBlock = 512;

        static constexpr float maxDriveDb = 36.0f;

        void processChunk (float* samples, int numSamples) noexcept;
    };
}
