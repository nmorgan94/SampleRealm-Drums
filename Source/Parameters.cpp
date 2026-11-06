#include "Parameters.h"
#include "params/ParameterFormats.h"

//==============================================================================
namespace
{
    using ParameterSpecs::Spec;

    juce::AudioParameterFloatAttributes textFunctions (const Spec& spec)
    {
        switch (spec.format)
        {
            case ParameterSpecs::Format::decibels: return srd::params::decibels (ParameterSpecs::minusInfinityDb);
            case ParameterSpecs::Format::hertz:    return srd::params::hertz();
            case ParameterSpecs::Format::suffix:   break;
        }

        return srd::params::withSuffix (juce::String::fromUTF8 (spec.suffix), spec.decimals);
    }

    std::unique_ptr<juce::RangedAudioParameter> makeParameter (const Spec& spec)
    {
        const juce::ParameterID id { spec.id, Parameters::versionHint };

        if (spec.kind == ParameterSpecs::Kind::choice)
            return std::make_unique<juce::AudioParameterChoice> (
                id, spec.name, juce::StringArray (spec.choices.data(), static_cast<int> (spec.choices.size())),
                juce::roundToInt (spec.defaultValue),
                juce::AudioParameterChoiceAttributes().withAutomatable (spec.automatable));

        return std::make_unique<juce::AudioParameterFloat> (
            id, spec.name, rangeOf (spec), spec.defaultValue, textFunctions (spec).withAutomatable (spec.automatable));
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout Parameters::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto* spec : ParameterSpecs::all)
        layout.add (makeParameter (*spec));

    return layout;
}
