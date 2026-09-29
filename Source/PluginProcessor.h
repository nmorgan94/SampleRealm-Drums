#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "dsp/EnvelopeModel.h"
#include "dsp/PeakMeter.h"
#include "dsp/VoicePool.h"
#include "engine/DrumEnvelopes.h"
#include "engine/EngineParams.h"
#include "engine/FxChain.h"
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
    EngineParams& getEngineParams()                     { return engineParams; }
    srd::PeakMeter& getOutputMeter()                    { return outputMeter; }

    /** Replays the last note played on the next audio block. */
    void triggerAudition() noexcept                     { auditionPending = true; }

    /** The current instrument's last note, so the UI can show where its tail lands with Key Track on. */
    int getLastNote() const noexcept;

    /** Increments on every hit, so the UI can flash without listening to MIDI. */
    juce::uint32 getHitCount() const noexcept           { return hitCount.load(); }

private:
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Parameters",
                                               Parameters::createLayout() };

    srd::EnvelopeModel envelopeModel { apvts, DrumEnvelopes::createSpecs() };
    srd::PresetManager presetManager { apvts, createPresetConfig() };
    EngineParams engineParams { apvts };

    srd::VoicePool<KickVoice, 4> kickVoices;
    srd::VoicePool<SnareVoice, 4> snareVoices;
    srd::VoicePool<CymbalVoice, 4> cymbalVoices;
    FxChain fxChain;
    srd::PeakMeter outputMeter;

    srd::EnvelopeModel::Snapshot envelopes;
    juce::uint32 envelopeVersion = 0;

    Parameters::Instrument playing = Parameters::Instrument::kick;  // audio thread only

    std::atomic<bool> auditionPending { false };
    std::atomic<juce::uint32> hitCount { 0 };

    // Each instrument keeps its own last note. Until MIDI arrives the Hit pad plays where each
    // default pitch envelope settles, so turning Key Track on doesn't change the sound
    std::atomic<int> lastKickNote   { 29 };  // F1, 43.65 Hz
    std::atomic<int> lastSnareNote  { 55 };  // G3, 196 Hz
    std::atomic<int> lastCymbalNote { 42 };  // the cymbal ignores the note; kept so lastNoteOf covers every instrument

    static srd::PresetManager::Config createPresetConfig();

    auto& lastNoteOf (this auto& self, Parameters::Instrument instrument) noexcept
    {
        switch (instrument)
        {
            case Parameters::Instrument::snare:  return self.lastSnareNote;
            case Parameters::Instrument::cymbal: return self.lastCymbalNote;
            case Parameters::Instrument::kick:   break;
        }

        return self.lastKickNote;
    }

    void startNote (int midiNote) noexcept;
    void fadeOutAll() noexcept;
    void renderVoices (float* output, int startSample, int endSample) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessor)
};
