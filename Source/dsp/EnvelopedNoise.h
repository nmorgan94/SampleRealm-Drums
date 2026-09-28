#pragma once

#include "EnvelopePlayer.h"
#include "NoiseLayer.h"

namespace srd
{
    //==============================================================================
    /**
     * A NoiseLayer played through an amp envelope (linear gain): the noisy part of a
     * drum, such as snare wires or a hat's hiss. The noise only runs while the envelope does.
     */
    class EnvelopedNoise
    {
    public:
        void prepare (double sampleRate, float releaseSeconds)
        {
            noise.prepare (sampleRate);
            amp.prepare (sampleRate, releaseSeconds);
        }

        void start (const EnvelopeData& ampEnvelope, float lengthScale, float gain, float lowCutHz, float highCutHz) noexcept
        {
            noise.setCutoffs (lowCutHz, highCutHz);
            noise.reset();
            amp.start (ampEnvelope, lengthScale, gain);
        }

        void stop() noexcept                    { amp.stop(); }
        bool isActive() const noexcept          { return amp.isActive(); }

        float next() noexcept
        {
            if (! amp.isActive())
                return 0.0f;

            return noise.next() * amp.next();
        }

    private:
        NoiseLayer noise;
        EnvelopePlayer amp;
    };
}
