#include <catch2/catch_test_macros.hpp>

#include <span>

#include "TestHelpers.h"

namespace
{
    // Shared with wasm/engine.test.mjs, which checks the engine against the same fingerprints
    const auto goldenFile = juce::File (__FILE__).getSiblingFile ("fixtures/golden.json");

    // RMS then peak of the (mono) output, for each frame of blocksPerFrame blocks
    using Fingerprint = std::vector<std::array<float, 2>>;

    Fingerprint render (const juce::var& golden, const juce::var& scenario)
    {
        const auto sampleRate = static_cast<double> (golden["sampleRate"]);
        const int blockSize = golden["blockSize"];
        const int blocksPerFrame = golden["blocksPerFrame"];

        juce::ScopedJuceInitialiser_GUI juceInitialiser;
        AudioPluginAudioProcessor processor;
        processor.setRateAndBufferSizeDetails (sampleRate, blockSize);

        if (const auto presetName = scenario["preset"].toString(); presetName.isNotEmpty())
        {
            auto& presets = processor.getPresetManager();
            const auto& factory = presets.getFactoryPresets();
            const auto found = std::ranges::find_if (factory, [&] (const auto& preset) { return preset.name == presetName; });

            REQUIRE (found != factory.end());
            REQUIRE (presets.loadPreset (*found));
        }

        for (const auto& [name, value] : scenario["params"].getDynamicObject()->getProperties())
            TestHelpers::setParameter (processor, { name.toString(), 1 }, value);

        processor.prepareToPlay (sampleRate, blockSize);

        Fingerprint fingerprint;
        double sumOfSquares = 0.0;
        float peak = 0.0f;
        const int numBlocks = scenario["blocks"];

        for (int block = 0; block < numBlocks; ++block)
        {
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;

            for (const auto& note : *scenario["notes"].getArray())
                if (static_cast<int> (note["block"]) == block)
                    midi.addEvent (juce::MidiMessage::noteOn (1, note["note"], static_cast<juce::uint8> (100)), note["offset"]);

            processor.processBlock (buffer, midi);

            for (const auto sample : std::span (buffer.getReadPointer (0), static_cast<size_t> (blockSize)))
            {
                sumOfSquares += sample * sample;
                peak = std::max (peak, std::abs (sample));
            }

            if ((block + 1) % blocksPerFrame == 0)
            {
                fingerprint.push_back ({ static_cast<float> (std::sqrt (sumOfSquares / (blockSize * blocksPerFrame))), peak });
                sumOfSquares = 0.0;
                peak = 0.0f;
            }
        }

        processor.releaseResources();
        return fingerprint;
    }

    // A line per frame and note, so a re-record's diff shows what moved
    juce::String toGoldenText (const juce::var& golden)
    {
        const auto options = juce::JSON::FormatOptions{}.withSpacing (juce::JSON::Spacing::singleLine).withMaxDecimalPlaces (7);
        const auto line = [&] (const juce::var& value) { return juce::JSON::toString (value, options); };

        juce::StringArray scenarios;

        for (const auto& scenario : *golden["scenarios"].getArray())
        {
            juce::StringArray properties;

            for (const auto& [name, value] : scenario.getDynamicObject()->getProperties())
            {
                auto text = "      " + line (name.toString()) + ": ";
                const auto* elements = value.getArray();

                if (elements == nullptr || elements->isEmpty())
                {
                    properties.add (text + line (value));
                    continue;
                }

                juce::StringArray lines;

                for (const auto& element : *elements)
                    lines.add ("        " + line (element));

                properties.add (text + "[\n" + lines.joinIntoString (",\n") + "\n      ]");
            }

            scenarios.add ("    {\n" + properties.joinIntoString (",\n") + "\n    }");
        }

        juce::StringArray header;

        for (const auto& [name, value] : golden.getDynamicObject()->getProperties())
            if (name.toString() != "scenarios")
                header.add ("  " + line (name.toString()) + ": " + line (value));

        return "{\n" + header.joinIntoString (",\n") + ",\n  \"scenarios\": [\n" + scenarios.joinIntoString (",\n") + "\n  ]\n}\n";
    }

    juce::var readGolden()
    {
        auto golden = juce::JSON::parse (goldenFile);
        REQUIRE (golden.isObject());
        return golden;
    }
}

//==============================================================================
TEST_CASE ("Renders match the golden fingerprints", "[dsp][golden]")
{
    const auto golden = readGolden();

    for (const auto& scenario : *golden["scenarios"].getArray())
    {
        CAPTURE (scenario["name"].toString());

        const auto* expected = scenario["fingerprint"].getArray();
        REQUIRE (expected != nullptr);

        const auto actual = render (golden, scenario);
        REQUIRE (std::cmp_equal (actual.size(), expected->size()));

        const auto differs = [] (float got, float want) { return std::abs (got - want) > 1.0e-5f + 1.0e-4f * std::abs (want); };
        int firstMismatch = -1;
        int frame = 0;

        for (const auto& [rms, peak] : actual)
        {
            const auto& want = (*expected)[frame];

            if (differs (rms, want[0]) || differs (peak, want[1]))
            {
                firstMismatch = frame;
                break;
            }

            ++frame;
        }

        CAPTURE (firstMismatch);
        CHECK (firstMismatch == -1);
    }
}

// Run only to record new fingerprints: Drums_Tests "[.write-golden]"
TEST_CASE ("Write the golden fingerprints", "[.write-golden]")
{
    auto golden = readGolden();

    for (auto& scenario : *golden["scenarios"].getArray())
    {
        juce::Array<juce::var> frames;

        for (const auto& frame : render (golden, scenario))
            frames.add (juce::Array<juce::var> { frame[0], frame[1] });

        scenario.getDynamicObject()->setProperty ("fingerprint", frames);
    }

    REQUIRE (goldenFile.replaceWithText (toGoldenText (golden)));
}
