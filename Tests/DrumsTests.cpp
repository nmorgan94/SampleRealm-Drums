#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"

using namespace TestHelpers;
using Catch::Matchers::WithinAbs;

//==============================================================================
TEST_CASE_METHOD (ProcessorFixture, "Float parameters have the expected ranges and defaults", "[parameters]")
{
    struct Expected { juce::ParameterID id; float min, max, defaultValue; };

    const auto expected = GENERATE (values<Expected> ({
        { Parameters::outputGainId,         -60.0f,     6.0f,    -2.5f },
        { Parameters::tuneId,               -24.0f,    24.0f,     0.0f },
        { Parameters::lengthId,               0.25f,    4.0f,     1.0f },
        { Parameters::pitchDepthId,           0.0f,   200.0f,   100.0f },
        { Parameters::subLevelId,           -60.0f,     6.0f,     0.0f },
        { Parameters::subHarmonicsId,         0.0f,   100.0f,     0.0f },
        { Parameters::subPhaseId,             0.0f,    90.0f,     0.0f },
        { Parameters::clickLevelId,         -60.0f,     6.0f,   -12.0f },
        { Parameters::clickToneId,          200.0f, 16000.0f,  4000.0f },
        { Parameters::clickDecayId,           1.0f,   100.0f,    12.0f },
        { Parameters::clickPitchId,         200.0f, 10000.0f,  2500.0f },
        { Parameters::snareBodyLevelId,     -60.0f,     6.0f,    -4.0f },
        { Parameters::snareBodyHarmonicsId,   0.0f,   100.0f,    20.0f },
        { Parameters::snareNoiseLevelId,    -60.0f,     6.0f,     0.0f },
        { Parameters::snareNoiseLowCutId,   100.0f,  8000.0f,   800.0f },
        { Parameters::snareNoiseHighCutId, 1000.0f, 18000.0f, 12000.0f },
        { Parameters::snareSnapLevelId,     -60.0f,     6.0f,    -6.0f },
        { Parameters::snareSnapToneId,      200.0f, 16000.0f,  3500.0f },
        { Parameters::snareSnapDecayId,       1.0f,   100.0f,     8.0f },
        { Parameters::snareSnapPitchId,     200.0f, 10000.0f,  2500.0f },
        { Parameters::cymbalMetalLevelId,   -60.0f,     6.0f,    -6.0f },
        { Parameters::cymbalMetalToneId,   2000.0f, 16000.0f,  8000.0f },
        { Parameters::cymbalMetalRingId,      0.0f,   100.0f,    30.0f },
        { Parameters::cymbalNoiseLevelId,   -60.0f,     6.0f,     0.0f },
        { Parameters::cymbalNoiseLowCutId,  200.0f, 12000.0f,  6000.0f },
        { Parameters::cymbalNoiseHighCutId,1000.0f, 18000.0f, 16000.0f },
        { Parameters::driveAmountId,          0.0f,   100.0f,    20.0f },
        { Parameters::driveMixId,             0.0f,   100.0f,   100.0f },
        { Parameters::eqLowGainId,          -12.0f,    12.0f,     0.0f },
        { Parameters::eqMidFreqId,          100.0f,  5000.0f,   400.0f },
        { Parameters::eqMidGainId,          -12.0f,    12.0f,     0.0f },
        { Parameters::eqHighGainId,         -12.0f,    12.0f,     0.0f },
        { Parameters::clipAmountId,           0.0f,   100.0f,     0.0f },
    }));

    CAPTURE (expected.id.getParamID());

    auto* parameter = processor.getAPVTS().getParameter (expected.id.getParamID());

    REQUIRE (parameter != nullptr);
    CHECK_THAT (parameter->getNormalisableRange().start, WithinAbs (expected.min, 1.0e-4));
    CHECK_THAT (parameter->getNormalisableRange().end, WithinAbs (expected.max, 1.0e-4));
    CHECK_THAT (getParameter (processor, expected.id), WithinAbs (expected.defaultValue, 1.0e-3));
}

