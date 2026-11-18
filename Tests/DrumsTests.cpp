#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "TestHelpers.h"
#include "ui/TriggerPad.h"

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
namespace
{
    /** The block a note-on of this velocity starts. */
    juce::AudioBuffer<float> hitBlock (AudioPluginAudioProcessor& processor, float velocity)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        process (processor, buffer, juce::MidiMessage::noteOn (1, 36, velocity));
        return buffer;
    }

    float hitLevel (AudioPluginAudioProcessor& processor, float velocity)
    {
        return hitBlock (processor, velocity).getRMSLevel (0, 0, blockSize);
    }

    /** Past any hit's tail. */
    void playSilence (AudioPluginAudioProcessor& processor)
    {
        for (int block = 0; block < 400; ++block)
        {
            juce::AudioBuffer<float> buffer (2, blockSize);
            process (processor, buffer);
        }
    }

    float largestDifference (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        auto largest = 0.0f;

        for (int i = 0; i < a.getNumSamples(); ++i)
            largest = std::max (largest, std::abs (a.getSample (0, i) - b.getSample (0, i)));

        return largest;
    }
}

TEST_CASE_METHOD (ProcessorFixture, "Velocity sets how hard a hit plays, by the Velocity amount", "[dsp][velocity]")
{
    const auto instrument = GENERATE (0.0f, 1.0f, 2.0f);
    CAPTURE (instrument);

    setParameter (processor, Parameters::instrumentId, instrument);

    SECTION ("A soft hit is quieter than a hard one")
    {
        setParameter (processor, Parameters::velocityId, 100.0f);

        // MIDI keeps 7 bits of it
        const auto soft = hitLevel (processor, 0.3f);
        CHECK_THAT (processor.getLastHitLevel(), WithinAbs (38.0 / 127.0, 1.0e-6));

        playSilence (processor);

        const auto hard = hitLevel (processor, 1.0f);
        CHECK_THAT (processor.getLastHitLevel(), WithinAbs (1.0, 1.0e-6));
        CHECK (soft < 0.6f * hard);
        CHECK (soft > 0.0f);
    }

    SECTION ("At 0 % every hit is a full one")
    {
        setParameter (processor, Parameters::velocityId, 0.0f);

        hitLevel (processor, 0.3f);
        CHECK_THAT (processor.getLastHitLevel(), WithinAbs (1.0, 1.0e-6));
    }
}

TEST_CASE_METHOD (ProcessorFixture, "The pad plays at the velocity it's pressed at", "[dsp][velocity]")
{
    setParameter (processor, Parameters::velocityId, 100.0f);

    processor.triggerAudition (0.5f);
    juce::AudioBuffer<float> buffer (2, blockSize);
    process (processor, buffer);

    CHECK_THAT (processor.getLastHitLevel(), WithinAbs (0.5, 1.0e-6));
    CHECK_THAT (processor.getLastHitVelocity(), WithinAbs (0.5, 1.0e-6));
    CHECK (buffer.getMagnitude (0, 0, blockSize) > 0.0f);

    CHECK_THAT (srd::TriggerPad::velocityAt (0.0f, 124.0f), WithinAbs (1.0, 1.0e-6));
    CHECK_THAT (srd::TriggerPad::velocityAt (62.0f, 124.0f), WithinAbs (0.5, 1.0e-6));
    CHECK_THAT (srd::TriggerPad::velocityAt (124.0f, 124.0f), WithinAbs (1.0 / 127.0, 1.0e-6));
}

TEST_CASE_METHOD (ProcessorFixture, "The same note plays the same hit every time", "[dsp]")
{
    const auto instrument = GENERATE (0.0f, 1.0f, 2.0f);
    CAPTURE (instrument);

    setParameter (processor, Parameters::instrumentId, instrument);

    const auto first = hitBlock (processor, 1.0f);
    playSilence (processor);
    const auto second = hitBlock (processor, 1.0f);

    CHECK (largestDifference (first, second) < 1.0e-6f);
}

