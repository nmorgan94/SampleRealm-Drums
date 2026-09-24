#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace srd
{
    //==============================================================================
    /** A titled section that lays its controls out in a grid.*/
    class Panel : public juce::Component
    {
    public:
        enum ColourIds
        {
            backgroundColourId = 0x5d10000,
            outlineColourId,
            titleColourId
        };

        explicit Panel (juce::String panelTitle) : title (std::move (panelTitle)) {}

        void setFont (juce::Font newFont)   { font = std::move (newFont); repaint(); }
        void setColumns (int numColumns)    { columns = numColumns; resized(); }

        /** Sits at the right of the title. */
        void setHeaderComponent (juce::Component& component, int width)
        {
            header = &component;
            headerWidth = width;
            addAndMakeVisible (component);
            resized();
        }

        void addControl (juce::Component& control)
        {
            controls.add (&control);
            addAndMakeVisible (control);
            resized();
        }

        int getNumControls() const noexcept     { return controls.size(); }

        void paint (juce::Graphics& g) override
        {
            const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (findColour (backgroundColourId));
            g.fillRoundedRectangle (bounds, cornerSize);
            g.setColour (findColour (outlineColourId));
            g.drawRoundedRectangle (bounds, cornerSize, 1.0f);

            g.setColour (findColour (titleColourId));
            g.setFont (font);
            g.drawText (title.toUpperCase(), getLocalBounds().removeFromTop (headerHeight).reduced (10, 0),
                        juce::Justification::centredLeft);
        }

        void resized() override
        {
            if (header != nullptr)
                header->setBounds (getLocalBounds().removeFromTop (headerHeight).reduced (6, 3).removeFromRight (headerWidth));

            if (controls.isEmpty())
                return;

            const auto numControls = controls.size();
            const auto numColumns  = columns > 0 ? columns : numControls;
            const auto numRows     = (numControls + numColumns - 1) / numColumns;

            auto area = getLocalBounds().reduced (6, 0).withTrimmedTop (headerHeight).withTrimmedBottom (6);
            const auto cellWidth  = area.getWidth() / numColumns;
            const auto cellHeight = area.getHeight() / numRows;

            for (int i = 0; i < numControls; ++i)
                controls[i]->setBounds (area.getX() + (i % numColumns) * cellWidth,
                                        area.getY() + (i / numColumns) * cellHeight,
                                        cellWidth, cellHeight);
        }

    private:
        static constexpr int headerHeight = 26;
        static constexpr float cornerSize = 6.0f;

        juce::String title;
        juce::Font font { juce::FontOptions (12.0f, juce::Font::bold) };
        juce::Array<juce::Component*> controls;
        juce::Component* header = nullptr;
        int headerWidth = 0, columns = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Panel)
    };
}
