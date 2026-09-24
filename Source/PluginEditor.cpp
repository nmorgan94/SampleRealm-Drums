#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "params/ParameterFormats.h"

namespace
{
    constexpr int editorWidth  = 1080;
    constexpr int editorHeight = 640;
    constexpr double previewSampleRate = 44100.0;

    using Palette = CustomLookAndFeel::Palette;

    struct EnvelopeTab
    {
        const char* name;
        std::size_t envelope;
    };

    const juce::Array<EnvelopeTab> kickTabs  { EnvelopeTab { "Pitch", DrumEnvelopes::kickPitch },
                                               EnvelopeTab { "Amp",   DrumEnvelopes::kickAmp } };

    const juce::Array<EnvelopeTab> snareTabs { EnvelopeTab { "Pitch", DrumEnvelopes::snarePitch },
                                               EnvelopeTab { "Body",  DrumEnvelopes::snareBody },
                                               EnvelopeTab { "Noise", DrumEnvelopes::snareNoise } };

    const juce::Array<EnvelopeTab>& getEnvelopeTabs (Parameters::Instrument instrument)
    {
        return instrument == Parameters::Instrument::snare ? snareTabs : kickTabs;
    }

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
    addAndMakeVisible (instrumentSwitch);
    addAndMakeVisible (presetBar);

    envelopeTabs.onChange = [this] (int index) { showEnvelope (getEnvelopeTabs (getInstrument())[index].envelope); };
    addAndMakeVisible (envelopeTabs);

    envelopeEditor.setFont (customLookAndFeel.font (11.0f));
    envelopeEditor.paintBackground = [this] (juce::Graphics& g, juce::Rectangle<float> area)
    {
        waveform.draw (g, area, envelopeEditor.getViewSeconds(), Palette::waveform.withAlpha (0.5f));
    };
    addAndMakeVisible (envelopeEditor);

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
    addPanel (bodyPanel,   { &bodyLevel, &bodyHarmonics });
    addPanel (noisePanel,  { &noiseLevel, &noiseLowCut, &noiseHighCut });
    addPanel (snapPanel,   { &snapLevel, &snapTone, &snapDecay, &snapPitch });
    addPanel (drivePanel,  { &driveAmount, &driveMix });
    addPanel (eqPanel,     { &eqLow, &eqMidFreq, &eqMid, &eqHigh });
    addPanel (outputPanel, { &clip, &output });

    globalPanel.setColumns (2);
    clickPanel.setHeaderComponent (clickType, 84);
    snapPanel.setHeaderComponent (snapType, 84);
    drivePanel.setHeaderComponent (driveType, 72);

