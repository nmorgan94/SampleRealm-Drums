#include "EnvelopeEditor.h"

namespace srd
{
    namespace
    {
        constexpr float nodeRadius   = 4.5f;
        constexpr float handleRadius = 3.0f;
        constexpr float hitRadius    = 8.0f;
        constexpr float curvePerPixel = 1.0f / 120.0f;

        juce::String formatTime (float seconds)
        {
            const auto ms = seconds * 1000.0f;

            if (ms <= 0.0f)     return "0";
            if (ms >= 1000.0f)  return juce::String (seconds, 2) + " s";
            if (ms >= 10.0f)    return juce::String (juce::roundToInt (ms)) + " ms";
            return juce::String (ms, 1) + " ms";
        }

        /** The first step that gives no more than maxLines lines across the span. */
        float chooseGridStep (float span, float maxLines)
        {
            for (auto step : { 0.001f, 0.002f, 0.005f, 0.01f, 0.02f, 0.025f, 0.05f, 0.1f, 0.2f, 0.25f, 0.5f, 1.0f })
                if (span / step <= maxLines)
                    return step;

            return 2.0f;
        }
    }

    //==============================================================================
    EnvelopeEditor::EnvelopeEditor (EnvelopeModel& m) : model (m)
    {
        formatValue = formatGridValue = [] (float v) { return juce::String (v, 2); };

        setWantsKeyboardFocus (true);
        refresh();
        startTimerHz (30);
    }

    void EnvelopeEditor::setEnvelope (std::size_t env)
    {
        jassert (env < model.getNumEnvelopes());
        envelope = env;
        hover = drag = {};
        refresh();
    }

    void EnvelopeEditor::setFont (juce::Font newFont)
    {
        font = std::move (newFont);
        repaint();
    }

    void EnvelopeEditor::setTimeScale (float scale)
    {
        if (juce::approximatelyEqual (scale, timeScale))
            return;

        timeScale = std::max (scale, 0.01f);

        if (drag.target == Target::none)
            updateView();

        repaint();
    }

    //==============================================================================
    void EnvelopeEditor::refresh()
    {
        lastVersion = model.getVersion();
        data = model.getData (envelope);

        nodes.clearQuick();

        for (int i = 0; i < model.getNumNodes (envelope); ++i)
            nodes.add (model.getNode (envelope, i));

        if (hover.index >= nodes.size()) hover = {};
        if (drag.index >= nodes.size())  drag = {};

        if (drag.target == Target::none)
            updateView();

        repaint();
    }

    void EnvelopeEditor::updateView()
    {
        const auto end = nodes.isEmpty() ? 0.0f : nodes.getLast().time * timeScale * 1.15f;

        for (auto seconds : { 0.05f, 0.1f, 0.15f, 0.2f, 0.3f, 0.4f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f })
        {
            if (seconds >= end)
            {
                viewSeconds = seconds;
                return;
            }
        }

        viewSeconds = 8.0f;
    }

    void EnvelopeEditor::timerCallback()
    {
        if (model.getVersion() != lastVersion)
            refresh();
    }

    //==============================================================================
    juce::Rectangle<float> EnvelopeEditor::getPlotArea() const
    {
        return getLocalBounds().toFloat().withTrimmedLeft (48.0f).withTrimmedRight (12.0f)
                                         .withTrimmedTop (10.0f).withTrimmedBottom (20.0f);
    }

    float EnvelopeEditor::timeToX (float nodeTime) const
    {
        const auto plot = getPlotArea();
        return plot.getX() + nodeTime * timeScale / viewSeconds * plot.getWidth();
    }

    float EnvelopeEditor::xToTime (float x) const
    {
        const auto plot = getPlotArea();
        return (x - plot.getX()) / plot.getWidth() * viewSeconds / timeScale;
    }

    float EnvelopeEditor::domainToY (float domainValue) const
    {
        const auto& spec = model.getSpec (envelope);
        const auto plot = getPlotArea();
        return juce::jmap (domainValue, model.toDomain (envelope, spec.minValue), model.toDomain (envelope, spec.maxValue),
                           plot.getBottom(), plot.getY());
    }

    float EnvelopeEditor::valueToY (float displayValue) const
    {
        return domainToY (model.toDomain (envelope, displayValue));
    }

    float EnvelopeEditor::yToValue (float y) const
    {
        const auto& spec = model.getSpec (envelope);
        const auto plot = getPlotArea();
        const auto domainValue = juce::jmap (y, plot.getBottom(), plot.getY(),
                                             model.toDomain (envelope, spec.minValue), model.toDomain (envelope, spec.maxValue));

        return model.clampValue (envelope, model.toDisplay (envelope, domainValue));
    }

    juce::Point<float> EnvelopeEditor::nodePosition (int index) const
    {
        const auto& node = nodes.getReference (index);
        return { timeToX (node.time), valueToY (node.value) };
    }

