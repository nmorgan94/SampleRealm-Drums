#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "params/ParameterFormats.h"

namespace
{
    constexpr int editorWidth  = 1080;
    constexpr int editorHeight = 640;
    constexpr double previewSampleRate = 44100.0;
    constexpr float maxPreviewSeconds = 8.0f;

    using Palette = CustomLookAndFeel::Palette;

    using Curve = srd::EnvelopeEditor::Curve;

    // The first curve is the one shown when switching to the instrument
    const juce::Array<Curve> kickCurves  { Curve { DrumEnvelopes::kickPitch, "Pitch", Palette::accent },
                                           Curve { DrumEnvelopes::kickAmp,   "Amp",   Palette::accentAlt } };

    const juce::Array<Curve> snareCurves { Curve { DrumEnvelopes::snarePitch, "Pitch", Palette::accent },
                                           Curve { DrumEnvelopes::snareBody,  "Body",  Palette::accentAlt },
                                           Curve { DrumEnvelopes::snareNoise, "Noise", Palette::accentAlt2 } };

    const juce::Array<Curve> cymbalCurves { Curve { DrumEnvelopes::cymbalMetal, "Metal", Palette::accentAlt },
                                            Curve { DrumEnvelopes::cymbalNoise, "Noise", Palette::accentAlt2 } };

