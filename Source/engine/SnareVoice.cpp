#include "SnareVoice.h"

void SnareVoice::prepare (double sampleRate)
{
    body.prepare (sampleRate, fadeSeconds);
    noise.prepare (sampleRate);
    noiseEnvelope.prepare (sampleRate, fadeSeconds);
    snap.prepare (sampleRate);
    fade.reset (sampleRate, fadeSeconds);
    stop();
}

void SnareVoice::start (const srd::EnvelopeData& pitch, const srd::EnvelopeData& bodyEnvelope, const srd::EnvelopeData& noiseEnv,
                        const Settings& s) noexcept
{
    body.start (pitch, bodyEnvelope, s.body);

    noise.setCutoffs (s.noiseLowCutHz, s.noiseHighCutHz);
    noise.reset();
    noiseEnvelope.start (noiseEnv, s.body.lengthScale, s.noiseGain);

    snap.trigger (s.snap);
    fade.setCurrentAndTargetValue (1.0f);
}

void SnareVoice::fadeOut() noexcept
{
    if (isActive())
        fade.setTargetValue (0.0f);
}

void SnareVoice::stop() noexcept
{
    body.stop();
    noiseEnvelope.stop();
    snap.stop();
}

void SnareVoice::render (float* output, int numSamples) noexcept
{
    for (int i = 0; i < numSamples && isActive(); ++i)
    {
        const auto wires = noiseEnvelope.isActive() ? noise.next() * noiseEnvelope.next() : 0.0f;
        output[i] += (body.next() + wires + snap.next()) * fade.getNextValue();

        if (fade.getTargetValue() <= 0.0f && ! fade.isSmoothing())
            stop();
    }
}
