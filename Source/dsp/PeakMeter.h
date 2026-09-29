#pragma once

#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>

namespace srd
{
    //==============================================================================
    /**
     * Collects the loudest sample on the audio thread for a meter to read on the UI thread.
     * It keeps the maximum since the last read, so short transients are never missed.
     */
    class PeakMeter
    {
    public:
        void process (const float* samples, int numSamples) noexcept
        {
            const auto range = juce::FloatVectorOperations::findMinAndMax (samples, numSamples);
            const auto blockPeak = std::max (-range.getStart(), range.getEnd());

            auto current = peak.load();
            while (blockPeak > current && ! peak.compare_exchange_weak (current, blockPeak)) {}
        }

        /** The loudest sample since the last call, as linear gain. */
        float readAndReset() noexcept   { return peak.exchange (0.0f); }

    private:
        std::atomic<float> peak { 0.0f };
    };
}
