#pragma once

#include "FxChain.h"
#include "KickVoice.h"
#include "SnareVoice.h"

//==============================================================================
/**
 * Renders single hits for the editor's waveform preview.
 */
class PreviewRenderer
{
public:
    void prepare (double sampleRate);

    const float* renderKick (const srd::EnvelopeData& pitch, const srd::EnvelopeData& amp,
                             const KickVoice::Settings&, const FxChain::Settings&, int numSamples);

    const float* renderSnare (const srd::EnvelopeData& pitch, const srd::EnvelopeData& body, const srd::EnvelopeData& noise,
                              const SnareVoice::Settings&, const FxChain::Settings&, int numSamples);

private:
    KickVoice kick;
    SnareVoice snare;
    FxChain fxChain;
    juce::Array<float> scratch;

    template <typename Voice>
    const float* render (Voice&, const FxChain::Settings&, int numSamples);
};
