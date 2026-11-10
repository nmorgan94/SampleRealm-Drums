// Mirrors Source/ui/About.cpp
import type { UiInfo } from './backend.ts';

export type About = Pick<UiInfo, 'version' | 'commit' | 'build'>;

export const aboutText = ({ version, build, commit }: About, running: string) =>
  [`Version ${version}`, `${build} build`, `Commit ${commit}`, running, 'WebView UI'].join('\n');
