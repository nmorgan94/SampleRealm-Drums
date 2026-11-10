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
// The same boxes as webui/src/HeaderBar.tsx gives webui/src/Editor.tsx
TEST_CASE_METHOD (ProcessorFixture, "The header lays out the switch, the preset bar, the menu and the tail meter", "[editor]")
{
    AudioPluginAudioProcessorEditor editor (processor);

    auto* header = childOf<srd::HeaderBar> (editor);
    REQUIRE (header != nullptr);
    CHECK (header->getBounds() == juce::Rectangle<int> (0, 0, 1080, 68));
    CHECK (header->getTitle() == "SAMPLEREALM DRUMS");

    auto* instrumentSwitch = childOf<srd::ChoiceButtons> (*header);
    auto* presetBar = childOf<srd::PresetBar> (*header);
    auto* menuButton = childOf<juce::TextButton> (*header);
    auto* tailMeter = childOf<srd::TailMeter> (*header);

    REQUIRE (instrumentSwitch != nullptr);
    REQUIRE (presetBar != nullptr);
    REQUIRE (menuButton != nullptr);
    REQUIRE (tailMeter != nullptr);

    CHECK (instrumentSwitch->getBounds() == juce::Rectangle<int> (166, 19, 220, 30));
    CHECK (presetBar->getBounds() == juce::Rectangle<int> (408, 19, 428, 30));
    CHECK (menuButton->getBounds() == juce::Rectangle<int> (840, 19, 28, 30));
    CHECK (tailMeter->getBounds() == juce::Rectangle<int> (890, 12, 174, 44));

    auto* meter = childOf<srd::LevelMeter> (*tailMeter);
    REQUIRE (meter != nullptr);
    CHECK (meter->getBounds() == juce::Rectangle<int> (160, 2, 14, 40));
}

TEST_CASE_METHOD (ProcessorFixture, "The overlays' titles and messages are in the bright text colour", "[editor]")
{
    AudioPluginAudioProcessorEditor editor (processor);

    auto* prompt = childOf<srd::PromptOverlay> (editor);
    auto* manual = childOf<srd::ManualOverlay> (editor);
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
