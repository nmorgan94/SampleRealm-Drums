#include "DrumSynth.h"

namespace
{
    /** Ratio applied to the whole pitch envelope. With Key Track on, the tail lands on the
        played note; Tune then offsets it either way. */
    float frequencyRatio (const DrumSynth::Settings& s, int midiNote, float envelopeTailHz) noexcept
    {
        if (! s.keyTrack || envelopeTailHz <= 0.0f)
            return s.tuneRatio;

        return (float) juce::MidiMessage::getMidiNoteInHertz (midiNote) / envelopeTailHz * s.tuneRatio;
    }

    /** The tonal settings every drum shares: tuning, Length and Pitch Depth. */
    srd::EnvelopedOscillator::Settings toneSettings (const DrumSynth::Settings& s, int midiNote, const srd::EnvelopeData& pitch) noexcept
    {
        srd::EnvelopedOscillator::Settings tone;
        tone.frequencyRatio = frequencyRatio (s, midiNote, srd::EnvelopedOscillator::getTailHz (pitch));
        tone.lengthScale    = s.lengthScale;
        tone.pitchDepth     = s.pitchDepth;
        return tone;
    }

    /** Decay follows Length and pitch follows tuning. */
    srd::ClickGenerator::Settings clickSettings (const DrumSynth::Click& click, float lengthScale, float ratio) noexcept
    {
        return { click.type, click.gain, click.toneHz, click.decayMs * lengthScale, click.pitchHz * ratio };
    }
}

//==============================================================================
KickVoice::Settings DrumSynth::kickSettings (const Settings& s, int midiNote, const srd::EnvelopeData& pitch) noexcept
{
    KickVoice::Settings kick;
    kick.sub            = toneSettings (s, midiNote, pitch);
    kick.sub.startPhase = s.subStartPhase;
    kick.sub.harmonics  = s.subHarmonics;
    kick.sub.gain       = s.subGain;
    kick.click          = clickSettings (s.click, s.lengthScale, kick.sub.frequencyRatio);
    return kick;
}

SnareVoice::Settings DrumSynth::snareSettings (const Settings& s, int midiNote, const srd::EnvelopeData& pitch) noexcept
{
    SnareVoice::Settings snare;
    snare.body           = toneSettings (s, midiNote, pitch);
    snare.body.harmonics = s.bodyHarmonics;
    snare.body.gain      = s.bodyGain;
    snare.noiseGain      = s.snareNoiseGain;
    snare.noiseLowCutHz  = s.snareNoiseLowCutHz;
    snare.noiseHighCutHz = s.snareNoiseHighCutHz;
    snare.snap           = clickSettings (s.snap, s.lengthScale, snare.body.frequencyRatio);
    return snare;
}

float DrumSynth::start (KickVoice& voice, const Settings& s, const Envelopes& envelopes, int midiNote) noexcept
{
    const auto& pitch = envelopes[DrumEnvelopes::kickPitch];
    const auto kick = kickSettings (s, midiNote, pitch);
    voice.start (pitch, envelopes[DrumEnvelopes::kickAmp], kick);
    return srd::EnvelopedOscillator::getTailHz (pitch, kick.sub.frequencyRatio);
}

float DrumSynth::start (SnareVoice& voice, const Settings& s, const Envelopes& envelopes, int midiNote) noexcept
{
    const auto& pitch = envelopes[DrumEnvelopes::snarePitch];
    const auto snare = snareSettings (s, midiNote, pitch);
    voice.start (pitch, envelopes[DrumEnvelopes::snareBody], envelopes[DrumEnvelopes::snareNoise], snare);
    return srd::EnvelopedOscillator::getTailHz (pitch, snare.body.frequencyRatio);
}

float DrumSynth::start (CymbalVoice& voice, const Settings& s, const Envelopes& envelopes, int) noexcept
{
    voice.start (envelopes[DrumEnvelopes::cymbalMetal], envelopes[DrumEnvelopes::cymbalNoise], s.cymbal);
    return 0.0f;
}

float DrumSynth::getHitSeconds (const Settings& s, const Durations& durations) noexcept
{
    using namespace DrumEnvelopes;

    switch (s.instrument)
    {
        case Instrument::snare:
            return SnareVoice::getDurationSeconds (durations[snareBody], durations[snareNoise], snareSettings (s, 0, {}));
        case Instrument::cymbal:
            return CymbalVoice::getDurationSeconds (durations[cymbalMetal], durations[cymbalNoise], s.cymbal);
        case Instrument::kick:
            break;
    }

    return KickVoice::getDurationSeconds (durations[kickAmp], kickSettings (s, 0, {}));
}

//==============================================================================
void DrumSynth::prepare (double sampleRate, int maxBlockSize, const Settings& newSettings)
{
    kickVoices.prepare (sampleRate);
    snareVoices.prepare (sampleRate);
    cymbalVoices.prepare (sampleRate);

    settings = newSettings;
    fxChain.prepare (sampleRate, maxBlockSize);
    fxChain.setSettings (settings.fx);
    fxChain.reset();
}

void DrumSynth::stop() noexcept
{
    kickVoices.stop();
    snareVoices.stop();
    cymbalVoices.stop();
}

void DrumSynth::setSettings (const Settings& newSettings) noexcept
{
    settings = newSettings;
    fxChain.setSettings (settings.fx);

    if (settings.instrument != playing)
    {
        fadeOutAll();
        playing = settings.instrument;
    }
}

void DrumSynth::setEnvelope (std::size_t index, const srd::EnvelopeData& envelope) noexcept
{
    envelopes[index] = envelope;
}

int DrumSynth::getLastNote (Instrument instrument) const noexcept
{
    return lastNoteOf (instrument).load();
}

//==============================================================================
void DrumSynth::startNote (int midiNote) noexcept
{
    fadeOutAll();

    switch (playing)
    {
        case Instrument::kick:   start (kickVoices.getFreeVoice(), settings, envelopes, midiNote);   break;
        case Instrument::snare:  start (snareVoices.getFreeVoice(), settings, envelopes, midiNote);  break;
        case Instrument::cymbal: start (cymbalVoices.getFreeVoice(), settings, envelopes, midiNote); break;
    }

    // The cymbal ignores the note
    if (playing != Instrument::cymbal)
        lastNoteOf (playing) = midiNote;

    ++hitCount;
}

void DrumSynth::fadeOutAll() noexcept
{
    kickVoices.fadeOut();
    snareVoices.fadeOut();
    cymbalVoices.fadeOut();
}

void DrumSynth::renderVoices (float* output, int startSample, int endSample) noexcept
{
    kickVoices.render (output + startSample, endSample - startSample);
    snareVoices.render (output + startSample, endSample - startSample);
    cymbalVoices.render (output + startSample, endSample - startSample);
}

void DrumSynth::process (float* output, int numSamples, const juce::MidiBuffer& midi) noexcept
{
    juce::FloatVectorOperations::clear (output, numSamples);

    if (auditionPending.exchange (false))
        startNote (lastNoteOf (playing).load());

    // Split the block at each note-on so hits are sample-accurate
    int position = 0;

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        if (! message.isNoteOn() && ! message.isAllNotesOff() && ! message.isAllSoundOff())
            continue;

        const auto eventPosition = juce::jlimit (0, numSamples, metadata.samplePosition);
        renderVoices (output, position, eventPosition);
        position = eventPosition;

        if (message.isNoteOn())
            startNote (message.getNoteNumber());
        else
            fadeOutAll();
    }

    renderVoices (output, position, numSamples);
    fxChain.process (output, numSamples);
}
