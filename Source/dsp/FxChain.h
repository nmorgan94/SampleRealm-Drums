#pragma once

#include "Clipper.h"
#include "Drive.h"
#include "StereoSpread.h"
#include "ThreeBandEQ.h"

//==============================================================================
/**
 * The output chain: Drive → DC blocker → EQ → Clipper → Spread → Output gain. Mono as far as the
 * spread; while that's widening and the clipper is on, each channel is held to the clipper's ceiling again.
 */
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
        float width       = 0.0f;   // 0..1, the stereo spread; at 0 both channels are the same
        float outputGain  = 1.0f;   // linear
    };

    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    int getLatencySamples() const noexcept  { return drive.getLatencySamples(); }

    void setSettings (const Settings&) noexcept;

    /** Works on left's mono signal. A null right leaves it mono, without the spread. */
    void process (float* left, float* right, int numSamples) noexcept;

private:
    srd::Drive drive;
    juce::dsp::FirstOrderTPTFilter<float> dcBlocker;
    srd::ThreeBandEQ eq;
    srd::Clipper clipper;
    srd::StereoSpread spread;
    std::array<srd::SmoothedShaper, 2> ceilings;
    juce::SmoothedValue<float> outputGain { 1.0f };
};
