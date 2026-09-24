#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/EnvelopeModel.h"

namespace srd
{
    //==============================================================================
    /**
     * Edits one EnvelopeModel envelope.
     *
     * Drag a node to move it. Drag a segment's handle to bend the curve. Double-click
     * empty space to add a node; double-click (or right-click) a node to remove it;
     * double-click a handle to straighten its segment. Cmd+Z / Cmd+Shift+Z undo and redo.
     */
    class EnvelopeEditor : public juce::Component,
                           private juce::Timer
    {
    public:
        enum ColourIds
        {
            backgroundColourId = 0x5d10100,
            gridColourId,
            gridTextColourId,
            lineColourId,
            nodeColourId,
            readoutColourId
        };

        explicit EnvelopeEditor (EnvelopeModel&);

        void setEnvelope (std::size_t env);
        std::size_t getEnvelope() const noexcept              { return envelope; }

        void setFont (juce::Font);

        /** Stretches the time axis, e.g. by a length parameter applied at playback. */
        void setTimeScale (float scale);

        /** Seconds across the plot, after the time scale. */
        float getViewSeconds() const noexcept                 { return viewSeconds; }

        /** Value text for the drag readout and the value axis. Defaults to two decimals. */
        std::function<juce::String (float displayValue)> formatValue, formatGridValue;

        /** Draws behind the grid and curve, e.g. a waveform, in the plot area. */
        std::function<void (juce::Graphics&, juce::Rectangle<float> plotArea)> paintBackground;

        //==============================================================================
        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        bool keyPressed (const juce::KeyPress&) override;

    private:
        enum class Target { none, node, handle };

        struct Hit
        {
            Target target = Target::none;
            int index = -1; // the node, or for a handle the node its segment leads into
        };

        EnvelopeModel& model;
        std::size_t envelope = 0;
        juce::Array<EnvelopeNode> nodes; // display units, for hit-testing and dragging
        EnvelopeData data;               // domain units, for drawing the curve
        juce::uint32 lastVersion = 0;

        juce::Font font { juce::FontOptions (11.0f) };
        float timeScale   = 1.0f;
        float viewSeconds = 0.5f;

        Hit hover, drag;
        float dragStartCurve = 0.0f;

        void refresh();
        void updateView();

        /** The area inside the axis labels, where the envelope is drawn. */
        juce::Rectangle<float> getPlotArea() const;

        // The value axis is linear in the model's domain (log2 Hz for a logarithmic envelope)
        float timeToX (float nodeTime) const;
        float xToTime (float x) const;
        float domainToY (float domainValue) const;
        float valueToY (float displayValue) const;
        float yToValue (float y) const;
        juce::Point<float> nodePosition (int index) const;
        juce::Point<float> handlePosition (int index) const;

        Hit findTarget (juce::Point<float>) const;
        void setHover (Hit);
        juce::String readoutText() const;

        void drawGrid (juce::Graphics&, juce::Rectangle<float> plot) const;
        void drawCurve (juce::Graphics&, juce::Rectangle<float> plot) const;
        void drawNodes (juce::Graphics&) const;

        void timerCallback() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopeEditor)
    };
}