TEST_CASE_METHOD (ProcessorFixture, "Choice parameters have the expected options and default to the first", "[parameters]")
{
    struct Expected { juce::ParameterID id; int numChoices; };

    const auto expected = GENERATE (values<Expected> ({
        { Parameters::instrumentId,    3 },
        { Parameters::keyTrackId,      2 },
        { Parameters::clickTypeId,     3 },
        { Parameters::snareSnapTypeId, 3 },
        { Parameters::driveTypeId,     4 },
    }));

    CAPTURE (expected.id.getParamID());

    auto* parameter = processor.getAPVTS().getParameter (expected.id.getParamID());

    REQUIRE (parameter != nullptr);
    CHECK (parameter->getNumSteps() == expected.numChoices);
    CHECK (getParameter (processor, expected.id) == 0.0f);
}

TEST_CASE_METHOD (ProcessorFixture, "Every parameter is covered by the tests above", "[parameters]")
{
    CHECK (processor.getParameters().size() == 38);
}

//==============================================================================
TEST_CASE_METHOD (ProcessorFixture, "Only mono or stereo output with no input is supported", "[buses]")
{
    using Set = juce::AudioChannelSet;

    // Each layout is { { input bus }, { output bus } }
    CHECK (processor.isBusesLayoutSupported ({ {}, { Set::mono() } }));
    CHECK (processor.isBusesLayoutSupported ({ {}, { Set::stereo() } }));

    CHECK_FALSE (processor.isBusesLayoutSupported ({ {}, { Set::create5point1() } }));
    CHECK_FALSE (processor.isBusesLayoutSupported ({ { Set::stereo() }, { Set::stereo() } }));
}

//==============================================================================
TEST_CASE_METHOD (ProcessorFixture, "Output stays finite for any block size, instrument and settings", "[dsp]")
{
    // Includes a block larger than the size given to prepareToPlay, which some hosts send
    const auto numSamples = GENERATE (1, 64, blockSize, blockSize * 4);
    const auto instrument = GENERATE (0.0f, 1.0f, 2.0f);

    // Every parameter at its minimum, middle and maximum
    const auto normalisedValue = GENERATE (0.0f, 0.5f, 1.0f);

    CAPTURE (numSamples, instrument, normalisedValue);

    for (auto* parameter : processor.getParameters())
    {
        // Except the levels' minimum, which is silence and would leave nothing to test
        const auto isSilent = parameter->getText (normalisedValue, 32) == "-inf dB";
        parameter->setValueNotifyingHost (isSilent ? parameter->getDefaultValue() : normalisedValue);
    }

    setParameter (processor, Parameters::instrumentId, instrument);

    // About a quarter of a second, retriggering partway through
    const auto numBlocks = std::max (4, juce::roundToInt (sampleRate / 4) / numSamples);

    for (int block = 0; block < numBlocks; ++block)
    {
        juce::MidiBuffer midi;

        if (block == 0 || block == numBlocks / 2)
            midi.addEvent (juce::MidiMessage::noteOn (1, 36, 1.0f), numSamples / 2);

        juce::AudioBuffer<float> buffer (2, numSamples);
        processor.processBlock (buffer, midi);

        REQUIRE (allSamplesFinite (buffer));
    }
}

//==============================================================================
TEST_CASE_METHOD (ProcessorFixture, "State round trips into a new instance", "[state]")
{
    setParameter (processor, Parameters::instrumentId, 1.0f);
    setParameter (processor, Parameters::tuneId, 7.5f);
    setParameter (processor, Parameters::driveTypeId, 2.0f);

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    AudioPluginAudioProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    CHECK_THAT (getParameter (restored, Parameters::instrumentId), WithinAbs (1.0, 1.0e-4));
    CHECK_THAT (getParameter (restored, Parameters::tuneId), WithinAbs (7.5, 1.0e-4));
    CHECK_THAT (getParameter (restored, Parameters::driveTypeId), WithinAbs (2.0, 1.0e-4));
}

TEST_CASE_METHOD (ProcessorFixture, "Invalid state is ignored", "[state]")
{
    setParameter (processor, Parameters::tuneId, -3.0f);

    SECTION ("Random bytes")
    {
        const char garbage[] = "definitely not plugin state";
        processor.setStateInformation (garbage, static_cast<int> (sizeof (garbage)));
    }

    SECTION ("Empty data")
    {
        processor.setStateInformation (nullptr, 0);
    }

    SECTION ("Valid XML with the wrong root tag")
    {
        juce::MemoryBlock state;
        juce::AudioProcessor::copyXmlToBinary (juce::XmlElement ("SomethingElse"), state);
        processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    }

    CHECK_THAT (getParameter (processor, Parameters::tuneId), WithinAbs (-3.0, 1.0e-4));
}
