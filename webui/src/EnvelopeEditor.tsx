// Mirrors Source/ui/EnvelopeEditor.cpp
import {
  useEffect, useId, useMemo, useRef, useState, type KeyboardEvent, type MouseEvent as ReactMouseEvent, type PointerEvent as ReactPointerEvent,
} from 'react';
import type { DrumsBackend, EnvelopeNode, Preview } from './backend.ts';
import { clampValue, duration, evaluate, formatTime, gridStep, gridValues, insertNode, removeNode, setNode, toDisplay, toDomain, toDomainNodes, viewSeconds } from './envelope.ts';
import { juceHeight, textWidth } from './fonts.ts';
import type { Box } from './layout.ts';

const NODE_RADIUS = 4.5;
const HANDLE_RADIUS = 3;
const HIT_RADIUS = 8;
const CURVE_PER_PIXEL = 1 / 120;
const CURVE_HIT_DISTANCE = 6;
const LEGEND_DOT = 8;
const LEGEND_GAP = 5;
const LEGEND_SPACING = 14;
const FONT = 11;

export interface Curve {
  envelope: number;
  name: string;
  colour: string;
}

type Target = 'node' | 'handle' | 'curve' | 'legend';
// The node; for a handle, the node its segment leads into; for a curve or legend dot, the curve
type Hit = { target: Target; index: number } | null;

type Point = readonly [x: number, y: number];

const editsNode = (hit: Hit) => hit?.target === 'node' || hit?.target === 'handle';
const choosesCurve = (hit: Hit) => hit?.target === 'curve' || hit?.target === 'legend';
const sameHit = (a: Hit, b: Hit) => a?.target === b?.target && a?.index === b?.index;

const distance = ([x1, y1]: Point, [x2, y2]: Point) => Math.hypot(x1 - x2, y1 - y2);

const distanceToPolyline = (p: Point, points: readonly Point[]) => {
  let best = Infinity;
  for (let i = 1; i < points.length; i++) {
    const [ax, ay] = points[i - 1]!;
    const [bx, by] = points[i]!;
    const length = (bx - ax) ** 2 + (by - ay) ** 2;
    const t = length === 0 ? 0 : Math.max(0, Math.min(1, ((p[0] - ax) * (bx - ax) + (p[1] - ay) * (by - ay)) / length));
    best = Math.min(best, distance(p, [ax + t * (bx - ax), ay + t * (by - ay)]));
  }
  return best;
};

const toPath = (points: readonly Point[]) => points.map(([x, y], i) => `${i === 0 ? 'M' : 'L'} ${x} ${y}`).join(' ');

interface EnvelopeEditorProps {
  backend: DrumsBackend;
  box: Box;
  // Shown together, each in its own colour; envelope is the one being edited
  curves: Curve[];
  envelope: number;
  onSelect(envelope: number): void;
  // Stretches the time axis, as Length does at playback
  timeScale: number;
  // Drawn behind the grid and curves
  preview: Preview | null;
  formatValue(envelope: number, value: number): string;
  formatGridValue(envelope: number, value: number): string;
  // Seconds across the plot, after the time scale
  onViewChange(seconds: number): void;
}

