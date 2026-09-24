#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
 * Dark theme set in Orbitron.
 *
 */
class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    struct Palette
    {
        static inline const juce::Colour background { 0xff0d0f13 };
        static inline const juce::Colour panel      { 0xff161a21 };
        static inline const juce::Colour raised     { 0xff1f242d };
        static inline const juce::Colour outline    { 0xff2a303b };
        static inline const juce::Colour grid       { 0xff212631 };
        static inline const juce::Colour text       { 0xffe6e9ef };
        static inline const juce::Colour textDim    { 0xff7d8595 };
        static inline const juce::Colour accent     { 0xffff6a2b }; // pitch, knob arcs
        static inline const juce::Colour accentAlt  { 0xff38d5f2 }; // amp
        static inline const juce::Colour waveform   { 0xff4a5263 };
    };

    CustomLookAndFeel();

    juce::Font font (float height, bool bold = false) const;

    //==============================================================================
    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;

private:
    juce::Typeface::Ptr regular, bold;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomLookAndFeel)
};
