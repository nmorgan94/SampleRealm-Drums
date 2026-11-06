export type Box = { left: number; top: number; width: number; height: number };

// juce::Rectangle::withSizeKeepingCentre
export const centred = (outer: number, inner: number) => Math.trunc((outer - inner) / 2);

// Source/ui/LabeledSlider.h's layOutAboveLabel: the label right under a square control, the pair centred
export function layOutAboveLabel(width: number, height: number): { control: Box; label: Box } {
  const LABEL_HEIGHT = 16;
  const size = Math.max(0, Math.min(width, height - LABEL_HEIGHT));
  const top = centred(height, size + LABEL_HEIGHT);
  return {
    control: { left: centred(width, size), top, width: size, height: size },
    label: { left: 0, top: top + size, width, height: LABEL_HEIGHT },
  };
}

export interface RowPanel {
  numControls: number;
  minimumWidth: number;
}

// Source/ui/Panel.h's layOutRow: panels as wide as their number of controls, none narrower than its
// minimum, the last taking what rounding leaves
export function layOutRow(panels: readonly RowPanel[], area: Box, gap: number): Box[] {
  const atMinimum = new Set<RowPanel>();
  let controlWidth = 0;

  for (let changed = true; changed; ) {
    let spare = area.width - gap * (panels.length - 1);
    let numControls = 0;

    for (const panel of panels) {
      if (atMinimum.has(panel)) spare -= panel.minimumWidth;
      else numControls += panel.numControls;
    }

    controlWidth = Math.trunc(spare / Math.max(1, numControls));
    changed = false;

    for (const panel of panels) {
      if (!atMinimum.has(panel) && controlWidth * panel.numControls < panel.minimumWidth) {
        atMinimum.add(panel);
        changed = true;
      }
    }
  }

  let { left, width } = area;
  const removeFromLeft = (amount: number) => {
    const taken = Math.min(Math.max(0, amount), width);
    const box = { left, top: area.top, width: taken, height: area.height };
    left += taken;
    width -= taken;
    return box;
  };

  return panels.map((panel, index) => {
    const wanted = atMinimum.has(panel) ? panel.minimumWidth : controlWidth * panel.numControls;
    const box = index === panels.length - 1 ? { left, top: area.top, width, height: area.height } : removeFromLeft(wanted);
    removeFromLeft(gap);
    return box;
  });
}