export function EnvelopeEditor({ backend, box, curves, envelope, onSelect, timeScale, preview, formatValue, formatGridValue, onViewChange }: EnvelopeEditorProps) {
  const [all, setAll] = useState(() => backend.envelopes.map((_, index) => backend.getEnvelope(index)));
  const nodes = all[envelope]!;
  const spec = backend.envelopes[envelope]!;
  const [hover, setHover] = useState<Hit>(null);
  const [drag, setDrag] = useState<Hit>(null);
  const dragStart = useRef({ y: 0, curve: 0 });
  const choseOnPointerDown = useRef(false);
  const undo = useRef<{ index: number; nodes: EnvelopeNode[] }[]>([]);
  const redo = useRef<typeof undo.current>([]);
  // Pushed on a drag's first move, so a click alone isn't an undo step
  const dragUndo = useRef<(typeof undo.current)[number] | null>(null);
  const sent = useRef<EnvelopeNode[] | null>(null);
  const svg = useRef<SVGSVGElement>(null);

  // As EnvelopeModel::reload: anything this editor didn't send is a preset or state restore
  useEffect(() => {
    setAll(backend.envelopes.map((_, index) => backend.getEnvelope(index)));
    const unsubscribes = backend.envelopes.map((_, index) =>
      backend.subscribeEnvelope(index, (nodes) => {
        setAll((current) => current.map((existing, i) => (i === index ? nodes : existing)));
        if (nodes === sent.current) return;
        undo.current = [];
        redo.current = [];
      })
    );
    return () => unsubscribes.forEach((unsubscribe) => unsubscribe());
  }, [backend]);
  const ids = useId();

  // Spans the longest curve shown, so choosing another doesn't rescale the view; held still while dragging
  const longest = Math.max(duration(nodes), ...curves.map((curve) => duration(all[curve.envelope]!)));
  const [view, setView] = useState(() => viewSeconds(longest, timeScale));

  useEffect(() => {
    if (!drag) setView(viewSeconds(longest, timeScale));
  }, [drag, longest, timeScale]);

  useEffect(() => onViewChange(view), [view, onViewChange]);

  const plot = { left: 48, top: 10, width: box.width - 60, height: box.height - 30 };
  const plotRight = plot.left + plot.width;
  const plotBottom = plot.top + plot.height;

  const timeToX = (time: number) => plot.left + ((time * timeScale) / view) * plot.width;
  const xToTime = (x: number) => (((x - plot.left) / plot.width) * view) / timeScale;

  const domainToY = (value: number, index: number) => {
    const info = backend.envelopes[index]!;
    const low = toDomain(info, info.min);
    const high = toDomain(info, info.max);
    return plotBottom - ((value - low) / (high - low)) * plot.height;
  };

  const valueToY = (value: number) => domainToY(toDomain(spec, value), envelope);

  const yToValue = (y: number) => {
    const low = toDomain(spec, spec.min);
    const high = toDomain(spec, spec.max);
    return clampValue(spec, toDisplay(spec, low + ((plotBottom - y) / plot.height) * (high - low)));
  };

  const domainNodes = useMemo(() => all.map((envelopeNodes, index) => toDomainNodes(backend.envelopes[index]!, envelopeNodes)), [all, backend]);

  const nodePosition = (index: number): Point => [timeToX(nodes[index]!.time), valueToY(nodes[index]!.value)];

  const handlePosition = (index: number): Point => {
    const midTime = 0.5 * (nodes[index - 1]!.time + nodes[index]!.time);
    return [timeToX(midTime), domainToY(evaluate(domainNodes[envelope]!, midTime), envelope)];
  };

  // One point per pixel up to the last node; callers draw the held tail
  const curvePoints = (index: number): Point[] => {
    const curve = domainNodes[index]!;
    const endX = Math.min(timeToX(duration(curve)), plotRight);
    const points: Point[] = [[plot.left, domainToY(evaluate(curve, 0), index)]];
    for (let x = plot.left + 1; x <= endX; x++) points.push([x, domainToY(evaluate(curve, xToTime(x)), index)]);
    points.push([endX, domainToY(curve.at(-1)?.value ?? 0, index)]);
    return points;
  };

  // Legend items sit in a row at the plot's top right, in curve order
  const legendBoxes = useMemo(() => {
    const boxes: Box[] = [];
    let right = plotRight - 8;
    for (let i = curves.length - 1; i >= 0; i--) {
      const width = LEGEND_DOT + LEGEND_GAP + textWidth(curves[i]!.name, FONT);
      boxes[i] = { left: right - width, top: plot.top + 6, width, height: 16 };
      right -= width + LEGEND_SPACING;
    }
    return boxes;
  }, [curves, plotRight, plot.top]);

  const hasLegend = curves.length > 1;

  //==============================================================================
  const findFadedCurve = (p: Point): Hit => {
    if (p[0] < plot.left || p[0] > plotRight || p[1] < plot.top || p[1] > plotBottom) return null;

    // Clicks near the curve being edited, its held tail included, stay with it
    const active = curvePoints(envelope);
    let bestDistance = Math.min(CURVE_HIT_DISTANCE, distanceToPolyline(p, [...active, [plotRight, active.at(-1)![1]]]));
    let best: Hit = null;

    curves.forEach((curve, i) => {
      if (curve.envelope === envelope || all[curve.envelope]!.length < 2) return;
      const d = distanceToPolyline(p, curvePoints(curve.envelope));
      if (d < bestDistance) {
        best = { target: 'curve', index: i };
        bestDistance = d;
      }
    });

    return best;
  };

  const findTarget = (p: Point): Hit => {
    const nearest = (target: Target, first: number, position: (index: number) => Point): Hit => {
      let best: Hit = null;
      let bestDistance = HIT_RADIUS;
      for (let i = first; i < nodes.length; i++) {
        const d = distance(position(i), p);
        if (d < bestDistance) {
          best = { target, index: i };
          bestDistance = d;
        }
      }
      return best;
    };

    // The curve being edited wins; only a miss can choose another
    const onCurve = nearest('node', 0, nodePosition) ?? nearest('handle', 1, handlePosition);
    if (onCurve) return onCurve;

    if (hasLegend) {
      const legend = legendBoxes.findIndex((item) => p[0] >= item.left - 4 && p[0] <= item.left + item.width + 4 && p[1] >= item.top - 4 && p[1] <= item.top + item.height + 4);
      if (legend >= 0) return { target: 'legend', index: legend };
    }

    return findFadedCurve(p);
  };

  //==============================================================================
  // The editor may be scaled, as the demo page scales it
  const pointerPosition = (e: { clientX: number; clientY: number }): Point => {
    const rect = svg.current!.getBoundingClientRect();
    return [((e.clientX - rect.left) * box.width) / rect.width, ((e.clientY - rect.top) * box.height) / rect.height];
  };

  const send = (index: number, next: EnvelopeNode[]) => {
    sent.current = next;
    backend.setEnvelope(index, next);
  };

  const remember = (entry: (typeof undo.current)[number]) => {
    undo.current.push(entry);
    redo.current = [];
  };

  const edit = (next: EnvelopeNode[] | null) => {
    if (!next) return;
    remember({ index: envelope, nodes });
    send(envelope, next);
  };

  const step = (from: typeof undo, to: typeof undo) => {
    const entry = from.current.pop();
    if (!entry) return;
    to.current.push({ index: entry.index, nodes: all[entry.index]! });
    send(entry.index, entry.nodes);
  };

  const onPointerDown = (e: ReactPointerEvent<SVGSVGElement>) => {
    svg.current?.focus();
    const hit = findTarget(pointerPosition(e));

    // The second click of a double-click keeps the first click's answer
    if (e.detail <= 1) choseOnPointerDown.current = choosesCurve(hit);
    if (!hit) return;

    if (choosesCurve(hit)) {
      setHover(null);
      onSelect(curves[hit.index]!.envelope);
      return;
    }

    if (e.button === 2) {
      if (hit.target === 'node') edit(removeNode(nodes, hit.index));
      return;
    }

    if (e.button !== 0) return;
    e.currentTarget.setPointerCapture(e.pointerId);
    dragUndo.current = { index: envelope, nodes };
    dragStart.current = { y: pointerPosition(e)[1], curve: nodes[hit.index]!.curve };
    setDrag(hit);
  };

  const onPointerMove = (e: ReactPointerEvent<SVGSVGElement>) => {
    const p = pointerPosition(e);

    if (!drag) {
      const hit = findTarget(p);
      if (!sameHit(hit, hover)) setHover(hit);
      return;
    }

    const node = { ...nodes[drag.index]! };

    if (drag.target === 'node') {
      node.time = xToTime(p[0]);
      node.value = yToValue(p[1]);
    } else {
      // Dragging the handle towards where the segment ends pushes the curve that way
      const direction = nodePosition(drag.index)[1] > nodePosition(drag.index - 1)[1] ? 1 : -1;
      node.curve = dragStart.current.curve + (p[1] - dragStart.current.y) * direction * CURVE_PER_PIXEL;
    }

    const next = setNode(spec, nodes, drag.index, node);
    const [before, after] = [nodes[drag.index]!, next[drag.index]!];
    if (before.time === after.time && before.value === after.value && before.curve === after.curve) return;

    if (dragUndo.current) remember(dragUndo.current);
    dragUndo.current = null;
    send(envelope, next);
  };

  const onPointerUp = (e: ReactPointerEvent<SVGSVGElement>) => {
    setDrag(null);
    setHover(findTarget(pointerPosition(e)));
  };

  const onDoubleClick = (e: ReactMouseEvent<SVGSVGElement>) => {
    // The first click chose this curve, so the second shouldn't also edit it
    if (choseOnPointerDown.current) return;

    const p = pointerPosition(e);
    const hit = findTarget(p);

    if (hit?.target === 'node') edit(removeNode(nodes, hit.index));
    else if (hit?.target === 'handle') edit(setNode(spec, nodes, hit.index, { ...nodes[hit.index]!, curve: 0 }));
    else if (!hit && p[0] >= plot.left && p[0] <= plotRight && p[1] >= plot.top && p[1] <= plotBottom)
      edit(insertNode(spec, nodes, { time: xToTime(p[0]), value: yToValue(p[1]), curve: 0 })?.nodes ?? null);
  };

  const onKeyDown = (e: KeyboardEvent) => {
    if (e.key.toLowerCase() !== 'z' || !(e.metaKey || e.ctrlKey)) return;
    e.preventDefault();
    if (e.shiftKey) step(redo, undo);
    else step(undo, redo);
  };

  //==============================================================================
  const active = drag ?? hover;
  const isActive = (target: Target, index: number) => sameHit(active, { target, index });
  const hoveredCurve = choosesCurve(hover) ? hover!.index : -1;
  const colourOf = (index: number) => curves.find((curve) => curve.envelope === index)?.colour ?? 'var(--color-accent)';
  const colour = colourOf(envelope);

  const readout = (() => {
    if (!editsNode(active)) return '';
    const node = nodes[active!.index];
    if (!node) return '';
    if (active!.target === 'handle') return `Curve ${node.curve.toFixed(2)}`;
    return `${formatTime(node.time * timeScale)}   ${formatValue(envelope, node.value)}`;
  })();

  const waveform = useMemo(() => {
    if (!preview || preview.samples.length === 0 || plot.width <= 0) return '';
    const { samples, sampleRate } = preview;
    const samplesPerPixel = (view * sampleRate) / plot.width;
    const centreY = plot.top + plot.height / 2;
    const half = plot.height / 2;
    const toY = (sample: number) => centreY - Math.max(-1, Math.min(1, sample)) * half;
    const upper: Point[] = [];
    const lower: Point[] = [];

    for (let x = 0; x < plot.width; x++) {
      const start = Math.round(x * samplesPerPixel);
      if (start >= samples.length) break;
      const end = Math.min(samples.length, Math.max(start + 1, Math.round((x + 1) * samplesPerPixel)));
      let min = Infinity;
      let max = -Infinity;
      for (let i = start; i < end; i++) {
        min = Math.min(min, samples[i]!);
        max = Math.max(max, samples[i]!);
      }
      upper.push([plot.left + x, toY(max)]);
      lower.push([plot.left + x, toY(min)]);
    }

    return upper.length ? `${toPath([...upper, ...lower.reverse()])} Z` : '';
  }, [preview, view, plot.left, plot.top, plot.width, plot.height]);

  const timeStep = gridStep(view, plot.width / 70);
  const times = Array.from({ length: Math.floor(view / timeStep + 1e-4) + 1 }, (_, i) => i * timeStep);
  const activePoints = nodes.length >= 2 ? curvePoints(envelope) : [];
  const activeEnd = activePoints.at(-1);
  const clip = `${ids}-plot`;
  const gradient = `${ids}-fill`;
  const textStyle = { fontSize: juceHeight(FONT) };
  const readoutTop = hasLegend ? legendBoxes[0]!.top + legendBoxes[0]!.height + 4 : plot.top + 6;

  const cursor = hover?.target === 'node' ? 'grab' : hover?.target === 'handle' ? 'ns-resize' : hover ? 'pointer' : 'default';

  return (
    <svg
      ref={svg}
      className="absolute outline-none select-none touch-none"
      style={{ ...box, cursor: drag?.target === 'node' ? 'grabbing' : cursor }}
      width={box.width}
      height={box.height}
      tabIndex={0}
      role="application"
      aria-label="Envelope editor"
      onPointerDown={onPointerDown}
      onPointerMove={onPointerMove}
      onPointerUp={onPointerUp}
      onPointerCancel={onPointerUp}
      onPointerLeave={() => !drag && setHover(null)}
      onDoubleClick={onDoubleClick}
      onContextMenu={(e) => e.preventDefault()}
      onKeyDown={onKeyDown}
    >
      <defs>
        <clipPath id={clip}>
          <rect x={plot.left} y={plot.top} width={Math.max(0, plot.width)} height={Math.max(0, plot.height)} />
        </clipPath>
        <linearGradient id={gradient} x1={0} y1={0} x2={0} y2={1}>
          <stop offset="0%" stopColor={colour} stopOpacity={0.22} />
          <stop offset="100%" stopColor={colour} stopOpacity={0.02} />
        </linearGradient>
      </defs>

      <rect x={plot.left} y={plot.top} width={Math.max(0, plot.width)} height={Math.max(0, plot.height)} rx={4} fill="var(--color-background)" />
      {waveform && <path d={waveform} fill="var(--color-waveform)" fillOpacity={0.5} clipPath={`url(#${clip})`} />}

      {/* Time, in seconds after the time scale; labels kept inside the component */}
      {times.map((seconds) => {
        const x = plot.left + (seconds / view) * plot.width;
        const atEdge = x + 30 > box.width;
        return (
          <g key={seconds}>
            <line x1={Math.round(x) + 0.5} y1={plot.top} x2={Math.round(x) + 0.5} y2={plotBottom} stroke="var(--color-grid)" />
            <text x={atEdge ? box.width : x} y={plotBottom + 10} textAnchor={atEdge ? 'end' : 'middle'} dominantBaseline="middle" fill="var(--color-dim)" style={textStyle}>
              {formatTime(seconds)}
            </text>
          </g>
        );
      })}

      {gridValues(spec).map((value) => {
        const y = valueToY(value);
        return (
          <g key={value}>
            <line x1={plot.left} y1={Math.round(y) + 0.5} x2={plotRight} y2={Math.round(y) + 0.5} stroke="var(--color-grid)" />
            <text x={plot.left - 6} y={y} textAnchor="end" dominantBaseline="middle" fill="var(--color-dim)" style={textStyle}>
              {formatGridValue(envelope, value)}
            </text>
          </g>
        );
      })}

      <g clipPath={`url(#${clip})`}>
        {curves.map((curve, i) => {
          if (curve.envelope === envelope || all[curve.envelope]!.length < 2) return null;
          const points = curvePoints(curve.envelope);
          const end = points.at(-1)!;
          const opacity = hoveredCurve === i ? 0.8 : 0.35;
          return (
            <g key={curve.envelope} stroke={curve.colour} strokeOpacity={opacity} fill="none">
              <path d={toPath(points)} strokeWidth={1.5} strokeLinejoin="round" strokeLinecap="round" />
              <line x1={end[0]} y1={end[1]} x2={plotRight} y2={end[1]} strokeWidth={1} />
            </g>
          );
        })}

        {activeEnd && (
          <>
            <path d={`${toPath(activePoints)} L ${activeEnd[0]} ${plotBottom} L ${plot.left} ${plotBottom} Z`} fill={`url(#${gradient})`} />
            <path d={toPath(activePoints)} stroke={colour} strokeWidth={2} strokeLinejoin="round" strokeLinecap="round" fill="none" />
            <line x1={activeEnd[0]} y1={activeEnd[1]} x2={plotRight} y2={activeEnd[1]} stroke={colour} strokeOpacity={0.35} strokeWidth={1.5} />
          </>
        )}
      </g>

      {activeEnd &&
        nodes.slice(1).map((_, i) => {
          const index = i + 1;
          const [x, y] = handlePosition(index);
          const on = isActive('handle', index);
          return <circle key={`h${index}`} cx={x} cy={y} r={on ? HANDLE_RADIUS + 1.5 : HANDLE_RADIUS} fill={colour} fillOpacity={on ? 1 : 0.5} />;
        })}

      {activeEnd &&
        nodes.map((_, index) => {
          const [x, y] = nodePosition(index);
          const on = isActive('node', index);
          return <circle key={`n${index}`} cx={x} cy={y} r={on ? NODE_RADIUS + 1.5 : NODE_RADIUS} fill={on ? colour : 'var(--color-background)'} stroke={colour} strokeWidth={1.5} />;
        })}

      {hasLegend &&
        curves.map((curve, i) => {
          const item = legendBoxes[i]!;
          const selected = curve.envelope === envelope;
          const hovered = hoveredCurve === i;
          const cy = item.top + item.height / 2;
          return (
            <g key={curve.envelope}>
              {/* Filled for the curve being edited, hollow for the rest */}
              {selected ? (
                <circle cx={item.left + LEGEND_DOT / 2} cy={cy} r={LEGEND_DOT / 2} fill={curve.colour} />
              ) : (
                <circle cx={item.left + LEGEND_DOT / 2} cy={cy} r={LEGEND_DOT / 2 - 0.75} fill="none" stroke={curve.colour} strokeWidth={1.5} />
              )}
              <text x={item.left + LEGEND_DOT + LEGEND_GAP} y={cy} dominantBaseline="middle" fill={selected || hovered ? 'var(--color-fg)' : 'var(--color-dim)'} style={textStyle}>
                {curve.name}
              </text>
            </g>
          );
        })}

      {readout && (
        <text x={plotRight - 8} y={readoutTop} textAnchor="end" dominantBaseline="hanging" fill="var(--color-fg)" style={{ ...textStyle, whiteSpace: 'pre' }}>
          {readout}
        </text>
      )}
    </svg>
  );
}
