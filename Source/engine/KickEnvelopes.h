#pragma once

#include "../dsp/EnvelopeModel.h"

//==============================================================================
/** The kick's breakpoint envelopes and their default shapes. */
namespace KickEnvelopes
{
    enum Index : std::size_t
    {
        pitch = 0,
        amp   = 1
    };

    inline std::vector<srd::EnvelopeSpec> createSpecs()
    {
        srd::EnvelopeSpec pitchSpec;
        pitchSpec.id       = "pitch";
        pitchSpec.scale    = srd::EnvelopeSpec::Scale::logarithmic;
        pitchSpec.minValue = 20.0f;
        pitchSpec.maxValue = 12000.0f;
        pitchSpec.maxTime  = 2.0f;

        // A punchy DnB kick: a fast drop from a 3 kHz click down to a tail on F1 (43.65 Hz)
        pitchSpec.defaultNodes = { { 0.0f,   3000.0f,  0.0f },
                                   { 0.006f,  420.0f,  0.6f },
                                   { 0.045f,   95.0f,  0.5f },
                                   { 0.120f,   48.0f,  0.4f },
                                   { 0.350f,   43.65f, 0.0f } };

        srd::EnvelopeSpec ampSpec;
        ampSpec.id       = "amp";
        ampSpec.scale    = srd::EnvelopeSpec::Scale::linear;
        ampSpec.minValue = 0.0f;
        ampSpec.maxValue = 1.0f;
        ampSpec.maxTime  = 2.0f;

        ampSpec.defaultNodes = { { 0.0f,   1.0f,  0.0f },
                                 { 0.010f, 1.0f,  0.0f },
                                 { 0.200f, 0.7f,  0.0f },
                                 { 0.350f, 0.0f, -0.4f } };

        return { pitchSpec, ampSpec };
    }
}
