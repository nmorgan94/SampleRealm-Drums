#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "params/ParameterFormats.h"

namespace
{
    constexpr int editorWidth  = 1080;
    constexpr int editorHeight = 640;
    constexpr double previewSampleRate = 44100.0;

    using Palette = CustomLookAndFeel::Palette;

    /** Nearest note, with middle C as C4 (so 43.65 Hz is F1). */
    juce::String noteName (float hz)
    {
        const auto note = juce::roundToInt (69.0f + 12.0f * std::log2 (hz / 440.0f));
        return juce::MidiMessage::getMidiNoteName (note, true, true, 4);
    }
}

//==============================================================================
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), apvts (p.getAPVTS())
{
    // Top bar
    addAndMakeVisible (presetBar);

    // Envelopes
    for (auto* tab : { &pitchTab, &ampTab })
    {
        tab->setRadioGroupId (1);
        tab->setClickingTogglesState (true);
        addAndMakeVisible (tab);
    }

    ampTab.setColour (juce::TextButton::buttonOnColourId, Palette::accentAlt);
    pitchTab.onClick = [this] { showEnvelope (KickEnvelopes::pitch); };
    ampTab.onClick   = [this] { showEnvelope (KickEnvelopes::amp); };

    envelopeEditor.setFont (customLookAndFeel.font (11.0f));
    envelopeEditor.paintBackground = [this] (juce::Graphics& g, juce::Rectangle<float> area)
    {
        waveform.draw (g, area, envelopeEditor.getViewSeconds(), Palette::waveform.withAlpha (0.5f));
    };
    addAndMakeVisible (envelopeEditor);
    pitchTab.setToggleState (true, juce::dontSendNotification);
    showEnvelope (KickEnvelopes::pitch);

    renderer.prepare (previewSampleRate);

    // Panels
    const auto titleFont = customLookAndFeel.font (12.0f, true);

    const auto addPanel = [&] (srd::Panel& panel, std::initializer_list<juce::Component*> controls)
    {
        panel.setFont (titleFont);

        for (auto* c : controls)
            panel.addControl (*c);

        addAndMakeVisible (panel);
    };

    addPanel (globalPanel, { &tune, &keyTrack, &length, &pitchDepth });
    addPanel (subPanel,    { &subLevel, &subHarmonics, &subPhase });
    addPanel (clickPanel,  { &clickLevel, &clickTone, &clickDecay, &clickPitch });
    addPanel (drivePanel,  { &driveAmount, &driveMix });
    addPanel (eqPanel,     { &eqLow, &eqMidFreq, &eqMid, &eqHigh });
    addPanel (outputPanel, { &clip, &output });

    globalPanel.setColumns (2);
    clickPanel.setHeaderComponent (clickType, 84);
    drivePanel.setHeaderComponent (driveType, 72);

    triggerPad.setFont (customLookAndFeel.font (18.0f, true));
    triggerPad.onTrigger = [this] { processorRef.triggerAudition(); };
    addAndMakeVisible (triggerPad);

    prompt.setFont (customLookAndFeel.font (14.0f));
    addChildComponent (prompt);

    setLookAndFeel (&customLookAndFeel);

    processorRef.addListener (this);
    lastHitCount = processorRef.getHitCount();
    updatePreview();
    startTimerHz (30);

    setSize (editorWidth, editorHeight);
}

AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor()
{
    processorRef.removeListener (this);
    setLookAndFeel (nullptr);
}

//==============================================================================
void AudioPluginAudioProcessorEditor::showEnvelope (std::size_t env)
{
    const auto isPitch = env == KickEnvelopes::pitch;

    envelopeEditor.setEnvelope (env);
    envelopeEditor.setColour (srd::EnvelopeEditor::lineColourId, isPitch ? Palette::accent : Palette::accentAlt);

    if (isPitch)
    {
        envelopeEditor.formatValue = [] (float hz) { return srd::params::hertzText (hz) + "  " + noteName (hz); };
        envelopeEditor.formatGridValue = [] (float hz)
        {
            return hz >= 1000.0f ? juce::String (juce::roundToInt (hz / 1000.0f)) + "k" : juce::String (juce::roundToInt (hz));
        };
    }
    else
    {
        envelopeEditor.formatValue = envelopeEditor.formatGridValue = [] (float gain)
        {
            return juce::String (juce::roundToInt (gain * 100.0f)) + "%";
        };
    }
}

