// Mirrors Source/ui/StepRow.h
import { useEffect, useRef, useState } from 'react';
import type { Rect } from '@samplerealm/modules/editor/layout.ts';
import { STEPS_PER_BEAT, STEP_ON_VELOCITY, toggledStep, type DrumsBackend } from './backend.ts';
import { clamp } from './envelope.ts';
import { Text } from './Text.tsx';

// StepRow::stepWidth, stepHeight, stepGap, beatGap, cornerSize and minFill
const STEP_ROW = { width: 12, height: 20, gap: 2, beatGap: 8, corner: 2, minFill: 2 };

// StepRow::dragThreshold and velocityPerPixel
const DRAG_THRESHOLD = 3;
const VELOCITY_PER_PIXEL = 2;

// StepRow::stepBounds
const stepLeft = (step: number) =>
  step * (STEP_ROW.width + STEP_ROW.gap) + Math.trunc(step / STEPS_PER_BEAT) * (STEP_ROW.beatGap - STEP_ROW.gap);

// StepRow::draggedVelocity
const draggedVelocity = (from: number, distanceUp: number) =>
  clamp(Math.max(from, 0) + distanceUp * VELOCITY_PER_PIXEL, 1, STEP_ON_VELOCITY);

interface StepRowProps {
  backend: DrumsBackend;
  box: Rect;
  // Where the velocity being dragged shows
  readout: Rect;
}

// One bar of the audition loop. A bar fills to its step's velocity: a click switches the step,
// a drag up or down sets how hard it hits, and the step playing is outlined
export function StepRow({ backend, box, readout }: StepRowProps) {
  const [{ steps, playing }, setLoopSteps] = useState(backend.getLoopSteps);
  const [dragged, setDragged] = useState(0);
  // velocity is the step's when pressed, sent the last one dragged to
  const press = useRef<{ step: number; velocity: number; y: number; dragging: boolean; sent: number } | null>(null);

  useEffect(() => backend.subscribeLoopSteps(setLoopSteps), [backend]);

  const release = () => {
    press.current = null;
    setDragged(0);
  };

  return (
    <>
      {dragged > 0 && (
        <Text style={readout} size={11} bold colour="dim" right>
          {dragged}
        </Text>
      )}
      <div className="absolute" style={box} role="group" aria-label="Loop steps">
        {steps.map((velocity, step) => (
          <button
            key={step}
            type="button"
            aria-label={`Step ${step + 1}`}
            aria-pressed={velocity > 0}
            className="absolute cursor-pointer touch-none overflow-hidden bg-widget"
            style={{
              left: stepLeft(step),
              top: 0,
              width: STEP_ROW.width,
              height: STEP_ROW.height,
              borderRadius: STEP_ROW.corner,
            }}
            onPointerDown={(e) => {
              if (e.button !== 0) return;
              e.currentTarget.setPointerCapture(e.pointerId);
              press.current = { step, velocity, y: e.clientY, dragging: false, sent: velocity };
            }}
            onPointerMove={(e) => {
              const pressed = press.current;
              if (pressed?.step !== step) return;

              // The editor can be scaled, so the drag is measured against the step as drawn
              const scale = e.currentTarget.getBoundingClientRect().height / STEP_ROW.height;
              const distanceUp = Math.round((pressed.y - e.clientY) / scale);
              if (!pressed.dragging && Math.abs(distanceUp) < DRAG_THRESHOLD) return;

              const next = draggedVelocity(pressed.velocity, distanceUp);
              if (pressed.dragging && next === pressed.sent) return;

              pressed.dragging = true;
              pressed.sent = next;
              backend.setLoopStep(step, next);
              setDragged(next);
            }}
            onPointerUp={() => {
              const pressed = press.current;
              if (pressed && !pressed.dragging) backend.setLoopStep(step, toggledStep(pressed.velocity));
              release();
            }}
            onPointerCancel={release}
            // From the keyboard
            onClick={(e) => e.detail === 0 && backend.setLoopStep(step, toggledStep(velocity))}
          >
            {velocity > 0 && (
              <span
                className="pointer-events-none absolute inset-x-0 bottom-0 bg-accent"
                style={{
                  height: Math.max(STEP_ROW.minFill, (STEP_ROW.height * velocity) / STEP_ON_VELOCITY),
                  borderRadius: STEP_ROW.corner,
                }}
              />
            )}
            {step === playing && (
              <span className="pointer-events-none absolute inset-0" style={{ borderRadius: STEP_ROW.corner, boxShadow: 'inset 0 0 0 1px var(--color-fg)' }} />
            )}
          </button>
        ))}
      </div>
    </>
  );
}
