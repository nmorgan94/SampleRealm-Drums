#pragma once

#include <samplerealm/ParameterSpec.h>

//==============================================================================
namespace ParameterSpecs
{
    // How a real value reads. The unit is part of the text, so there are no labels
    enum class Format
    {
        decibels,   // "-inf dB" at minusInfinityDb, else 1 decimal place
        hertz,      // "x.xx kHz" from 1000, "x.x Hz" below 100, else whole Hz
        suffix      // decimals places, then suffix
    };

    /** A spec, and how its value reads. */
    struct DrumSpec : Spec
    {
        Format format = Format::suffix;
        const char* suffix = "";
        int decimals = 0;
    };

    // The bottom of every level range; it reads "-inf dB" and the engine treats it as silence
    inline constexpr float minusInfinityDb = -60.0f;

    /** centre is the value at the middle of the range; without one the range is linear. */
    constexpr DrumSpec real (const char* id, const char* name, float min, float max, float defaultValue,
                             float step, const char* suffix, int decimals, std::optional<float> centre = std::nullopt)
    {
        Spec spec { id, name, Kind::real, min, max, defaultValue, step };
        spec.centre = centre;
        return { spec, Format::suffix, suffix, decimals };
    }

    constexpr DrumSpec level (const char* id, const char* name, float min, float max, float defaultValue)
    {
        return { { id, name, Kind::real, min, max, defaultValue, 0.1f }, Format::decibels };
    }

    constexpr DrumSpec percent (const char* id, const char* name, float max, float defaultValue)
    {
        return real (id, name, 0.0f, max, defaultValue, 0.1f, " %", 0);
    }

    constexpr DrumSpec hertz (const char* id, const char* name, float min, float max, float centre, float defaultValue)
    {
        return { withCentre ({ id, name, Kind::real, min, max, defaultValue }, centre), Format::hertz };
    }

