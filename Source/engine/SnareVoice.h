#pragma once

#include "../dsp/ClickGenerator.h"
#include "../dsp/EnvelopedOscillator.h"
#include "../dsp/NoiseLayer.h"

//==============================================================================
/**
 * One snare hit: a pitch-enveloped body, band-limited noise for the wires,
 * and a synthesised snap.
 */
class SnareVoice
{
public:
    /** Length of the retrigger fade and of each layer's release after its last node. */
    static constexpr float fadeSeconds = 0.003f;

    struct Settings
    {
        srd::EnvelopedOscillator::Settings body;
        float noiseGain      = 1.0f;  // linear
        float noiseLowCutHz  = 800.0f;
        float noiseHighCutHz = 12000.0f;
        srd::ClickGenerator::Settings snap;
    };

    static float getDurationSeconds (float bodyDuration, float noiseDuration, const Settings& s) noexcept
    {
        return std::max (std::max (bodyDuration, noiseDuration) * s.body.lengthScale + fadeSeconds,
                         srd::ClickGenerator::getDurationSeconds (s.snap));
    }

    void prepare (double sampleRate);

    void start (const srd::EnvelopeData& pitch, const srd::EnvelopeData& body, const srd::EnvelopeData& noise,
                const Settings&) noexcept;

    void fadeOut() noexcept;

    void stop() noexcept;
    bool isActive() const noexcept     { return body.isActive() || noiseEnvelope.isActive() || snap.isActive(); }

    void render (float* output, int numSamples) noexcept;

private:
    srd::EnvelopedOscillator body;
    srd::NoiseLayer noise;
    srd::EnvelopePlayer noiseEnvelope;
    srd::ClickGenerator snap;

    juce::SmoothedValue<float> fade { 1.0f }; // retrigger fade-out
};
