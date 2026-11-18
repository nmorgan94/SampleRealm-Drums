#pragma once

#include <atomic>
#include "LoopSteps.h"
#include "PatternPlayer.h"

namespace srd
{
    //==============================================================================
    /**
     * The audition loop: an instrument's steps, played against a position in quarter notes.
     * Not saved. process and stop are the audio thread's; the rest is safe from any thread.
     */
    class AuditionLoop
    {
    public:
        using Instrument = ParameterSpecs::Instrument;

        /** A new pattern in Patterns::all replaces every instrument's steps. Switching on or off keeps them as edited. */
        void set (int newPattern, bool shouldPlay) noexcept
        {
            if (pattern.exchange (newPattern) != newPattern)
                steps.load (Patterns::at (newPattern));

            on = shouldPlay;
        }

        int getPattern() const noexcept     { return pattern.load(); }
        bool isOn() const noexcept          { return on.load(); }

        /** A velocity each, as LoopSteps::setStep takes. */
        Patterns::Part getSteps (Instrument instrument) const noexcept          { return steps.get (instrument); }
        void setStep (Instrument instrument, int step, int velocity) noexcept   { steps.setStep (instrument, step, velocity); }

        /** The step the loop is on, or -1 while it's off. */
        int getPlayingStep() const noexcept     { return isOn() ? playingStep.load() : -1; }

        /** A block with the loop on, starting at ppq: adds the instrument's steps to midi as note-ons of note. */
        void process (juce::MidiBuffer& midi, Instrument instrument, int note,
                      double ppq, double bpm, double sampleRate, int numSamples) noexcept
        {
            player.process (midi, steps.get (instrument), note, ppq, bpm, sampleRate, numSamples);
            playingStep = PatternPlayer::stepAt (ppq);
        }

        /** A block with the loop off. */
        void stop() noexcept
        {
            player.reset();
            playingStep = -1;
        }

    private:
        std::atomic<int> pattern { 0 };
        std::atomic<bool> on { false };
        std::atomic<int> playingStep { -1 };
        LoopSteps steps;
        PatternPlayer player;
    };
}
