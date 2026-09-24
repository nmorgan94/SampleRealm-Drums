#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
/**
 * CustomLookAndFeel
 *
 * Override LookAndFeel_V4 methods here to customise the appearance of your plugin.
 * See: https://docs.juce.com/master/classLookAndFeel__V4.html
 */
class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel()           = default;
    ~CustomLookAndFeel() override = default;

    // Add your custom drawing overrides below, e.g.:
    // void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
    //                        float sliderPosProportional, float rotaryStartAngle,
    //                        float rotaryEndAngle, juce::Slider&) override { ... }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomLookAndFeel)
};
