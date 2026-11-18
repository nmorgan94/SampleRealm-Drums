#pragma once

#include <array>
#include "ParameterSpecs.h"

//==============================================================================
/** The audition loop's fixed patterns: one bar of sixteenths, a part for each instrument. */
namespace Patterns
{
    inline constexpr int stepsPerBeat = 4;
    inline constexpr int stepsPerBar = 16;

    /** A velocity for each step, 1..127; 0 rests. */
    using Part = std::array<int, stepsPerBar>;

    struct Pattern
    {
        const char* name;
        Part kick, snare, hat;
    };

    inline constexpr std::array<Pattern, 4> all {{
        { "Two-step",
          { 127, 0, 0, 0,   0, 0, 0, 0,   0, 0, 112, 0,   0, 0, 0, 0 },
          {   0, 0, 0, 0, 127, 0, 0, 0,   0, 0,   0, 0, 127, 0, 0, 0 },
          { 110, 0, 70, 0, 100, 0, 70, 0, 110, 0, 70, 0, 100, 0, 70, 0 } },

        { "Halftime",
          { 127, 0, 0, 0,   0, 0, 90, 0,   0, 0, 0, 0,   0, 0, 0, 0 },
          {   0, 0, 0, 0,   0, 0,  0, 0, 127, 0, 0, 0,   0, 0, 0, 0 },
          { 110, 0, 70, 0, 100, 0, 70, 0, 110, 0, 70, 0, 100, 0, 70, 0 } },

        { "Four-floor",
          { 127, 0, 0, 0, 127, 0, 0, 0, 127, 0, 0, 0, 127, 0, 0, 0 },
          {   0, 0, 0, 0, 127, 0, 0, 0,   0, 0, 0, 0, 127, 0, 0, 0 },
          {   0, 0, 110, 0, 0, 0, 110, 0, 0, 0, 110, 0, 0, 0, 110, 0 } },

        // For hearing Vel Sens
        { "Sixteenths",
          { 127, 0, 0, 0,   0, 0, 0, 0,   0, 0, 112, 0,   0, 0, 0, 0 },
          {   0, 0, 0, 0, 127, 0, 0, 50,  0, 60,  0, 0, 127, 70, 90, 110 },
          { 110, 60, 80, 60, 100, 60, 80, 60, 110, 60, 80, 60, 100, 60, 80, 60 } }
    }};

    inline constexpr auto names = []
    {
        std::array<const char*, all.size()> result {};

        for (std::size_t i = 0; i < all.size(); ++i)
            result[i] = all[i].name;

        return result;
    }();

    /** The pattern at index, or the first for an index out of range. */
    constexpr const Pattern& at (int index) noexcept
    {
        return all[index >= 0 && index < std::ssize (all) ? static_cast<std::size_t> (index) : 0];
    }

    /** The cymbal plays the hat. */
    constexpr const Part& partFor (const Pattern& pattern, ParameterSpecs::Instrument instrument) noexcept
    {
        switch (instrument)
        {
            case ParameterSpecs::Instrument::snare:  return pattern.snare;
            case ParameterSpecs::Instrument::cymbal: return pattern.hat;
            case ParameterSpecs::Instrument::kick:   break;
        }

        return pattern.kick;
    }
}
