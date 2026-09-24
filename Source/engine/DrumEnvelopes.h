#pragma once

#include "../dsp/EnvelopeModel.h"

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
        snareNoise
    };

    inline srd::EnvelopeSpec pitchSpec (const juce::String& id, float minHz, float maxHz, std::vector<srd::EnvelopeNode> nodes)
    {
        return { id, srd::EnvelopeSpec::Scale::logarithmic, minHz, maxHz, 2.0f, std::move (nodes) };
    }

    inline srd::EnvelopeSpec gainSpec (const juce::String& id, std::vector<srd::EnvelopeNode> nodes)
    {
        return { id, srd::EnvelopeSpec::Scale::linear, 0.0f, 1.0f, 2.0f, std::move (nodes) };
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
                                      { 0.250f, 0.0f,  0.3f } })
        };
    }
}
