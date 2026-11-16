#include "Parameters.h"

//==============================================================================
namespace
{
    juce::String number (float value, int decimals)
    {
        if (decimals <= 0)
            return juce::String (juce::roundToInt (value));

        auto text = juce::String (value, decimals);
        return text.containsOnly ("-0.") ? text.trimCharactersAtStart ("-") : text;
    }
}

juce::String Parameters::hertzText (float hz)
{
    if (hz >= 1000.0f)
        return number (hz / 1000.0f, 2) + " kHz";

    return number (hz, hz < 100.0f ? 1 : 0) + " Hz";
}

// Each has a matching parser, so typed values read back
juce::AudioParameterFloatAttributes Parameters::withValueText (const ParameterSpecs::Spec& spec, juce::AudioParameterFloatAttributes attributes)
{
    using ParameterSpecs::minusInfinityDb;
    const auto& drumSpec = ParameterSpecs::drumSpecOf (spec);

    switch (drumSpec.format)
    {
        case ParameterSpecs::Format::decibels:
            return attributes
                .withStringFromValueFunction ([] (float value, int)
                {
                    return value <= minusInfinityDb + 0.05f ? juce::String ("-inf dB") : number (value, 1) + " dB";
                })
                .withValueFromStringFunction ([] (const juce::String& text)
                {
                    return text.containsIgnoreCase ("inf") ? minusInfinityDb : text.getFloatValue();
                });

        case ParameterSpecs::Format::hertz:
            return attributes
                .withStringFromValueFunction ([] (float value, int) { return hertzText (value); })
                .withValueFromStringFunction ([] (const juce::String& text)
                {
                    const auto value = text.getFloatValue();
                    return text.containsIgnoreCase ("k") ? value * 1000.0f : value;
                });

        case ParameterSpecs::Format::suffix:
            break;
    }

    // getFloatValue reads the leading number and ignores the suffix
    return attributes
        .withStringFromValueFunction ([suffix = juce::String::fromUTF8 (drumSpec.suffix), decimals = drumSpec.decimals] (float value, int)
        {
            return number (value, decimals) + suffix;
        })
        .withValueFromStringFunction ([] (const juce::String& text) { return text.getFloatValue(); });
}
