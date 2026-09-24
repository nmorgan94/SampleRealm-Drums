#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

//==============================================================================
/**
 * Breakpoint envelopes shared by the audio engine, the editor and the waveform
 * preview.
 */
namespace srd
{
    struct EnvelopeNode
    {
        float time  = 0.0f; // seconds
        float value = 0.0f; // in the envelope's interpolation domain (log2 Hz for pitch, gain for amp)
        float curve = 0.0f; // tension of the segment leading INTO this node, -1..1, 0 = linear
    };

    struct EnvelopeData
    {
        static constexpr std::size_t maxNodes = 32;

        std::array<EnvelopeNode, maxNodes> nodes {};
        std::size_t numNodes = 0;

        float getDuration() const noexcept   { return numNodes > 0 ? nodes[numNodes - 1].time : 0.0f; }
        float getFinalValue() const noexcept { return numNodes > 0 ? nodes[numNodes - 1].value : 0.0f; }
    };

    /** Maps x in [0, 1] through a tension curve. Positive curve moves fast early (like an
        exponential decay), negative moves slowly early; 0 is linear. */
    inline float applyCurve (float x, float curve) noexcept
    {
        const auto k = curve * 10.0f;

        if (std::abs (k) < 1.0e-3f)
            return x;

        return (1.0f - std::exp (-k * x)) / (1.0f - std::exp (-k));
    }

    inline float interpolateSegment (const EnvelopeNode& a, const EnvelopeNode& b, float t) noexcept
    {
        const auto span = b.time - a.time;

        if (span <= 0.0f)
            return b.value;

        const auto x = (t - a.time) / span;
        return a.value + (b.value - a.value) * applyCurve (std::clamp (x, 0.0f, 1.0f), b.curve);
    }

    /** Random-access evaluation, O(numNodes). Used by the UI and for one-off lookups. */
    inline float evaluate (const EnvelopeData& env, float t) noexcept
    {
        if (env.numNodes == 0)
            return 0.0f;

        if (t <= env.nodes[0].time)
            return env.nodes[0].value;

        for (std::size_t i = 1; i < env.numNodes; ++i)
            if (t < env.nodes[i].time)
                return interpolateSegment (env.nodes[i - 1], env.nodes[i], t);

        return env.getFinalValue();
    }

    //==============================================================================
    /** Plays an envelope forward in time. Keeps a segment cursor so each call is
        amortised O(1) as long as time only moves forward. */
    class EnvelopeCursor
    {
    public:
        void reset() noexcept { segment = 1; }

        float getValue (const EnvelopeData& env, float t) noexcept
        {
            if (env.numNodes == 0)
                return 0.0f;

            if (env.numNodes == 1 || t <= env.nodes[0].time)
                return env.nodes[0].value;

            while (segment < env.numNodes && t >= env.nodes[segment].time)
                ++segment;

            if (segment >= env.numNodes)
                return env.getFinalValue();

            return interpolateSegment (env.nodes[segment - 1], env.nodes[segment], t);
        }

    private:
        std::size_t segment = 1;
    };
}
