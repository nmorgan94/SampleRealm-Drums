#pragma once

#include "../dsp/Clipper.h"
#include "../dsp/Drive.h"
#include "../dsp/ThreeBandEQ.h"

//==============================================================================
/** The kick's mono output chain: Drive → DC blocker → EQ → Clipper → Output gain. */
class FxChain
{
public:
    struct Settings
    {
        srd::Drive::Type driveType = srd::Drive::Type::soft;
        float driveAmount = 0.0f;   // 0..1
        float driveMix    = 1.0f;   // 0..1
        srd::ThreeBandEQ::Settings eq;
        float clipAmount  = 0.0f;   // 0..1
        float outputGain  = 1.0f;   // linear
    };

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    int getLatencySamples() const noexcept  { return drive.getLatencySamples(); }

    void setSettings (const Settings&) noexcept;
    void process (float* samples, int numSamples) noexcept;

private:
    srd::Drive drive;
    juce::dsp::FirstOrderTPTFilter<float> dcBlocker;
    srd::ThreeBandEQ eq;
    srd::Clipper clipper;
    juce::SmoothedValue<float> outputGain { 1.0f };
};
