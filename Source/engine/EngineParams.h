#pragma once

#include "../Parameters.h"
#include "FxChain.h"
#include "KickVoice.h"
#include "SnareVoice.h"

//==============================================================================
/**
 * Converts parameter values into voice and FxChain settings. Caches the raw
 * parameter pointers once, so reading is lock-free and safe on the audio thread.
 */
class EngineParams
{
public:
    /** The note the Hit pad plays until MIDI arrives: where each default tail sits (F1, G3). */
    static constexpr int auditionNote (Parameters::Instrument instrument) noexcept
    {
        return instrument == Parameters::Instrument::snare ? 55 : 29;
    }

    explicit EngineParams (juce::AudioProcessorValueTreeState& apvts)
        : instrument   (get (apvts, Parameters::instrumentId)),
          outputGain   (get (apvts, Parameters::outputGainId)),
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
          snareBodyLevel     (get (apvts, Parameters::snareBodyLevelId)),
          snareBodyHarmonics (get (apvts, Parameters::snareBodyHarmonicsId)),
          snareNoiseLevel    (get (apvts, Parameters::snareNoiseLevelId)),
          snareNoiseLowCut   (get (apvts, Parameters::snareNoiseLowCutId)),
          snareNoiseHighCut  (get (apvts, Parameters::snareNoiseHighCutId)),
          snareSnapType      (get (apvts, Parameters::snareSnapTypeId)),
          snareSnapLevel     (get (apvts, Parameters::snareSnapLevelId)),
          snareSnapTone      (get (apvts, Parameters::snareSnapToneId)),
          snareSnapDecay     (get (apvts, Parameters::snareSnapDecayId)),
          snareSnapPitch     (get (apvts, Parameters::snareSnapPitchId)),
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

    Parameters::Instrument getInstrument() const noexcept
    {
        return Parameters::instrumentFromIndex (juce::roundToInt (instrument.load()));
    }

    float getLengthScale() const noexcept       { return length.load(); }

    /** pitch is the envelope the hit will play (log2 Hz), which Key Track tunes against. */
    KickVoice::Settings kickSettings (int midiNote, const srd::EnvelopeData& pitch) const noexcept
    {
        KickVoice::Settings s;
        s.sub            = toneSettings (midiNote, pitch);
        s.sub.startPhase = subPhase.load() / 360.0f;
        s.sub.harmonics  = subHarmonics.load() * 0.01f;
        s.sub.gain       = decibelsToGain (subLevel.load());
        s.click          = clickSettings (clickType, clickLevel, clickTone, clickDecay, clickPitch, s.sub.frequencyRatio);
        return s;
    }

    SnareVoice::Settings snareSettings (int midiNote, const srd::EnvelopeData& pitch) const noexcept
    {
        SnareVoice::Settings s;
        s.body           = toneSettings (midiNote, pitch);
        s.body.harmonics = snareBodyHarmonics.load() * 0.01f;
        s.body.gain      = decibelsToGain (snareBodyLevel.load());
        s.noiseGain      = decibelsToGain (snareNoiseLevel.load());
        s.noiseLowCutHz  = snareNoiseLowCut.load();
        s.noiseHighCutHz = snareNoiseHighCut.load();
        s.snap           = clickSettings (snareSnapType, snareSnapLevel, snareSnapTone, snareSnapDecay, snareSnapPitch, s.body.frequencyRatio);
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
    using Param = std::atomic<float>;

    const Param& instrument, & outputGain, & tune, & keyTrack, & length, & pitchDepth,
               & subLevel, & subHarmonics, & subPhase,
               & clickType, & clickLevel, & clickTone, & clickDecay, & clickPitch,
               & snareBodyLevel, & snareBodyHarmonics,
               & snareNoiseLevel, & snareNoiseLowCut, & snareNoiseHighCut,
               & snareSnapType, & snareSnapLevel, & snareSnapTone, & snareSnapDecay, & snareSnapPitch,
               & driveType, & driveAmount, & driveMix,
               & eqLowGain, & eqMidFreq, & eqMidGain, & eqHighGain,
               & clipAmount;

    static const Param& get (juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& id)
    {
        auto* p = apvts.getRawParameterValue (id.getParamID());
        jassert (p != nullptr);
        return *p;
    }

    static float decibelsToGain (float db) noexcept { return juce::Decibels::decibelsToGain (db, Parameters::minusInfinityDb); }

    /** The tonal settings every drum shares: tuning, Length and Pitch Depth. */
    srd::EnvelopedOscillator::Settings toneSettings (int midiNote, const srd::EnvelopeData& pitch) const noexcept
    {
        srd::EnvelopedOscillator::Settings s;
        s.frequencyRatio = frequencyRatio (midiNote, srd::EnvelopedOscillator::getTailHz (pitch));
        s.lengthScale    = length.load();
        s.pitchDepth     = pitchDepth.load() * 0.01f;
        return s;
    }

    /** The kick's click and the snare's snap share a generator; decay follows Length and pitch follows tuning. */
    srd::ClickGenerator::Settings clickSettings (const Param& type, const Param& level, const Param& tone,
                                                 const Param& decay, const Param& pitch, float ratio) const noexcept
    {
        srd::ClickGenerator::Settings s;
        s.type    = srd::ClickGenerator::typeFromIndex (juce::roundToInt (type.load()));
        s.gain    = decibelsToGain (level.load());
        s.toneHz  = tone.load();
        s.decayMs = decay.load() * length.load();
        s.pitchHz = pitch.load() * ratio;
        return s;
    }

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
