#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
/**
 * Ranges and value text for AudioParameterFloat. The unit is part of each value's
 * text, so none of these set a label (hosts that append the label would show it
 * twice), and each has a matching parser so typed values read back correctly.
 */
namespace srd::params
{
    inline juce::NormalisableRange<float> skewedRange (float min, float max, float centre, float interval = 0.0f)
    {
        juce::NormalisableRange<float> range { min, max, interval };
        range.setSkewForCentre (centre);
        return range;
    }

    inline juce::String number (float v, int decimals)
    {
        if (decimals <= 0)
            return juce::String (juce::roundToInt (v));

        auto text = juce::String (v, decimals);
        return text.containsOnly ("-0.") ? text.trimCharactersAtStart ("-") : text;
    }

    inline juce::String hertzText (float hz)
    {
        if (hz >= 1000.0f)
            return number (hz / 1000.0f, 2) + " kHz";

        return number (hz, hz < 100.0f ? 1 : 0) + " Hz";
    }

    /** Values at or below minusInfinityDb show as "-inf dB". */
    inline juce::AudioParameterFloatAttributes decibels (float minusInfinityDb)
    {
        return juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([minusInfinityDb] (float v, int)
            {
                return v <= minusInfinityDb + 0.05f ? juce::String ("-inf dB") : number (v, 1) + " dB";
            })
            .withValueFromStringFunction ([minusInfinityDb] (const juce::String& text)
            {
                return text.containsIgnoreCase ("inf") ? minusInfinityDb : text.getFloatValue();
            });
    }

    inline juce::AudioParameterFloatAttributes hertz()
    {
        return juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return hertzText (v); })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                const auto value = text.getFloatValue();
                return text.containsIgnoreCase ("k") ? value * 1000.0f : value;
            });
    }

    /** getFloatValue reads the leading number and ignores the suffix. */
    inline juce::AudioParameterFloatAttributes withSuffix (const juce::String& suffix, int decimals)
    {
        return juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([suffix, decimals] (float v, int) { return number (v, decimals) + suffix; })
            .withValueFromStringFunction ([] (const juce::String& text)
            {
                return text.getFloatValue();
            });
    }

    inline juce::AudioParameterFloatAttributes percent()   { return withSuffix (" %", 0); }
}
