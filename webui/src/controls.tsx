// Mirrors Source/ui/Controls.h and SegmentedButtons.h as CustomLookAndFeel draws them
import type { CSSProperties } from 'react';
import { juceHeight } from '@samplerealm/modules/editor/fonts.ts';
import { useParam } from '@samplerealm/modules/editor/hooks.ts';
import { arcPath, LabeledSlider, pointAt, ROTARY_END, ROTARY_START, styleLabel, type RotaryProps } from '@samplerealm/modules/editor/LabeledSlider.tsx';
import { centred, layOutAboveLabel, type Rect } from '@samplerealm/modules/editor/layout.ts';
import { useMenu } from '@samplerealm/modules/editor/Menu.tsx';
import type { DrumsBackend } from './backend.ts';

// srd::controlLabelHeight and srd::controlNameHeight
const LABEL_HEIGHT = 16;
const NAME_HEIGHT = 11;

// The LookAndFeel's Label::textColourId
const LABEL_CLASS = 'text-dim whitespace-nowrap';

// CustomLookAndFeel::drawRotarySlider
function RotaryGraphic({ width, height, proportion, param, enabled }: RotaryProps & { enabled: boolean }) {
  const radius = Math.max(0, Math.min(width, height) - 8) / 2;
  const cx = width / 2;
  const cy = height / 2;
  const trackWidth = Math.max(2.5, radius * 0.12);
  const arcRadius = radius - trackWidth * 0.5;
  const angle = ROTARY_START + proportion * (ROTARY_END - ROTARY_START);
  // Symmetric ranges (tune, EQ gain) fill from the centre
  const isBipolar = param.min < 0 && param.min === -param.max;
  const fromAngle = isBipolar ? 0.5 * (ROTARY_START + ROTARY_END) : ROTARY_START;
  const bodyRadius = arcRadius - trackWidth * 1.5;
  const [x1, y1] = pointAt(cx, cy, bodyRadius * 0.25, angle);
  const [x2, y2] = pointAt(cx, cy, bodyRadius * 0.8, angle);
  const stroke = { strokeWidth: trackWidth, strokeLinecap: 'round', fill: 'none' } as const;

  if (radius <= 0) return null;

  return (
    <svg width={width} height={height} aria-hidden>
      <path d={arcPath(cx, cy, arcRadius, ROTARY_START, ROTARY_END)} stroke="var(--color-line)" {...stroke} />
      {enabled && Math.abs(angle - fromAngle) > 1e-3 && (
        <path d={arcPath(cx, cy, arcRadius, Math.min(fromAngle, angle), Math.max(fromAngle, angle))} stroke="var(--color-accent)" {...stroke} />
      )}
      {bodyRadius > 0 && <circle cx={cx} cy={cy} r={bodyRadius} fill="var(--color-widget)" />}
      <line x1={x1} y1={y1} x2={x2} y2={y2} stroke="var(--color-fg)" strokeWidth={2} />
    </svg>
  );
}

const EnabledRotary = (props: RotaryProps) => <RotaryGraphic {...props} enabled />;
const DisabledRotary = (props: RotaryProps) => <RotaryGraphic {...props} enabled={false} />;

// Greyed out and inert when disabled
const placed = (box: Rect, enabled: boolean): CSSProperties => ({ ...box, opacity: enabled ? 1 : 0.35, pointerEvents: enabled ? undefined : 'none' });

interface ControlProps {
  backend: DrumsBackend;
  id: string;
  // The parameter's name if not given
  label?: string;
  box: Rect;
  enabled?: boolean;
}

// The shared LabeledSlider for a parameter, at Drums' sizes and with its value text
export function Knob({ backend, id, label, box, enabled = true }: ControlProps) {
  return (
    <div className="absolute" style={placed(box, enabled)}>
      <LabeledSlider
        backend={backend}
        id={id}
        label={label ?? backend.param(id).name}
        width={box.width}
        height={box.height}
        rotary={enabled ? EnabledRotary : DisabledRotary}
        labelClassName={LABEL_CLASS}
        nameHeight={NAME_HEIGHT}
        labelHeight={LABEL_HEIGHT}
        valueText={backend.formatValue}
        parseText={backend.parseValue}
      />
    </div>
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
function TextButton({ text, style, onClick, toggled = false, joined = {}, label }: TextButtonProps) {
  const radius = (square?: boolean) => (square ? 0 : 4);

  return (
    <button
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
}

//==============================================================================
// A row of joined buttons, one per value of a choice parameter
export function ChoiceButtons({ backend, id, box }: { backend: DrumsBackend; id: string; box: Rect }) {
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
  const { control, label: labelBox } = layOutAboveLabel(box.width, box.height, LABEL_HEIGHT);
  const nameLabel = styleLabel(labelBox, NAME_HEIGHT);
  const width = Math.min(control.width, 64);
  const height = Math.min(control.height, 24);

  return (
    <div className="absolute" style={placed(box, enabled)}>
      <TextButton
        text={backend.formatValue(param, on ? 1 : 0)}
        label={name}
        toggled={on}
        style={{ left: control.left + centred(control.width, width), top: control.top + centred(control.height, height), width, height }}
        onClick={() => commit(on ? 0 : 1)}
      />
      <div className={`${nameLabel.className} ${LABEL_CLASS}`} style={nameLabel.style}>
        {name}
      </div>
    </div>
  );
}

// juce::ComboBox listing a choice parameter's values, as LookAndFeel_V4 draws it
export function ChoiceBox({ backend, id, box }: { backend: DrumsBackend; id: string; box: Rect }) {
  const { value, commit, param } = useParam(backend, id);
  const menu = useMenu();
  const current = Math.round(value);

  return (
    <>
      <button
        type="button"
        aria-label={param.name}
        aria-haspopup="menu"
        className="absolute flex cursor-pointer items-center rounded-[3px] border border-line bg-widget pl-1.5 text-left text-fg"
        style={{ ...box, fontSize: juceHeight(12) }}
        onClick={(e) =>
          menu.open(
            e.currentTarget,
            param.choices.map((choice, index) => ({ kind: 'item', label: choice, ticked: index === current, onSelect: () => commit(index) }))
          )
        }
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
