#include <juce_audio_basics/juce_audio_basics.h>
#include "dsp/PreviewRenderer.h"
#include "shared/DrumSettings.h"
#include "shared/UiInfoJson.h"

//==============================================================================
namespace
{
    constexpr int maxBlockSize = 128;

    // In ParameterSpecs::all's order
    auto values = []
    {
        std::array<float, ParameterSpecs::all.size()> defaults;
        std::ranges::transform (ParameterSpecs::all, defaults.begin(), &ParameterSpecs::Spec::defaultValue);
        return defaults;
    }();

    float valueOf (const ParameterSpecs::Spec& spec)
    {
        return values[ParameterSpecs::indexOf (spec)];
    }

    DrumSynth::Settings currentSettings()
    {
        return makeDrumSettings (valueOf);
    }

    const auto envelopeSpecs = DrumEnvelopes::createSpecs();

    // The synth's own, kept for the preview
    DrumSynth::Envelopes envelopes = DrumEnvelopes::defaults();

    // Display units, written by the page before setEnvelope
    std::array<srd::EnvelopeNode, srd::EnvelopeData::maxNodes> nodeScratch {};

    DrumSynth synth;
    std::array<float, maxBlockSize> output {};

    // Played at the start of the next block
    juce::MidiBuffer pendingMidi;

    PreviewRenderer previewRenderer;
    PreviewRenderer::Preview lastPreview;
}

#define EXPORT(name) extern "C" __attribute__ ((export_name (#name)))

EXPORT (uiInfo) const char* uiInfo()
{
    static const auto json = uiInfoJson (PLUGIN_VERSION);
    return json.c_str();
}

EXPORT (prepare) void prepare (double sampleRate)
{
    synth.prepare (sampleRate, maxBlockSize, currentSettings());
    previewRenderer.prepare();
}

EXPORT (setParam) void setParam (int index, float value)
{
    if (index >= 0 && index < std::ssize (values))
        values[index] = value;
}

EXPORT (envelopeNodes) srd::EnvelopeNode* envelopeNodes()
{
    return nodeScratch.data();
}

// Takes the first numNodes of envelopeNodes(), each { time, value, curve }
EXPORT (setEnvelope) void setEnvelope (int env, int numNodes)
{
    if (env < 0 || env >= std::ssize (envelopeSpecs) || numNodes < 0 || numNodes > std::ssize (nodeScratch))
        return;

    std::vector nodes (nodeScratch.begin(), nodeScratch.begin() + numNodes);

    if (! envelopeSpecs[env].sanitise (nodes))
        return;

    envelopes[env] = envelopeSpecs[env].toData (nodes);
    synth.setEnvelope (env, envelopes[env]);
}

EXPORT (midi) void midi (int status, int data1, int data2)
{
    if (status < 0x80 || status >= 0xf0)
        return;

    pendingMidi.addEvent (juce::MidiMessage (status, data1 & 0x7f, data2 & 0x7f), 0);
}

EXPORT (audition) void audition()
{
    synth.audition();
}

EXPORT (hitCount) juce::uint32 hitCount()
{
    return synth.getHitCount();
}

EXPORT (lastNote) int lastNote()
{
    return synth.getLastNote (ParameterSpecs::instrumentFromIndex (juce::roundToInt (valueOf (ParameterSpecs::instrument))));
}

EXPORT (render) float* render (int numSamples)
{
    const auto length = std::clamp (numSamples, 0, maxBlockSize);

    synth.setSettings (currentSettings());
    synth.process (output.data(), length, pendingMidi);
    pendingMidi.clear();

    return output.data();
}

// One hit for the waveform: previewLength() samples at previewSampleRate()
EXPORT (preview) const float* preview (int note, float maxSeconds)
{
    lastPreview = previewRenderer.render (currentSettings(), envelopes, note, maxSeconds);
    return lastPreview.samples;
}

EXPORT (previewLength) int previewLength()
{
    return lastPreview.numSamples;
}

EXPORT (previewTailHz) float previewTailHz()
{
    return lastPreview.tailHz;
}

EXPORT (previewSampleRate) double previewSampleRate()
{
    return PreviewRenderer::sampleRate;
}
