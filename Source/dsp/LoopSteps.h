#pragma once

#include <algorithm>
#include <atomic>
#include "../shared/Patterns.h"

namespace srd
{
    //==============================================================================
    /**
     * The audition loop's steps: each instrument's part, loaded from a pattern and then
     * edited a step at a time. Any thread may read or write.
     */
    class LoopSteps
    {
    public:
        using Instrument = ParameterSpecs::Instrument;

        /** The hardest a step plays, and what one switched on for the first time plays at. */
        static constexpr int onVelocity = 127;

        /** A step switched off keeps its velocity as a negative, so switching it on brings it back. */
        static constexpr int toggled (int velocity) noexcept    { return velocity == 0 ? onVelocity : -velocity; }

        LoopSteps() noexcept        { load (Patterns::all[0]); }

        /** Replaces every instrument's steps. */
        void load (const Patterns::Pattern& pattern) noexcept
        {
            for (const auto instrument : { Instrument::kick, Instrument::snare, Instrument::cymbal })
            {
                const auto& part = Patterns::partFor (pattern, instrument);
                auto& steps = parts[indexOf (instrument)];

                for (std::size_t i = 0; i < steps.size(); ++i)
                    steps[i] = part[i];
            }
        }

        /** velocity is 1..127 for a hit and 0 or below for a rest. A step outside the bar is ignored. */
        void setStep (Instrument instrument, int step, int velocity) noexcept
        {
            if (step >= 0 && step < Patterns::stepsPerBar)
                parts[indexOf (instrument)][static_cast<std::size_t> (step)] = std::clamp (velocity, -onVelocity, onVelocity);
        }

        Patterns::Part get (Instrument instrument) const noexcept
        {
            const auto& steps = parts[indexOf (instrument)];
            Patterns::Part part {};

            for (std::size_t i = 0; i < steps.size(); ++i)
                part[i] = steps[i].load();

            return part;
        }

    private:
        std::array<std::array<std::atomic<int>, Patterns::stepsPerBar>, 3> parts;

        static constexpr std::size_t indexOf (Instrument instrument) noexcept
        {
            switch (instrument)
            {
                case Instrument::snare:  return 1;
                case Instrument::cymbal: return 2;
                case Instrument::kick:   break;
            }

            return 0;
        }
    };
}
