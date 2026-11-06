import { readFileSync } from 'node:fs';
import { localEnginePath } from '../localEngine.ts';
import { paramLookup } from '../src/backend.ts';
import { instantiate, readUiInfo } from '../src/demo/engine.ts';

export const wasm = readFileSync(localEnginePath());

export const info = readUiInfo(instantiate(wasm));

export const param = paramLookup(new Map(info.params.map((p) => [p.id, p])));
