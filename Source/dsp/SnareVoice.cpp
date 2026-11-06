#include "SnareVoice.h"

void SnareVoice::prepare (double sampleRate)
{
    body.prepare (sampleRate, fadeSeconds);
    wires.prepare (sampleRate, fadeSeconds);
    snap.prepare (sampleRate);
    fade.prepare (sampleRate, fadeSeconds);
    stop();
}

void SnareVoice::start (const srd::EnvelopeData& pitch, const srd::EnvelopeData& bodyEnvelope, const srd::EnvelopeData& noiseEnv,
                        const Settings& s) noexcept
{
    body.start (pitch, bodyEnvelope, s.body);
    wires.start (noiseEnv, s.body.lengthScale, s.noiseGain, s.noiseLowCutHz, s.noiseHighCutHz);
    snap.trigger (s.snap);
    fade.reset();
}

void SnareVoice::fadeOut() noexcept
{
    if (isActive())
        fade.fadeOut();
}

void SnareVoice::stop() noexcept
{
    body.stop();
    wires.stop();
    snap.stop();
}

void SnareVoice::render (float* output, int numSamples) noexcept
{
    for (int i = 0; i < numSamples && isActive(); ++i)
    {
        output[i] += (body.next() + wires.next() + snap.next()) * fade.next();

        if (fade.isFinished())
            stop();
    }
}
