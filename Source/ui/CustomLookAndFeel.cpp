#include "CustomLookAndFeel.h"
#include "EnvelopeEditor.h"
#include "Panel.h"
#include "PromptOverlay.h"
#include "TriggerPad.h"
#include <BinaryData.h>

CustomLookAndFeel::CustomLookAndFeel()
    : LookAndFeel_V4 (ColourScheme (Palette::background, Palette::raised, Palette::panel, Palette::outline, Palette::text,
                                    Palette::raised, Palette::background, Palette::accent, Palette::text)),
      regular (juce::Typeface::createSystemTypefaceFor (BinaryData::OrbitronRegular_ttf, BinaryData::OrbitronRegular_ttfSize)),
      bold    (juce::Typeface::createSystemTypefaceFor (BinaryData::OrbitronBold_ttf, BinaryData::OrbitronBold_ttfSize))
{
    using P = Palette;

    const std::initializer_list<std::pair<int, juce::Colour>> colours
    {
        { juce::Label::textColourId,                     P::textDim },

        { juce::Slider::rotarySliderOutlineColourId,     P::outline },
        { juce::Slider::thumbColourId,                   P::text },
        { juce::Slider::textBoxOutlineColourId,          juce::Colours::transparentBlack },
        { juce::Slider::textBoxHighlightColourId,        P::accent.withAlpha (0.4f) },

        { juce::ComboBox::arrowColourId,                 P::textDim },
        { juce::PopupMenu::headerTextColourId,           P::textDim },

        { juce::TextEditor::backgroundColourId,          P::background },
        { juce::TextEditor::focusedOutlineColourId,      P::accent },
        { juce::TextEditor::highlightColourId,           P::accent.withAlpha (0.4f) },
        { juce::CaretComponent::caretColourId,           P::accent },

        { srd::Panel::backgroundColourId,                P::panel },
        { srd::Panel::outlineColourId,                   P::outline },
        { srd::Panel::titleColourId,                     P::text },

        { srd::EnvelopeEditor::backgroundColourId,       P::background },
        { srd::EnvelopeEditor::gridColourId,             P::grid },
        { srd::EnvelopeEditor::gridTextColourId,         P::textDim },
        { srd::EnvelopeEditor::lineColourId,             P::accent },
        { srd::EnvelopeEditor::nodeColourId,             P::background },
        { srd::EnvelopeEditor::readoutColourId,          P::text },

        { srd::TriggerPad::backgroundColourId,           P::raised },
        { srd::TriggerPad::flashColourId,                P::accent },
        { srd::TriggerPad::textColourId,                 P::text },

        { srd::PromptOverlay::dimColourId,               P::background.withAlpha (0.75f) },
        { srd::PromptOverlay::panelColourId,             P::panel },
        { srd::PromptOverlay::outlineColourId,           P::outline },
    };

    for (const auto& [id, colour] : colours)
        setColour (id, colour);
}

juce::Font CustomLookAndFeel::font (float height, bool isBold) const
{
    return juce::Font (juce::FontOptions (isBold ? bold : regular).withHeight (height));
}

//==============================================================================
juce::Font CustomLookAndFeel::getLabelFont (juce::Label& label)
{
    return font (label.getFont().getHeight(), label.getFont().isBold());
}

juce::Font CustomLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return font (12.0f);
}

juce::Font CustomLookAndFeel::getPopupMenuFont()
{
    return font (13.0f);
}

juce::Font CustomLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return font (12.0f);
}

juce::Label* CustomLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::FontOptions (11.0f));
    return label;
}

//==============================================================================
void CustomLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const auto radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto trackWidth = std::max (2.5f, radius * 0.12f);
    const auto arcRadius = radius - trackWidth * 0.5f;
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Symmetric ranges (tune, EQ gain) fill from the centre
    const auto isBipolar = slider.getMinimum() < 0.0 && juce::approximatelyEqual (slider.getMinimum(), -slider.getMaximum());
    const auto fromAngle = isBipolar ? 0.5f * (rotaryStartAngle + rotaryEndAngle) : rotaryStartAngle;

    const auto arc = [&] (float from, float to)
    {
        juce::Path path;
        path.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, from, to, true);
        return path;
    };

    const juce::PathStrokeType stroke (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    g.setColour (slider.findColour (juce::Slider::rotarySliderOutlineColourId));
    g.strokePath (arc (rotaryStartAngle, rotaryEndAngle), stroke);

    if (slider.isEnabled())
    {
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (arc (std::min (fromAngle, angle), std::max (fromAngle, angle)), stroke);
    }

    // Knob body and pointer
    const auto bodyRadius = arcRadius - trackWidth * 1.5f;
    g.setColour (Palette::raised);
    g.fillEllipse (juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre));

    g.setColour (slider.findColour (juce::Slider::thumbColourId));
    g.drawLine ({ centre.getPointOnCircumference (bodyRadius * 0.25f, angle),
                  centre.getPointOnCircumference (bodyRadius * 0.8f, angle) }, 2.0f);
}

void CustomLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                              bool isMouseOverButton, bool isButtonDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto colour = backgroundColour; // already the "on" colour when toggled

    if (isButtonDown)
        colour = colour.brighter (0.2f);
    else if (isMouseOverButton)
        colour = colour.brighter (0.08f);

    // Corners on a connected edge stay square, so joined buttons read as one control
    const auto left = ! button.isConnectedOnLeft(), right = ! button.isConnectedOnRight();
    juce::Path shape;
    shape.addRoundedRectangle (bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), 4.0f, 4.0f,
                               left, right, left, right);

    g.setColour (colour);
    g.fillPath (shape);
    g.setColour (button.getToggleState() ? colour : Palette::outline);
    g.strokePath (shape, juce::PathStrokeType (1.0f));
}