    /** Starts at its first choice. */
    constexpr DrumSpec choiceOf (const char* id, const char* name, std::span<const char* const> choices)
    {
        return { choice (id, name, choices, 0) };
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
    inline constexpr DrumSpec instrument { notAutomatable (choice ("instrument", "Instrument", instrumentNames, 0)) };
    inline constexpr DrumSpec outputGain = level ("outputGain", "Output", minusInfinityDb, 6.0f, -2.5f);
    inline constexpr DrumSpec tune       = real ("tune", "Tune", -24.0f, 24.0f, 0.0f, 0.01f, " st", 2);
    inline constexpr DrumSpec keyTrack   = choiceOf ("keyTrack", "Key Track", offOn);
    inline constexpr DrumSpec length     = real ("length", "Length", 0.25f, 4.0f, 1.0f, 0.01f, "x", 2, 1.0f);
    inline constexpr DrumSpec pitchDepth = percent ("pitchDepth", "Pitch Depth", 200.0f, 100.0f);
    inline constexpr DrumSpec velocity   = percent ("velocity", "Vel Sens", 100.0f, 50.0f);

    // Sub
    inline constexpr DrumSpec subLevel     = level ("subLevel", "Sub Level", minusInfinityDb, 6.0f, 0.0f);
    inline constexpr DrumSpec subHarmonics = percent ("subHarmonics", "Harmonics", 100.0f, 0.0f);
    inline constexpr DrumSpec subPhase     = real ("subPhase", "Phase", 0.0f, 90.0f, 0.0f, 1.0f, "\xc2\xb0", 0);

    // Click
    inline constexpr DrumSpec clickType  = choiceOf ("clickType", "Click Type", clickTypeNames);
    inline constexpr DrumSpec clickLevel = level ("clickLevel", "Click Level", minusInfinityDb, 6.0f, -12.0f);
    inline constexpr DrumSpec clickTone  = hertz ("clickTone", "Click Tone", 200.0f, 16000.0f, 3000.0f, 4000.0f);
    inline constexpr DrumSpec clickDecay = real ("clickDecay", "Click Decay", 1.0f, 100.0f, 12.0f, 0.1f, " ms", 1, 15.0f);
    inline constexpr DrumSpec clickPitch = hertz ("clickPitch", "Click Pitch", 200.0f, 10000.0f, 2000.0f, 2500.0f);

    // Snare body
    inline constexpr DrumSpec snareBodyLevel     = level ("snareBodyLevel", "Body Level", minusInfinityDb, 6.0f, -4.0f);
    inline constexpr DrumSpec snareBodyHarmonics = percent ("snareBodyHarmonics", "Body Harmonics", 100.0f, 20.0f);

    // Snare noise
    inline constexpr DrumSpec snareNoiseLevel   = level ("snareNoiseLevel", "Noise Level", minusInfinityDb, 6.0f, 0.0f);
    inline constexpr DrumSpec snareNoiseLowCut  = hertz ("snareNoiseLowCut", "Noise Low Cut", 100.0f, 8000.0f, 1500.0f, 800.0f);
    inline constexpr DrumSpec snareNoiseHighCut = hertz ("snareNoiseHighCut", "Noise High Cut", 1000.0f, 18000.0f, 6000.0f, 12000.0f);

    // Snare snap
    inline constexpr DrumSpec snareSnapType  = choiceOf ("snareSnapType", "Snap Type", clickTypeNames);
    inline constexpr DrumSpec snareSnapLevel = level ("snareSnapLevel", "Snap Level", minusInfinityDb, 6.0f, -6.0f);
    inline constexpr DrumSpec snareSnapTone  = hertz ("snareSnapTone", "Snap Tone", 200.0f, 16000.0f, 3000.0f, 3500.0f);
    inline constexpr DrumSpec snareSnapDecay = real ("snareSnapDecay", "Snap Decay", 1.0f, 100.0f, 8.0f, 0.1f, " ms", 1, 15.0f);
    inline constexpr DrumSpec snareSnapPitch = hertz ("snareSnapPitch", "Snap Pitch", 200.0f, 10000.0f, 2000.0f, 2500.0f);

    // Cymbal metal
    inline constexpr DrumSpec cymbalMetalLevel = level ("cymbalMetalLevel", "Metal Level", minusInfinityDb, 6.0f, -6.0f);
    inline constexpr DrumSpec cymbalMetalTone  = hertz ("cymbalMetalTone", "Metal Tone", 2000.0f, 16000.0f, 6000.0f, 8000.0f);
    inline constexpr DrumSpec cymbalMetalRing  = percent ("cymbalMetalRing", "Metal Ring", 100.0f, 30.0f);

    // Cymbal noise
    inline constexpr DrumSpec cymbalNoiseLevel   = level ("cymbalNoiseLevel", "Cymbal Noise Level", minusInfinityDb, 6.0f, 0.0f);
    inline constexpr DrumSpec cymbalNoiseLowCut  = hertz ("cymbalNoiseLowCut", "Cymbal Noise Low Cut", 200.0f, 12000.0f, 3000.0f, 6000.0f);
    inline constexpr DrumSpec cymbalNoiseHighCut = hertz ("cymbalNoiseHighCut", "Cymbal Noise High Cut", 1000.0f, 18000.0f, 6000.0f, 16000.0f);

    // Cymbal stereo spread; 0 is mono
    inline constexpr DrumSpec cymbalWidth = percent ("cymbalWidth", "Width", 100.0f, 0.0f);

    // Drive
    inline constexpr DrumSpec driveType   = choiceOf ("driveType", "Drive Type", driveTypeNames);
    inline constexpr DrumSpec driveAmount = percent ("driveAmount", "Drive", 100.0f, 20.0f);
    inline constexpr DrumSpec driveMix    = percent ("driveMix", "Drive Mix", 100.0f, 100.0f);

    // EQ
    inline constexpr DrumSpec eqLowGain  = level ("eqLowGain", "Low", -12.0f, 12.0f, 0.0f);
    inline constexpr DrumSpec eqMidFreq  = hertz ("eqMidFreq", "Mid Freq", 100.0f, 5000.0f, 600.0f, 400.0f);
    inline constexpr DrumSpec eqMidGain  = level ("eqMidGain", "Mid", -12.0f, 12.0f, 0.0f);
    inline constexpr DrumSpec eqHighGain = level ("eqHighGain", "High", -12.0f, 12.0f, 0.0f);

    // Clip
    inline constexpr DrumSpec clipAmount = percent ("clipAmount", "Clip", 100.0f, 0.0f);

    // The host's parameter order
    inline constexpr std::array drumSpecs {
        &instrument, &outputGain, &tune, &keyTrack, &length, &pitchDepth, &velocity,
        &subLevel, &subHarmonics, &subPhase,
        &clickType, &clickLevel, &clickTone, &clickDecay, &clickPitch,
        &snareBodyLevel, &snareBodyHarmonics,
        &snareNoiseLevel, &snareNoiseLowCut, &snareNoiseHighCut,
        &snareSnapType, &snareSnapLevel, &snareSnapTone, &snareSnapDecay, &snareSnapPitch,
        &cymbalMetalLevel, &cymbalMetalTone, &cymbalMetalRing,
        &cymbalNoiseLevel, &cymbalNoiseLowCut, &cymbalNoiseHighCut, &cymbalWidth,
        &driveType, &driveAmount, &driveMix,
        &eqLowGain, &eqMidFreq, &eqMidGain, &eqHighGain,
        &clipAmount
    };

    // The same list as the shared code reads it
    inline constexpr auto all = []
    {
        std::array<const Spec*, drumSpecs.size()> specs {};
        std::ranges::copy (drumSpecs, specs.begin());
        return specs;
    }();

    static_assert (areValid (all));

    /** The spec's place in all, and so in any table of values kept in the host's order. */
    constexpr std::size_t indexOf (const Spec& spec)
    {
        std::size_t index = 0;

        while (index < all.size() - 1 && all[index] != &spec)
            ++index;

        return index;
    }

    /** The spec with how its value reads. */
    constexpr const DrumSpec& drumSpecOf (const Spec& spec)
    {
        return *drumSpecs[indexOf (spec)];
    }
}
