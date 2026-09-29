#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // Curve edits count as changes to the loaded preset, just like parameter moves
    envelopeModel.onUserEdit = [this] { presetManager.markModified(); };
    envelopeModel.tryGetLatest (envelopes, envelopeVersion);
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
    envelopeModel.onUserEdit = nullptr;
}

srd::PresetManager::Config AudioPluginAudioProcessor::createPresetConfig()
{
    srd::PresetManager::Config config;
    config.fileExtension = ".srdrum";
    config.userDirectory = srd::PresetManager::getDefaultUserDirectory (JucePlugin_Manufacturer, JucePlugin_Name);
    config.author        = JucePlugin_Manufacturer;
    config.formatVersion = Parameters::versionHint;

    // Presets are typed by instrument, and Init keeps the instrument you're on
    config.typeParameterId = Parameters::instrumentId.getParamID();

    // Factory presets are embedded from Assets/Presets alongside the fonts
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const auto* resource = BinaryData::namedResourceList[i];

        if (! juce::String (BinaryData::getNamedResourceOriginalFilename (resource)).endsWithIgnoreCase (config.fileExtension))
            continue;

        int size = 0;

        if (const auto* data = BinaryData::getNamedResource (resource, size))
            config.factoryPresets.emplace_back (data, (size_t) size);
    }

    return config;
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
    return true;
}

bool AudioPluginAudioProcessor::producesMidi() const
{
    return false;
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
    return false;
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    const auto latency = getSampleRate() > 0.0 ? getLatencySamples() / getSampleRate() : 0.0;

    switch (engineParams.getInstrument())
    {
        case Parameters::Instrument::snare:
            return latency + SnareVoice::getDurationSeconds (envelopeModel.getDuration (DrumEnvelopes::snareBody),
                                                             envelopeModel.getDuration (DrumEnvelopes::snareNoise),
                                                             engineParams.snareSettings (0, {}));
        case Parameters::Instrument::cymbal:
            return latency + CymbalVoice::getDurationSeconds (envelopeModel.getDuration (DrumEnvelopes::cymbalMetal),
                                                              envelopeModel.getDuration (DrumEnvelopes::cymbalNoise),
                                                              engineParams.cymbalSettings());
        case Parameters::Instrument::kick:
            break;
    }

    return latency + KickVoice::getDurationSeconds (envelopeModel.getDuration (DrumEnvelopes::kickAmp), engineParams.kickSettings (0, {}));
}

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1. Presets are managed in the plugin UI instead.
}

int AudioPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram (int)
{
}

const juce::String AudioPluginAudioProcessor::getProgramName (int)
{
    return {};
}

void AudioPluginAudioProcessor::changeProgramName (int, const juce::String&)
{
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    kickVoices.prepare (sampleRate);
    snareVoices.prepare (sampleRate);
    cymbalVoices.prepare (sampleRate);

    fxChain.prepare (sampleRate, samplesPerBlock);
    fxChain.setSettings (engineParams.fxSettings());
    fxChain.reset();

    setLatencySamples (fxChain.getLatencySamples());
}

void AudioPluginAudioProcessor::releaseResources()
{
    kickVoices.stop();
    snareVoices.stop();
    cymbalVoices.stop();
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Output only: the drum is mono, copied to every output channel
    if (! layouts.getMainInputChannelSet().isDisabled())
        return false;

    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
int AudioPluginAudioProcessor::getLastNote() const noexcept
{
    return lastNoteOf (engineParams.getInstrument()).load();
}

void AudioPluginAudioProcessor::startNote (int midiNote) noexcept
{
    fadeOutAll();

    switch (playing)
    {
        case Parameters::Instrument::kick:
        {
            const auto& pitch = envelopes[DrumEnvelopes::kickPitch];
            kickVoices.getFreeVoice().start (pitch, envelopes[DrumEnvelopes::kickAmp], engineParams.kickSettings (midiNote, pitch));
            break;
        }
        case Parameters::Instrument::snare:
        {
            const auto& pitch = envelopes[DrumEnvelopes::snarePitch];
            snareVoices.getFreeVoice().start (pitch, envelopes[DrumEnvelopes::snareBody], envelopes[DrumEnvelopes::snareNoise],
                                              engineParams.snareSettings (midiNote, pitch));
            break;
        }
        case Parameters::Instrument::cymbal:
            cymbalVoices.getFreeVoice().start (envelopes[DrumEnvelopes::cymbalMetal], envelopes[DrumEnvelopes::cymbalNoise],
                                               engineParams.cymbalSettings());
            break;
    }

    // The cymbal ignores the note
    if (playing != Parameters::Instrument::cymbal)
        lastNoteOf (playing) = midiNote;

    ++hitCount;
}

void AudioPluginAudioProcessor::fadeOutAll() noexcept
{
    kickVoices.fadeOut();
    snareVoices.fadeOut();
    cymbalVoices.fadeOut();
}

void AudioPluginAudioProcessor::renderVoices (float* output, int startSample, int endSample) noexcept
{
    kickVoices.render (output + startSample, endSample - startSample);
    snareVoices.render (output + startSample, endSample - startSample);
    cymbalVoices.render (output + startSample, endSample - startSample);
}

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();

    if (numSamples == 0 || buffer.getNumChannels() == 0)
        return;

    envelopeModel.tryGetLatest (envelopes, envelopeVersion);
    fxChain.setSettings (engineParams.fxSettings());

    // A new instrument fades out the old one's hits
    if (const auto instrument = engineParams.getInstrument(); instrument != playing)
    {
        fadeOutAll();
        playing = instrument;
    }

    // The drum is mono: render into the first channel, then copy it to the rest
    auto* mono = buffer.getWritePointer (0);
    juce::FloatVectorOperations::clear (mono, numSamples);

    if (auditionPending.exchange (false))
        startNote (lastNoteOf (playing).load());

    // Split the block at each note-on so hits are sample-accurate
    int position = 0;

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();

        if (! message.isNoteOn() && ! message.isAllNotesOff() && ! message.isAllSoundOff())
            continue;

        const auto eventPosition = juce::jlimit (0, numSamples, metadata.samplePosition);
        renderVoices (mono, position, eventPosition);
        position = eventPosition;

        if (message.isNoteOn())
            startNote (message.getNoteNumber());
        else
            fadeOutAll();
    }

    renderVoices (mono, position, numSamples);
    fxChain.process (mono, numSamples);
    outputMeter.process (mono, numSamples);

    for (int channel = 1; channel < buffer.getNumChannels(); ++channel)
        buffer.copyFrom (channel, 0, mono, numSamples);
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor (*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    presetManager.addPresetInfoTo (state);

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    // Envelopes reload automatically when the state tree is replaced
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        presetManager.restoreState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}
