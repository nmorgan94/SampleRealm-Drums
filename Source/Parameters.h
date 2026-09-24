#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
namespace Parameters
{
    // Bump this when adding or restructuring parameters in a new plugin version.
    // All ParameterIDs must use this so there is one place to update.
    constexpr int versionHint = 1;

    // Parameter IDs — use these constants everywhere instead of raw strings.
    // inline const means one definition shared across all translation units (C++17).

    // Global
    inline const juce::ParameterID instrumentId  { "instrument",  versionHint };
    inline const juce::ParameterID outputGainId  { "outputGain",  versionHint };
    inline const juce::ParameterID tuneId        { "tune",        versionHint };
    inline const juce::ParameterID keyTrackId    { "keyTrack",    versionHint };
    inline const juce::ParameterID lengthId      { "length",      versionHint };
    inline const juce::ParameterID pitchDepthId  { "pitchDepth",  versionHint };

    // Sub
    inline const juce::ParameterID subLevelId     { "subLevel",     versionHint };
    inline const juce::ParameterID subHarmonicsId { "subHarmonics", versionHint };
    inline const juce::ParameterID subPhaseId     { "subPhase",     versionHint };

    // Click
    inline const juce::ParameterID clickTypeId  { "clickType",  versionHint };
    inline const juce::ParameterID clickLevelId { "clickLevel", versionHint };
    inline const juce::ParameterID clickToneId  { "clickTone",  versionHint };
    inline const juce::ParameterID clickDecayId { "clickDecay", versionHint };
    inline const juce::ParameterID clickPitchId { "clickPitch", versionHint };

    // Snare body
    inline const juce::ParameterID snareBodyLevelId     { "snareBodyLevel",     versionHint };
    inline const juce::ParameterID snareBodyHarmonicsId { "snareBodyHarmonics", versionHint };

    // Snare noise
    inline const juce::ParameterID snareNoiseLevelId    { "snareNoiseLevel",    versionHint };
    inline const juce::ParameterID snareNoiseLowCutId   { "snareNoiseLowCut",   versionHint };
    inline const juce::ParameterID snareNoiseHighCutId  { "snareNoiseHighCut",  versionHint };

    // Snare snap
    inline const juce::ParameterID snareSnapTypeId  { "snareSnapType",  versionHint };
    inline const juce::ParameterID snareSnapLevelId { "snareSnapLevel", versionHint };
    inline const juce::ParameterID snareSnapToneId  { "snareSnapTone",  versionHint };
    inline const juce::ParameterID snareSnapDecayId { "snareSnapDecay", versionHint };
    inline const juce::ParameterID snareSnapPitchId { "snareSnapPitch", versionHint };

    // Drive
    inline const juce::ParameterID driveTypeId   { "driveType",   versionHint };
    inline const juce::ParameterID driveAmountId { "driveAmount", versionHint };
    inline const juce::ParameterID driveMixId    { "driveMix",    versionHint };

    // EQ
    inline const juce::ParameterID eqLowGainId  { "eqLowGain",  versionHint };
    inline const juce::ParameterID eqMidFreqId  { "eqMidFreq",  versionHint };
    inline const juce::ParameterID eqMidGainId  { "eqMidGain",  versionHint };
    inline const juce::ParameterID eqHighGainId { "eqHighGain", versionHint };

    // Clip
    inline const juce::ParameterID clipAmountId { "clipAmount", versionHint };

    enum class Instrument { kick, snare };
    inline const juce::StringArray instrumentNames { "Kick", "Snare" };

    constexpr Instrument instrumentFromIndex (int index) noexcept
    {
        switch (index)
        {
            case 1:  return Instrument::snare;
            default: return Instrument::kick;
        }
    }

    // The bottom of every level range; the display shows it as "-inf dB" and the engine as silence
    constexpr float minusInfinityDb = -60.0f;

    // Parameter layout factory — defined in Parameters.cpp, compiled once.
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
