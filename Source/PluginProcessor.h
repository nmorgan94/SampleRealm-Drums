#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "dsp/DrumSynth.h"
#include "dsp/PeakMeter.h"
#include "envelopes/EnvelopeModel.h"
#include "presets/PresetManager.h"

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
    srd::PresetManager& getPresetManager()              { return presetManager; }
    srd::PeakMeter& getOutputMeter()                    { return outputMeter; }

    /** The current parameter values as the engine uses them. Safe from any thread. */
    DrumSynth::Settings getDrumSettings() const noexcept;

    /** The envelopes as the engine uses them. Message thread. */
    DrumSynth::Envelopes getDrumEnvelopes() const;

    /** Replays the last note played on the next audio block. */
    void triggerAudition() noexcept                     { synth.audition(); }

    /** The current instrument's last note, so the UI can show where its tail lands with Key Track on. */
    int getLastNote() const noexcept;
    int getLastNote (Parameters::Instrument instrument) const noexcept   { return synth.getLastNote (instrument); }

    /** Increments on every hit, so the UI can flash without listening to MIDI. */
    juce::uint32 getHitCount() const noexcept           { return synth.getHitCount(); }

private:
    // In ParameterSpecs::all's order
    using ParameterValues = std::array<const std::atomic<float>*, ParameterSpecs::all.size()>;

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters",
                                               Parameters::createLayout() };

    // Raw parameter pointers, cached once so reading is lock-free on the audio thread
    const ParameterValues parameterValues = findParameterValues (apvts);

    srd::EnvelopeModel envelopeModel { apvts, DrumEnvelopes::createSpecs() };
    srd::PresetManager presetManager { apvts, createPresetConfig() };

    DrumSynth synth;
    srd::PeakMeter outputMeter;

    srd::EnvelopeModel::Snapshot envelopes;
    juce::uint32 envelopeVersion = 0;

    static srd::PresetManager::Config createPresetConfig();
    static ParameterValues findParameterValues (juce::AudioProcessorValueTreeState&);

    float valueOf (const ParameterSpecs::Spec&) const noexcept;
    void updateEnvelopes() noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
