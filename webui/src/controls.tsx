// Mirrors Source/ui/LabeledSlider.h, Controls.h and SegmentedButtons.h as CustomLookAndFeel draws them
import { forwardRef, useEffect, useRef, useState, type CSSProperties, type PointerEvent as ReactPointerEvent, type ReactNode } from 'react';
import type { DrumsBackend } from './backend.ts';
import { juceHeight } from './fonts.ts';
import { useParam } from './hooks.ts';
import { centred, layOutAboveLabel, type Box } from './layout.ts';
import { useMenu } from './Menu.tsx';
import { formatValue, fromProportion, parseValue, toProportion } from './params.ts';

// juce::Slider's defaults
const ROTARY_START = 1.2 * Math.PI;
const ROTARY_END = 2.8 * Math.PI;
const PIXELS_FOR_FULL_DRAG = 250;

// Source/ui/LabeledSlider.h
const NAME_DELAY_MS = 600;

// Angles run clockwise from 12 o'clock, as in juce::Path::addCentredArc
const pointAt = (cx: number, cy: number, radius: number, angle: number) =>
  [cx + radius * Math.sin(angle), cy - radius * Math.cos(angle)] as const;

const arcPath = (cx: number, cy: number, radius: number, from: number, to: number) => {
  const [x1, y1] = pointAt(cx, cy, radius, from);
  const [x2, y2] = pointAt(cx, cy, radius, to);
  return `M ${x1} ${y1} A ${radius} ${radius} 0 ${to - from > Math.PI ? 1 : 0} 1 ${x2} ${y2}`;
};

// CustomLookAndFeel::drawRotarySlider
function RotaryGraphic({ size, proportion, bipolar, enabled }: { size: number; proportion: number; bipolar: boolean; enabled: boolean }) {
  const radius = Math.max(0, size - 8) / 2;
  const centre = size / 2;
  const trackWidth = Math.max(2.5, radius * 0.12);
  const arcRadius = radius - trackWidth * 0.5;
  const angle = ROTARY_START + proportion * (ROTARY_END - ROTARY_START);
  const fromAngle = bipolar ? 0.5 * (ROTARY_START + ROTARY_END) : ROTARY_START;
  const bodyRadius = arcRadius - trackWidth * 1.5;
  const [x1, y1] = pointAt(centre, centre, bodyRadius * 0.25, angle);
  const [x2, y2] = pointAt(centre, centre, bodyRadius * 0.8, angle);
  const stroke = { strokeWidth: trackWidth, strokeLinecap: 'round', fill: 'none' } as const;

  if (radius <= 0) return null;

  return (
    <svg width={size} height={size} aria-hidden>
      <path d={arcPath(centre, centre, arcRadius, ROTARY_START, ROTARY_END)} stroke="var(--color-line)" {...stroke} />
      {enabled && Math.abs(angle - fromAngle) > 1e-3 && (
        <path d={arcPath(centre, centre, arcRadius, Math.min(fromAngle, angle), Math.max(fromAngle, angle))} stroke="var(--color-accent)" {...stroke} />
      )}
      {bodyRadius > 0 && <circle cx={centre} cy={centre} r={bodyRadius} fill="var(--color-widget)" />}
      <line x1={x1} y1={y1} x2={x2} y2={y2} stroke="var(--color-fg)" strokeWidth={2} />
    </svg>
  );
}

// LabeledSlider::styleLabel, as the props of the label's element
const styleLabel =(box: Box) => ({
  className: 'absolute flex items-center justify-center text-center font-bold leading-none text-dim whitespace-nowrap',
  style: { ...box, fontSize: juceHeight(11) },
});

interface LabeledProps {
  box: Box;
  enabled: boolean;
  children(layout: { control: Box; label: ReturnType<typeof styleLabel> }): ReactNode;
}

// A control above its label, as LabeledSlider lays them out, greyed out and inert when disabled
function Labeled({ box, enabled, children }: LabeledProps) {
  const layout = layOutAboveLabel(box.width, box.height);

  return (
    <div className="absolute" style={{ ...box, opacity: enabled ? 1 : 0.35, pointerEvents: enabled ? undefined : 'none' }}>
      {children({ control: layout.control, label: styleLabel(layout.label) })}
    </div>
  );
}

