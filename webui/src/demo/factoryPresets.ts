// The plugin's own factory presets, as the files' text
const files = import.meta.glob<string>('../../../Assets/Presets/*.srdrum', { query: '?raw', import: 'default', eager: true });

export const factoryPresets = Object.values(files);
