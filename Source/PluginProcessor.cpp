#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "shared/DrumSettings.h"
#include <BinaryData.h>
#include <samplerealm/Transport.h>

#if PLUGIN_WEBVIEW_EDITOR
 #include "webview/DrumsWebView.h"
#endif

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // Curve edits count as changes to the loaded preset, just like parameter moves: PresetManager hears this
    envelopeModel.onUserEdit = [this] { updateHostDisplay (ChangeDetails{}.withNonParameterStateChanged (true)); };
    updateEnvelopes();
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
    envelopeModel.onUserEdit = nullptr;
}

AudioPluginAudioProcessor::ParameterValues AudioPluginAudioProcessor::findParameterValues (juce::AudioProcessorValueTreeState& state)
{
    ParameterValues values;

    std::ranges::transform (ParameterSpecs::all, values.begin(), [&state] (const ParameterSpecs::Spec* spec)
    {
        auto* value = state.getRawParameterValue (spec->id);
        jassert (value != nullptr);
        return value;
    });

    return values;
}

float AudioPluginAudioProcessor::valueOf (const ParameterSpecs::Spec& spec) const noexcept
{
    return parameterValues[ParameterSpecs::indexOf (spec)]->load();
}

DrumSynth::Settings AudioPluginAudioProcessor::getDrumSettings() const noexcept
{
    return makeDrumSettings ([this] (const ParameterSpecs::Spec& spec) { return valueOf (spec); });
}

DrumSynth::Envelopes AudioPluginAudioProcessor::getDrumEnvelopes() const
{
    DrumSynth::Envelopes current;

    for (std::size_t env = 0; env < current.size(); ++env)
        current[env] = envelopeModel.getData (env);

    return current;
}

void AudioPluginAudioProcessor::updateEnvelopes() noexcept
{
    if (envelopeModel.tryGetLatest (envelopes, envelopeVersion))
        for (std::size_t env = 0; env < DrumEnvelopes::count; ++env)
            synth.setEnvelope (env, envelopes[env]);
}

PresetManager::Config AudioPluginAudioProcessor::createPresetConfig()
{
    PresetManager::Config config;
    config.fileExtension = ".srdrum";
    config.userDirectory = PresetManager::getDefaultUserDirectory (JucePlugin_Manufacturer, JucePlugin_Name);
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

    DrumSynth::Durations durations;

    for (std::size_t env = 0; env < durations.size(); ++env)
        durations[env] = envelopeModel.getDuration (env);

    return latency + DrumSynth::getHitSeconds (getDrumSettings(), durations);
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
    synth.prepare (sampleRate, samplesPerBlock, getDrumSettings());
    setLatencySamples (synth.getLatencySamples());
    outputMeter.setNumChannels (getTotalNumOutputChannels());
    midiWithLoop.ensureSize (2048);
}

void AudioPluginAudioProcessor::releaseResources()
{
    synth.stop();
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Output only
    if (! layouts.getMainInputChannelSet().isDisabled())
        return false;

    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
int AudioPluginAudioProcessor::getLastNote() const noexcept
{
    return synth.getLastNote (getInstrument());
}

Parameters::Instrument AudioPluginAudioProcessor::getInstrument() const noexcept
{
    return Parameters::instrumentFromIndex (juce::roundToInt (valueOf (ParameterSpecs::instrument)));
}

const juce::MidiBuffer& AudioPluginAudioProcessor::withLoopHits (const juce::MidiBuffer& midi, int numSamples) noexcept
{
    if (! loop.isOn())
    {
        loopClock.reset();
        loop.stop();
        return midi;
    }

    const auto transport = Transport::from (getPlayHead());
    const auto instrument = getInstrument();
    const auto ppq = loopClock.advance (transport, juce::Time::getMillisecondCounterHiRes() * 0.001, numSamples, getSampleRate());

    midiWithLoop.clear();
    midiWithLoop.addEvents (midi, 0, numSamples, 0);
    loop.process (midiWithLoop, instrument, synth.getLastNote (instrument), ppq, transport.bpm, getSampleRate(), numSamples);
    return midiWithLoop;
}

void AudioPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();

    if (numSamples == 0 || buffer.getNumChannels() == 0)
        return;

    updateEnvelopes();
    synth.setSettings (getDrumSettings());

    // A mono layout gets the drum without the cymbal's Width
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    synth.process (buffer.getWritePointer (0), right, numSamples, withLoopHits (midiMessages, numSamples));
    outputMeter.process (buffer);
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
   #if PLUGIN_WEBVIEW_EDITOR
    return createDrumsWebViewEditor (*this);
   #else
    return new AudioPluginAudioProcessorEditor (*this);
   #endif
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
