#pragma once

#include "PluginProcessor.h"
#include "dsp/PreviewRenderer.h"
#include "ui/About.h"
#include "ui/Controls.h"
#include "ui/CustomLookAndFeel.h"
#include "ui/EnvelopeEditor.h"
#include "ui/HeaderBar.h"
#include "ui/Manual.h"
#include "ui/Panel.h"
#include "ui/PromptOverlay.h"
#include "ui/TailMeter.h"
#include "ui/TriggerPad.h"
#include "ui/Waveform.h"

//==============================================================================
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              private juce::Timer,
                                              private juce::AudioProcessorListener
{
public:
    static constexpr int width = 1080, height = 610;

    // As HEADER_TITLE and HEADER_LAYOUT in webui/src/Editor.tsx
    inline static const srd::HeaderBar::Title headerTitle { "SAMPLEREALM", "DRUMS" };
    static constexpr srd::HeaderBar::Layout headerLayout { .height = 68, .edge = 16, .titleWidth = 150, .titleHeight = 22.0f,
                                                           .switchWidth = 220, .gap = 22, .controlsHeight = 30, .menuWidth = 28, .menuGap = 4,
                                                           .meterWidth = srd::TailMeter::preferredWidth, .meterInset = 12 };

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
    CustomLookAndFeel overlayLookAndFeel; // labels in the bright text colour

    // Top bar
    srd::PromptOverlay prompt;
    srd::ManualOverlay manual;
    srd::ChoiceButtons instrumentSwitch { apvts, Parameters::instrumentId };
    srd::TailMeter tailMeter { processorRef.getOutputMeter() };
    srd::HeaderBar header { headerTitle, headerLayout, instrumentSwitch, processorRef.getPresetManager(), prompt,
                            [this] { manual.show(); }, About::text (processorRef), tailMeter };

    // The instrument's envelopes, drawn together over a preview of one hit
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

    srd::Panel snareNoisePanel { "Noise" };
    srd::Knob snareNoiseLevel   { apvts, Parameters::snareNoiseLevelId,   "Level" };
    srd::Knob snareNoiseLowCut  { apvts, Parameters::snareNoiseLowCutId,  "Low Cut" };
    srd::Knob snareNoiseHighCut { apvts, Parameters::snareNoiseHighCutId, "High Cut" };

    srd::Panel snapPanel { "Snap" };
    srd::ChoiceBox snapType { apvts, Parameters::snareSnapTypeId };
    srd::Knob snapLevel { apvts, Parameters::snareSnapLevelId, "Level" };
    srd::Knob snapTone  { apvts, Parameters::snareSnapToneId,  "Tone" };
    srd::Knob snapDecay { apvts, Parameters::snareSnapDecayId, "Decay" };
    srd::Knob snapPitch { apvts, Parameters::snareSnapPitchId, "Pitch" };

    // Cymbal
    srd::Panel metalPanel { "Metal" };
    srd::Knob metalLevel { apvts, Parameters::cymbalMetalLevelId, "Level" };
    srd::Knob metalTone  { apvts, Parameters::cymbalMetalToneId,  "Tone" };
    srd::Knob metalRing  { apvts, Parameters::cymbalMetalRingId,  "Ring" };

    srd::Panel cymbalNoisePanel { "Noise" };
    srd::Knob cymbalNoiseLevel   { apvts, Parameters::cymbalNoiseLevelId,   "Level" };
    srd::Knob cymbalNoiseLowCut  { apvts, Parameters::cymbalNoiseLowCutId,  "Low Cut" };
    srd::Knob cymbalNoiseHighCut { apvts, Parameters::cymbalNoiseHighCutId, "High Cut" };

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
    juce::Array<srd::Panel*> getSoundPanels (Parameters::Instrument);

    void updatePreview();

    void timerCallback() override;
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override   { parametersChanged = true; }
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
