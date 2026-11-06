// Mirrors Source/ui/Panel.h
import { textWidth } from './fonts.ts';
import type { Box } from './layout.ts';
import { Text } from './Text.tsx';

const HEADER_HEIGHT = 26;
const TITLE_FONT = 12;

// Room for the title and the header component side by side
export const minimumWidth = (title: string, headerWidth = 0) => Math.round(textWidth(title.toUpperCase(), TITLE_FONT, true)) + headerWidth + 32;

// Panel::resized: the header component at the title's right, the controls in a grid below
export function panelLayout(box: Box, numControls: number, { columns = 0, headerWidth = 0 } = {}) {
  const numColumns = columns > 0 ? columns : numControls;
  const numRows = Math.ceil(numControls / Math.max(1, numColumns));
  const area = { left: box.left + 6, top: box.top + HEADER_HEIGHT, width: box.width - 12, height: box.height - HEADER_HEIGHT - 6 };
  const cellWidth = Math.trunc(area.width / Math.max(1, numColumns));
  const cellHeight = Math.trunc(area.height / Math.max(1, numRows));

  return {
    header: { left: box.left + box.width - 6 - headerWidth, top: box.top + 3, width: headerWidth, height: HEADER_HEIGHT - 6 },
    controls: Array.from({ length: numControls }, (_, i): Box => ({
      left: area.left + (i % numColumns) * cellWidth,
      top: area.top + Math.trunc(i / numColumns) * cellHeight,
      width: cellWidth,
      height: cellHeight,
    })),
  };
}

// The frame and title; the controls are laid out in the editor by panelLayout
export function Panel({ title, box }: { title?: string; box: Box }) {
  return (
    <div className="absolute rounded-md border border-line bg-panel" style={box}>
      {title && (
        <Text style={{ left: 9, top: 0, height: HEADER_HEIGHT }} size={TITLE_FONT} bold>
          {title.toUpperCase()}
        </Text>
      )}
    </div>
  );
}
