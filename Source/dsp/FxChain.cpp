#include "FxChain.h"

void FxChain::prepare (double sampleRate, int maxBlockSize)
{
    drive.prepare (sampleRate, maxBlockSize);

    // A 10 Hz high-pass removes the DC that asymmetric drive and start phase leave behind
    dcBlocker.prepare ({ sampleRate, 1, 1 });
    dcBlocker.setType (juce::dsp::FirstOrderTPTFilterType::highpass);
    dcBlocker.setCutoffFrequency (10.0f);

    eq.prepare (sampleRate);
    clipper.prepare (sampleRate);
    outputGain.reset (sampleRate, 0.02);
}

void FxChain::reset() noexcept
{
    drive.reset();
    dcBlocker.reset();
    eq.reset();
    clipper.reset();
    outputGain.setCurrentAndTargetValue (outputGain.getTargetValue());
}

void FxChain::setSettings (const Settings& s) noexcept
{
    drive.setParameters (s.driveType, s.driveAmount, s.driveMix);
    eq.setSettings (s.eq);
    clipper.setAmount (s.clipAmount);
    outputGain.setTargetValue (s.outputGain);
}

void FxChain::process (float* samples, int numSamples) noexcept
{
    drive.process (samples, numSamples);

    for (int i = 0; i < numSamples; ++i)
        samples[i] = dcBlocker.processSample (0, samples[i]);

    eq.process (samples, numSamples);
    clipper.process (samples, numSamples);
    outputGain.applyGain (samples, numSamples);
}
