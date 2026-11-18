#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/LoopSteps.h"

namespace srd
{
    //==============================================================================
    /**
     * One bar of the audition loop as a row of bars in beats of four. A bar fills to its
     * step's velocity: a click switches the step, a drag up or down sets how hard it hits,
     * and the step playing is outlined.
     */
    class StepRow : public juce::Component
    {
    public:
        enum ColourIds
        {
            offColourId = 0x5d10800,
            onColourId,
            playingColourId
        };

        // As STEP_ROW in webui/src/StepRow.tsx
        static constexpr int stepWidth = 12, stepHeight = 20, stepGap = 2, beatGap = 8;

        StepRow()
        {
            setTitle ("Loop steps");
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        /** velocity is as LoopSteps::setStep takes. */
        std::function<void (int step, int velocity)> onChange;

        /** The velocity being dragged, or 0 once the drag ends. */
        std::function<void (int velocity)> onDrag;

        void setSteps (const Patterns::Part& newSteps)
        {
            if (steps != newSteps)
            {
                steps = newSteps;
                repaint();
            }
        }

        /** -1 for none. */
        void setPlayingStep (int step)
        {
            if (playing != step)
            {
                playing = step;
                repaint();
            }
        }

        static juce::Rectangle<int> stepBounds (int step) noexcept
        {
            return { step * (stepWidth + stepGap) + (step / Patterns::stepsPerBeat) * (beatGap - stepGap), 0, stepWidth, stepHeight };
        }

        /** What a drag of distanceUp pixels makes of a step: a rest is dragged up from nothing. */
        static int draggedVelocity (int from, int distanceUp) noexcept
        {
            return juce::jlimit (1, LoopSteps::onVelocity, std::max (from, 0) + distanceUp * velocityPerPixel);
        }

        void mouseDown (const juce::MouseEvent& event) override
        {
            pressed = -1;
            dragging = false;

            for (int step = 0; step < Patterns::stepsPerBar; ++step)
                if (stepBounds (step).contains (event.getPosition()))
                    pressed = step;

            if (pressed >= 0)
                pressedVelocity = steps[static_cast<std::size_t> (pressed)];
        }

        void mouseDrag (const juce::MouseEvent& event) override
        {
            if (pressed < 0 || (! dragging && std::abs (event.getDistanceFromDragStartY()) < dragThreshold))
                return;

            dragging = true;
            setStep (draggedVelocity (pressedVelocity, -event.getDistanceFromDragStartY()));

            if (onDrag != nullptr)
                onDrag (steps[static_cast<std::size_t> (pressed)]);
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (pressed < 0)
                return;

            if (! dragging)
                setStep (LoopSteps::toggled (pressedVelocity));
            else if (onDrag != nullptr)
                onDrag (0);

            pressed = -1;
        }

        void paint (juce::Graphics& g) override
        {
            for (int step = 0; step < Patterns::stepsPerBar; ++step)
            {
                const auto bounds = stepBounds (step).toFloat();
                const auto velocity = steps[static_cast<std::size_t> (step)];

                g.setColour (findColour (offColourId));
                g.fillRoundedRectangle (bounds, cornerSize);

                if (velocity > 0)
                {
                    const auto filled = std::max (minFill, bounds.getHeight() * (float) velocity / (float) LoopSteps::onVelocity);

                    g.setColour (findColour (onColourId));
                    g.fillRoundedRectangle (bounds.withTop (bounds.getBottom() - filled), cornerSize);
                }

                if (step == playing)
                {
                    g.setColour (findColour (playingColourId));
                    g.drawRoundedRectangle (bounds.reduced (0.5f), cornerSize, 1.0f);
                }
            }
        }

    private:
        Patterns::Part steps {};
        int playing = -1;

        // The step under the mouse since it went down, and what it was then
        int pressed = -1, pressedVelocity = 0;
        bool dragging = false;

        static constexpr int dragThreshold = 3, velocityPerPixel = 2;

        // The softest step still shows
        static constexpr float minFill = 2.0f, cornerSize = 2.0f;

        void setStep (int velocity)
        {
            steps[static_cast<std::size_t> (pressed)] = velocity;
            repaint();

            if (onChange != nullptr)
                onChange (pressed, velocity);
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepRow)
    };
}
