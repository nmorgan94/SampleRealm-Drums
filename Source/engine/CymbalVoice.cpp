#include "CymbalVoice.h"

void CymbalVoice::prepare (double sampleRate)
{
    metal.prepare (sampleRate);
    metalEnvelope.prepare (sampleRate, fadeSeconds);
    noise.prepare (sampleRate, fadeSeconds);
    fade.prepare (sampleRate, fadeSeconds);
    stop();
}

void CymbalVoice::start (const srd::EnvelopeData& metalEnv, const srd::EnvelopeData& noiseEnv, const Settings& s) noexcept
{
    metal.setFrequencyRatio (s.frequencyRatio);
    metal.setTone (s.metalToneHz, s.metalRing);
    metal.reset();
    metalEnvelope.start (metalEnv, s.lengthScale, s.metalGain);

    noise.start (noiseEnv, s.lengthScale, s.noiseGain, s.noiseLowCutHz, s.noiseHighCutHz);

    fade.reset();
}

void CymbalVoice::fadeOut() noexcept
{
    if (isActive())
        fade.fadeOut();
}

void CymbalVoice::stop() noexcept
{
    metalEnvelope.stop();
    noise.stop();
}

void CymbalVoice::render (float* output, int numSamples) noexcept
{
    for (int i = 0; i < numSamples && isActive(); ++i)
    {
        const auto metalOut = metalEnvelope.isActive() ? metal.next() * metalEnvelope.next() : 0.0f;
        output[i] += (metalOut + noise.next()) * fade.next();

        if (fade.isFinished())
            stop();
    }
}
