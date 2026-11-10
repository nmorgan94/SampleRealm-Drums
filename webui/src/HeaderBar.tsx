// Mirrors Source/ui/HeaderBar.cpp
import { useRef, type ReactNode } from 'react';
import { aboutText, type About } from './about.ts';
import type { PresetBackend } from './backend.ts';
import { TextButton } from './controls.tsx';
import { centred, type Box } from './layout.ts';
import { useMenu } from './Menu.tsx';
import { presetActions } from './presetActions.ts';
import { PresetBar } from './PresetBar.tsx';
import type { Prompt } from './PromptOverlay.tsx';
import { Text } from './Text.tsx';

// HeaderBar::Title: small above large
export interface HeaderTitle {
  small: string;
  large: string;
}

// HeaderBar::Layout, which each plugin sets
export interface HeaderLayout {
  height: number;
  // Left of the title, right of the meter
  edge: number;
  titleWidth: number;
  // The large line
  titleHeight: number;
  // Right after the title
  switchWidth: number;
  // Either side of the preset bar and menu
  gap: number;
  // The switch too
  controlsHeight: number;
  menuWidth: number;
  // Between the preset bar and the menu
  menuGap: number;
  meterWidth: number;
  // Above and below the title and the meter
  meterInset: number;
}

const SMALL_LINE_HEIGHT = 16;
const SMALL_FONT_HEIGHT = 11;

// HeaderBar::resized
const layOut = (width: number, layout: HeaderLayout) => {
  const { height, edge, titleWidth, switchWidth, gap, controlsHeight, menuWidth, menuGap, meterWidth, meterInset } = layout;
  const inner = { top: meterInset, height: height - 2 * meterInset };
  const meter: Box = { left: width - edge - meterWidth, width: meterWidth, ...inner };
  const controlsTop = centred(height, controlsHeight);
  const switchBox: Box = { left: edge + titleWidth, top: controlsTop, width: switchWidth, height: controlsHeight };
  const left = switchBox.left + switchWidth + gap;
  const controls: Box = { left, top: controlsTop, width: meter.left - gap - left, height: controlsHeight };

  return {
    title: { left: edge, width: titleWidth, ...inner },
    switch: switchBox,
    meter,
    menu: { ...controls, left: controls.left + controls.width - menuWidth, width: menuWidth },
    presetBar: { ...controls, width: controls.width - menuWidth - menuGap },
  };
};

interface HeaderBarProps {
  title: HeaderTitle;
  width: number;
  layout: HeaderLayout;
  backend: { info: About; running: string; presets: PresetBackend };
  prompt: Prompt;
  showManual(): void;
  switchControl(box: Box): ReactNode;
  meter(box: Box): ReactNode;
}

// A fragment, not a box, so the menus stay positioned against the editor
export function HeaderBar({ title, width, layout, backend, prompt, showManual, switchControl, meter }: HeaderBarProps) {
  const menu = useMenu();
  const menuButton = useRef<HTMLButtonElement>(null);
  const boxes = layOut(width, layout);
  const actions = presetActions(backend.presets, prompt);

  const showMenu = async () => {
    if (menu.isOpen) return menu.close();

    menu.open({
      target: boxes.menu,
      owner: menuButton.current,
      items: [
        ...(await actions.menuItems()),
        { kind: 'separator' },
        { kind: 'item', label: 'Manual', onSelect: showManual },
        { kind: 'item', label: 'About', onSelect: () => prompt.showMessage('About', aboutText(backend.info, backend.running)) },
      ],
    });
  };

  return (
    <>
      <h1 aria-label={`${title.small} ${title.large}`}>
        <Text style={{ ...boxes.title, height: SMALL_LINE_HEIGHT }} size={SMALL_FONT_HEIGHT} bold colour="dim" bottom>
          {title.small}
        </Text>
        <Text style={{ ...boxes.title, top: boxes.title.top + SMALL_LINE_HEIGHT, height: boxes.title.height - SMALL_LINE_HEIGHT }} size={layout.titleHeight} bold>
          {title.large}
        </Text>
      </h1>
      {switchControl(boxes.switch)}
      <PresetBar presets={backend.presets} actions={actions} box={boxes.presetBar} />
      <TextButton ref={menuButton} text="..." label="Menu" style={boxes.menu} onClick={() => void showMenu()} />
      {menu.element}
      {meter(boxes.meter)}
    </>
  );
}
