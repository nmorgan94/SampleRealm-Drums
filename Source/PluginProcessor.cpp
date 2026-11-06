#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "shared/DrumSettings.h"
#include <BinaryData.h>

#if PLUGIN_WEBVIEW_EDITOR
 #include "webview/WebViewEditor.h"
#endif

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // Curve edits count as changes to the loaded preset, just like parameter moves
    envelopeModel.onUserEdit = [this] { presetManager.markModified(); };
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
}

void AudioPluginAudioProcessor::releaseResources()
{
    synth.stop();
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
    return synth.getLastNote (Parameters::instrumentFromIndex (juce::roundToInt (valueOf (ParameterSpecs::instrument))));
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

    // The drum is mono: render into the first channel, then copy it to the rest
    auto* mono = buffer.getWritePointer (0);
    synth.process (mono, numSamples, midiMessages);
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
   #if PLUGIN_WEBVIEW_EDITOR
    return new WebViewEditor (*this);
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
