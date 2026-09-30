#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace srd
{
    //==============================================================================
    /** A rotary slider with its name below, which shows the value while it's being moved. Click the name to type a value. */
    class LabeledSlider : public juce::Component,
                          private juce::Slider::Listener,
                          private juce::Timer
    {
    public:
        LabeledSlider()
        {
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.addListener (this);

            styleLabel (label);

            // Click the name to type a value
            label.setEditable (true);
            label.onEditorShow = [this]
            {
                stopTimer(); // showing the name would close the editor
                if (auto* editor = label.getCurrentTextEditor())
                {
                    editor->setText (valueText(), false);
                    editor->selectAll();
                }
            };
            label.onEditorHide = [this] { startTimer (nameDelayMs); };
            label.onTextChange = [this]
            {
                const auto typedValue = slider.getValueFromText (label.getText());

                juce::Slider::ScopedDragNotification drag (slider);
                slider.setValue (typedValue, juce::sendNotificationSync);
            };

            addAndMakeVisible (slider);
            addAndMakeVisible (label);
        }

        /** Also the component's name, which screen readers use. */
        void setLabelText (const juce::String& newText)
        {
            setName (newText);
            showName();
        }

        juce::Slider& getSlider() noexcept   { return slider; }

        // Other labelled controls can use these two to match and line up with this one

        static void styleLabel (juce::Label& labelBelow)
        {
            labelBelow.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            labelBelow.setJustificationType (juce::Justification::centred);
        }

        /** Puts the label right under a square control area, the pair centred, and returns that area. */
        static juce::Rectangle<int> layOutAboveLabel (juce::Label& labelBelow, juce::Rectangle<int> bounds)
        {
            constexpr int labelHeight = 16;
            const auto size = std::max (0, std::min (bounds.getWidth(), bounds.getHeight() - labelHeight));
            bounds = bounds.withSizeKeepingCentre (bounds.getWidth(), size + labelHeight);
            labelBelow.setBounds (bounds.removeFromBottom (labelHeight));
            return bounds.withSizeKeepingCentre (size, size);
        }

        void resized() override
        {
            slider.setBounds (layOutAboveLabel (label, getLocalBounds()));
        }

    private:
        static constexpr int nameDelayMs = 600;

        juce::Slider slider;
        juce::Label label;
        bool dragging = false;

        void showName()             { label.setText (getName(), juce::dontSendNotification); }
        juce::String valueText()    { return slider.getTextFromValue (slider.getValue()); }
        void showValue()            { label.setText (valueText(), juce::dontSendNotification); }

        // The mouse wheel, double-click reset and typed values are drags too, so they show the value as well
        void sliderDragStarted (juce::Slider*) override     { stopTimer(); dragging = true; showValue(); }
        void sliderValueChanged (juce::Slider*) override    { if (dragging) showValue(); }
        void sliderDragEnded (juce::Slider*) override       { dragging = false; startTimer (nameDelayMs); }

        void timerCallback() override
        {
            stopTimer();
            showName();
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LabeledSlider)
    };
}