    juce::Point<float> EnvelopeEditor::handlePosition (int index) const
    {
        const auto midTime = 0.5f * (nodes.getReference (index - 1).time + nodes.getReference (index).time);
        return { timeToX (midTime), domainToY (evaluate (data, midTime)) };
    }

    //==============================================================================
    EnvelopeEditor::Hit EnvelopeEditor::findTarget (juce::Point<float> position) const
    {
        Hit best;
        auto bestDistance = hitRadius;

        for (int i = 0; i < nodes.size(); ++i)
        {
            if (const auto d = nodePosition (i).getDistanceFrom (position); d < bestDistance)
            {
                best = { Target::node, i };
                bestDistance = d;
            }
        }

        if (best.target != Target::none)
            return best;

        for (int i = 1; i < nodes.size(); ++i)
        {
            if (const auto d = handlePosition (i).getDistanceFrom (position); d < bestDistance)
            {
                best = { Target::handle, i };
                bestDistance = d;
            }
        }

        return best;
    }

    void EnvelopeEditor::setHover (Hit newHover)
    {
        if (newHover.target == hover.target && newHover.index == hover.index)
            return;

        hover = newHover;
        setMouseCursor (hover.target == Target::node   ? juce::MouseCursor::DraggingHandCursor
                      : hover.target == Target::handle ? juce::MouseCursor::UpDownResizeCursor
                                                       : juce::MouseCursor::NormalCursor);
        repaint();
    }

    juce::String EnvelopeEditor::readoutText() const
    {
        const auto& target = drag.target != Target::none ? drag : hover;

        if (target.target == Target::none)
            return {};

        const auto& node = nodes.getReference (target.index);

        if (target.target == Target::handle)
            return "Curve " + juce::String (node.curve, 2);

        return formatTime (node.time * timeScale) + "   " + formatValue (node.value);
    }

    //==============================================================================
    void EnvelopeEditor::mouseMove (const juce::MouseEvent& e)
    {
        setHover (findTarget (e.position));
    }

    void EnvelopeEditor::mouseExit (const juce::MouseEvent&)
    {
        setHover ({});
    }

    void EnvelopeEditor::mouseDown (const juce::MouseEvent& e)
    {
        grabKeyboardFocus();

        const auto hit = findTarget (e.position);

        if (hit.target == Target::none)
            return;

        model.getUndoManager().beginNewTransaction();

        if (e.mods.isPopupMenu())
        {
            if (hit.target == Target::node && model.removeNode (envelope, hit.index))
                refresh();

            return;
        }

        drag = hit;
        dragStartCurve = nodes.getReference (hit.index).curve;
        repaint();
    }

    void EnvelopeEditor::mouseDrag (const juce::MouseEvent& e)
    {
        if (drag.target == Target::none)
            return;

        auto node = nodes.getReference (drag.index);

        if (drag.target == Target::node)
        {
            node.time  = xToTime (e.position.x);
            node.value = yToValue (e.position.y);
        }
        else
        {
            // Dragging the handle towards where the segment ends pushes the curve that way
            const auto direction = nodePosition (drag.index).y > nodePosition (drag.index - 1).y ? 1.0f : -1.0f;
            node.curve = dragStartCurve + (e.position.y - e.mouseDownPosition.y) * direction * curvePerPixel;
        }

        model.setNode (envelope, drag.index, node);
        refresh();
    }

    void EnvelopeEditor::mouseUp (const juce::MouseEvent& e)
    {
        drag = {};
        updateView();
        setHover (findTarget (e.position));
        repaint();
    }

    void EnvelopeEditor::mouseDoubleClick (const juce::MouseEvent& e)
    {
        const auto hit = findTarget (e.position);
        model.getUndoManager().beginNewTransaction();

        if (hit.target == Target::node)
        {
            model.removeNode (envelope, hit.index);
        }
        else if (hit.target == Target::handle)
        {
            auto node = nodes.getReference (hit.index);
            node.curve = 0.0f;
            model.setNode (envelope, hit.index, node);
        }
        else if (getPlotArea().contains (e.position))
        {
            model.insertNode (envelope, { xToTime (e.position.x), yToValue (e.position.y), 0.0f });
        }

        refresh();
        setHover (findTarget (e.position));
    }

