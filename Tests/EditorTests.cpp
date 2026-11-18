#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.h"

using TestHelpers::ProcessorFixture;

namespace
{
    template <typename ComponentType>
    ComponentType* childOf (juce::Component& parent)
    {
        for (auto* child : parent.getChildren())
            if (auto* found = dynamic_cast<ComponentType*> (child))
                return found;

        return nullptr;
    }
}

//==============================================================================
// The same boxes as the shared HeaderBar.tsx gives webui/src/Editor.tsx
TEST_CASE_METHOD (ProcessorFixture, "The header lays out the title with its switch, the preset bar, the menu and the tail meter", "[editor]")
{
    AudioPluginAudioProcessorEditor editor (processor);

    auto* header = childOf<HeaderBar> (editor);
    REQUIRE (header != nullptr);
    CHECK (header->getBounds() == juce::Rectangle<int> (0, 0, 1080, 68));
    CHECK (header->getTitle() == "SAMPLEREALM DRUMS");

    auto* title = childOf<srd::HeaderTitle> (*header);
    auto* presetBar = childOf<PresetBar> (*header);
    auto* menuButton = childOf<juce::TextButton> (*header);
    auto* tailMeter = childOf<srd::TailMeter> (*header);

    REQUIRE (title != nullptr);
    REQUIRE (presetBar != nullptr);
    REQUIRE (menuButton != nullptr);
    REQUIRE (tailMeter != nullptr);

    CHECK (title->getBounds() == juce::Rectangle<int> (16, 0, 370, 68));
    CHECK (presetBar->getBounds() == juce::Rectangle<int> (408, 19, 428, 30));
    CHECK (menuButton->getBounds() == juce::Rectangle<int> (840, 19, 28, 30));
    CHECK (tailMeter->getBounds() == juce::Rectangle<int> (890, 12, 174, 44));

    auto* instrumentSwitch = childOf<srd::ChoiceButtons> (*title);
    REQUIRE (instrumentSwitch != nullptr);
    CHECK (instrumentSwitch->getBounds() == juce::Rectangle<int> (150, 19, 220, 30));

    auto* meter = childOf<LevelMeter> (*tailMeter);
    REQUIRE (meter != nullptr);
    CHECK (meter->getBounds() == juce::Rectangle<int> (147, 2, 27, 40));
}

// As LABEL_HEIGHT and NAME_HEIGHT give the Knob in webui/src/controls.tsx
TEST_CASE_METHOD (ProcessorFixture, "A knob's name takes Drums' label height and font height", "[editor]")
{
    srd::Knob knob (processor.getAPVTS(), Parameters::tuneId);
    knob.setSize (80, 90);

    auto* name = childOf<juce::Label> (knob);
    REQUIRE (name != nullptr);

    CHECK (knob.getSlider().getBounds() == juce::Rectangle<int> (3, 0, 74, 74));
    CHECK (name->getBounds() == juce::Rectangle<int> (0, 74, 80, 16));
    CHECK (name->getFont().getHeight() == 11.0f);
}

TEST_CASE_METHOD (ProcessorFixture, "The overlays' titles and messages are in the bright text colour", "[editor]")
{
    AudioPluginAudioProcessorEditor editor (processor);

    auto* prompt = childOf<PromptOverlay> (editor);
    auto* manual = childOf<ManualOverlay> (editor);
    REQUIRE (prompt != nullptr);
    REQUIRE (manual != nullptr);

    int numLabels = 0;

    for (auto* overlay : std::initializer_list<juce::Component*> { prompt, manual })
    {
        for (auto* child : overlay->getChildren())
        {
            if (auto* label = dynamic_cast<juce::Label*> (child))
            {
                ++numLabels;
                CHECK (label->findColour (juce::Label::textColourId) == CustomLookAndFeel::Palette::text);
            }
        }
    }

    CHECK (numLabels == 3);
}
