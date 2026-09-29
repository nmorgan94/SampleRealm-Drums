#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/EnvelopeModel.h"

namespace srd
{
    //==============================================================================
    /**
     * Edits one EnvelopeModel envelope, with any others given to setCurves drawn faded
     * behind it. Click a faded curve, or its dot in the legend, to choose it.
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
            lineColourId,   // for an envelope with no Curve colour
            nodeColourId,
            readoutColourId
        };

        struct Curve
        {
            std::size_t envelope;
            juce::String name;
            juce::Colour colour;
        };

        explicit EnvelopeEditor (EnvelopeModel&);

        /** The envelopes shown together, each in its own colour; setEnvelope picks the one being edited. */
        void setCurves (juce::Array<Curve>);

        void setEnvelope (std::size_t env);
        std::size_t getEnvelope() const noexcept              { return envelope; }

        void setFont (juce::Font);

        /** Stretches the time axis, e.g. by a length parameter applied at playback. */
        void setTimeScale (float scale);

        /** Seconds across the plot, after the time scale. */
        float getViewSeconds() const noexcept                 { return viewSeconds; }

        /** Value text for the drag readout and the value axis, per envelope. Defaults to two decimals. */
        std::function<juce::String (std::size_t env, float displayValue)> formatValue, formatGridValue;

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
        enum class Target { none, node, handle, curve, legend };

        struct Hit
        {
            Target target = Target::none;
            int index = -1; // the node; for a handle, the node its segment leads into; for a curve or legend dot, the curve

            bool editsNode() const noexcept      { return target == Target::node || target == Target::handle; }
            bool choosesCurve() const noexcept   { return target == Target::curve || target == Target::legend; }
        };

        EnvelopeModel& model;
        std::size_t envelope = 0;
        juce::Array<Curve> curves;
        juce::Array<EnvelopeData> curveData; // per curve, domain units
        juce::Array<EnvelopeNode> nodes; // display units, for hit-testing and dragging
        EnvelopeData data;               // domain units, for drawing the curve
        juce::uint32 lastVersion = 0;

        juce::Font font { juce::FontOptions (11.0f) };
        float timeScale   = 1.0f;
        float viewSeconds = 0.5f;

        Hit hover, drag;
        float dragStartCurve = 0.0f;
        bool choseOnMouseDown = false; // so the double-click that follows doesn't add a node

        void refresh();
        void updateView();

        /** The area inside the axis labels, where the envelope is drawn. */
        juce::Rectangle<float> getPlotArea() const;

        // The value axis is linear in the model's domain (log2 Hz for a logarithmic envelope)
        float timeToX (float nodeTime) const;
        float xToTime (float x) const;
        float domainToY (float domainValue, std::size_t env) const;
        float valueToY (float displayValue) const;
        float yToValue (float y) const;
        juce::Point<float> nodePosition (int index) const;
        juce::Point<float> handlePosition (int index) const;

        juce::Colour colourOf (std::size_t env) const;
        bool hasLegend() const noexcept                        { return curves.size() > 1; }
        juce::Rectangle<float> getLegendBounds (int curve) const;
        juce::Path curvePath (const EnvelopeData&, std::size_t env, juce::Rectangle<float> plot) const;

        Hit findTarget (juce::Point<float>) const;
        Hit findFadedCurve (juce::Point<float>) const;
        void setHover (Hit);
        juce::String readoutText() const;

        void drawGrid (juce::Graphics&, juce::Rectangle<float> plot) const;
        void drawFadedCurves (juce::Graphics&, juce::Rectangle<float> plot) const;
        void drawCurve (juce::Graphics&, juce::Rectangle<float> plot) const;
        void drawNodes (juce::Graphics&) const;
        void drawLegend (juce::Graphics&) const;

        void timerCallback() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopeEditor)
    };
}