//==============================================================================
namespace
{
    /** A playing host that gives a tempo and a position. */
    struct TempoPlayHead final : juce::AudioPlayHead
    {
        explicit TempoPlayHead (double tempo) : bpm (tempo) {}

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo position;
            position.setBpm (bpm);
            position.setIsPlaying (true);
            position.setPpqPosition (ppq);
            return position;
        }

        /** As a host moves on after a block. */
        void advance (int numSamples)       { ppq += numSamples * bpm / (60.0 * sampleRate); }

        double bpm, ppq = 0.0;
    };
}

//==============================================================================
TEST_CASE_METHOD (ProcessorFixture, "Width spreads the cymbal over two channels that still sum to the mono hit", "[dsp][width]")
{
    // The largest sample of left + rightSign * right - twice mono's left, where given
    const auto largest = [] (const juce::AudioBuffer<float>& buffer, float rightSign, const juce::AudioBuffer<float>* mono = nullptr)
    {
        auto result = 0.0f;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto expected = mono != nullptr ? 2.0f * mono->getSample (0, i) : 0.0f;
            result = std::max (result, std::abs (buffer.getSample (0, i) + rightSign * buffer.getSample (1, i) - expected));
        }

        return result;
    };

    SECTION ("The cymbal")
    {
        setParameter (processor, Parameters::instrumentId, 2.0f);

        const auto mono = hitBlock (processor, 1.0f);
        CHECK (largest (mono, -1.0f) == 0.0f);

        setParameter (processor, Parameters::cymbalWidthId, 100.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        const auto wide = hitBlock (processor, 1.0f);
        CHECK (largest (wide, -1.0f) > 0.05f);
        CHECK (largest (wide, 1.0f, &mono) < 1.0e-5f);
    }

    SECTION ("With Clip on, each channel stays under its ceiling")
    {
        setParameter (processor, Parameters::instrumentId, 2.0f);
        setParameter (processor, Parameters::clipAmountId, 100.0f);
        setParameter (processor, Parameters::outputGainId, 0.0f);
        setParameter (processor, Parameters::cymbalWidthId, 100.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        const auto wide = hitBlock (processor, 1.0f);
        CHECK (largest (wide, -1.0f) > 0.05f);
        CHECK (wide.getMagnitude (0, blockSize) <= 1.0f + 1.0e-6f);
        CHECK (wide.getMagnitude (0, blockSize) > 0.9f);
    }

    SECTION ("Not the kick")
    {
        setParameter (processor, Parameters::cymbalWidthId, 100.0f);
        processor.prepareToPlay (sampleRate, blockSize);
        CHECK (largest (hitBlock (processor, 1.0f), -1.0f) == 0.0f);
    }

    SECTION ("A mono output gets the hit without Width")
    {
        setParameter (processor, Parameters::instrumentId, 2.0f);
        const auto stereo = hitBlock (processor, 1.0f);

        setParameter (processor, Parameters::cymbalWidthId, 100.0f);
        processor.prepareToPlay (sampleRate, blockSize);

        juce::AudioBuffer<float> mono (1, blockSize);
        process (processor, mono, juce::MidiMessage::noteOn (1, 36, 1.0f));

        CHECK (largestDifference (mono, stereo) < 1.0e-6f);
    }
}

//==============================================================================
TEST_CASE ("The pattern player puts each step on its sample, whatever the block size", "[dsp][loop]")
{
    constexpr double bpm = 174.0;
    const auto samplesPerStep = 60.0 * sampleRate / (bpm * Patterns::stepsPerBeat);
    const auto blockLength = GENERATE (64, 512, 1000);
    CAPTURE (blockLength);

    // Two-step's kick: the first and eleventh sixteenths of each bar
    const auto& part = Patterns::all[0].kick;

    srd::PatternPlayer player;
    std::vector<std::pair<int, int>> hits;  // sample, velocity

    for (int position = 0; position < juce::roundToInt (4 * Patterns::stepsPerBar * samplesPerStep); position += blockLength)
    {
        juce::MidiBuffer midi;
        player.process (midi, part, 36, position * bpm / (60.0 * sampleRate), bpm, sampleRate, blockLength);

        for (const auto event : midi)
            hits.emplace_back (position + event.samplePosition, event.getMessage().getVelocity());
    }

    REQUIRE (hits.size() >= 8);

    for (std::size_t i = 0; i < 8; ++i)
    {
        const auto step = static_cast<int> (i / 2) * Patterns::stepsPerBar + (i % 2 == 0 ? 0 : 10);
        CAPTURE (i);
        CHECK (std::abs (hits[i].first - step * samplesPerStep) <= 1.0);
        CHECK (hits[i].second == (i % 2 == 0 ? 127 : 112));
    }
}

TEST_CASE ("The pattern player replays a step the host jumps back to", "[dsp][loop]")
{
    constexpr double bpm = 174.0;
    const auto samplesPerStep = juce::roundToInt (60.0 * sampleRate / (bpm * Patterns::stepsPerBeat));

    Patterns::Part part {};
    part[3] = 100;

    srd::PatternPlayer player;
    int hits = 0;

    // A host cycling the fourth sixteenth
    for (int cycle = 0; cycle < 4; ++cycle)
    {
        juce::MidiBuffer midi;
        player.process (midi, part, 36, 3.0 / Patterns::stepsPerBeat, bpm, sampleRate, samplesPerStep - 1);
        hits += midi.getNumEvents();
    }

    CHECK (hits == 4);
}

TEST_CASE ("The pattern player plays each step once when its position is nudged", "[dsp][loop]")
{
    constexpr double bpm = 174.0;
    const auto beatsPerBlock = blockSize * bpm / (60.0 * sampleRate);

    Patterns::Part part {};
    part.fill (100);

    srd::PatternPlayer player;
    juce::Random nudge (3);
    int hits = 0;

    // As the loop clock eases: each block starts up to 2 ms either side of where the last ended
    for (int block = 0; block < 1000; ++block)
    {
        juce::MidiBuffer midi;
        const auto ppq = block * beatsPerBlock + (nudge.nextDouble() - 0.5) * 0.004 * bpm / 60.0;
        player.process (midi, part, 36, ppq, bpm, sampleRate, blockSize);
        hits += midi.getNumEvents();
    }

    CHECK (std::abs (hits - juce::roundToInt (1000 * beatsPerBlock * Patterns::stepsPerBeat)) <= 1);
}

TEST_CASE ("The loop clock follows a playing host, and otherwise settles on the system's clock", "[dsp][loop]")
{
    Transport transport;
    transport.bpm = 174.0;

    SECTION ("Playing")
    {
        transport.playing = true;
        transport.ppq = 12.5;

        srd::LoopClock clock;
        CHECK (clock.advance (transport, 1000.0, blockSize, sampleRate) == 12.5);
    }

    SECTION ("Stopped, two that start at different times agree")
    {
        // Blocks arrive up to 4 ms late, each instance's differently
        juce::Random lateness (7);
        const auto arrival = [&] (int block) { return 1000.0 + block * blockSize / sampleRate + lateness.nextDouble() * 0.004; };

        srd::LoopClock first, second;
        double firstPosition = 0.0, secondPosition = 0.0;

        for (int block = 0; block < 400; ++block)
        {
            firstPosition = first.advance (transport, arrival (block), blockSize, sampleRate);

            if (block >= 37)
                secondPosition = second.advance (transport, arrival (block), blockSize, sampleRate);
        }

        // Within a millisecond
        CHECK (std::abs (firstPosition - secondPosition) < 0.001 * transport.bpm / 60.0);
    }

    SECTION ("Stopped, blocks that arrive four at a time still move on evenly")
    {
        constexpr int burst = 4, smallBlock = 1024;
        const auto beatsPerBlock = smallBlock * transport.bpm / (60.0 * sampleRate);

        srd::LoopClock clock;
        double last = 0.0, furthestOff = 0.0;

        for (int block = 0; block < 400; ++block)
        {
            const auto arrival = 1000.0 + (block / burst) * burst * smallBlock / sampleRate;
            const auto position = clock.advance (transport, arrival, smallBlock, sampleRate);

            if (block > 0)
                furthestOff = std::max (furthestOff, std::abs (position - last - beatsPerBlock));

            last = position;
        }

        // Within 2 ms of a block's length
        CHECK (furthestOff < 0.002 * transport.bpm / 60.0);
    }

    SECTION ("Stopped, a new tempo starts again from the clock")
    {
        srd::LoopClock clock;
        clock.advance (transport, 1000.0, blockSize, sampleRate);

        transport.bpm = 120.0;
        CHECK (clock.advance (transport, 1000.0, blockSize, sampleRate) == 2000.0);
    }
}

TEST_CASE_METHOD (ProcessorFixture, "The loop plays the instrument's part of the pattern in time with the host", "[dsp][loop]")
{
    // At 120 bpm a bar is 96000 samples: 187 blocks stop just short of the next
    TempoPlayHead playHead { 120.0 };
    processor.setPlayHead (&playHead);

    const auto playBar = [&]
    {
        const auto before = processor.getHitCount();

        for (int block = 0; block < 187; ++block)
        {
            juce::AudioBuffer<float> buffer (2, blockSize);
            process (processor, buffer);
            playHead.advance (blockSize);
        }

        return processor.getHitCount() - before;
    };

    SECTION ("Off, nothing plays")
    {
        CHECK (playBar() == 0);
    }

    SECTION ("Each instrument plays its own part")
    {
        processor.setLoop (0, true);
        CHECK (playBar() == 2);

        // The kick's last hit was at velocity 112, with Velocity at 50 %
        CHECK_THAT (processor.getLastHitLevel(), WithinAbs (1.0 - 0.5 * (1.0 - 112.0 / 127.0), 1.0e-5));

        playHead.ppq = 0.0;
        setParameter (processor, Parameters::instrumentId, 2.0f);
        CHECK (playBar() == 8);
    }

    SECTION ("Turning it off stops it")
    {
        processor.setLoop (0, true);
        playBar();
        processor.setLoop (0, false);
        CHECK (playBar() == 0);
        CHECK (processor.getPlayingStep() == -1);
    }

    SECTION ("A step can be set, for the current instrument only, until a new pattern")
    {
        using srd::LoopSteps;

        // A rest comes on at full velocity, and a step switched off keeps its own for when it's back
        CHECK (LoopSteps::toggled (0) == 127);
        CHECK (LoopSteps::toggled (112) == -112);
        CHECK (LoopSteps::toggled (-112) == 112);

        processor.setLoop (0, true);
        processor.setLoopStep (4, 64);
        processor.setLoopStep (10, LoopSteps::toggled (112));
        processor.setLoopStep (16, 100);
        CHECK (processor.getLoopSteps() == Patterns::Part { 127, 0, 0, 0, 64, 0, 0, 0, 0, 0, -112, 0, 0, 0, 0, 0 });
        CHECK (playBar() == 2);

        // The last hit was at velocity 64, with Velocity at 50 %
        CHECK_THAT (processor.getLastHitLevel(), WithinAbs (1.0 - 0.5 * (1.0 - 64.0 / 127.0), 1.0e-5));
        CHECK (processor.getPlayingStep() == 15);

        processor.setLoopStep (4, 500);
        CHECK (processor.getLoopSteps()[4] == 127);

        // Switching the loop off and on keeps the edit
        processor.setLoop (0, false);
        processor.setLoop (0, true);
        CHECK (processor.getLoopSteps()[4] == 127);

        setParameter (processor, Parameters::instrumentId, 1.0f);
        CHECK (processor.getLoopSteps() == Patterns::all[0].snare);

        setParameter (processor, Parameters::instrumentId, 0.0f);
        processor.setLoop (1, true);
        CHECK (processor.getLoopSteps() == Patterns::all[1].kick);
    }

    processor.setPlayHead (nullptr);
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
