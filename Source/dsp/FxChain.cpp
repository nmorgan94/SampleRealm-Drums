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
    spread.prepare (sampleRate);

    for (auto& ceiling : ceilings)
        ceiling.prepare (sampleRate);

    outputGain.reset (sampleRate, 0.02);
}

void FxChain::reset() noexcept
{
    drive.reset();
    dcBlocker.reset();
    eq.reset();
    clipper.reset();
    spread.reset();

    for (auto& ceiling : ceilings)
        ceiling.reset();

    outputGain.setCurrentAndTargetValue (outputGain.getTargetValue());
}

void FxChain::setSettings (const Settings& s) noexcept
{
    drive.setParameters (s.driveType, s.driveAmount, s.driveMix);
    eq.setSettings (s.eq);
    clipper.setAmount (s.clipAmount);
    spread.setWidth (s.width);

    for (auto& ceiling : ceilings)
        ceiling.setTargets (1.0f, s.clipAmount > 0.0f ? 1.0f : 0.0f);

    outputGain.setTargetValue (s.outputGain);
}

void FxChain::process (float* left, float* right, int numSamples) noexcept
{
    drive.process (left, numSamples);

    for (int i = 0; i < numSamples; ++i)
        left[i] = dcBlocker.processSample (0, left[i]);

    eq.process (left, numSamples);
    clipper.process (left, numSamples);

    if (right != nullptr)
    {
        spread.process (left, right, numSamples);

        // The side signal adds to one channel what it takes from the other
        if (spread.isSpreading())
        {
            ceilings[0].process (left, numSamples, srd::Clipper::clip);
            ceilings[1].process (right, numSamples, srd::Clipper::clip);
        }
    }

    float* channels[] { left, right };
    juce::AudioBuffer<float> block { channels, right != nullptr ? 2 : 1, numSamples };
    outputGain.applyGain (block, numSamples);
}
