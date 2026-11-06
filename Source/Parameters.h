#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "shared/ParameterSpecs.h"

//==============================================================================
namespace Parameters
{
    // Bump this when adding or restructuring parameters in a new plugin version.
    // All ParameterIDs must use this so there is one place to update.
    constexpr int versionHint = 1;

    // Parameter IDs — use these constants everywhere instead of raw strings.
    // inline const means one definition shared across all translation units (C++17).
    namespace Specs = ParameterSpecs;

    // Global
    inline const juce::ParameterID instrumentId  { Specs::instrument.id, versionHint };
    inline const juce::ParameterID outputGainId  { Specs::outputGain.id, versionHint };
    inline const juce::ParameterID tuneId        { Specs::tune.id,       versionHint };
    inline const juce::ParameterID keyTrackId    { Specs::keyTrack.id,   versionHint };
    inline const juce::ParameterID lengthId      { Specs::length.id,     versionHint };
    inline const juce::ParameterID pitchDepthId  { Specs::pitchDepth.id, versionHint };

    // Sub
    inline const juce::ParameterID subLevelId     { Specs::subLevel.id,     versionHint };
    inline const juce::ParameterID subHarmonicsId { Specs::subHarmonics.id, versionHint };
    inline const juce::ParameterID subPhaseId     { Specs::subPhase.id,     versionHint };

    // Click
    inline const juce::ParameterID clickTypeId  { Specs::clickType.id,  versionHint };
    inline const juce::ParameterID clickLevelId { Specs::clickLevel.id, versionHint };
    inline const juce::ParameterID clickToneId  { Specs::clickTone.id,  versionHint };
    inline const juce::ParameterID clickDecayId { Specs::clickDecay.id, versionHint };
    inline const juce::ParameterID clickPitchId { Specs::clickPitch.id, versionHint };

    // Snare body
    inline const juce::ParameterID snareBodyLevelId     { Specs::snareBodyLevel.id,     versionHint };
    inline const juce::ParameterID snareBodyHarmonicsId { Specs::snareBodyHarmonics.id, versionHint };

    // Snare noise
    inline const juce::ParameterID snareNoiseLevelId    { Specs::snareNoiseLevel.id,   versionHint };
    inline const juce::ParameterID snareNoiseLowCutId   { Specs::snareNoiseLowCut.id,  versionHint };
    inline const juce::ParameterID snareNoiseHighCutId  { Specs::snareNoiseHighCut.id, versionHint };

    // Snare snap
    inline const juce::ParameterID snareSnapTypeId  { Specs::snareSnapType.id,  versionHint };
    inline const juce::ParameterID snareSnapLevelId { Specs::snareSnapLevel.id, versionHint };
    inline const juce::ParameterID snareSnapToneId  { Specs::snareSnapTone.id,  versionHint };
    inline const juce::ParameterID snareSnapDecayId { Specs::snareSnapDecay.id, versionHint };
    inline const juce::ParameterID snareSnapPitchId { Specs::snareSnapPitch.id, versionHint };

    // Cymbal metal
    inline const juce::ParameterID cymbalMetalLevelId { Specs::cymbalMetalLevel.id, versionHint };
    inline const juce::ParameterID cymbalMetalToneId  { Specs::cymbalMetalTone.id,  versionHint };
    inline const juce::ParameterID cymbalMetalRingId  { Specs::cymbalMetalRing.id,  versionHint };

    // Cymbal noise
    inline const juce::ParameterID cymbalNoiseLevelId   { Specs::cymbalNoiseLevel.id,   versionHint };
    inline const juce::ParameterID cymbalNoiseLowCutId  { Specs::cymbalNoiseLowCut.id,  versionHint };
    inline const juce::ParameterID cymbalNoiseHighCutId { Specs::cymbalNoiseHighCut.id, versionHint };

    // Drive
    inline const juce::ParameterID driveTypeId   { Specs::driveType.id,   versionHint };
    inline const juce::ParameterID driveAmountId { Specs::driveAmount.id, versionHint };
    inline const juce::ParameterID driveMixId    { Specs::driveMix.id,    versionHint };

    // EQ
    inline const juce::ParameterID eqLowGainId  { Specs::eqLowGain.id,  versionHint };
    inline const juce::ParameterID eqMidFreqId  { Specs::eqMidFreq.id,  versionHint };
    inline const juce::ParameterID eqMidGainId  { Specs::eqMidGain.id,  versionHint };
    inline const juce::ParameterID eqHighGainId { Specs::eqHighGain.id, versionHint };

    // Clip
    inline const juce::ParameterID clipAmountId { Specs::clipAmount.id, versionHint };

    using ParameterSpecs::Instrument;
    using ParameterSpecs::instrumentFromIndex;

    // Built from ParameterSpecs::all, in its order
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
