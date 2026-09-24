#include "KickVoice.h"

void KickVoice::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    oscillator.prepare (sampleRate);
    click.prepare (sampleRate);
    fade.reset (sampleRate, fadeSeconds);
    release.reset (sampleRate, fadeSeconds);
    stop();
}

void KickVoice::start (const srd::EnvelopeData& pitch, const srd::EnvelopeData& amp, const Settings& newSettings) noexcept
{
    pitchEnv = pitch;
    ampEnv   = amp;
    settings = newSettings;
    settings.lengthScale = juce::jmax (0.01f, settings.lengthScale);

    pitchCursor.reset();
    ampCursor.reset();
    sampleIndex = 0;

    oscillator.reset (settings.startPhase);
    oscillator.setHarmonics (settings.harmonics);
    click.trigger (settings.click);

    fade.setCurrentAndTargetValue (1.0f);
    release.setCurrentAndTargetValue (1.0f);
    subActive = ampEnv.numNodes > 0 && settings.subGain > 0.0f;
}

void KickVoice::fadeOut() noexcept
{
    if (isActive())
        fade.setTargetValue (0.0f);
}

void KickVoice::stop() noexcept
{
    subActive = false;
    click.stop();
}

void KickVoice::render (float* output, int numSamples) noexcept
{
    if (! isActive())
        return;

    const auto secondsPerSample = 1.0f / ((float) sampleRate * settings.lengthScale);
    const auto tailLog2    = pitchEnv.getFinalValue();
    const auto envelopeEnd = ampEnv.getDuration();

    for (int i = 0; i < numSamples; ++i)
    {
        float sample = 0.0f;

        if (subActive)
        {
            const auto t = (float) sampleIndex * secondsPerSample;

            // Past the last amp node the sub holds its final level and ramps down,
            // so an envelope that doesn't end at zero still finishes without a click
            if (t > envelopeEnd)
                release.setTargetValue (0.0f);

            const auto nodeLog2 = pitchCursor.getValue (pitchEnv, t);
            const auto log2Hz   = tailLog2 + (nodeLog2 - tailLog2) * settings.pitchDepth;
            const auto hz       = std::exp2 (log2Hz) * settings.frequencyRatio;

            sample = oscillator.next (hz) * ampCursor.getValue (ampEnv, t) * settings.subGain * release.getNextValue();
            subActive = release.isSmoothing() || release.getTargetValue() > 0.0f;
        }

        sample += click.next();
        output[i] += sample * fade.getNextValue();
        ++sampleIndex;

        const auto fadedOut = fade.getTargetValue() <= 0.0f && ! fade.isSmoothing();

        if (fadedOut)
            stop();

        if (! isActive())
            return;
    }
}
