// Source/presets/PresetManager.cpp's rules, with user presets in the browser instead of files
import { isSamePreset, type EnvelopeNode, type PresetBackend, type PresetEntry, type PresetStatus } from '../backend.ts';

export type Values = Record<string, number>;
export type Envelopes = Record<string, EnvelopeNode[]>;
// type is the instrument a preset is for, or empty for any
export type StoredPreset = PresetEntry & { type: string; values: Values; envelopes: Envelopes };
export type PresetStorage = Pick<Storage, 'getItem' | 'setItem'>;

// Each plugin's demo shares the site's storage
export const STORAGE_KEY = 'samplerealm-drums-presets';

const INIT: PresetEntry = { name: 'Init', category: '', isFactory: true };

const decode = (text: string) =>
  text.replace(/&(quot|apos|lt|gt|amp);/g, (_, entity: string) => ({ quot: '"', apos: "'", lt: '<', gt: '>', amp: '&' })[entity]!);

const attributes = (tag: string) =>
  Object.fromEntries([...tag.matchAll(/([\w:-]+)="([^"]*)"/g)].map(([, key, value]) => [key, decode(value!)]));

const tags = (xml: string, name: string) => [...xml.matchAll(new RegExp(`<${name}\\s[^>]*>`, 'g'))].map(([tag]) => attributes(tag));

// Reads the files PresetManager writes; there's no DOMParser in node
export function parsePreset(xml: string): StoredPreset {
  const preset = tags(xml, 'PRESET')[0];
  if (!preset) throw new Error('Not a preset');

  const values = Object.fromEntries(tags(xml, 'PARAM').map(({ id, value }) => [id, Number(value)]));

  const envelopes = Object.fromEntries(
    [...xml.matchAll(/<ENV\s[^>]*>([\s\S]*?)<\/ENV>/g)].map(([env, body]) => [
      attributes(env.slice(0, env.indexOf('>') + 1)).id ?? '',
      tags(body!, 'NODE').map(({ t, v, c }) => ({ time: Number(t), value: Number(v), curve: Number(c) })),
    ])
  );

  return { name: preset.name ?? '', category: preset.category ?? '', type: preset.type ?? '', isFactory: true, values, envelopes };
}

// juce::String::compareNatural
const collator = new Intl.Collator(undefined, { numeric: true, sensitivity: 'base' });

const sorted = <T extends PresetEntry>(presets: T[]) =>
  [...presets].sort((a, b) => collator.compare(a.category, b.category) || collator.compare(a.name, b.name));

// juce::File::createLegalFileName
const legalName = (name: string) => name.trim().replace(/["#@,;:<>*^|?\\/]/g, '');

// juce::String::equalsIgnoreCase, and macOS filenames
const sameName = (a: string, b: string) => a.toLowerCase() === b.toLowerCase();

interface Options {
  factory: StoredPreset[];
  storage?: PresetStorage;
  defaults: Values;
  // The parameter that types presets, as PresetManager::Config::typeParameterId
  typeParameter: string;
  // The current type's name, e.g. "Kick"
  currentType(): string;
  values(): Values;
  envelopes(): Envelopes;
  // Every value and every envelope; missing ones are at their defaults already
  apply(values: Values, envelopes: Envelopes): void;
}

export type LocalPresets = PresetBackend & { markModified(): void };

export function localPresets({ factory, storage, defaults, typeParameter, currentType, values, envelopes, apply }: Options): LocalPresets {
  const factoryPresets = sorted(factory);
  let users: StoredPreset[] = [];
  let status: PresetStatus = { current: INIT, modified: false };
  const listeners = new Set<(status: PresetStatus) => void>();

  const read = () => {
    try {
      const stored = JSON.parse(storage?.getItem(STORAGE_KEY) ?? '[]') as StoredPreset[];
      users = sorted(stored.map((preset) => ({ ...preset, type: preset.type ?? '', envelopes: preset.envelopes ?? {}, isFactory: false })));
    } catch {
      // Unreadable storage keeps what's already listed
    }
  };

  const write = () => {
    try {
      storage?.setItem(STORAGE_KEY, JSON.stringify(users.map(({ name, category, type, values, envelopes }) => ({ name, category, type, values, envelopes }))));
    } catch {
      // Private windows can refuse storage; the presets last until the page closes
    }
  };

  const setStatus = ({ name, category, isFactory }: PresetEntry, modified: boolean) => {
    status = { current: { name, category, isFactory }, modified };
    listeners.forEach((listener) => listener(status));
  };

  // Only presets of the current type, or of none, are listed
  const all = () => {
    const type = currentType();
    return [...factoryPresets, ...users].filter((preset) => !preset.type || preset.type === type);
  };

  const find = (preset: PresetEntry) => all().find((candidate) => isSamePreset(candidate, preset));
  const findUser = (name: string) => users.find((preset) => sameName(preset.name, name));

  const checkName = (name: string) => {
    if (!name) return 'Enter a preset name.';
    const factoryMatch = factoryPresets.find((preset) => sameName(preset.name, name));
    return factoryMatch ? `"${factoryMatch.name}" is a factory preset. Choose another name.` : null;
  };

  // Anything missing from a preset loads at its default
  const load = (preset: StoredPreset) => {
    apply({ ...defaults, ...preset.values }, preset.envelopes);
    setStatus(preset, false);
  };

  const markModified = () => {
    if (!status.modified) setStatus(status.current, true);
  };

  // Not in this list (after Init, or another type's preset): start from either end
  const step = async (delta: number) => {
    read();
    const presets = all();
    if (presets.length === 0) return;

    const index = presets.findIndex((preset) => isSamePreset(preset, status.current));
    if (index < 0) load(delta > 0 ? presets[0]! : presets[presets.length - 1]!);
    else load(presets[(index + delta + presets.length) % presets.length]!);
  };

  read();

  return {
    status: () => status,
    onStatus: (listener) => {
      listeners.add(listener);
      return () => listeners.delete(listener);
    },
    list: async () => {
      read();
      return all();
    },
    load: async (preset) => {
      const found = find(preset);
      if (found) load(found);
    },
    // Init keeps the type
    loadInit: async () => load({ ...INIT, type: '', values: { [typeParameter]: values()[typeParameter]! }, envelopes: {} }),
    loadNext: () => step(1),
    loadPrevious: () => step(-1),
    exists: async (name) => findUser(legalName(name)) !== undefined,
    save: async (name, category) => {
      const trimmed = legalName(name);
      const error = checkName(trimmed);
      if (error) return error;

      const preset: StoredPreset = { name: trimmed, category, type: currentType(), isFactory: false, values: values(), envelopes: envelopes() };
      users = sorted([...users.filter((user) => !sameName(user.name, trimmed)), preset]);
      write();
      setStatus(preset, false);
      return null;
    },
    rename: async (preset, newName) => {
      const trimmed = legalName(newName);
      const target = preset.isFactory ? undefined : findUser(preset.name);
      if (!target) return 'Only user presets can be renamed.';

      const error = checkName(trimmed);
      if (error) return error;

      const existing = findUser(trimmed);
      if (existing && existing !== target) return `A preset called "${trimmed}" already exists.`;

      users = sorted(users.map((user) => (user === target ? { ...user, name: trimmed } : user)));
      write();

      if (isSamePreset(status.current, target)) setStatus({ ...status.current, name: trimmed }, status.modified);
      return null;
    },
    remove: async (preset) => {
      const target = preset.isFactory ? undefined : findUser(preset.name);
      if (!target) return 'Only user presets can be deleted.';

      users = users.filter((user) => user !== target);
      write();

      if (isSamePreset(status.current, target)) markModified();
      return null;
    },
    markModified,
  };
}
