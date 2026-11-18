#pragma once

#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../shared/Patterns.h"

namespace srd
{
    //==============================================================================
    /**
     * Plays one part of a pattern against a position in quarter notes, a block at a time:
     * each sixteenth that starts inside the block becomes a note-on at its sample.
     */
    class PatternPlayer
    {
    public:
        /** The next block starts afresh from its own position. */
        void reset() noexcept       { nextStart = noStep; }

        /** The step of the bar a position is in. */
        static int stepAt (double ppq) noexcept
        {
            return static_cast<int> (indexOf (std::floor (ppq * Patterns::stepsPerBeat)));
        }

        /** ppq is where the block starts. Adds the part's steps to midi as note-ons of note. */
        void process (juce::MidiBuffer& midi, const Patterns::Part& part, int note,
                      double ppq, double bpm, double sampleRate, int numSamples) noexcept
        {
            const auto stepsPerSample = bpm * Patterns::stepsPerBeat / (60.0 * sampleRate);

            if (stepsPerSample <= 0.0 || numSamples <= 0)
                return;

            const auto start = ppq * Patterns::stepsPerBeat;
            const auto end = start + numSamples * stepsPerSample;
            auto step = std::ceil (start);

            // A block that follows on carries straight on, so a nudge at its edge can't drop or repeat a step
            if (std::abs (start - nextStart) <= furthestNudge)
                step = nextStep;

            nextStart = end;

            for (; step < end; ++step)
            {
                const auto velocity = part[indexOf (step)];

                if (velocity > 0)
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, juce::uint8 (velocity)),
                                   juce::jlimit (0, numSamples - 1, juce::roundToInt ((step - start) / stepsPerSample)));
            }

            nextStep = step;
        }

    private:
        static constexpr double noStep = -1.0e15;

        // In steps: a block starting further than this from where the last ended is a jump
        static constexpr double furthestNudge = 0.25;

        // The first step not played yet, and where the next block should start
        double nextStep = noStep, nextStart = noStep;

        /** A whole step's place in the bar, for positions before 0 too. */
        static std::size_t indexOf (double step) noexcept
        {
            const auto inBar = std::fmod (step, Patterns::stepsPerBar);
            return (std::size_t) (inBar < 0.0 ? inBar + Patterns::stepsPerBar : inBar);
        }
    };
}
