#pragma once

#include "../PluginProcessor.h"
#include "../dsp/PreviewRenderer.h"

//==============================================================================
class WebViewEditor final : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit WebViewEditor (AudioPluginAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    juce::WebBrowserComponent::Options createOptions();
    juce::WebBrowserComponent::Options withPresetFunctions (const juce::WebBrowserComponent::Options&);
    juce::WebBrowserComponent::Options withDrumFunctions (const juce::WebBrowserComponent::Options&);
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);
    juce::var lastNotes() const;

    AudioPluginAudioProcessor& processorRef;
    PreviewRenderer previewRenderer;

    // Declaration order matters: zip, relays, browser, attachments
    juce::MemoryInputStream uiZipStream;
    juce::ZipFile uiZip { uiZipStream };
    std::vector<std::unique_ptr<juce::WebSliderRelay>> relays;
    std::unique_ptr<juce::WebBrowserComponent> browser;
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>> attachments;

    // What the page has seen
    juce::uint32 lastPresetChange = 0, lastEnvelopeVersion = 0, lastHitCount = 0;
    juce::var lastSentNotes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebViewEditor)
};
