#pragma once

#include <algorithm>
#include <array>
#include <span>
#include <juce_core/juce_core.h>

//==============================================================================
namespace ParameterSpecs
{
    enum class Kind { real, choice };

    // How a real value reads. The unit is part of the text, so there are no labels
    enum class Format
    {
        decibels,   // "-inf dB" at minusInfinityDb, else 1 decimal place
        hertz,      // "x.xx kHz" from 1000, "x.x Hz" below 100, else whole Hz
        suffix      // decimals places, then suffix
    };

    struct Spec
    {
        const char* id;
        const char* name;
        Kind kind;
        float min, max, defaultValue;
        float step = 0.0f;
        float centre = 0.0f;   // the value at the middle of the range, or 0 for a linear range
        Format format = Format::suffix;
        const char* suffix = "";
        int decimals = 0;
        std::span<const char* const> choices = {};
        bool automatable = true;
    };

    // The bottom of every level range; it reads "-inf dB" and the engine treats it as silence
    inline constexpr float minusInfinityDb = -60.0f;

    constexpr Spec real (const char* id, const char* name, float min, float max, float defaultValue,
                         float step, const char* suffix, int decimals, float centre = 0.0f)
    {
        return { id, name, Kind::real, min, max, defaultValue, step, centre, Format::suffix, suffix, decimals };
    }

    constexpr Spec level (const char* id, const char* name, float min, float max, float defaultValue)
    {
        return { id, name, Kind::real, min, max, defaultValue, 0.1f, 0.0f, Format::decibels };
    }

    constexpr Spec percent (const char* id, const char* name, float max, float defaultValue)
    {
        return real (id, name, 0.0f, max, defaultValue, 0.1f, " %", 0);
    }

    constexpr Spec hertz (const char* id, const char* name, float min, float max, float centre, float defaultValue)
    {
        return { id, name, Kind::real, min, max, defaultValue, 0.0f, centre, Format::hertz };
    }

    constexpr Spec choice (const char* id, const char* name, std::span<const char* const> choices, bool automatable = true)
    {
        return { id, name, Kind::choice, 0.0f, static_cast<float> (choices.size() - 1), 0.0f, 1.0f, 0.0f,
                 Format::suffix, "", 0, choices, automatable };
    }

    enum class Instrument { kick, snare, cymbal };

    constexpr Instrument instrumentFromIndex (int index) noexcept
    {
        switch (index)
        {
            case 1:  return Instrument::snare;
            case 2:  return Instrument::cymbal;
            default: return Instrument::kick;
        }
    }

    inline constexpr std::array<const char*, 3> instrumentNames { "Kick", "Snare", "Cymbal" };
    inline constexpr std::array<const char*, 2> offOn { "Off", "On" };
    inline constexpr std::array<const char*, 3> clickTypeNames { "Noise", "Sweep", "Impulse" };
    inline constexpr std::array<const char*, 4> driveTypeNames { "Soft", "Hard", "Fold", "Tube" };

    // Global. Switching instrument swaps the whole UI, so it isn't automatable
    inline constexpr Spec instrument = choice ("instrument", "Instrument", instrumentNames, false);
    inline constexpr Spec outputGain = level ("outputGain", "Output", minusInfinityDb, 6.0f, -2.5f);
    inline constexpr Spec tune       = real ("tune", "Tune", -24.0f, 24.0f, 0.0f, 0.01f, " st", 2);
    inline constexpr Spec keyTrack   = choice ("keyTrack", "Key Track", offOn);
    inline constexpr Spec length     = real ("length", "Length", 0.25f, 4.0f, 1.0f, 0.01f, "x", 2, 1.0f);
    inline constexpr Spec pitchDepth = percent ("pitchDepth", "Pitch Depth", 200.0f, 100.0f);

    // Sub
    inline constexpr Spec subLevel     = level ("subLevel", "Sub Level", minusInfinityDb, 6.0f, 0.0f);
    inline constexpr Spec subHarmonics = percent ("subHarmonics", "Harmonics", 100.0f, 0.0f);
    inline constexpr Spec subPhase     = real ("subPhase", "Phase", 0.0f, 90.0f, 0.0f, 1.0f, "\xc2\xb0", 0);

    // Click
    inline constexpr Spec clickType  = choice ("clickType", "Click Type", clickTypeNames);
    inline constexpr Spec clickLevel = level ("clickLevel", "Click Level", minusInfinityDb, 6.0f, -12.0f);
    inline constexpr Spec clickTone  = hertz ("clickTone", "Click Tone", 200.0f, 16000.0f, 3000.0f, 4000.0f);
    inline constexpr Spec clickDecay = real ("clickDecay", "Click Decay", 1.0f, 100.0f, 12.0f, 0.1f, " ms", 1, 15.0f);
    inline constexpr Spec clickPitch = hertz ("clickPitch", "Click Pitch", 200.0f, 10000.0f, 2000.0f, 2500.0f);

    // Snare body
    inline constexpr Spec snareBodyLevel     = level ("snareBodyLevel", "Body Level", minusInfinityDb, 6.0f, -4.0f);
    inline constexpr Spec snareBodyHarmonics = percent ("snareBodyHarmonics", "Body Harmonics", 100.0f, 20.0f);

