#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <samplerealm/PeakMeter.h>
#include "Parameters.h"
#include "dsp/AuditionLoop.h"
#include "dsp/DrumSynth.h"
#include "dsp/LoopClock.h"
#include "envelopes/EnvelopeModel.h"
#include <editor/editor.h>

//==============================================================================
class AudioPluginAudioProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState& getAPVTS()      { return apvts; }
    srd::EnvelopeModel& getEnvelopeModel()              { return envelopeModel; }
    PresetManager& getPresetManager()                   { return presetManager; }
    PeakMeter& getOutputMeter()                         { return outputMeter; }

    /** The current parameter values as the engine uses them. Safe from any thread. */
    DrumSynth::Settings getDrumSettings() const noexcept;

    /** The envelopes as the engine uses them. Message thread. */
    DrumSynth::Envelopes getDrumEnvelopes() const;

    /** Replays the last note played on the next audio block, at a velocity above 0. */
    void triggerAudition (float velocity = 1.0f) noexcept   { synth.audition (velocity); }

    /** The current instrument's last note, so the UI can show where its tail lands with Key Track on. */
    int getLastNote() const noexcept;
    int getLastNote (Parameters::Instrument instrument) const noexcept   { return synth.getLastNote (instrument); }

    /** Increments on every hit, so the UI can flash without listening to MIDI. */
    juce::uint32 getHitCount() const noexcept           { return synth.getHitCount(); }

    /** The last hit's gain against a full one, 0..1. */
    float getLastHitLevel() const noexcept              { return synth.getLastHitLevel(); }

    /** The velocity the last hit was played at, 0..1. */
    float getLastHitVelocity() const noexcept           { return synth.getLastHitVelocity(); }

    /** The audition loop: the current instrument's steps, in time with the host. A new pattern in
        Patterns::all replaces every instrument's steps. Not a parameter and not saved. */
    void setLoop (int pattern, bool on) noexcept        { loop.set (pattern, on); }
    int getLoopPattern() const noexcept                 { return loop.getPattern(); }
    bool isLooping() const noexcept                     { return loop.isOn(); }

    /** The current instrument's steps: a velocity each, as srd::LoopSteps::setStep takes. */
    Patterns::Part getLoopSteps() const noexcept        { return loop.getSteps (getInstrument()); }
    void setLoopStep (int step, int velocity) noexcept  { loop.setStep (getInstrument(), step, velocity); }

    /** The step the loop is on, or -1 while it's off. */
    int getPlayingStep() const noexcept                 { return loop.getPlayingStep(); }

private:
    // In ParameterSpecs::all's order
    using ParameterValues = std::array<const std::atomic<float>*, ParameterSpecs::all.size()>;

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters",
                                               createParameterLayout (ParameterSpecs::all, Parameters::versionHint,
                                                                      Parameters::withValueText) };

    // Raw parameter pointers, cached once so reading is lock-free on the audio thread
    const ParameterValues parameterValues = findParameterValues (apvts);

    srd::EnvelopeModel envelopeModel { apvts, DrumEnvelopes::createSpecs() };
    PresetManager presetManager { apvts, createPresetConfig() };

    DrumSynth synth;

    srd::AuditionLoop loop;
    srd::LoopClock loopClock;

    // The host's MIDI with the loop's hits added, while it's on
    juce::MidiBuffer midiWithLoop;

    Parameters::Instrument getInstrument() const noexcept;
    const juce::MidiBuffer& withLoopHits (const juce::MidiBuffer& midi, int numSamples) noexcept;

    // As many channels as the output, set in prepareToPlay
    PeakMeter outputMeter { 2 };

    srd::EnvelopeModel::Snapshot envelopes;
    juce::uint32 envelopeVersion = 0;

    static PresetManager::Config createPresetConfig();
    static ParameterValues findParameterValues (juce::AudioProcessorValueTreeState&);

    float valueOf (const ParameterSpecs::Spec&) const noexcept;
    void updateEnvelopes() noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
