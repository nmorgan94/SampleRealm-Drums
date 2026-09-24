#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&customLookAndFeel);

    triggerButton.onClick  = [this] { processorRef.triggerAudition(); };
    previousButton.onClick = [this] { processorRef.getPresetManager().loadPrevious(); };
    nextButton.onClick     = [this] { processorRef.getPresetManager().loadNext(); };

    presetLabel.setJustificationType (juce::Justification::centred);

    for (auto* c : std::initializer_list<juce::Component*> { &triggerButton, &previousButton, &nextButton,
                                                             &presetLabel, &genericEditor })
        addAndMakeVisible (c);

    updatePresetLabel();
    startTimerHz (15);

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (520, 720);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void AudioPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (8);

    auto top = bounds.removeFromTop (32);
    triggerButton.setBounds (top.removeFromLeft (100));
    top.removeFromLeft (8);
    previousButton.setBounds (top.removeFromLeft (32));
    nextButton.setBounds (top.removeFromRight (32));
    presetLabel.setBounds (top);

    bounds.removeFromTop (8);
    genericEditor.setBounds (bounds);
}

void AudioPluginAudioProcessorEditor::timerCallback()
{
    if (processorRef.getPresetManager().getChangeCounter() != lastPresetChange)
        updatePresetLabel();
}

void AudioPluginAudioProcessorEditor::updatePresetLabel()
{
    auto& presets = processorRef.getPresetManager();
    lastPresetChange = presets.getChangeCounter();
    presetLabel.setText (presets.getCurrent().name + (presets.isModified() ? " *" : ""), juce::dontSendNotification);
}
