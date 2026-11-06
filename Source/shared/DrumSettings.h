#pragma once

#include "ParameterSpecs.h"
#include "../dsp/DrumSynth.h"

//==============================================================================
template <typename ValueOf>
DrumSynth::Settings makeDrumSettings (ValueOf&& valueOf)
{
    using namespace ParameterSpecs;

    const auto gain     = [&] (const Spec& spec) { return juce::Decibels::decibelsToGain (valueOf (spec), minusInfinityDb); };
    const auto fraction = [&] (const Spec& spec) { return valueOf (spec) * 0.01f; };
    const auto index    = [&] (const Spec& spec) { return juce::roundToInt (valueOf (spec)); };

    const auto click = [&] (const Spec& type, const Spec& level, const Spec& tone, const Spec& decay, const Spec& pitch)
    {
        return DrumSynth::Click { srd::ClickGenerator::typeFromIndex (index (type)), gain (level),
                                  valueOf (tone), valueOf (decay), valueOf (pitch) };
    };

    DrumSynth::Settings s;
    s.instrument  = instrumentFromIndex (index (instrument));
    s.tuneRatio   = std::exp2 (valueOf (tune) / 12.0f);
    s.keyTrack    = valueOf (keyTrack) >= 0.5f;
    s.lengthScale = valueOf (length);
    s.pitchDepth  = fraction (pitchDepth);

    s.subGain       = gain (subLevel);
    s.subHarmonics  = fraction (subHarmonics);
    s.subStartPhase = valueOf (subPhase) / 360.0f;
    s.click         = click (clickType, clickLevel, clickTone, clickDecay, clickPitch);

    s.bodyGain            = gain (snareBodyLevel);
    s.bodyHarmonics       = fraction (snareBodyHarmonics);
    s.snareNoiseGain      = gain (snareNoiseLevel);
    s.snareNoiseLowCutHz  = valueOf (snareNoiseLowCut);
    s.snareNoiseHighCutHz = valueOf (snareNoiseHighCut);
    s.snap                = click (snareSnapType, snareSnapLevel, snareSnapTone, snareSnapDecay, snareSnapPitch);

    // The cymbal has no pitch envelope, so it follows Tune but not the note
    s.cymbal.frequencyRatio = s.tuneRatio;
    s.cymbal.lengthScale    = s.lengthScale;
    s.cymbal.metalGain      = gain (cymbalMetalLevel);
    s.cymbal.metalToneHz    = valueOf (cymbalMetalTone);
    s.cymbal.metalRing      = fraction (cymbalMetalRing);
    s.cymbal.noiseGain      = gain (cymbalNoiseLevel);
    s.cymbal.noiseLowCutHz  = valueOf (cymbalNoiseLowCut);
    s.cymbal.noiseHighCutHz = valueOf (cymbalNoiseHighCut);

    s.fx.driveType     = srd::Drive::typeFromIndex (index (driveType));
    s.fx.driveAmount   = fraction (driveAmount);
    s.fx.driveMix      = fraction (driveMix);
    s.fx.eq.lowGainDb  = valueOf (eqLowGain);
    s.fx.eq.midFreqHz  = valueOf (eqMidFreq);
    s.fx.eq.midGainDb  = valueOf (eqMidGain);
    s.fx.eq.highGainDb = valueOf (eqHighGain);
    s.fx.clipAmount    = fraction (clipAmount);
    s.fx.outputGain    = gain (outputGain);
    return s;
}
