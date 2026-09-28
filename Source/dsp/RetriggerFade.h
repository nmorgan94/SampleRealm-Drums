#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace srd
{
    //==============================================================================
    /**
     * A few milliseconds of fade-out, so a new hit can cut off the last one without a click.
     */
    class RetriggerFade
    {
    public:
        void prepare (double sampleRate, float seconds)     { gain.reset (sampleRate, seconds); }

        /** Back to full level, for a new hit. */
        void reset() noexcept                               { gain.setCurrentAndTargetValue (1.0f); }

        void fadeOut() noexcept                             { gain.setTargetValue (0.0f); }

        /** True once a fade-out has reached silence, when the voice can stop. */
        bool isFinished() const noexcept                    { return gain.getTargetValue() <= 0.0f && ! gain.isSmoothing(); }

        float next() noexcept                               { return gain.getNextValue(); }

    private:
        juce::SmoothedValue<float> gain { 1.0f };
    };
}
