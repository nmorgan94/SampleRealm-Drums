#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"

using namespace TestHelpers;
using Catch::Matchers::WithinAbs;

//==============================================================================
// What hosts and saved sessions see. Change it only on purpose
TEST_CASE_METHOD (ProcessorFixture, "Parameters match the snapshot hosts rely on", "[parameters]")
{
    struct Expected
    {
        const char* id;
        const char* name;
        bool automatable;
        float start, end, interval, skew, defaultValue;
        std::array<const char*, 3> textAt0HalfAnd1;
        const char* typed;
        float typedValue;
    };

    const std::vector<Expected> snapshot {
        { "instrument",         "Instrument",            false,  0.0f,     2.0f, 1.0f,  1.0f,      0.0f,     { "Kick", "Snare", "Cymbal" },               "Cymbal",   2.0f },
        { "outputGain",         "Output",                true, -60.0f,     6.0f, 0.1f,  1.0f,     -2.5f,     { "-inf dB", "-27.0 dB", "6.0 dB" },         "-inf dB", -60.0f },
        { "tune",               "Tune",                  true, -24.0f,    24.0f, 0.01f, 1.0f,      0.0f,     { "-24.00 st", "0.00 st", "24.00 st" },      "3.5 st",   3.5f },
        { "keyTrack",           "Key Track",             true,   0.0f,     1.0f, 1.0f,  1.0f,      0.0f,     { "Off", "Off", "On" },                      "On",       1.0f },
        { "length",             "Length",                true,   0.25f,    4.0f, 0.01f, 0.430677f, 1.0f,     { "0.25x", "1.00x", "4.00x" },               "2x",       2.0f },
        { "pitchDepth",         "Pitch Depth",           true,   0.0f,   200.0f, 0.1f,  1.0f,    100.0f,     { "0 %", "100 %", "200 %" },                 "150 %",  150.0f },
        { "subLevel",           "Sub Level",             true, -60.0f,     6.0f, 0.1f,  1.0f,      0.0f,     { "-inf dB", "-27.0 dB", "6.0 dB" },         "-6 dB",   -6.0f },
        { "subHarmonics",       "Harmonics",             true,   0.0f,   100.0f, 0.1f,  1.0f,      0.0f,     { "0 %", "50 %", "100 %" },                  "25 %",    25.0f },
        { "subPhase",           "Phase",                 true,   0.0f,    90.0f, 1.0f,  1.0f,      0.0f,     { "0\xc2\xb0", "45\xc2\xb0", "90\xc2\xb0" }, "30",      30.0f },
        { "clickType",          "Click Type",            true,   0.0f,     2.0f, 1.0f,  1.0f,      0.0f,     { "Noise", "Sweep", "Impulse" },             "Impulse",  2.0f },
        { "clickLevel",         "Click Level",           true, -60.0f,     6.0f, 0.1f,  1.0f,    -12.0f,     { "-inf dB", "-27.0 dB", "6.0 dB" },         "-inf",   -60.0f },
        { "clickTone",          "Click Tone",            true, 200.0f, 16000.0f, 0.0f,  0.400573f, 4000.0f,  { "200 Hz", "3.00 kHz", "16.00 kHz" },       "1.5 kHz", 1500.0f },
        { "clickDecay",         "Click Decay",           true,   1.0f,   100.0f, 0.1f,  0.354358f, 12.0f,    { "1.0 ms", "15.0 ms", "100.0 ms" },         "12 ms",   12.0f },
        { "clickPitch",         "Click Pitch",           true, 200.0f, 10000.0f, 0.0f,  0.409034f, 2500.0f,  { "200 Hz", "2.00 kHz", "10.00 kHz" },       "800 Hz",  800.0f },
        { "snareBodyLevel",     "Body Level",            true, -60.0f,     6.0f, 0.1f,  1.0f,     -4.0f,     { "-inf dB", "-27.0 dB", "6.0 dB" },         "3 dB",     3.0f },
        { "snareBodyHarmonics", "Body Harmonics",        true,   0.0f,   100.0f, 0.1f,  1.0f,     20.0f,     { "0 %", "50 %", "100 %" },                  "75",      75.0f },
        { "snareNoiseLevel",    "Noise Level",           true, -60.0f,     6.0f, 0.1f,  1.0f,      0.0f,     { "-inf dB", "-27.0 dB", "6.0 dB" },         "-12.5 dB", -12.5f },
        { "snareNoiseLowCut",   "Noise Low Cut",         true, 100.0f,  8000.0f, 0.0f,  0.400573f, 800.0f,   { "100 Hz", "1.50 kHz", "8.00 kHz" },        "2k",     2000.0f },
        { "snareNoiseHighCut",  "Noise High Cut",        true, 1000.0f, 18000.0f, 0.0f, 0.566401f, 12000.0f, { "1.00 kHz", "6.00 kHz", "18.00 kHz" },     "9000 Hz", 9000.0f },
        { "snareSnapType",      "Snap Type",             true,   0.0f,     2.0f, 1.0f,  1.0f,      0.0f,     { "Noise", "Sweep", "Impulse" },             "Sweep",    1.0f },
        { "snareSnapLevel",     "Snap Level",            true, -60.0f,     6.0f, 0.1f,  1.0f,     -6.0f,     { "-inf dB", "-27.0 dB", "6.0 dB" },         "0 dB",     0.0f },
        { "snareSnapTone",      "Snap Tone",             true, 200.0f, 16000.0f, 0.0f,  0.400573f, 3500.0f,  { "200 Hz", "3.00 kHz", "16.00 kHz" },       "10 kHz", 10000.0f },
        { "snareSnapDecay",     "Snap Decay",            true,   1.0f,   100.0f, 0.1f,  0.354358f, 8.0f,     { "1.0 ms", "15.0 ms", "100.0 ms" },         "40.5 ms", 40.5f },
        { "snareSnapPitch",     "Snap Pitch",            true, 200.0f, 10000.0f, 0.0f,  0.409034f, 2500.0f,  { "200 Hz", "2.00 kHz", "10.00 kHz" },       "5 kHz",  5000.0f },
        { "cymbalMetalLevel",   "Metal Level",           true, -60.0f,     6.0f, 0.1f,  1.0f,     -6.0f,     { "-inf dB", "-27.0 dB", "6.0 dB" },         "-30 dB", -30.0f },
        { "cymbalMetalTone",    "Metal Tone",            true, 2000.0f, 16000.0f, 0.0f, 0.553295f, 8000.0f,  { "2.00 kHz", "6.00 kHz", "16.00 kHz" },     "4 kHz",  4000.0f },
        { "cymbalMetalRing",    "Metal Ring",            true,   0.0f,   100.0f, 0.1f,  1.0f,     30.0f,     { "0 %", "50 %", "100 %" },                  "60 %",    60.0f },
        { "cymbalNoiseLevel",   "Cymbal Noise Level",    true, -60.0f,     6.0f, 0.1f,  1.0f,      0.0f,     { "-inf dB", "-27.0 dB", "6.0 dB" },         "-inf dB", -60.0f },
        { "cymbalNoiseLowCut",  "Cymbal Noise Low Cut",  true, 200.0f, 12000.0f, 0.0f,  0.481861f, 6000.0f,  { "200 Hz", "3.00 kHz", "12.00 kHz" },       "500 Hz",  500.0f },
        { "cymbalNoiseHighCut", "Cymbal Noise High Cut", true, 1000.0f, 18000.0f, 0.0f, 0.566401f, 16000.0f, { "1.00 kHz", "6.00 kHz", "18.00 kHz" },     "15 kHz", 15000.0f },
        { "driveType",          "Drive Type",            true,   0.0f,     3.0f, 1.0f,  1.0f,      0.0f,     { "Soft", "Fold", "Tube" },                  "Hard",     1.0f },
        { "driveAmount",        "Drive",                 true,   0.0f,   100.0f, 0.1f,  1.0f,     20.0f,     { "0 %", "50 %", "100 %" },                  "35 %",    35.0f },
        { "driveMix",           "Drive Mix",             true,   0.0f,   100.0f, 0.1f,  1.0f,    100.0f,     { "0 %", "50 %", "100 %" },                  "80 %",    80.0f },
        { "eqLowGain",          "Low",                   true, -12.0f,    12.0f, 0.1f,  1.0f,      0.0f,     { "-12.0 dB", "0.0 dB", "12.0 dB" },         "-3 dB",   -3.0f },
        { "eqMidFreq",          "Mid Freq",              true, 100.0f,  5000.0f, 0.0f,  0.303695f, 400.0f,   { "100 Hz", "600 Hz", "5.00 kHz" },          "1 kHz",  1000.0f },
        { "eqMidGain",          "Mid",                   true, -12.0f,    12.0f, 0.1f,  1.0f,      0.0f,     { "-12.0 dB", "0.0 dB", "12.0 dB" },         "4.5 dB",   4.5f },
        { "eqHighGain",         "High",                  true, -12.0f,    12.0f, 0.1f,  1.0f,      0.0f,     { "-12.0 dB", "0.0 dB", "12.0 dB" },         "6 dB",     6.0f },
        { "clipAmount",         "Clip",                  true,   0.0f,   100.0f, 0.1f,  1.0f,      0.0f,     { "0 %", "50 %", "100 %" },                  "10 %",    10.0f },
    };

    const auto& parameters = processor.getParameters();
    REQUIRE (static_cast<size_t> (parameters.size()) == snapshot.size());

    for (size_t i = 0; i < snapshot.size(); ++i)
    {
        const auto& expected = snapshot[i];
        CAPTURE (i, expected.id);

        auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (parameters[static_cast<int> (i)]);
        REQUIRE (parameter != nullptr);

        CHECK (parameter->getParameterID() == expected.id);
        CHECK (parameter->getVersionHint() == 1);
        CHECK (parameter->getName (100) == expected.name);
        CHECK (parameter->getLabel().isEmpty());
        CHECK (parameter->isAutomatable() == expected.automatable);

        const auto& range = parameter->getNormalisableRange();
        CHECK_THAT (range.start, WithinAbs (expected.start, 1.0e-6));
        CHECK_THAT (range.end, WithinAbs (expected.end, 1.0e-6));
        CHECK_THAT (range.interval, WithinAbs (expected.interval, 1.0e-6));
        CHECK_THAT (range.skew, WithinAbs (expected.skew, 1.0e-5));
        CHECK_THAT (parameter->convertFrom0to1 (parameter->getDefaultValue()), WithinAbs (expected.defaultValue, 1.0e-4 + 1.0e-6 * std::abs (expected.defaultValue)));

        CHECK (parameter->getText (0.0f, 100) == juce::String::fromUTF8 (expected.textAt0HalfAnd1[0]));
        CHECK (parameter->getText (0.5f, 100) == juce::String::fromUTF8 (expected.textAt0HalfAnd1[1]));
        CHECK (parameter->getText (1.0f, 100) == juce::String::fromUTF8 (expected.textAt0HalfAnd1[2]));

        CHECK_THAT (parameter->convertFrom0to1 (parameter->getValueForText (expected.typed)), WithinAbs (expected.typedValue, 1.0e-2));
    }
}
