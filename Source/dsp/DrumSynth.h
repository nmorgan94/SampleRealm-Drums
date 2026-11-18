#pragma once

#include <atomic>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../shared/DrumEnvelopes.h"
#include "../shared/ParameterSpecs.h"
#include "CymbalVoice.h"
#include "FxChain.h"
#include "KickVoice.h"
#include "SnareVoice.h"
#include "VoicePool.h"

//==============================================================================
/**
 * The drum: the current instrument's voices, started by MIDI note-ons, through the FxChain.
 * Mono, until the cymbal's Width spreads it.
 */
class DrumSynth
{
public:
    using Instrument = ParameterSpecs::Instrument;

    /** The kick's click or the snare's snap, before Length and tuning apply. */
    struct Click
    {
        srd::ClickGenerator::Type type = srd::ClickGenerator::Type::noise;
        float gain    = 0.25f;  // linear
        float toneHz  = 4000.0f;
        float decayMs = 12.0f;
        float pitchHz = 2500.0f;
    };

    struct Settings
    {
        Instrument instrument = Instrument::kick;
        float tuneRatio   = 1.0f;
        bool  keyTrack    = false;
        float lengthScale = 1.0f;
        float pitchDepth  = 1.0f;
        float velocityAmount = 0.0f;  // 0..1

        float subGain       = 1.0f;  // linear
        float subHarmonics  = 0.0f;  // 0..1
        float subStartPhase = 0.0f;  // cycles
        Click click;

        float bodyGain            = 1.0f;
        float bodyHarmonics       = 0.0f;
        float snareNoiseGain      = 1.0f;
        float snareNoiseLowCutHz  = 800.0f;
        float snareNoiseHighCutHz = 12000.0f;
        Click snap;

        CymbalVoice::Settings cymbal;

        FxChain::Settings fx;
    };

    /** A hit's gain against a full one, for a velocity of 0..1. */
    static float velocityGain (float amount, float velocity) noexcept;

    /** The settings one hit plays with. The click and snap take the gain twice, so a soft hit is duller too. */
    static Settings hitSettings (const Settings&, float gain) noexcept;

    /** By DrumEnvelopes::Index, with values in each envelope's domain (log2 Hz for pitch). */
    using Envelopes = std::array<srd::EnvelopeData, DrumEnvelopes::count>;

    /** Each envelope's unscaled length in seconds, by DrumEnvelopes::Index. */
    using Durations = std::array<float, DrumEnvelopes::count>;

    /** One hit's voice settings. pitch is the envelope it will play, which Key Track tunes against. */
    static KickVoice::Settings kickSettings (const Settings&, int midiNote, const srd::EnvelopeData& pitch) noexcept;
    static SnareVoice::Settings snareSettings (const Settings&, int midiNote, const srd::EnvelopeData& pitch) noexcept;

    /** Starts a hit on the voice with its instrument's envelopes. Returns where its pitch settles; 0 for the cymbal. */
    static float start (KickVoice&, const Settings&, const Envelopes&, int midiNote) noexcept;
    static float start (SnareVoice&, const Settings&, const Envelopes&, int midiNote) noexcept;
    static float start (CymbalVoice&, const Settings&, const Envelopes&, int midiNote) noexcept;

    /** How long a hit of the settings' instrument sounds. */
    static float getHitSeconds (const Settings&, const Durations&) noexcept;

    //==============================================================================
    void prepare (double sampleRate, int maxBlockSize, const Settings&);
    void stop() noexcept;

    int getLatencySamples() const noexcept         { return fxChain.getLatencySamples(); }

    /** A new instrument fades out the old one's hits. */
    void setSettings (const Settings&) noexcept;

    /** Values in the envelope's domain (log2 Hz for pitch), as EnvelopeSpec::toDomain gives. */
    void setEnvelope (std::size_t index, const srd::EnvelopeData&) noexcept;

    /** Overwrites both channels, starting each note-on at its sample. right can be null, for the mono signal alone. */
    void process (float* left, float* right, int numSamples, const juce::MidiBuffer&) noexcept;

    //==============================================================================
    // Safe from any thread

    /** Replays the current instrument's last note at the start of the next process(), at a velocity above 0. */
    void audition (float velocity = 1.0f) noexcept { auditionVelocity = velocity; }

    /** Where Key Track tunes the instrument's next audition. */
    int getLastNote (Instrument) const noexcept;

    /** Increments on every hit, so the UI can flash without listening to MIDI. */
    juce::uint32 getHitCount() const noexcept      { return hitCount.load(); }

    /** The last hit's gain against a full one, 0..1, so the UI can flash as hard as it played. */
    float getLastHitLevel() const noexcept         { return lastHitLevel.load(); }

    /** The velocity the last hit was played at, 0..1. */
    float getLastHitVelocity() const noexcept      { return lastHitVelocity.load(); }

private:
    srd::VoicePool<KickVoice, 4> kickVoices;
    srd::VoicePool<SnareVoice, 4> snareVoices;
    srd::VoicePool<CymbalVoice, 4> cymbalVoices;
    FxChain fxChain;

    Settings settings;
    Envelopes envelopes = DrumEnvelopes::defaults();
    Instrument playing = Instrument::kick;

    // 0 when no audition is waiting
    std::atomic<float> auditionVelocity { 0.0f };
    std::atomic<juce::uint32> hitCount { 0 };
    std::atomic<float> lastHitLevel { 1.0f };
    std::atomic<float> lastHitVelocity { 1.0f };

    // Each instrument keeps its own last note. Until MIDI arrives the Hit pad plays where each
    // default pitch envelope settles, so turning Key Track on doesn't change the sound
    std::atomic<int> lastKickNote   { 29 };  // F1, 43.65 Hz
    std::atomic<int> lastSnareNote  { 55 };  // G3, 196 Hz
    std::atomic<int> lastCymbalNote { 42 };  // the cymbal ignores the note; kept so lastNoteOf covers every instrument

    auto& lastNoteOf (this auto& self, Instrument instrument) noexcept
    {
        switch (instrument)
        {
            case Instrument::snare:  return self.lastSnareNote;
            case Instrument::cymbal: return self.lastCymbalNote;
            case Instrument::kick:   break;
        }

        return self.lastKickNote;
    }

    void startNote (int midiNote, float velocity) noexcept;
    void fadeOutAll() noexcept;
    void renderVoices (float* output, int startSample, int endSample) noexcept;
};
