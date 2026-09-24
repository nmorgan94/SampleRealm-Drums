#pragma once

#include "FxChain.h"
#include "KickVoice.h"

//==============================================================================
/** Renders single hits offline, e.g. for the editor's waveform preview. */
class KickRenderer
{
public:
    void prepare (double sampleRate);

    /** Renders a hit from its first sample, with the FxChain latency removed.
        The samples stay valid until the next render. */
    const float* render (const srd::EnvelopeData& pitch, const srd::EnvelopeData& amp,
                         const KickVoice::Settings&, const FxChain::Settings&, int numSamples);

private:
    KickVoice voice;
    FxChain fxChain;
    juce::Array<float> scratch;
};
