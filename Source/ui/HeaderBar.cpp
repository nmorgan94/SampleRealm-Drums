#include "HeaderBar.h"
#include "CustomLookAndFeel.h"

namespace srd
{
    namespace
    {
        constexpr int smallLineHeight = 16;
        constexpr float smallFontHeight = 11.0f;
    }

    HeaderBar::HeaderBar (const Title& titleText, const Layout& l, juce::Component& switchToShow, PresetManager& presets,
                          PromptOverlay& o, std::function<void()> m, const juce::String& about, juce::Component& meterToShow)
        : title (titleText), layout (l), prompt (o), showManual (std::move (m)), aboutText (about),
          switchControl (switchToShow), presetActions (presets, prompt), presetBar (presets, presetActions), meter (meterToShow)
    {
        // For screen readers, as the title is painted
        setTitle (title.small + " " + title.large);

        setOpaque (true);
        menuButton.onClick = [this] { showMenu(); };

        addAndMakeVisible (switchControl);
        addAndMakeVisible (presetBar);
        addAndMakeVisible (menuButton);
        addAndMakeVisible (meter);
    }

    void HeaderBar::paint (juce::Graphics& g)
    {
        g.fillAll (findColour (backgroundColourId));

        // The meter's repaints reach here too
        auto titleArea = getLocalBounds().withTrimmedLeft (layout.edge).withWidth (layout.titleWidth).reduced (0, layout.meterInset);
        if (! g.clipRegionIntersects (titleArea))
            return;

        // Painted, so the colours come from the LookAndFeel
        g.setColour (findColour (subtitleColourId));
        g.setFont (CustomLookAndFeel::font (smallFontHeight, true));
        g.drawText (title.small, titleArea.removeFromTop (smallLineHeight), juce::Justification::bottomLeft);
        g.setColour (findColour (titleColourId));
        g.setFont (CustomLookAndFeel::font (layout.titleHeight, true));
        g.drawText (title.large, titleArea, juce::Justification::centredLeft);
    }

    void HeaderBar::resized()
    {
        auto bounds = getLocalBounds().reduced (layout.edge, 0);
        meter.setBounds (bounds.removeFromRight (layout.meterWidth).reduced (0, layout.meterInset));

        bounds.removeFromLeft (layout.titleWidth);
        switchControl.setBounds (bounds.removeFromLeft (layout.switchWidth)
                                     .withSizeKeepingCentre (layout.switchWidth, layout.controlsHeight));

        auto controls = bounds.reduced (layout.gap, 0);
        controls = controls.withSizeKeepingCentre (controls.getWidth(), layout.controlsHeight);
        menuButton.setBounds (controls.removeFromRight (layout.menuWidth));
        controls.removeFromRight (layout.menuGap);
        presetBar.setBounds (controls);
    }

    void HeaderBar::showMenu()
    {
        juce::PopupMenu menu;
        menu.setLookAndFeel (&getLookAndFeel());
        presetActions.addMenuItems (menu);
        menu.addSeparator();
        menu.addItem ("Manual", showManual);
        menu.addItem ("About", [this] { prompt.showMessage ("About", aboutText); });

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (menuButton));
    }
}
