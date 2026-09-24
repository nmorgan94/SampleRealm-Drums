#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

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

        /** Shows labelText, or the parameter's name if that's empty. */
        inline void initLabel (juce::Label& label, const juce::RangedAudioParameter& param, const juce::String& labelText)
        {
            label.setText (labelText.isNotEmpty() ? labelText : param.getName (32), juce::dontSendNotification);
            label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            label.setJustificationType (juce::Justification::centred);
            label.setInterceptsMouseClicks (false, false);
        }
    }

    //==============================================================================
    /** A rotary slider with its name above. Double-click returns it to the default. */
    class Knob : public juce::Component
    {
    public:
        Knob (juce::AudioProcessorValueTreeState& apvts, const juce::ParameterID& id, const juce::String& labelText = {})
            : attachment (apvts, id.getParamID(), slider)
        {
            const auto& param = detail::getParameter (apvts, id);
            detail::initLabel (label, param, labelText);

            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
            slider.setDoubleClickReturnValue (true, param.convertFrom0to1 (param.getDefaultValue()));

            addAndMakeVisible (label);
            addAndMakeVisible (slider);
        }

        juce::Slider& getSlider() noexcept   { return slider; }

        void resized() override
        {
            auto bounds = getLocalBounds();
            label.setBounds (bounds.removeFromTop (16));
            slider.setBounds (bounds);
        }

    private:
        juce::Label label;
        juce::Slider slider;
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
            auto bounds = getLocalBounds();
            label.setBounds (bounds.removeFromTop (16));
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
