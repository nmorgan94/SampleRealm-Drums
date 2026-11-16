#include <juce_audio_basics/juce_audio_basics.h>
#include <samplerealm/BuildCommit.h>
#include <samplerealm/ParameterValues.h>
#include <samplerealm/PeakMeter.h>
#include <samplerealm/UiInfoJson.h>
#include <samplerealm/WasmExport.h>
#include "dsp/PreviewRenderer.h"
#include "shared/DrumInfoJson.h"
#include "shared/DrumSettings.h"

//==============================================================================
namespace
{
    constexpr int maxBlockSize = 128;

    ParameterValues values { ParameterSpecs::all };

    DrumSynth::Settings currentSettings()
    {
        return makeDrumSettings (values);
    }

    const auto envelopeSpecs = DrumEnvelopes::createSpecs();

    // The synth's own, kept for the preview
    DrumSynth::Envelopes envelopes = DrumEnvelopes::defaults();

    // Display units, written by the page before setEnvelope
    std::array<srd::EnvelopeNode, srd::EnvelopeData::maxNodes> nodeScratch {};

    DrumSynth synth;
    PeakMeter meter { 1 };
    // Mono: the page copies the one channel to each of its outputs
    juce::AudioBuffer<float> buffer { 1, maxBlockSize };

    // Played at the start of the next block
    juce::MidiBuffer pendingMidi;

    PreviewRenderer previewRenderer;
    PreviewRenderer::Preview lastPreview;
}

WASM_EXPORT (uiInfo) const char* uiInfo()
{
    static const auto json = uiInfoJson (ParameterSpecs::all, PLUGIN_VERSION, buildCommit);
    return json.c_str();
}

// How each value reads, and the envelopes
WASM_EXPORT (drumInfo) const char* drumInfo()
{
    static const auto json = drumInfoJson();
    return json.c_str();
}

// Always one channel, whatever the page asks for
WASM_EXPORT (prepare) void prepare (double sampleRate, int)
{
    synth.prepare (sampleRate, maxBlockSize, currentSettings());
    previewRenderer.prepare();
}

WASM_EXPORT (setParam) void setParam (int index, float value)
{
    values.set (index, value);
}

WASM_EXPORT (envelopeNodes) srd::EnvelopeNode* envelopeNodes()
{
    return nodeScratch.data();
}

// Takes the first numNodes of envelopeNodes(), each { time, value, curve }
WASM_EXPORT (setEnvelope) void setEnvelope (int env, int numNodes)
{
    if (env < 0 || env >= std::ssize (envelopeSpecs) || numNodes < 0 || numNodes > std::ssize (nodeScratch))
        return;

    std::vector nodes (nodeScratch.begin(), nodeScratch.begin() + numNodes);

    if (! envelopeSpecs[env].sanitise (nodes))
        return;

    envelopes[env] = envelopeSpecs[env].toData (nodes);
    synth.setEnvelope (env, envelopes[env]);
}

WASM_EXPORT (midi) void midi (int status, int data1, int data2)
{
    if (status < 0x80 || status >= 0xf0)
        return;

    pendingMidi.addEvent (juce::MidiMessage (status, data1 & 0x7f, data2 & 0x7f), 0);
}

WASM_EXPORT (audition) void audition()
{
    synth.audition();
}

WASM_EXPORT (hitCount) juce::uint32 hitCount()
{
    return synth.getHitCount();
}

WASM_EXPORT (lastNote) int lastNote()
{
    return synth.getLastNote (ParameterSpecs::instrumentFromIndex (juce::roundToInt (values (ParameterSpecs::instrument))));
}

// The output is read back from here after render, for any channel the page asks for
WASM_EXPORT (channel) float* channel (int)
{
    return buffer.getWritePointer (0);
}

WASM_EXPORT (render) void render (int numSamples)
{
    const auto length = std::clamp (numSamples, 0, maxBlockSize);
    juce::AudioBuffer<float> block { buffer.getArrayOfWritePointers(), 1, length };

    synth.setSettings (currentSettings());
    synth.process (block.getWritePointer (0), length, pendingMidi);
    pendingMidi.clear();
    meter.process (block);
}

// The loudest output sample since the last call, as linear gain
WASM_EXPORT (peak) float peak (int index)
{
    return index == 0 ? meter.readAndReset (0) : 0.0f;
}

// One hit for the waveform: previewLength() samples at previewSampleRate()
WASM_EXPORT (preview) const float* preview (int note, float maxSeconds)
{
    lastPreview = previewRenderer.render (currentSettings(), envelopes, note, maxSeconds);
    return lastPreview.samples;
}

WASM_EXPORT (previewLength) int previewLength()
{
    return lastPreview.numSamples;
}

WASM_EXPORT (previewTailHz) float previewTailHz()
{
    return lastPreview.tailHz;
}

WASM_EXPORT (previewSampleRate) double previewSampleRate()
{
    return PreviewRenderer::sampleRate;
}
