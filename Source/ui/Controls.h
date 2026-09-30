#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "LabeledSlider.h"
#include "SegmentedButtons.h"

//==============================================================================
/**
 * Parameter controls with their APVTS attachments. Labels show the parameter's name
 * unless given other text; the LookAndFeel supplies the typeface.
 */
namespace srd
{
    namespace detail
    {
        inline juce::RangedAudioParameter& getParameter (juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& id)
        {
            auto* param = apvts.getParameter (id.getParamID());
            jassert (param != nullptr);
            return *param;
        }

        /** labelText, or the parameter's name if that's empty. */
        inline juce::String labelFor (const juce::RangedAudioParameter& param, const juce::String& labelText)
        {
            return labelText.isNotEmpty() ? labelText : param.getName (32);
        }

        inline void initLabel (juce::Label& label, const juce::RangedAudioParameter& param, const juce::String& labelText)
        {
            label.setText (labelFor (param, labelText), juce::dontSendNotification);
            LabeledSlider::styleLabel (label);
            label.setInterceptsMouseClicks (false, false);
        }
    }

    //==============================================================================
    /** A LabeledSlider for a parameter. Double-click returns it to the default. */
    class Knob : public LabeledSlider
    {
    public:
        Knob (juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& id, const juce::String& labelText = {})
            : attachment (apvts, id.getParamID(), getSlider())
        {
            setLabelText (detail::labelFor (detail::getParameter (apvts, id), labelText));
        }

    private:
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
    };

    //==============================================================================
    /** A combo box listing a choice parameter's values. Unlabelled, e.g. for a panel header. */
    class ChoiceBox : public juce::ComboBox
    {
    public:
        ChoiceBox (juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& id)
        {
            // Items must exist before the attachment reads the current value
            addItemList (detail::getParameter (apvts, id).getAllValueStrings(), 1);
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id.getParamID(), *this);
        }

    private:
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoiceBox)
    };

    //==============================================================================
    /** A row of joined buttons, one per value of a choice parameter. */
    class ChoiceButtons : public SegmentedButtons
    {
    public:
        /** onChange is called whenever the parameter changes, from here or anywhere else. */
        ChoiceButtons (juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& id)
            : param (detail::getParameter (apvts, id)),
              attachment (param, [this] (float value) { setSelectedIndex (juce::roundToInt (value), juce::sendNotificationSync); },
                          apvts.undoManager)
        {
            setItems (param.getAllValueStrings());
            attachment.sendInitialUpdate();
        }

    private:
        juce::RangedAudioParameter& param;
        juce::ParameterAttachment attachment;

        void buttonClicked (int index) override
        {
            attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getValueForText (param.getAllValueStrings()[index])));
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoiceButtons)
    };

    //==============================================================================
    /** An on/off button for a two-state parameter, showing the parameter's value text. */
    class Toggle : public juce::Component
    {
    public:
        Toggle (juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& id, const juce::String& labelText = {})
            : param (detail::getParameter (apvts, id)),
              attachment (apvts, id.getParamID(), button)
        {
            detail::initLabel (label, param, labelText);

            // Reads the toggle state: on a click, this runs before the attachment updates the parameter
            button.setClickingTogglesState (true);
            button.onStateChange = [this] { button.setButtonText (param.getText (button.getToggleState() ? 1.0f : 0.0f, 32)); };
            button.onStateChange();

            addAndMakeVisible (label);
            addAndMakeVisible (button);
        }

        void resized() override
        {
            const auto bounds = LabeledSlider::layOutAboveLabel (label, getLocalBounds());
            button.setBounds (bounds.withSizeKeepingCentre (std::min (bounds.getWidth(), 64), std::min (bounds.getHeight(), 24)));
        }

    private:
        juce::RangedAudioParameter& param;
        juce::Label label;
        juce::TextButton button;
        juce::AudioProcessorValueTreeState::ButtonAttachment attachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Toggle)
    };
}
