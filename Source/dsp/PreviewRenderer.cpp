#include "PreviewRenderer.h"

void PreviewRenderer::prepare()
{
    kick.prepare (sampleRate);
    snare.prepare (sampleRate);
    cymbal.prepare (sampleRate);
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

PreviewRenderer::Preview PreviewRenderer::render (const DrumSynth::Settings& drum, const DrumSynth::Envelopes& envelopes,
                                                  int note, float maxSeconds)
{
    note = std::clamp (note, 0, 127);

    DrumSynth::Durations durations;

    for (std::size_t env = 0; env < durations.size(); ++env)
        durations[env] = envelopes[env].getDuration();

    // In this order, a NaN maxSeconds gives 0
    const auto seconds = std::max (0.0f, std::min (maxSeconds, DrumSynth::getHitSeconds (drum, durations)));
    const auto numSamples = juce::roundToInt (seconds * sampleRate);

    const auto hit = [&] (auto& voice) -> Preview
    {
        const auto tailHz = DrumSynth::start (voice, drum, envelopes, note);
        return { render (voice, drum.fx, numSamples), numSamples, tailHz };
    };

    switch (drum.instrument)
    {
        case DrumSynth::Instrument::kick:   return hit (kick);
        case DrumSynth::Instrument::snare:  return hit (snare);
        case DrumSynth::Instrument::cymbal: break;
    }

    return hit (cymbal);
}
