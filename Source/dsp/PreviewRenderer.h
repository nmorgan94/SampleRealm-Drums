#pragma once

#include "DrumSynth.h"

//==============================================================================
/**
 * Renders single hits for the editor's waveform preview.
 */
class PreviewRenderer
{
public:
    struct Preview
    {
        const float* samples = nullptr;
        int numSamples = 0;
        float tailHz = 0.0f;  // where the pitch settles; 0 for the cymbal, which has no pitch envelope
    };

    /** Whatever the host's rate, so every editor draws the same waveform. */
    static constexpr double sampleRate = 44100.0;

    void prepare();

    /** One hit of the settings' instrument at note, cut off at maxSeconds. */
    Preview render (const DrumSynth::Settings&, const DrumSynth::Envelopes&, int note, float maxSeconds);

private:
    KickVoice kick;
    SnareVoice snare;
    CymbalVoice cymbal;
    FxChain fxChain;
    juce::Array<float> scratch;

    template <typename Voice>
    const float* render (Voice&, const FxChain::Settings&, int numSamples);
};
