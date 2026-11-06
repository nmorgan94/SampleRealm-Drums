#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/** Shows text in a small bubble while the mouse is over the target. Add it with addChildComponent.
    Unlike TooltipWindow, it shows at once and in a fixed place. */
class HoverTip : public juce::Label
{
public:
    HoverTip (juce::Component& targetToWatch, const juce::String& text) : target (&targetToWatch)
    {
        setText (text, juce::dontSendNotification);
        setFont (juce::FontOptions (13.0f));
        setJustificationType (juce::Justification::centred);
        setInterceptsMouseClicks (false, false);
        target->addMouseListener (this, false);
    }

    ~HoverTip() override
    {
        if (target != nullptr)
            target->removeMouseListener (this);
    }

    int getIdealWidth()
    {
        const auto textWidth = juce::GlyphArrangement::getStringWidth (getLookAndFeel().getLabelFont (*this), getText());
        return static_cast<int> (std::ceil (textWidth)) + getBorderSize().getLeftAndRight();
    }

    void mouseEnter (const juce::MouseEvent&) override   { setVisible (true); }
    void mouseExit (const juce::MouseEvent&) override    { setVisible (false); }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

        g.setColour (findColour (juce::Slider::backgroundColourId));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

        juce::Label::paint (g);
    }

private:
    juce::Component::SafePointer<juce::Component> target;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HoverTip)
};