    const juce::Array<Curve>& getCurves (Parameters::Instrument instrument)
    {
        switch (instrument)
        {
            case Parameters::Instrument::snare:  return snareCurves;
            case Parameters::Instrument::cymbal: return cymbalCurves;
            case Parameters::Instrument::kick:   break;
        }

        return kickCurves;
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
    addAndMakeVisible (outputMeter);

    // Pitch envelopes read in Hz and note names, level envelopes as percentages
    const auto isPitch = [this] (std::size_t env)
    {
        return processorRef.getEnvelopeModel().getSpec (env).scale == srd::EnvelopeSpec::Scale::logarithmic;
    };

    const auto percent = [] (float gain) { return juce::String (juce::roundToInt (gain * 100.0f)) + "%"; };

    envelopeEditor.formatValue = [isPitch, percent] (std::size_t env, float value)
    {
        return isPitch (env) ? srd::params::hertzText (value) + "  " + noteName (value) : percent (value);
    };

    envelopeEditor.formatGridValue = [isPitch, percent] (std::size_t env, float value)
    {
        if (! isPitch (env))
            return percent (value);

        return value >= 1000.0f ? juce::String (juce::roundToInt (value / 1000.0f)) + "k" : juce::String (juce::roundToInt (value));
    };

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
    addPanel (snareNoisePanel, { &snareNoiseLevel, &snareNoiseLowCut, &snareNoiseHighCut });
    addPanel (snapPanel,   { &snapLevel, &snapTone, &snapDecay, &snapPitch });
    addPanel (metalPanel,  { &metalLevel, &metalTone, &metalRing });
    addPanel (cymbalNoisePanel, { &cymbalNoiseLevel, &cymbalNoiseLowCut, &cymbalNoiseHighCut });
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

/** Swaps in the instrument's sound panels and envelope curves, editing its first curve. */
void AudioPluginAudioProcessorEditor::showInstrument()
{
    const auto instrument = getInstrument();

    // Hide every instrument's panels, then show this one's; the shared panels come straight back
    for (int i = 0; i < Parameters::instrumentNames.size(); ++i)
        for (auto* panel : getSoundPanels (Parameters::instrumentFromIndex (i)))
            panel->setVisible (false);

    for (auto* panel : getSoundPanels (instrument))
        panel->setVisible (true);

    // The cymbal has no pitch envelope for these to act on
    const auto hasPitch = instrument != Parameters::Instrument::cymbal;

    for (auto* control : std::initializer_list<juce::Component*> { &keyTrack, &pitchDepth })
    {
        control->setEnabled (hasPitch);
        control->setAlpha (hasPitch ? 1.0f : 0.35f);
    }

    const auto& curves = getCurves (instrument);
    envelopeEditor.setCurves (curves);
    envelopeEditor.setEnvelope (curves.getFirst().envelope);

    parametersChanged = true;
    resized();
}

juce::Array<srd::Panel*> AudioPluginAudioProcessorEditor::getSoundPanels (Parameters::Instrument instrument)
{
    switch (instrument)
    {
        case Parameters::Instrument::snare:  return { &bodyPanel, &snareNoisePanel, &snapPanel, &drivePanel, &eqPanel, &outputPanel };
        case Parameters::Instrument::cymbal: return { &metalPanel, &cymbalNoisePanel, &drivePanel, &eqPanel, &outputPanel };
        case Parameters::Instrument::kick:   break;
    }

    return { &subPanel, &clickPanel, &drivePanel, &eqPanel, &outputPanel };
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
        return juce::roundToInt (std::min ({ renderedViewSeconds, hitSeconds, maxPreviewSeconds }) * previewSampleRate);
    };

    const float* samples = nullptr;
    int numSamples = 0;
    std::optional<float> hz;  // where the pitch settles, for the Tail readout

    switch (getInstrument())
    {
        case Parameters::Instrument::kick:
        {
            const auto pitch = model.getData (DrumEnvelopes::kickPitch);
            const auto amp   = model.getData (DrumEnvelopes::kickAmp);
            const auto settings = engineParams.kickSettings (note, pitch);

            numSamples = samplesToShow (KickVoice::getDurationSeconds (amp.getDuration(), settings));
            samples = renderer.renderKick (pitch, amp, settings, fxSettings, numSamples);
            hz = srd::EnvelopedOscillator::getTailHz (pitch, settings.sub.frequencyRatio);
            break;
        }
        case Parameters::Instrument::snare:
        {
            const auto pitch = model.getData (DrumEnvelopes::snarePitch);
            const auto body  = model.getData (DrumEnvelopes::snareBody);
            const auto noise = model.getData (DrumEnvelopes::snareNoise);
            const auto settings = engineParams.snareSettings (note, pitch);

            numSamples = samplesToShow (SnareVoice::getDurationSeconds (body.getDuration(), noise.getDuration(), settings));
            samples = renderer.renderSnare (pitch, body, noise, settings, fxSettings, numSamples);
            hz = srd::EnvelopedOscillator::getTailHz (pitch, settings.body.frequencyRatio);
            break;
        }
        case Parameters::Instrument::cymbal:
        {
            const auto metal = model.getData (DrumEnvelopes::cymbalMetal);
            const auto noise = model.getData (DrumEnvelopes::cymbalNoise);
            const auto settings = engineParams.cymbalSettings();

            numSamples = samplesToShow (CymbalVoice::getDurationSeconds (metal.getDuration(), noise.getDuration(), settings));
            samples = renderer.renderCymbal (metal, noise, settings, fxSettings, numSamples);
            break;  // no tail note, so hz stays empty
        }
    }

    renderedEnvelopeVersion = version;
    renderedNote = note;

    waveform.setSamples (samples, numSamples, previewSampleRate);
    envelopeEditor.repaint();

    juce::String newNote, newHz;

    if (hz.has_value())
    {
        newNote = noteName (*hz);
        newHz = srd::params::hertzText (*hz);
    }

    if (newNote != tailNote || newHz != tailHz)
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
    if (tailNote.isNotEmpty())
    {
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
    }

    // Envelope panel
    const auto envelopeArea = envelopeEditor.getBounds().expanded (8).toFloat();
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
    instrumentSwitch.setBounds (top.removeFromLeft (220).withSizeKeepingCentre (220, 30));
    outputMeter.setBounds (top.removeFromRight (14).withSizeKeepingCentre (14, 40));
    top.removeFromRight (10);
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
    envelopeEditor.setBounds (bounds.withTrimmedRight (gap + 8).reduced (8));
}
