#include <juce_audio_basics/juce_audio_basics.h>
#include <samplerealm/BuildCommit.h>
#include <samplerealm/ParameterValues.h>
#include <samplerealm/PeakMeter.h>
#include <samplerealm/Transport.h>
#include <samplerealm/UiInfoJson.h>
#include <samplerealm/WasmExport.h>
#include "dsp/AuditionLoop.h"
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
    constexpr int numChannels = 2;

    PeakMeter meter { numChannels };
    juce::AudioBuffer<float> buffer { numChannels, maxBlockSize };

    // Played at the start of the next block
    juce::MidiBuffer pendingMidi;

    // The audition loop and its own playhead, which starts from the first step when it's turned on
    // and takes only its tempo from the page
    srd::AuditionLoop loop;
    Transport loopPlayhead { .playing = true };
    double engineSampleRate = 44100.0;

    ParameterSpecs::Instrument currentInstrument()
    {
        return ParameterSpecs::instrumentFromIndex (juce::roundToInt (values (ParameterSpecs::instrument)));
    }

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

// Always two channels, whatever the page asks for
WASM_EXPORT (prepare) void prepare (double sampleRate, int)
{
    engineSampleRate = sampleRate;
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

WASM_EXPORT (setTransport) void setTransport (double bpm, bool, double)
{
    loopPlayhead.bpm = bpm;
}

WASM_EXPORT (midi) void midi (int status, int data1, int data2)
{
    if (status < 0x80 || status >= 0xf0)
        return;

    pendingMidi.addEvent (juce::MidiMessage (status, data1 & 0x7f, data2 & 0x7f), 0);
}

// The current instrument's steps. A new pattern in Patterns::all replaces every instrument's
WASM_EXPORT (setLoop) void setLoop (int pattern, bool on)
{
    if (on && ! loop.isOn())
        loopPlayhead.ppq = 0.0;

    loop.set (pattern, on);
}

// As srd::LoopSteps::setStep
WASM_EXPORT (setLoopStep) void setLoopStep (int step, int velocity)
{
    loop.setStep (currentInstrument(), step, velocity);
}

// 0 for a step outside the bar
WASM_EXPORT (loopStepVelocity) int loopStepVelocity (int step)
{
    return step >= 0 && step < Patterns::stepsPerBar ? loop.getSteps (currentInstrument())[static_cast<std::size_t> (step)] : 0;
}

// -1 while the loop is off
WASM_EXPORT (playingStep) int playingStep()
{
    return loop.getPlayingStep();
}

WASM_EXPORT (audition) void audition (float velocity)
{
    synth.audition (velocity);
}

WASM_EXPORT (hitCount) juce::uint32 hitCount()
{
    return synth.getHitCount();
}

WASM_EXPORT (lastHitLevel) float lastHitLevel()
{
    return synth.getLastHitLevel();
}

WASM_EXPORT (lastHitVelocity) float lastHitVelocity()
{
    return synth.getLastHitVelocity();
}

WASM_EXPORT (lastNote) int lastNote()
{
    return synth.getLastNote (currentInstrument());
}

// The output is read back from here after render; a channel it doesn't have gives the nearest
WASM_EXPORT (channel) float* channel (int index)
{
    return buffer.getWritePointer (std::clamp (index, 0, numChannels - 1));
}

WASM_EXPORT (render) void render (int numSamples)
{
    const auto length = std::clamp (numSamples, 0, maxBlockSize);
    juce::AudioBuffer<float> block { buffer.getArrayOfWritePointers(), numChannels, length };

    synth.setSettings (currentSettings());

    if (loop.isOn())
    {
        const auto instrument = currentInstrument();
        loop.process (pendingMidi, instrument, synth.getLastNote (instrument),
                      loopPlayhead.ppq, loopPlayhead.bpm, engineSampleRate, length);
        loopPlayhead.advance (length, engineSampleRate);
    }
    else
    {
        loop.stop();
    }

    synth.process (block.getWritePointer (0), block.getWritePointer (1), length, pendingMidi);
    pendingMidi.clear();
    meter.process (block);
}

// The loudest output sample since the last call, as linear gain
WASM_EXPORT (peak) float peak (int index)
{
    return juce::isPositiveAndBelow (index, numChannels) ? meter.readAndReset (index) : 0.0f;
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
