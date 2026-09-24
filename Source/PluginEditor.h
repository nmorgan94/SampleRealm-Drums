#pragma once

#include "PluginProcessor.h"
#include "engine/PreviewRenderer.h"
#include "ui/Controls.h"
#include "ui/CustomLookAndFeel.h"
#include "ui/EnvelopeEditor.h"
#include "ui/Panel.h"
#include "ui/PresetBar.h"
#include "ui/PromptOverlay.h"
#include "ui/TriggerPad.h"
#include "ui/Waveform.h"

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              private juce::Timer,
                                              private juce::AudioProcessorListener
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor&);
    ~AudioPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    AudioPluginAudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& apvts;

    CustomLookAndFeel customLookAndFeel;

    // Top bar
    srd::PromptOverlay prompt;
    srd::ChoiceButtons instrumentSwitch { apvts, Parameters::instrumentId };
    srd::PresetBar presetBar { processorRef.getPresetManager(), prompt };
    juce::String tailNote, tailHz;
    juce::Rectangle<int> titleArea, readoutArea;

    // Envelopes, drawn over a preview of one hit. The tabs are relabelled per instrument
    srd::SegmentedButtons envelopeTabs;
    srd::EnvelopeEditor envelopeEditor { processorRef.getEnvelopeModel() };
    srd::Waveform waveform;
    PreviewRenderer renderer;
    std::atomic<bool> parametersChanged { true };
    juce::uint32 renderedEnvelopeVersion = 0;
    int renderedNote = -1;
    float renderedViewSeconds = 0.0f;

    // Global
    srd::Panel globalPanel { "Global" };
    srd::Knob tune       { apvts, Parameters::tuneId };
    srd::Toggle keyTrack { apvts, Parameters::keyTrackId };
    srd::Knob length     { apvts, Parameters::lengthId };
    srd::Knob pitchDepth { apvts, Parameters::pitchDepthId };
    srd::TriggerPad triggerPad { "Hit" };
    juce::uint32 lastHitCount = 0;

    // Kick
    srd::Panel subPanel { "Sub" };
    srd::Knob subLevel     { apvts, Parameters::subLevelId, "Level" };
    srd::Knob subHarmonics { apvts, Parameters::subHarmonicsId };
    srd::Knob subPhase     { apvts, Parameters::subPhaseId };

    srd::Panel clickPanel { "Click" };
    srd::ChoiceBox clickType { apvts, Parameters::clickTypeId };
    srd::Knob clickLevel { apvts, Parameters::clickLevelId, "Level" };
    srd::Knob clickTone  { apvts, Parameters::clickToneId,  "Tone" };
    srd::Knob clickDecay { apvts, Parameters::clickDecayId, "Decay" };
    srd::Knob clickPitch { apvts, Parameters::clickPitchId, "Pitch" };

    // Snare
    srd::Panel bodyPanel { "Body" };
    srd::Knob bodyLevel     { apvts, Parameters::snareBodyLevelId,     "Level" };
    srd::Knob bodyHarmonics { apvts, Parameters::snareBodyHarmonicsId, "Harmonics" };

    srd::Panel noisePanel { "Noise" };
    srd::Knob noiseLevel    { apvts, Parameters::snareNoiseLevelId,   "Level" };
    srd::Knob noiseLowCut   { apvts, Parameters::snareNoiseLowCutId,  "Low Cut" };
    srd::Knob noiseHighCut  { apvts, Parameters::snareNoiseHighCutId, "High Cut" };

    srd::Panel snapPanel { "Snap" };
    srd::ChoiceBox snapType { apvts, Parameters::snareSnapTypeId };
    srd::Knob snapLevel { apvts, Parameters::snareSnapLevelId, "Level" };
    srd::Knob snapTone  { apvts, Parameters::snareSnapToneId,  "Tone" };
    srd::Knob snapDecay { apvts, Parameters::snareSnapDecayId, "Decay" };
    srd::Knob snapPitch { apvts, Parameters::snareSnapPitchId, "Pitch" };

    // Shared
    srd::Panel drivePanel { "Drive" };
    srd::ChoiceBox driveType { apvts, Parameters::driveTypeId };
    srd::Knob driveAmount { apvts, Parameters::driveAmountId, "Amount" };
    srd::Knob driveMix    { apvts, Parameters::driveMixId,    "Mix" };

    srd::Panel eqPanel { "EQ" };
    srd::Knob eqLow     { apvts, Parameters::eqLowGainId };
    srd::Knob eqMidFreq { apvts, Parameters::eqMidFreqId };
    srd::Knob eqMid     { apvts, Parameters::eqMidGainId };
    srd::Knob eqHigh    { apvts, Parameters::eqHighGainId };

    srd::Panel outputPanel { "Output" };
    srd::Knob clip   { apvts, Parameters::clipAmountId };
    srd::Knob output { apvts, Parameters::outputGainId, "Gain" };

    Parameters::Instrument getInstrument() const;
    void showInstrument();
    void showEnvelope (std::size_t env);
    juce::Array<srd::Panel*> getSoundPanels (Parameters::Instrument);

    void updatePreview();

    void timerCallback() override;
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override   { parametersChanged = true; }
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
