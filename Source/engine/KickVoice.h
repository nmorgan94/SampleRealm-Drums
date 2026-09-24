#pragma once

#include "../dsp/ClickGenerator.h"
#include "../dsp/EnvelopedOscillator.h"

//==============================================================================
/**
 * One kick hit: a pitch-enveloped sub oscillator plus a synthesised click.
 */
class KickVoice
{
public:
    /** Length of the retrigger fade and of the sub's release after the last amp node. */
    static constexpr float fadeSeconds = 0.003f;

    struct Settings
    {
        srd::EnvelopedOscillator::Settings sub;
        srd::ClickGenerator::Settings click;
    };

    /** How long a hit with these settings sounds, given the amp envelope's unscaled duration. */
    static float getDurationSeconds (float ampDuration, const Settings& s) noexcept
    {
        return std::max (ampDuration * s.sub.lengthScale + fadeSeconds, srd::ClickGenerator::getDurationSeconds (s.click));
    }

    void prepare (double sampleRate);

    /** Starts a hit. pitch is in log2 Hz, amp in linear gain (as EnvelopeModel publishes them). */
    void start (const srd::EnvelopeData& pitch, const srd::EnvelopeData& amp, const Settings&) noexcept;

    /** Fades out over a few ms so a retrigger doesn't click. */
    void fadeOut() noexcept;

    void stop() noexcept;
    bool isActive() const noexcept     { return sub.isActive() || click.isActive(); }

    /** Adds this voice's output into the buffer. */
    void render (float* output, int numSamples) noexcept;

private:
    srd::EnvelopedOscillator sub;
    srd::ClickGenerator click;
    juce::SmoothedValue<float> fade { 1.0f }; // retrigger fade-out
};
