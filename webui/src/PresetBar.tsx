// Mirrors Source/ui/PresetBar.cpp
import { useCallback, useRef, useState, useSyncExternalStore } from 'react';
import { isSamePreset, type PresetBackend, type PresetEntry } from './backend.ts';
import { TextButton } from './controls.tsx';
import type { Box } from './layout.ts';
import { Menu, type MenuItem, type MenuState } from './Menu.tsx';
import { presetActions } from './presetActions.ts';
import type { Prompt } from './PromptOverlay.tsx';

// PresetBar::resized
function layOut({ left, top, width, height }: Box) {
  const at = (x: number, w: number): Box => ({ left: x, top, width: w, height });
  const more = at(left + width - 32, 32);
  const save = at(more.left - 4 - 60, 60);
  const previous = at(left, 28);
  const next = at(save.left - 8 - 28, 28);
  const nameLeft = previous.left + previous.width + 4;
  return { previous, next, save, more, name: at(nameLeft, next.left - 4 - nameLeft) };
}

interface PresetBarProps {
  presets: PresetBackend;
  prompt: Prompt;
  box: Box;
}

// A fragment, not a box, so the menus stay positioned against the editor
export function PresetBar({ presets, prompt, box }: PresetBarProps) {
  const status = useSyncExternalStore(presets.onStatus, presets.status);
  const [menu, setMenu] = useState<MenuState | null>(null);
  const nameButton = useRef<HTMLButtonElement>(null);
  const moreButton = useRef<HTMLButtonElement>(null);
  const layout = layOut(box);
  const actions = presetActions(presets, prompt);
  const closeMenu = useCallback(() => setMenu(null), []);

  const showPresetMenu = async () => {
    if (menu) return setMenu(null);

    const list = await presets.list();
    const { current } = presets.status();
    const item = (preset: PresetEntry): MenuItem => ({
      kind: 'item',
      label: preset.name,
      ticked: isSamePreset(preset, current),
      onSelect: () => void presets.load(preset),
    });

    // Factory presets by category, user presets flat
    const categories = new Map<string, PresetEntry[]>();
    for (const preset of list.filter((p) => p.isFactory)) {
      const category = preset.category || 'Other';
      categories.set(category, [...(categories.get(category) ?? []), preset]);
    }
    const users = list.filter((preset) => !preset.isFactory);

    const items: MenuItem[] = [{ kind: 'header', label: 'Factory' }];
    for (const category of [...categories.keys()].sort()) {
      const entries = categories.get(category)!;
      items.push({ kind: 'submenu', label: category, ticked: entries.some((p) => isSamePreset(p, current)), items: entries.map(item) });
    }
    if (categories.size === 0) items.push({ kind: 'item', label: 'No factory presets yet', disabled: true });

    items.push({ kind: 'header', label: 'User' }, ...users.map(item));
    if (users.length === 0) items.push({ kind: 'item', label: 'No user presets yet', disabled: true });

    setMenu({ items, target: layout.name, owner: nameButton.current });
  };

  const showMoreMenu = async () => {
    if (menu) return setMenu(null);
    setMenu({ items: await actions.menuItems(), target: layout.more, owner: moreButton.current });
  };

  return (
    <>
      <TextButton text="<" label="Previous preset" style={layout.previous} onClick={() => void presets.loadPrevious()} />
      <TextButton ref={nameButton} text={status.current.name + (status.modified ? ' *' : '')} style={layout.name} onClick={() => void showPresetMenu()} />
      <TextButton text=">" label="Next preset" style={layout.next} onClick={() => void presets.loadNext()} />
      <TextButton text="Save" style={layout.save} onClick={() => void actions.save()} />
      <TextButton ref={moreButton} text="..." label="Preset options" style={layout.more} onClick={() => void showMoreMenu()} />
      {menu && <Menu {...menu} onClose={closeMenu} />}
    </>
  );
}