/** Re-renders the waveform when the sound, the note or the view changes, and updates the tail readout. */
void AudioPluginAudioProcessorEditor::updatePreview()
{
    auto& model = processorRef.getEnvelopeModel();
    const auto version = model.getVersion();
    const auto note = processorRef.getLastNote();

    // The editor refreshes its view on its own timer, so a new view can arrive a tick after the curve
    if (! parametersChanged.exchange (false) && version == renderedEnvelopeVersion && note == renderedNote
        && juce::approximatelyEqual (envelopeEditor.getViewSeconds(), renderedViewSeconds))
        return;

    const auto pitch = model.getData (KickEnvelopes::pitch);
    const auto amp   = model.getData (KickEnvelopes::amp);

    const auto& engineParams = processorRef.getEngineParams();
    const auto settings = engineParams.voiceSettings (note, pitch);

    // The time scale sets the view, which sets how much to show
    envelopeEditor.setTimeScale (settings.lengthScale);

    renderedEnvelopeVersion = version;
    renderedNote = note;
    renderedViewSeconds = envelopeEditor.getViewSeconds();

    // Only the hit itself is rendered; the rest of the view is silence
    const auto hitSeconds = KickVoice::getDurationSeconds (amp.getDuration(), settings);
    const auto numSamples = juce::roundToInt (std::min (renderedViewSeconds, hitSeconds) * previewSampleRate);

    waveform.setSamples (renderer.render (pitch, amp, settings, engineParams.fxSettings(), numSamples),
                         numSamples, previewSampleRate);
    envelopeEditor.repaint();

    const auto hz = KickVoice::getTailHz (pitch, settings.frequencyRatio);

    if (const auto newNote = noteName (hz), newHz = srd::params::hertzText (hz); newNote != tailNote || newHz != tailHz)
    {
        tailNote = newNote;
        tailHz = newHz;
        repaint (readoutArea);
    }
}

void AudioPluginAudioProcessorEditor::timerCallback()
{
    if (const auto hits = processorRef.getHitCount(); hits != lastHitCount)
    {
        lastHitCount = hits;
        triggerPad.flash();
    }

    updatePreview();
}

//==============================================================================
void AudioPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    // Title
    auto title = titleArea;

    g.setColour (Palette::textDim);
    g.setFont (customLookAndFeel.font (11.0f, true));
    g.drawText ("SAMPLEREALM", title.removeFromTop (16), juce::Justification::bottomLeft);
    g.setColour (Palette::text);
    g.setFont (customLookAndFeel.font (22.0f, true));
    g.drawText ("DRUMS", title, juce::Justification::centredLeft);

    // Tail readout: where the last note played (and the Hit pad) settles
    auto readout = readoutArea;
    g.setColour (Palette::textDim);
    g.setFont (customLookAndFeel.font (11.0f, true));
    g.drawText ("TAIL", readout.removeFromTop (16), juce::Justification::centredRight);
    g.setColour (Palette::accent);
    g.setFont (customLookAndFeel.font (18.0f, true));
    g.drawText (tailNote, readout.removeFromTop (22), juce::Justification::centredRight);
    g.setColour (Palette::textDim);
    g.setFont (customLookAndFeel.font (11.0f));
    g.drawText (tailHz, readout, juce::Justification::centredRight);

    // Envelope panel
    const auto envelopeArea = envelopeEditor.getBounds().getUnion (pitchTab.getBounds()).expanded (8).toFloat();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (envelopeArea, 6.0f);
    g.setColour (Palette::outline);
    g.drawRoundedRectangle (envelopeArea.reduced (0.5f), 6.0f, 1.0f);
}

void AudioPluginAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced (16, 12);

    // Top bar
    auto top = bounds.removeFromTop (44);
    titleArea = top.removeFromLeft (220);
    readoutArea = top.removeFromRight (150);
    presetBar.setBounds (top.withSizeKeepingCentre (std::min (top.getWidth() - 24, 460), 30));

    bounds.removeFromTop (14);

    // Sound panels along the bottom, sized by their number of controls
    auto bottom = bounds.removeFromBottom (150);
    bounds.removeFromBottom (12);

    const juce::Array<srd::Panel*> panels { &subPanel, &clickPanel, &drivePanel, &eqPanel, &outputPanel };
    constexpr int gap = 10;

    int numControls = 0;

    for (auto* panel : panels)
        numControls += panel->getNumControls();

    const auto controlWidth = (bottom.getWidth() - gap * (panels.size() - 1)) / numControls;

    for (auto* panel : panels)
    {
        // The last panel takes whatever the rounding leaves
        panel->setBounds (panel == panels.getLast() ? bottom : bottom.removeFromLeft (controlWidth * panel->getNumControls()));
        bottom.removeFromLeft (gap);
    }

    // Global controls and the trigger pad on the right
    auto right = bounds.removeFromRight (240);
    globalPanel.setBounds (right.removeFromTop (262));
    right.removeFromTop (gap);
    triggerPad.setBounds (right);

    // Envelope editor fills the rest, inside its panel
    auto envelope = bounds.withTrimmedRight (gap + 8).reduced (8);
    auto tabs = envelope.removeFromTop (24);
    pitchTab.setBounds (tabs.removeFromLeft (70));
    tabs.removeFromLeft (4);
    ampTab.setBounds (tabs.removeFromLeft (70));
    envelope.removeFromTop (4);
    envelopeEditor.setBounds (envelope);
}
