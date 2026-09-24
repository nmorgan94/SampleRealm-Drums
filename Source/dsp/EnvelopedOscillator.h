#pragma once

#include "EnvelopePlayer.h"
#include "HarmonicOscillator.h"

namespace srd
{
    //==============================================================================
    /**
     * A HarmonicOscillator played through a pitch envelope (log2 Hz) and an amp
     * envelope (linear gain) on one timeline: the tonal part of a drum.
     */
    class EnvelopedOscillator
    {
    public:
        struct Settings
        {
            float frequencyRatio = 1.0f;  // scales the whole pitch envelope (tuning / key tracking)
            float lengthScale    = 1.0f;  // stretches both envelopes in time
            float pitchDepth     = 1.0f;  // scales each pitch node's distance from the tail, in octaves
            float startPhase     = 0.0f;  // cycles
            float harmonics      = 0.0f;  // 0..1
            float gain           = 1.0f;  // linear
        };

        /** Where a pitch envelope settles, in Hz. */
        static float getTailHz (const EnvelopeData& pitch, float frequencyRatio = 1.0f) noexcept
        {
            return pitch.numNodes > 0 ? std::exp2 (pitch.getFinalValue()) * frequencyRatio : 0.0f;
        }

        void prepare (double sampleRate, float releaseSeconds)
        {
            oscillator.prepare (sampleRate);
            amp.prepare (sampleRate, releaseSeconds);
        }

        void start (const EnvelopeData& pitch, const EnvelopeData& ampEnvelope, const Settings& newSettings) noexcept
        {
            pitchEnv = pitch;
            pitchCursor.reset();
            tailLog2 = pitch.getFinalValue();
            settings = newSettings;

            oscillator.reset (settings.startPhase);
            oscillator.setHarmonics (settings.harmonics);
            amp.start (ampEnvelope, settings.lengthScale, settings.gain);
        }

        void stop() noexcept                    { amp.stop(); }
        bool isActive() const noexcept          { return amp.isActive(); }

        float next() noexcept
        {
            if (! amp.isActive())
                return 0.0f;

            const auto nodeLog2 = pitchCursor.getValue (pitchEnv, amp.getTime());
            const auto hz = std::exp2 (tailLog2 + (nodeLog2 - tailLog2) * settings.pitchDepth) * settings.frequencyRatio;

            return oscillator.next (hz) * amp.next();
        }

    private:
        EnvelopeData pitchEnv;
        EnvelopeCursor pitchCursor;
        float tailLog2 = 0.0f;
        EnvelopePlayer amp;
        HarmonicOscillator oscillator;
        Settings settings;
    };
}