    instrumentSwitch.onChange = [this] (int) { showInstrument(); };
    showInstrument();

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
Parameters::Instrument AudioPluginAudioProcessorEditor::getInstrument() const
{
    return Parameters::instrumentFromIndex (instrumentSwitch.getSelectedIndex());
}

/** Swaps in the instrument's sound panels and envelope tabs, keeping the same tab where it has one. */
void AudioPluginAudioProcessorEditor::showInstrument()
{
    const auto instrument = getInstrument();

    // Hide every instrument's panels, then show this one's; the shared panels come straight back
    for (auto each : { Parameters::Instrument::kick, Parameters::Instrument::snare })
        for (auto* panel : getSoundPanels (each))
            panel->setVisible (false);

    for (auto* panel : getSoundPanels (instrument))
        panel->setVisible (true);

    juce::StringArray tabNames;

    for (const auto& tab : getEnvelopeTabs (instrument))
        tabNames.add (tab.name);

    envelopeTabs.setItems (tabNames);
    showEnvelope (getEnvelopeTabs (instrument)[envelopeTabs.getSelectedIndex()].envelope);

    parametersChanged = true;
    resized();
}

juce::Array<srd::Panel*> AudioPluginAudioProcessorEditor::getSoundPanels (Parameters::Instrument instrument)
{
    if (instrument == Parameters::Instrument::snare)
        return { &bodyPanel, &noisePanel, &snapPanel, &drivePanel, &eqPanel, &outputPanel };

    return { &subPanel, &clickPanel, &drivePanel, &eqPanel, &outputPanel };
}

void AudioPluginAudioProcessorEditor::showEnvelope (std::size_t env)
{
    const auto isPitch = processorRef.getEnvelopeModel().getSpec (env).scale == srd::EnvelopeSpec::Scale::logarithmic;

    envelopeEditor.setEnvelope (env);
    // Pitch in the main accent, gain in the other, on both the tab and the curve
    const auto colour = isPitch ? Palette::accent : Palette::accentAlt;
    envelopeTabs.setSelectedColour (colour);
    envelopeEditor.setColour (srd::EnvelopeEditor::lineColourId, colour);

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

    const auto& engineParams = processorRef.getEngineParams();
    const auto fxSettings = engineParams.fxSettings();

    envelopeEditor.setTimeScale (engineParams.getLengthScale());
    renderedViewSeconds = envelopeEditor.getViewSeconds();

    const auto samplesToShow = [this] (float hitSeconds)
    {
        return juce::roundToInt (std::min (renderedViewSeconds, hitSeconds) * previewSampleRate);
    };

    const float* samples = nullptr;
    int numSamples = 0;
    float hz = 0.0f;

    if (getInstrument() == Parameters::Instrument::snare)
    {
        const auto pitch = model.getData (DrumEnvelopes::snarePitch);
        const auto body  = model.getData (DrumEnvelopes::snareBody);
        const auto noise = model.getData (DrumEnvelopes::snareNoise);
        const auto settings = engineParams.snareSettings (note, pitch);

        numSamples = samplesToShow (SnareVoice::getDurationSeconds (body.getDuration(), noise.getDuration(), settings));
        samples = renderer.renderSnare (pitch, body, noise, settings, fxSettings, numSamples);
        hz = srd::EnvelopedOscillator::getTailHz (pitch, settings.body.frequencyRatio);
    }
    else
    {
        const auto pitch = model.getData (DrumEnvelopes::kickPitch);
        const auto amp   = model.getData (DrumEnvelopes::kickAmp);
        const auto settings = engineParams.kickSettings (note, pitch);

        numSamples = samplesToShow (KickVoice::getDurationSeconds (amp.getDuration(), settings));
        samples = renderer.renderKick (pitch, amp, settings, fxSettings, numSamples);
        hz = srd::EnvelopedOscillator::getTailHz (pitch, settings.sub.frequencyRatio);
    }

    renderedEnvelopeVersion = version;
    renderedNote = note;

    waveform.setSamples (samples, numSamples, previewSampleRate);
    envelopeEditor.repaint();

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
    const auto envelopeArea = envelopeEditor.getBounds().getUnion (envelopeTabs.getBounds()).expanded (8).toFloat();
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
    titleArea = top.removeFromLeft (150);
    instrumentSwitch.setBounds (top.removeFromLeft (150).withSizeKeepingCentre (150, 30));
    readoutArea = top.removeFromRight (150);
    presetBar.setBounds (top.withSizeKeepingCentre (std::min (top.getWidth() - 24, 460), 30));

    bounds.removeFromTop (14);

    // Sound panels along the bottom, sized by their number of controls
    constexpr int gap = 10;
    srd::Panel::layOutRow (getSoundPanels (getInstrument()), bounds.removeFromBottom (150), gap);
    bounds.removeFromBottom (12);

    // Global controls and the trigger pad on the right
    auto right = bounds.removeFromRight (240);
    globalPanel.setBounds (right.removeFromTop (262));
    right.removeFromTop (gap);
    triggerPad.setBounds (right);

    // Envelope editor fills the rest, inside its panel
    auto envelope = bounds.withTrimmedRight (gap + 8).reduced (8);
    envelopeTabs.setBounds (envelope.removeFromTop (24).removeFromLeft (70 * envelopeTabs.getNumItems()));
    envelope.removeFromTop (4);
    envelopeEditor.setBounds (envelope);
}
