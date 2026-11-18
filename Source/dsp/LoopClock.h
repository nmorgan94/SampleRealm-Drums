#pragma once

#include <cmath>
#include <samplerealm/Transport.h>

namespace srd
{
    //==============================================================================
    /**
     * Where the audition loop is, in quarter notes. While the host plays, that's the host's
     * position. Otherwise it follows the system's clock at the host's tempo, which every
     * instance reads alike: it counts on in samples and is eased towards the clock, since
     * blocks arrive at uneven times.
     */
    class LoopClock
    {
    public:
        /** The next block starts again from the clock. */
        void reset() noexcept       { following = false; }

        /** The position at the start of a block. clockSeconds is the system's time as the block arrives. */
        double advance (const Transport& transport, double clockSeconds, int numSamples, double sampleRate) noexcept
        {
            if (transport.playing)
            {
                following = false;
                return transport.ppq;
            }

            const auto clockPosition = clockSeconds * transport.bpm / 60.0;
            const auto secondsOff = std::abs (clockPosition - position) * 60.0 / transport.bpm;

            // A new tempo moves the clock's position a long way
            if (following && juce::exactlyEqual (transport.bpm, bpm) && secondsOff < furthestEased)
                position += (clockPosition - position) * easing;
            else
                position = clockPosition;

            following = true;
            bpm = transport.bpm;

            const auto blockStart = position;
            position += numSamples * transport.bpm / (60.0 * sampleRate);
            return blockStart;
        }

    private:
        // The share of the way to the clock each block goes
        static constexpr double easing = 0.02;

        // In seconds, longer than a host's buffer; further off than this it jumps
        static constexpr double furthestEased = 0.5;

        double position = 0.0, bpm = 0.0;
        bool following = false;
    };
}
