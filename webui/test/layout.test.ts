import assert from 'node:assert/strict';
import { test } from 'node:test';
import { layOutAboveLabel, layOutRow } from '../src/layout.ts';

test('the label goes right under a square control, the pair centred', () => {
  assert.deepEqual(layOutAboveLabel(140, 190), {
    control: { left: 0, top: 17, width: 140, height: 140 },
    label: { left: 0, top: 157, width: 140, height: 16 },
  });
  assert.deepEqual(layOutAboveLabel(100, 101), {
    control: { left: 7, top: 0, width: 85, height: 85 },
    label: { left: 0, top: 85, width: 100, height: 16 },
  });
  assert.deepEqual(layOutAboveLabel(50, 10), {
    control: { left: 25, top: -3, width: 0, height: 0 },
    label: { left: 0, top: -3, width: 50, height: 16 },
  });
});

test('panels share the row by number of controls, none below its minimum, the last taking the rest', () => {
  const area = { left: 0, top: 5, width: 1048, height: 120 };
  const panels = [
    { numControls: 3, minimumWidth: 100 },
    { numControls: 4, minimumWidth: 150 },
    { numControls: 2, minimumWidth: 180 },
    { numControls: 4, minimumWidth: 100 },
    { numControls: 2, minimumWidth: 80 },
  ];

  assert.deepEqual(
    layOutRow(panels, area, 10).map(({ left, width }) => [left, width]),
    [[0, 189], [199, 252], [461, 180], [651, 252], [913, 135]]
  );
  assert.ok(layOutRow(panels, area, 10).every((box) => box.top === 5 && box.height === 120));
});
