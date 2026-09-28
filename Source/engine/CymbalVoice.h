#pragma once

#include "../dsp/EnvelopedNoise.h"
#include "../dsp/MetalOscillator.h"
#include "../dsp/RetriggerFade.h"

//==============================================================================
/**
 * One cymbal hit: band-passed metal plus filtered noise, each with its own
 * envelope. Shaped short it's a closed hat; long, an open hat or a ride.
 */
class CymbalVoice
{
public:
    /** Length of the retrigger fade and of each layer's release after its last node. */
    static constexpr float fadeSeconds = 0.003f;

    struct Settings
    {
        float frequencyRatio = 1.0f;
        float lengthScale    = 1.0f;
        float metalGain      = 0.5f;   // linear
        float metalToneHz    = 8000.0f;
        float metalRing      = 0.3f;   // 0..1
        float noiseGain      = 1.0f;   // linear
        float noiseLowCutHz  = 6000.0f;
        float noiseHighCutHz = 16000.0f;
    };

    static float getDurationSeconds (float metalDuration, float noiseDuration, const Settings& s) noexcept
    {
        return std::max (metalDuration, noiseDuration) * s.lengthScale + fadeSeconds;
    }

    void prepare (double sampleRate);

    void start (const srd::EnvelopeData& metal, const srd::EnvelopeData& noise, const Settings&) noexcept;

    void fadeOut() noexcept;

    void stop() noexcept;
    bool isActive() const noexcept     { return metalEnvelope.isActive() || noise.isActive(); }

    void render (float* output, int numSamples) noexcept;

private:
    srd::MetalOscillator metal;
    srd::EnvelopePlayer metalEnvelope;
    srd::EnvelopedNoise noise;
    srd::RetriggerFade fade;
};
