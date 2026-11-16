import assert from 'node:assert/strict';
import { test } from 'node:test';
import { layOutRow } from '../src/layout.ts';

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
