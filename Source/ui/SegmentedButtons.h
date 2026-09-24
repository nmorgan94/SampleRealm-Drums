#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace srd
{
    //==============================================================================
    /** A row of joined buttons with one selected at a time: tabs, or a mode switch. */
    class SegmentedButtons : public juce::Component
    {
    public:
        SegmentedButtons() = default;

        void setItems (const juce::StringArray& newItems)
        {
            buttons.clear();

            for (int i = 0; i < newItems.size(); ++i)
            {
                auto* button = buttons.add (std::make_unique<juce::TextButton> (newItems[i]));
                button->setRadioGroupId (1);
                button->setClickingTogglesState (true);
                button->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0)
                                           | (i < newItems.size() - 1 ? juce::Button::ConnectedOnRight : 0));
                button->onClick = [this, i]
                {
                    if (i != selected)
                        buttonClicked (i);
                };
                addAndMakeVisible (button);
            }

            applySelectedColour();
            setSelectedIndex (juce::jlimit (0, juce::jmax (0, newItems.size() - 1), selected));
            resized();
        }

        int getNumItems() const noexcept                        { return buttons.size(); }
        int getSelectedIndex() const noexcept                   { return selected; }

        void setSelectedIndex (int index, juce::NotificationType notification = juce::dontSendNotification)
        {
            selected = index;

            if (auto* button = buttons[index])
                button->setToggleState (true, juce::dontSendNotification);

            if (notification != juce::dontSendNotification && onChange != nullptr)
                onChange (index);
        }

        void setSelectedColour (juce::Colour colour)
        {
            selectedColour = colour;
            applySelectedColour();
        }

        std::function<void (int)> onChange;

        void resized() override
        {
            auto bounds = getLocalBounds();
            const auto width = bounds.getWidth() / juce::jmax (1, buttons.size());

            for (auto* button : buttons)
                button->setBounds (button == buttons.getLast() ? bounds : bounds.removeFromLeft (width));
        }

    protected:
        virtual void buttonClicked (int index)     { setSelectedIndex (index, juce::sendNotificationSync); }

    private:
        juce::OwnedArray<juce::TextButton> buttons;
        std::optional<juce::Colour> selectedColour;
        int selected = 0;

        void applySelectedColour()
        {
            if (selectedColour.has_value())
                for (auto* button : buttons)
                    button->setColour (juce::TextButton::buttonOnColourId, *selectedColour);
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedButtons)
    };
}