type DragStart = { x: number; y: number; proportion: number };

interface ControlProps {
  backend: DrumsBackend;
  id: string;
  // The parameter's name if not given
  label?: string;
  box: Box;
  enabled?: boolean;
}

// A rotary slider with its name below, which shows the value while it's being moved. Click the name to type a value.
export function Knob({ backend, id, label, box, enabled = true }: ControlProps) {
  const { value, set, commit, param } = useParam(backend, id);
  const drag = useRef<DragStart | null>(null);
  const nameTimer = useRef<number>(undefined);
  const [showValue, setShowValue] = useState(false);
  const [editing, setEditing] = useState(false);
  const proportion = toProportion(param, value);
  const text = formatValue(param, value);
  const name = label ?? param.name;

  useEffect(() => () => clearTimeout(nameTimer.current), []);

  const holdValue = () => {
    clearTimeout(nameTimer.current);
    setShowValue(true);
  };

  const releaseValue = () => {
    clearTimeout(nameTimer.current);
    nameTimer.current = window.setTimeout(() => setShowValue(false), NAME_DELAY_MS);
  };

  const flashValue = () => {
    setShowValue(true);
    releaseValue();
  };

  const end = () => {
    if (!drag.current) return;
    drag.current = null;
    backend.endGesture(id);
    releaseValue();
  };

  const commitText = (typed: string) => {
    setEditing(false);
    const parsed = parseValue(param, typed);
    if (parsed !== null) commit(parsed);
    flashValue();
  };

  return (
    <Labeled box={box} enabled={enabled}>
      {({ control, label: nameLabel }) => (
        <>
          <div
            className="absolute cursor-pointer touch-none"
            style={control}
            role="slider"
            aria-label={name}
            aria-valuemin={param.min}
            aria-valuemax={param.max}
            aria-valuenow={value}
            aria-valuetext={text}
            onPointerDown={(e: ReactPointerEvent) => {
              if (e.button !== 0) return;
              e.currentTarget.setPointerCapture(e.pointerId);
              drag.current = { x: e.clientX, y: e.clientY, proportion };
              backend.beginGesture(id);
              holdValue();
            }}
            onPointerMove={(e: ReactPointerEvent) => {
              const start = drag.current;
              if (!start) return;
              const moved = e.clientX - start.x - (e.clientY - start.y);
              const next = fromProportion(param, start.proportion + moved / PIXELS_FOR_FULL_DRAG);
              if (next !== value) set(next);
            }}
            onPointerUp={end}
            onPointerCancel={end}
            // As SliderAttachment's setDoubleClickReturnValue
            onDoubleClick={() => {
              commit(param.defaultValue);
              flashValue();
            }}
          >
            <RotaryGraphic size={control.width} proportion={proportion} bipolar={param.min < 0 && param.min === -param.max} enabled={enabled} />
          </div>
          {editing ? (
            <input
              className={`${nameLabel.className} border border-line bg-background text-fg outline-none`}
              style={nameLabel.style}
              autoFocus
              defaultValue={text}
              onFocus={(e) => e.currentTarget.select()}
              onBlur={(e) => commitText(e.currentTarget.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter') e.currentTarget.blur();
                if (e.key === 'Escape') {
                  setEditing(false);
                  releaseValue();
                }
              }}
            />
          ) : (
            <div
              className={`${nameLabel.className} cursor-text`}
              style={nameLabel.style}
              onClick={() => {
                clearTimeout(nameTimer.current);
                setEditing(true);
              }}
            >
              {showValue ? text : name}
            </div>
          )}
        </>
      )}
    </Labeled>
  );
}

//==============================================================================
interface TextButtonProps {
  text: string;
  style: CSSProperties;
  onClick(): void;
  toggled?: boolean;
  // Square corners where it joins a neighbour, as Button::setConnectedEdges
  joined?: { left?: boolean; right?: boolean };
  label?: string;
}

