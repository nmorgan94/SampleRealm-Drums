#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"

using namespace TestHelpers;
using Catch::Matchers::WithinAbs;

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