    bool EnvelopeEditor::keyPressed (const juce::KeyPress& key)
    {
        auto& undoManager = model.getUndoManager();

        if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0))
            return undoManager.undo();

        if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
            return undoManager.redo();

        return false;
    }

    //==============================================================================
    void EnvelopeEditor::paint (juce::Graphics& g)
    {
        const auto plot = getPlotArea();

        g.setColour (findColour (backgroundColourId));
        g.fillRoundedRectangle (plot, 4.0f);

        if (paintBackground != nullptr)
        {
            const juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (plot.toNearestInt());
            paintBackground (g, plot);
        }

        drawGrid (g, plot);

        if (nodes.size() < 2)
            return;

        drawCurve (g, plot);
        drawNodes (g);

        if (const auto text = readoutText(); text.isNotEmpty())
        {
            g.setColour (findColour (readoutColourId));
            g.setFont (font);
            g.drawText (text, plot.reduced (8.0f, 6.0f), juce::Justification::topRight);
        }
    }

    void EnvelopeEditor::drawGrid (juce::Graphics& g, juce::Rectangle<float> plot) const
    {
        const auto& spec = model.getSpec (envelope);
        const auto gridColour = findColour (gridColourId);
        const auto textColour = findColour (gridTextColourId);
        g.setFont (font);

        // Time, in seconds after the time scale
        const auto step = chooseGridStep (viewSeconds, plot.getWidth() / 70.0f);

        // Labels are kept inside the component, so the last one isn't cut off
        const auto right = getLocalBounds().toFloat().getRight();

        for (auto seconds = 0.0f; seconds <= viewSeconds + 1.0e-4f; seconds += step)
        {
            const auto x = plot.getX() + seconds / viewSeconds * plot.getWidth();

            g.setColour (gridColour);
            g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
            g.setColour (textColour);
            const auto label = juce::Rectangle<float> (60.0f, 14.0f).withCentre ({ x, plot.getBottom() + 10.0f });
            g.drawText (formatTime (seconds), label.withX (std::min (label.getX(), right - label.getWidth())),
                        label.getRight() > right ? juce::Justification::centredRight : juce::Justification::centred);
        }

        juce::Array<float> values;

        if (spec.scale == EnvelopeSpec::Scale::logarithmic)
        {
            for (auto decade = std::pow (10.0f, std::floor (std::log10 (spec.minValue))); decade <= spec.maxValue; decade *= 10.0f)
                for (auto multiple : { 1.0f, 2.0f, 5.0f })
                    if (const auto v = decade * multiple; v >= spec.minValue && v <= spec.maxValue)
                        values.add (v);
        }
        else
        {
            for (auto proportion : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
                values.add (spec.minValue + (spec.maxValue - spec.minValue) * proportion);
        }

        for (auto v : values)
        {
            const auto y = valueToY (v);

            g.setColour (gridColour);
            g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
            g.setColour (textColour);
            g.drawText (formatGridValue (v), juce::Rectangle<float> (0.0f, y - 7.0f, plot.getX() - 6.0f, 14.0f),
                        juce::Justification::centredRight);
        }
    }

    void EnvelopeEditor::drawCurve (juce::Graphics& g, juce::Rectangle<float> plot) const
    {
        const juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (plot.toNearestInt());

        const auto colour = findColour (lineColourId);
        const auto endX = std::min (timeToX (nodes.getLast().time), plot.getRight());

        // One point per pixel up to the last node; the tail holds the final value
        juce::Path curve;
        EnvelopeCursor cursor;

        curve.startNewSubPath (nodePosition (0));

        for (auto x = plot.getX() + 1.0f; x <= endX; x += 1.0f)
            curve.lineTo (x, domainToY (cursor.getValue (data, xToTime (x))));

        const auto end = nodePosition (nodes.size() - 1);
        curve.lineTo (std::min (end.x, plot.getRight()), end.y);

        auto fill = curve;
        fill.lineTo (curve.getCurrentPosition().x, plot.getBottom());
        fill.lineTo (plot.getX(), plot.getBottom());
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.22f), 0.0f, plot.getY(),
                                                 colour.withAlpha (0.02f), 0.0f, plot.getBottom(), false));
        g.fillPath (fill);

        g.setColour (colour);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (end.x < plot.getRight())
        {
            g.setColour (colour.withAlpha (0.35f));
            g.drawLine (end.x, end.y, plot.getRight(), end.y, 1.5f);
        }
    }

    void EnvelopeEditor::drawNodes (juce::Graphics& g) const
    {
        const auto lineColour = findColour (lineColourId);
        const auto nodeColour = findColour (nodeColourId);

        const auto isActive = [this] (Target target, int index)
        {
            return (drag.target == target && drag.index == index)
                || (drag.target == Target::none && hover.target == target && hover.index == index);
        };

        for (int i = 1; i < nodes.size(); ++i)
        {
            const auto active = isActive (Target::handle, i);
            const auto radius = active ? handleRadius + 1.5f : handleRadius;
            g.setColour (lineColour.withAlpha (active ? 1.0f : 0.5f));
            g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (handlePosition (i)));
        }

        for (int i = 0; i < nodes.size(); ++i)
        {
            const auto active = isActive (Target::node, i);
            const auto bounds = juce::Rectangle<float> (nodeRadius * 2.0f, nodeRadius * 2.0f)
                                    .withCentre (nodePosition (i))
                                    .expanded (active ? 1.5f : 0.0f);

            g.setColour (active ? lineColour : nodeColour);
            g.fillEllipse (bounds);
            g.setColour (lineColour);
            g.drawEllipse (bounds, 1.5f);
        }
    }
}
