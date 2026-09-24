#include "KickRenderer.h"

void KickRenderer::prepare (double sampleRate)
{
    voice.prepare (sampleRate);
    fxChain.prepare (sampleRate, 512);
}

const float* KickRenderer::render (const srd::EnvelopeData& pitch, const srd::EnvelopeData& amp,
                                  const KickVoice::Settings& voiceSettings, const FxChain::Settings& fxSettings,
                                  int numSamples)
{
    const auto latency = fxChain.getLatencySamples();
    scratch.resize (numSamples + latency);
    juce::FloatVectorOperations::clear (scratch.data(), scratch.size());

    fxChain.setSettings (fxSettings);
    fxChain.reset();

    voice.start (pitch, amp, voiceSettings);
    voice.render (scratch.data(), scratch.size());
    fxChain.process (scratch.data(), scratch.size());

    return scratch.data() + latency;
}
