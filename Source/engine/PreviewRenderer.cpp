#include "PreviewRenderer.h"

void PreviewRenderer::prepare (double sampleRate)
{
    kick.prepare (sampleRate);
    snare.prepare (sampleRate);
    fxChain.prepare (sampleRate, 512);
}

template <typename Voice>
const float* PreviewRenderer::render (Voice& voice, const FxChain::Settings& fxSettings, int numSamples)
{
    const auto latency = fxChain.getLatencySamples();
    scratch.resize (numSamples + latency);
    juce::FloatVectorOperations::clear (scratch.data(), scratch.size());

    fxChain.setSettings (fxSettings);
    fxChain.reset();

    voice.render (scratch.data(), scratch.size());
    fxChain.process (scratch.data(), scratch.size());

    return scratch.data() + latency;
}

const float* PreviewRenderer::renderKick (const srd::EnvelopeData& pitch, const srd::EnvelopeData& amp,
                                          const KickVoice::Settings& voiceSettings, const FxChain::Settings& fxSettings,
                                          int numSamples)
{
    kick.start (pitch, amp, voiceSettings);
    return render (kick, fxSettings, numSamples);
}

const float* PreviewRenderer::renderSnare (const srd::EnvelopeData& pitch, const srd::EnvelopeData& body, const srd::EnvelopeData& noise,
                                           const SnareVoice::Settings& voiceSettings, const FxChain::Settings& fxSettings,
                                           int numSamples)
{
    snare.start (pitch, body, noise, voiceSettings);
    return render (snare, fxSettings, numSamples);
}
