#pragma once

#include "../presets/PresetManager.h"
#include "PromptOverlay.h"

namespace srd
{
    //==============================================================================
    /**
     * Preset browsing and management for a PresetManager.
     */
    class PresetBar : public juce::Component,
                      private juce::Timer
    {
    public:
        PresetBar (PresetManager&, PromptOverlay&);

        void resized() override;

    private:
        PresetManager& presets;
        PromptOverlay& prompt;

        juce::TextButton previousButton { "<" }, nextButton { ">" }, nameButton,
                         saveButton { "Save" }, moreButton { "..." };
        juce::uint32 lastChange = 0;

        void updateName();
        void showPresetMenu();
        void showMoreMenu();

        void save();
        void saveAs();
        void saveAs (const juce::String& name);
        void rename (const PresetManager::Preset&);
        void remove (const PresetManager::Preset&);
        std::optional<PresetManager::Preset> currentUserPreset() const;
        void report (const juce::Result&);

        void timerCallback() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
    };
}
