import { createRoot } from 'react-dom/client';
import { drumsBackend } from './pluginBackend.ts';
import { Editor } from '../Editor.tsx';
import './styles.css';

const root = createRoot(document.getElementById('root')!);

try {
  root.render(<Editor backend={drumsBackend()} />);
} catch (error) {
  console.error(error);
  root.render(<pre className="p-5 text-fg">{`The editor couldn't connect to the plugin.\n${error}`}</pre>);
}
