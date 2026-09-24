#pragma once

#include "PluginProcessor.h"
#include "ui/CustomLookAndFeel.h"

//==============================================================================
/**
 * Interim editor: generic parameter controls, a Trigger button and preset stepping.
 * Replaced by the custom UI (envelope editor, knob panels, preset bar) in the next step.
 */
class AudioPluginAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                              private juce::Timer
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

    CustomLookAndFeel customLookAndFeel;

    juce::TextButton triggerButton { "Trigger" };
    juce::TextButton previousButton { "<" }, nextButton { ">" };
    juce::Label presetLabel;
    juce::GenericAudioProcessorEditor genericEditor { processorRef };

    juce::uint32 lastPresetChange = 0;

    void timerCallback() override;
    void updatePresetLabel();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
