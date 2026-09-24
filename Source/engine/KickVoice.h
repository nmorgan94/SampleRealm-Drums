#pragma once

#include "../dsp/BreakpointEnvelope.h"
#include "../dsp/ClickGenerator.h"
#include "../dsp/HarmonicOscillator.h"

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
        float frequencyRatio = 1.0f;  // scales the whole pitch envelope (tuning / key tracking)
        float lengthScale    = 1.0f;  // stretches both envelopes in time
        float pitchDepth     = 1.0f;  // scales each pitch node's distance from the tail, in octaves
        float startPhase     = 0.0f;  // cycles, 0..0.25
        float harmonics      = 0.0f;  // 0..1
        float subGain        = 1.0f;  // linear, velocity already applied
        srd::ClickGenerator::Settings click;
    };

    /** How long a hit with these settings sounds, given the amp envelope's unscaled duration. */
    static float getDurationSeconds (float ampDuration, const Settings& s) noexcept
    {
        return std::max (ampDuration * s.lengthScale + fadeSeconds, srd::ClickGenerator::getDurationSeconds (s.click));
    }

    void prepare (double sampleRate);

    /** Starts a hit. pitch is in log2 Hz, amp in linear gain (as EnvelopeModel publishes them). */
    void start (const srd::EnvelopeData& pitch, const srd::EnvelopeData& amp, const Settings&) noexcept;

    /** Fades out over a few ms so a retrigger doesn't click. */
    void fadeOut() noexcept;

    void stop() noexcept;
    bool isActive() const noexcept     { return subActive || click.isActive(); }

    /** Adds this voice's output into the buffer. */
    void render (float* output, int numSamples) noexcept;

private:
    srd::EnvelopeData pitchEnv, ampEnv;
    srd::EnvelopeCursor pitchCursor, ampCursor;
    srd::HarmonicOscillator oscillator;
    srd::ClickGenerator click;

    Settings settings;
    double sampleRate = 44100.0;
    juce::int64 sampleIndex = 0;
    bool subActive = false;

    // Retrigger fade-out, and the sub's release after the last amp node
    juce::SmoothedValue<float> fade { 1.0f }, release { 1.0f };
};
