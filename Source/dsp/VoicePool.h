#pragma once

#include <algorithm>
#include <array>

namespace srd
{
    //==============================================================================
    /**
     * A fixed set of voices of one kind. A new hit takes an idle voice, or steals
     * one in turn when they're all busy.
     */
    template <typename Voice, std::size_t numVoices>
    class VoicePool
    {
    public:
        void prepare (double sampleRate)
        {
            for (auto& voice : voices)
                voice.prepare (sampleRate);
        }

        void stop() noexcept
        {
            for (auto& voice : voices)
                voice.stop();
        }

        void fadeOut() noexcept
        {
            for (auto& voice : voices)
                voice.fadeOut();
        }

        Voice& getFreeVoice() noexcept
        {
            const auto idle = std::find_if (voices.begin(), voices.end(), [] (const Voice& v) { return ! v.isActive(); });
            return idle != voices.end() ? *idle : voices[nextStolenVoice++ % numVoices];
        }

        void render (float* output, int numSamples) noexcept
        {
            for (auto& voice : voices)
                voice.render (output, numSamples);
        }

    private:
        std::array<Voice, numVoices> voices;
        std::size_t nextStolenVoice = 0;
    };
}
