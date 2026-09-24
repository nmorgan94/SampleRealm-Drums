#include "Drive.h"

namespace srd
{
    Drive::Drive()
        : oversampling (1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true)
    {
    }

    void Drive::prepare (double sampleRate, int maxBlockSize)
    {
        maxBlock = juce::jmax (1, maxBlockSize);
        oversampling.initProcessing ((size_t) maxBlock);
        oversampling.reset();
        shaper.prepare (sampleRate * (double) oversampling.getOversamplingFactor());
    }

    void Drive::reset() noexcept
    {
        oversampling.reset();
        shaper.reset();
    }

    int Drive::getLatencySamples() const noexcept
    {
        return juce::roundToInt (oversampling.getLatencyInSamples());
    }

    void Drive::setParameters (Type newType, float amount, float mix) noexcept
    {
        type = newType;
        amount = juce::jlimit (0.0f, 1.0f, amount);
        shaper.setTargets (juce::Decibels::decibelsToGain (amount * maxDriveDb),
                           juce::jlimit (0.0f, 1.0f, mix) * juce::jmin (1.0f, amount * 5.0f));
    }

    float Drive::shape (Type type, float x) noexcept
    {
        switch (type)
        {
            case Type::soft: return std::tanh (x);
            case Type::hard: return juce::jlimit (-1.0f, 1.0f, x);
            case Type::fold: return std::sin (juce::MathConstants<float>::halfPi * x);
            case Type::tube:
            {
                // Asymmetric, so it adds even harmonics. The DC it creates is removed downstream.
                constexpr float bias = 0.2f;
                return std::tanh (x + bias) - std::tanh (bias);
            }
        }

        return x;
    }

    void Drive::process (float* samples, int numSamples) noexcept
    {
        // The oversampling buffers are sized for maxBlock, so longer host blocks are split
        for (int start = 0; start < numSamples; start += maxBlock)
            processChunk (samples + start, juce::jmin (maxBlock, numSamples - start));
    }

    void Drive::processChunk (float* samples, int numSamples) noexcept
    {
        float* channels[] = { samples };
        juce::dsp::AudioBlock<float> block (channels, 1, (size_t) numSamples);
        auto up = oversampling.processSamplesUp (block);

        // With no wet signal the filters still run, so latency stays constant either way
        shaper.process (up.getChannelPointer (0), up.getNumSamples(), [this] (float x) { return shape (type, x); });

        oversampling.processSamplesDown (block);
    }
}