// CustomLookAndFeel::drawButtonBackground: brighter on hover and while down, the accent when on
export const TextButton = forwardRef<HTMLButtonElement, TextButtonProps>(function TextButton({ text, style, onClick, toggled = false, joined = {}, label }, ref) {
  const radius = (square?: boolean) => (square ? 0 : 4);

  return (
    <button
      ref={ref}
      type="button"
      aria-label={label}
      aria-pressed={toggled || undefined}
      className={`absolute cursor-pointer overflow-hidden border px-1 text-ellipsis whitespace-nowrap ${
        toggled
          ? 'border-accent bg-accent text-background hover:bg-[color-mix(in_srgb,var(--color-accent),white_8%)]'
          : 'border-line bg-widget text-fg hover:bg-[color-mix(in_srgb,var(--color-widget),white_8%)] active:bg-[color-mix(in_srgb,var(--color-widget),white_20%)]'
      }`}
      style={{
        fontSize: juceHeight(12),
        borderTopLeftRadius: radius(joined.left),
        borderBottomLeftRadius: radius(joined.left),
        borderTopRightRadius: radius(joined.right),
        borderBottomRightRadius: radius(joined.right),
        ...style,
      }}
      onClick={onClick}
    >
      {text}
    </button>
  );
});

//==============================================================================
// A row of joined buttons, one per value of a choice parameter
export function ChoiceButtons({ backend, id, box }: { backend: DrumsBackend; id: string; box: Box }) {
  const { value, commit, param } = useParam(backend, id);
  const width = Math.trunc(box.width / param.choices.length);

  return (
    <div role="radiogroup" aria-label={param.name}>
      {param.choices.map((choice, index) => {
        const isLast = index === param.choices.length - 1;
        return (
          <TextButton
            key={choice}
            text={choice}
            toggled={Math.round(value) === index}
            joined={{ left: index > 0, right: !isLast }}
            style={{ left: box.left + index * width, top: box.top, width: isLast ? box.width - index * width : width, height: box.height }}
            onClick={() => commit(index)}
          />
        );
      })}
    </div>
  );
}

// An on/off button for a two-state parameter, showing the parameter's value text, its name below
export function Toggle({ backend, id, label, box, enabled = true }: ControlProps) {
  const { value, commit, param } = useParam(backend, id);
  const name = label ?? param.name;
  const on = value >= 0.5;

  return (
    <Labeled box={box} enabled={enabled}>
      {({ control, label: nameLabel }) => {
        const width = Math.min(control.width, 64);
        const height = Math.min(control.height, 24);

        return (
          <>
            <TextButton
              text={formatValue(param, on ? 1 : 0)}
              label={name}
              toggled={on}
              style={{ left: control.left + centred(control.width, width), top: control.top + centred(control.height, height), width, height }}
              onClick={() => commit(on ? 0 : 1)}
            />
            <div {...nameLabel}>{name}</div>
          </>
        );
      }}
    </Labeled>
  );
}

// juce::ComboBox listing a choice parameter's values, as LookAndFeel_V4 draws it
export function ChoiceBox({ backend, id, box }: { backend: DrumsBackend; id: string; box: Box }) {
  const { value, commit, param } = useParam(backend, id);
  const menu = useMenu();
  const button = useRef<HTMLButtonElement>(null);
  const current = Math.round(value);

  const toggleMenu = () => {
    if (menu.isOpen) return menu.close();
    menu.open({
      target: box,
      owner: button.current,
      items: param.choices.map((choice, index) => ({ kind: 'item', label: choice, ticked: index === current, onSelect: () => commit(index) })),
    });
  };

  return (
    <>
      <button
        ref={button}
        type="button"
        aria-label={param.name}
        aria-haspopup="menu"
        className="absolute flex cursor-pointer items-center rounded-[3px] border border-line bg-widget pl-1.5 text-left text-fg"
        style={{ ...box, fontSize: juceHeight(12) }}
        onClick={toggleMenu}
      >
        <span className="grow overflow-hidden text-ellipsis whitespace-nowrap">{param.choices[current]}</span>
        <svg className="mr-1.5 shrink-0" width={8} height={6} viewBox="0 0 8 6" aria-hidden>
          <path d="M 0 1 L 4 5 L 8 1" stroke="var(--color-dim)" strokeWidth={1.5} fill="none" />
        </svg>
      </button>
      {menu.element}
    </>
  );
}
