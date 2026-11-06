// Mirrors juce::PopupMenu as LookAndFeel_V4 draws it
import { useEffect, useLayoutEffect, useRef, useState, type CSSProperties } from 'react';
import { juceHeight } from './fonts.ts';
import type { Box } from './layout.ts';

export type MenuItem =
  | { kind: 'header'; label: string }
  | { kind: 'separator' }
  | { kind: 'item'; label: string; ticked?: boolean; disabled?: boolean; onSelect?: () => void }
  | { kind: 'submenu'; label: string; ticked?: boolean; items: MenuItem[] };

// getIdealPopupMenuItemSize: the font's height * 1.3
const ITEM_HEIGHT = 17;
const SEPARATOR_HEIGHT = 10;
const BORDER = 2;

// Where a list goes, and where it goes instead when there's no room to the right
type Placement = { left: number; top: number; leftIfNoRoom: number };

function Tick() {
  return (
    <svg className="absolute left-0.5 top-1/2 -translate-y-1/2" width={13} height={13} viewBox="0 0 13 13" aria-hidden>
      <path d="M2.5 7 L5.5 10 L10.5 3" stroke="currentColor" strokeWidth={1.6} fill="none" />
    </svg>
  );
}

function Arrow() {
  return (
    <svg className="absolute right-1 top-1/2 -translate-y-1/2" width={6} height={8} viewBox="0 0 6 8" aria-hidden>
      <path d="M0 0 L6 4 L0 8 Z" fill="currentColor" />
    </svg>
  );
}

function MenuList({ items, placement, onClose }: { items: MenuItem[]; placement: Placement; onClose(): void }) {
  const list = useRef<HTMLDivElement>(null);
  const rows = useRef<(HTMLDivElement | null)[]>([]);
  const [box, setBox] = useState<Box | null>(null);
  const [open, setOpen] = useState<number | null>(null);

  // Kept inside the editor, as JUCE keeps menus on screen
  useLayoutEffect(() => {
    const element = list.current;
    const area = element?.offsetParent as HTMLElement | null;
    if (!element || !area) return;

    const { offsetWidth: width, offsetHeight: height } = element;
    const fitsRight = placement.left + width <= area.clientWidth;
    const left = Math.max(0, fitsRight ? placement.left : placement.leftIfNoRoom - width);
    const top = Math.max(0, Math.min(placement.top, area.clientHeight - height));
    setBox({ left, top, width, height });
  }, [placement.left, placement.top, placement.leftIfNoRoom]);

  const submenu = open === null ? null : items[open];
  const row = open === null ? null : rows.current[open];

  const style: CSSProperties = {
    left: box?.left ?? placement.left,
    top: box?.top ?? placement.top,
    visibility: box ? 'visible' : 'hidden',
    padding: `${BORDER}px 0`,
    fontSize: juceHeight(13),
  };

  return (
    <>
      <div ref={list} role="menu" className="absolute z-40 border border-fg/60 bg-background whitespace-nowrap text-fg shadow-lg" style={style}>
        {items.map((item, index) => {
          if (item.kind === 'separator') {
            return (
              <div key={index} className="flex items-center px-1" style={{ height: SEPARATOR_HEIGHT }}>
                <div className="h-px w-full bg-fg/30" />
              </div>
            );
          }

          if (item.kind === 'header') {
            return (
              <div key={index} className="flex items-center pr-4 pl-3 font-bold" style={{ height: ITEM_HEIGHT }}>
                {item.label}
              </div>
            );
          }

          const disabled = item.kind === 'item' && item.disabled;
          const highlighted = open === index;

          return (
            <div
              key={index}
              ref={(element) => void (rows.current[index] = element)}
              role="menuitem"
              aria-disabled={disabled}
              aria-haspopup={item.kind === 'submenu'}
              className={`relative mx-px flex items-center rounded-sm pr-5 ${disabled ? 'text-fg/30' : 'cursor-pointer hover:bg-thumb'} ${highlighted ? 'bg-thumb' : ''}`}
              style={{ height: ITEM_HEIGHT, paddingLeft: ITEM_HEIGHT }}
              onPointerEnter={(e) => {
                if (e.pointerType !== 'touch') setOpen(item.kind === 'submenu' ? index : null);
              }}
              onClick={() => {
                if (item.kind === 'submenu') setOpen(highlighted ? null : index);
                else if (!disabled) {
                  onClose();
                  item.onSelect?.();
                }
              }}
            >
              {item.ticked && <Tick />}
              {item.label}
              {item.kind === 'submenu' && <Arrow />}
            </div>
          );
        })}
      </div>
      {box && submenu?.kind === 'submenu' && row && (
        <MenuList
          key={open}
          items={submenu.items}
          placement={{ left: box.left + box.width, top: box.top + row.offsetTop - BORDER, leftIfNoRoom: box.left }}
          onClose={onClose}
        />
      )}
    </>
  );
}

export type MenuState = Omit<MenuProps, 'onClose'>;

interface MenuProps {
  items: MenuItem[];
  // The button it drops from, which toggles it rather than dismissing it
  target: Box;
  owner: HTMLElement | null;
  onClose(): void;
}

export function Menu({ items, target, owner, onClose }: MenuProps) {
  const root = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const outside = (e: PointerEvent) => {
      const node = e.target as Node;
      if (!root.current?.contains(node) && !owner?.contains(node)) onClose();
    };
    const escape = (e: KeyboardEvent) => {
      if (e.key === 'Escape') onClose();
    };

    document.addEventListener('pointerdown', outside);
    document.addEventListener('keydown', escape);
    return () => {
      document.removeEventListener('pointerdown', outside);
      document.removeEventListener('keydown', escape);
    };
  }, [owner, onClose]);

  // display: contents keeps the lists positioned against the editor
  return (
    <div ref={root} className="contents">
      <MenuList items={items} placement={{ left: target.left, top: target.top + target.height, leftIfNoRoom: target.left + target.width }} onClose={onClose} />
    </div>
  );
}
