#pragma once

#include <algorithm>
#include <span>
#include <vector>
#include "../dsp/BreakpointEnvelope.h"

namespace srd
{
    //==============================================================================
    /** Describes one breakpoint envelope: its range, scale and default shape. */
    struct EnvelopeSpec
    {
        enum class Scale { linear, logarithmic };

        const char* id;                        // stored as ENV[id=...] in the state tree
        Scale scale     = Scale::linear;       // logarithmic interpolates in log2 (use for Hz)
        float minValue  = 0.0f;
        float maxValue  = 1.0f;
        float maxTime   = 2.0f;                // seconds
        std::vector<EnvelopeNode> defaultNodes; // display units, time-ordered, first at t = 0

        /** Display units (Hz, gain) to the domain the engine interpolates in. */
        float toDomain (float displayValue) const noexcept
        {
            return scale == Scale::logarithmic ? std::log2 (displayValue) : displayValue;
        }

        float toDisplay (float domainValue) const noexcept
        {
            return scale == Scale::logarithmic ? std::exp2 (domainValue) : domainValue;
        }

        float clampValue (float displayValue) const noexcept
        {
            return std::clamp (displayValue, minValue, maxValue);
        }

        static constexpr float minGap = 0.0005f; // seconds between neighbouring nodes

        EnvelopeNode clampNode (EnvelopeNode node, float earliest, float latest) const noexcept
        {
            node.time  = std::clamp (node.time, earliest, std::max (earliest, latest));
            node.value = clampValue (node.value);
            node.curve = std::clamp (node.curve, -1.0f, 1.0f);
            return node;
        }

        /** Sorted, trimmed, spaced and clamped, as a loaded envelope is. False for fewer than two nodes. */
        bool sanitise (std::vector<EnvelopeNode>& nodes) const
        {
            if (nodes.size() < 2)
                return false;

            std::stable_sort (nodes.begin(), nodes.end(), [] (auto& a, auto& b) { return a.time < b.time; });
            nodes.resize (std::min (nodes.size(), EnvelopeData::maxNodes));
            nodes[0] = clampNode (nodes[0], 0.0f, 0.0f);

            for (std::size_t i = 1; i < nodes.size(); ++i)
                nodes[i] = clampNode (nodes[i], nodes[i - 1].time + minGap, maxTime);

            return true;
        }

        /** Display-unit nodes, clamped to the range, as the engine plays them. */
        EnvelopeData toData (std::span<const EnvelopeNode> displayNodes) const noexcept
        {
            EnvelopeData data;
            data.numNodes = std::min (displayNodes.size(), EnvelopeData::maxNodes);

            for (std::size_t i = 0; i < data.numNodes; ++i)
            {
                const auto& node = displayNodes[i];
                data.nodes[i] = { node.time, toDomain (clampValue (node.value)), node.curve };
            }

            return data;
        }
    };
}

//==============================================================================
/** Every instrument's breakpoint envelopes and their default shapes. */
namespace DrumEnvelopes
{
    enum Index : std::size_t
    {
        kickPitch,
        kickAmp,
        snarePitch,
        snareBody,
        snareNoise,
        cymbalMetal,
        cymbalNoise,
        count
    };

    inline srd::EnvelopeSpec pitchSpec (const char* id, float minHz, float maxHz, std::vector<srd::EnvelopeNode> nodes)
    {
        return { id, srd::EnvelopeSpec::Scale::logarithmic, minHz, maxHz, 2.0f, std::move (nodes) };
    }

    inline srd::EnvelopeSpec gainSpec (const char* id, std::vector<srd::EnvelopeNode> nodes, float maxTime = 2.0f)
    {
        return { id, srd::EnvelopeSpec::Scale::linear, 0.0f, 1.0f, maxTime, std::move (nodes) };
    }

    /** In Index order. */
    inline std::vector<srd::EnvelopeSpec> createSpecs()
    {
        return {
            pitchSpec ("pitch", 20.0f, 12000.0f, { { 0.0f,   3000.0f,  0.0f },
                                                   { 0.006f,  420.0f,  0.6f },
                                                   { 0.045f,   95.0f,  0.5f },
                                                   { 0.120f,   48.0f,  0.4f },
                                                   { 0.350f,   43.65f, 0.0f } }),

            gainSpec ("amp", { { 0.0f,   1.0f,  0.0f },
                               { 0.010f, 1.0f,  0.0f },
                               { 0.200f, 0.7f,  0.0f },
                               { 0.350f, 0.0f, -0.4f } }),

            pitchSpec ("snarePitch", 40.0f, 8000.0f, { { 0.0f,   420.0f, 0.0f },
                                                       { 0.010f, 230.0f, 0.5f },
                                                       { 0.060f, 200.0f, 0.3f },
                                                       { 0.120f, 196.0f, 0.0f } }),

            gainSpec ("snareBody", { { 0.0f,   1.0f, 0.0f },
                                     { 0.003f, 1.0f, 0.0f },
                                     { 0.120f, 0.0f, 0.6f } }),

            gainSpec ("snareNoise", { { 0.0f,   1.0f,  0.0f },
                                      { 0.080f, 0.35f, 0.5f },
                                      { 0.250f, 0.0f,  0.3f } }),

            // A closed hat; up to 4 s leaves room for a ride
            gainSpec ("cymbalMetal", { { 0.0f,   1.0f, 0.0f },
                                       { 0.080f, 0.0f, 0.5f } }, 4.0f),

            gainSpec ("cymbalNoise", { { 0.0f,   1.0f, 0.0f },
                                       { 0.020f, 0.3f, 0.4f },
                                       { 0.090f, 0.0f, 0.3f } }, 4.0f)
        };
    }

    /** Every envelope's default shape, as the engine plays it. */
    inline std::array<srd::EnvelopeData, count> defaults()
    {
        std::array<srd::EnvelopeData, count> data;
        const auto specs = createSpecs();

        for (std::size_t env = 0; env < count; ++env)
            data[env] = specs[env].toData (specs[env].defaultNodes);

        return data;
    }
}