    // Snare noise
    inline constexpr Spec snareNoiseLevel   = level ("snareNoiseLevel", "Noise Level", minusInfinityDb, 6.0f, 0.0f);
    inline constexpr Spec snareNoiseLowCut  = hertz ("snareNoiseLowCut", "Noise Low Cut", 100.0f, 8000.0f, 1500.0f, 800.0f);
    inline constexpr Spec snareNoiseHighCut = hertz ("snareNoiseHighCut", "Noise High Cut", 1000.0f, 18000.0f, 6000.0f, 12000.0f);

    // Snare snap
    inline constexpr Spec snareSnapType  = choice ("snareSnapType", "Snap Type", clickTypeNames);
    inline constexpr Spec snareSnapLevel = level ("snareSnapLevel", "Snap Level", minusInfinityDb, 6.0f, -6.0f);
    inline constexpr Spec snareSnapTone  = hertz ("snareSnapTone", "Snap Tone", 200.0f, 16000.0f, 3000.0f, 3500.0f);
    inline constexpr Spec snareSnapDecay = real ("snareSnapDecay", "Snap Decay", 1.0f, 100.0f, 8.0f, 0.1f, " ms", 1, 15.0f);
    inline constexpr Spec snareSnapPitch = hertz ("snareSnapPitch", "Snap Pitch", 200.0f, 10000.0f, 2000.0f, 2500.0f);

    // Cymbal metal
    inline constexpr Spec cymbalMetalLevel = level ("cymbalMetalLevel", "Metal Level", minusInfinityDb, 6.0f, -6.0f);
    inline constexpr Spec cymbalMetalTone  = hertz ("cymbalMetalTone", "Metal Tone", 2000.0f, 16000.0f, 6000.0f, 8000.0f);
    inline constexpr Spec cymbalMetalRing  = percent ("cymbalMetalRing", "Metal Ring", 100.0f, 30.0f);

    // Cymbal noise
    inline constexpr Spec cymbalNoiseLevel   = level ("cymbalNoiseLevel", "Cymbal Noise Level", minusInfinityDb, 6.0f, 0.0f);
    inline constexpr Spec cymbalNoiseLowCut  = hertz ("cymbalNoiseLowCut", "Cymbal Noise Low Cut", 200.0f, 12000.0f, 3000.0f, 6000.0f);
    inline constexpr Spec cymbalNoiseHighCut = hertz ("cymbalNoiseHighCut", "Cymbal Noise High Cut", 1000.0f, 18000.0f, 6000.0f, 16000.0f);

    // Drive
    inline constexpr Spec driveType   = choice ("driveType", "Drive Type", driveTypeNames);
    inline constexpr Spec driveAmount = percent ("driveAmount", "Drive", 100.0f, 20.0f);
    inline constexpr Spec driveMix    = percent ("driveMix", "Drive Mix", 100.0f, 100.0f);

    // EQ
    inline constexpr Spec eqLowGain  = level ("eqLowGain", "Low", -12.0f, 12.0f, 0.0f);
    inline constexpr Spec eqMidFreq  = hertz ("eqMidFreq", "Mid Freq", 100.0f, 5000.0f, 600.0f, 400.0f);
    inline constexpr Spec eqMidGain  = level ("eqMidGain", "Mid", -12.0f, 12.0f, 0.0f);
    inline constexpr Spec eqHighGain = level ("eqHighGain", "High", -12.0f, 12.0f, 0.0f);

    // Clip
    inline constexpr Spec clipAmount = percent ("clipAmount", "Clip", 100.0f, 0.0f);

    // The host's parameter order
    inline constexpr std::array all {
        &instrument, &outputGain, &tune, &keyTrack, &length, &pitchDepth,
        &subLevel, &subHarmonics, &subPhase,
        &clickType, &clickLevel, &clickTone, &clickDecay, &clickPitch,
        &snareBodyLevel, &snareBodyHarmonics,
        &snareNoiseLevel, &snareNoiseLowCut, &snareNoiseHighCut,
        &snareSnapType, &snareSnapLevel, &snareSnapTone, &snareSnapDecay, &snareSnapPitch,
        &cymbalMetalLevel, &cymbalMetalTone, &cymbalMetalRing,
        &cymbalNoiseLevel, &cymbalNoiseLowCut, &cymbalNoiseHighCut,
        &driveType, &driveAmount, &driveMix,
        &eqLowGain, &eqMidFreq, &eqMidGain, &eqHighGain,
        &clipAmount
    };

    static_assert (std::ranges::all_of (all, [] (const Spec* spec)
    {
        if (spec->kind == Kind::real)
            return spec->centre == 0.0f || (spec->centre > spec->min && spec->centre < spec->max);

        return spec->step == 1.0f && spec->min == 0.0f && static_cast<size_t> (spec->max) == spec->choices.size() - 1;
    }), "A choice spec's range is 0 to its last choice, and a real spec's centre lies inside its range");

    /** The host's range, skewed so a real spec's centre sits half way. */
    inline juce::NormalisableRange<float> rangeOf (const Spec& spec)
    {
        juce::NormalisableRange<float> range { spec.min, spec.max, spec.step };

        if (! juce::exactlyEqual (spec.centre, 0.0f))
            range.setSkewForCentre (spec.centre);

        return range;
    }

    /** The spec's place in all, and so in any table of values kept in the host's order. */
    constexpr std::size_t indexOf (const Spec& spec)
    {
        std::size_t index = 0;

        while (index < all.size() - 1 && all[index] != &spec)
            ++index;

        return index;
    }
}
