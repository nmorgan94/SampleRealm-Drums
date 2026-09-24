#pragma once

#include "../Parameters.h"
#include "FxChain.h"
#include "KickVoice.h"

//==============================================================================
/**
 * Converts parameter values into KickVoice and FxChain settings. Caches the raw
 * parameter pointers once, so reading is lock-free and safe on the audio thread.
 */
class EngineParams
{
public:
    /** The note the Hit pad plays until MIDI arrives. */
    static constexpr int auditionNote = 29;

    explicit EngineParams (juce::AudioProcessorValueTreeState& apvts)
        : outputGain   (get (apvts, Parameters::outputGainId)),
          tune         (get (apvts, Parameters::tuneId)),
          keyTrack     (get (apvts, Parameters::keyTrackId)),
          length       (get (apvts, Parameters::lengthId)),
          pitchDepth   (get (apvts, Parameters::pitchDepthId)),
          subLevel     (get (apvts, Parameters::subLevelId)),
          subHarmonics (get (apvts, Parameters::subHarmonicsId)),
          subPhase     (get (apvts, Parameters::subPhaseId)),
          clickType    (get (apvts, Parameters::clickTypeId)),
          clickLevel   (get (apvts, Parameters::clickLevelId)),
          clickTone    (get (apvts, Parameters::clickToneId)),
          clickDecay   (get (apvts, Parameters::clickDecayId)),
          clickPitch   (get (apvts, Parameters::clickPitchId)),
          driveType    (get (apvts, Parameters::driveTypeId)),
          driveAmount  (get (apvts, Parameters::driveAmountId)),
          driveMix     (get (apvts, Parameters::driveMixId)),
          eqLowGain    (get (apvts, Parameters::eqLowGainId)),
          eqMidFreq    (get (apvts, Parameters::eqMidFreqId)),
          eqMidGain    (get (apvts, Parameters::eqMidGainId)),
          eqHighGain   (get (apvts, Parameters::eqHighGainId)),
          clipAmount   (get (apvts, Parameters::clipAmountId))
    {
    }

    /** pitch is the envelope the hit will play (log2 Hz), which Key Track tunes against. */
    KickVoice::Settings voiceSettings (int midiNote, const srd::EnvelopeData& pitch) const noexcept
    {
        KickVoice::Settings s;
        s.frequencyRatio = frequencyRatio (midiNote, KickVoice::getTailHz (pitch));
        s.lengthScale    = length.load();
        s.pitchDepth     = pitchDepth.load() * 0.01f;
        s.startPhase     = subPhase.load() / 360.0f;
        s.harmonics      = subHarmonics.load() * 0.01f;
        s.subGain        = decibelsToGain (subLevel.load());

        s.click.type    = srd::ClickGenerator::typeFromIndex (juce::roundToInt (clickType.load()));
        s.click.gain    = decibelsToGain (clickLevel.load());
        s.click.toneHz  = clickTone.load();
        s.click.decayMs = clickDecay.load() * length.load();
        s.click.pitchHz = clickPitch.load() * s.frequencyRatio;
        return s;
    }

    FxChain::Settings fxSettings() const noexcept
    {
        FxChain::Settings s;
        s.driveType     = srd::Drive::typeFromIndex (juce::roundToInt (driveType.load()));
        s.driveAmount   = driveAmount.load() * 0.01f;
        s.driveMix      = driveMix.load() * 0.01f;
        s.eq.lowGainDb  = eqLowGain.load();
        s.eq.midFreqHz  = eqMidFreq.load();
        s.eq.midGainDb  = eqMidGain.load();
        s.eq.highGainDb = eqHighGain.load();
        s.clipAmount    = clipAmount.load() * 0.01f;
        s.outputGain    = decibelsToGain (outputGain.load());
        return s;
    }

private:
    const std::atomic<float>& outputGain, & tune, & keyTrack, & length, & pitchDepth,
                            & subLevel, & subHarmonics, & subPhase,
                            & clickType, & clickLevel, & clickTone, & clickDecay, & clickPitch,
                            & driveType, & driveAmount, & driveMix,
                            & eqLowGain, & eqMidFreq, & eqMidGain, & eqHighGain,
                            & clipAmount;

    static const std::atomic<float>& get (juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& id)
    {
        auto* p = apvts.getRawParameterValue (id.getParamID());
        jassert (p != nullptr);
        return *p;
    }

    static float decibelsToGain (float db) noexcept { return juce::Decibels::decibelsToGain (db, Parameters::minusInfinityDb); }

    /** Ratio applied to the whole pitch envelope. With Key Track on, the tail lands on the
        played note; Tune then offsets it either way. */
    float frequencyRatio (int midiNote, float envelopeTailHz) const noexcept
    {
        const auto tuneRatio = std::exp2 (tune.load() / 12.0f);

        if (keyTrack.load() < 0.5f || envelopeTailHz <= 0.0f)
            return tuneRatio;

        return (float) juce::MidiMessage::getMidiNoteInHertz (midiNote) / envelopeTailHz * tuneRatio;
    }
};
