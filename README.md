# SampleRealm: Drums

A drum synthesizer for producers

Every sound is synthesised, with no samples, so kicks and snares can be tuned to the track's key and every drum can be reshaped hit by hit.

Each instance plays one instrument, chosen with the switch at the top: **Kick**, **Snare** or **Cymbal**. The instrument is saved with presets and DAW sessions. Switching keeps every instrument's settings, so you can flip back and forth without losing anything.

## Features

### Kick
- **Sub:** a sine oscillator with a blend of 2nd and 3rd harmonics. The harmonics are dropped above Nyquist, so they never alias.
  - Start phase from 0° to 90°, for a clean onset or a punchy click
  - Level from −∞ to +6 dB
- **Click:** a synthesised transient in three modes:
  - Noise: band-passed noise
  - Sweep: a falling sine
  - Impulse: a single-cycle tick

  It has Level, Tone, Decay and Pitch controls.
- **Envelopes:** Pitch (20 Hz to 12 kHz) and Amp

### Snare
- **Body:** a tuned oscillator with harmonics, with its own pitch sweep
- **Noise:** band-limited noise for the wires, with Level, Low Cut and High Cut
- **Snap:** the same transient generator as the kick's Click, tuned for a snare's crack
- **Envelopes:** Pitch (40 Hz to 8 kHz), Body and Noise, so the tone and the wires can decay separately

### Cymbal
One engine for closed hats, open hats and rides. The difference between them is how you shape it.
- **Metal:** six anti-aliased square waves at the 808's inharmonic ratios, through a band-pass
  - **Tone** sets the centre of the band, from 2 to 16 kHz.
  - **Ring** narrows the band, from trashy and broad to a bell-like ping.
- **Noise:** band-limited noise, with Level, Low Cut and High Cut
- **Envelopes:** Metal and Noise, each up to 4 s long. For example:
  - Short envelopes give a closed hat.
  - Longer ones give an open hat.
  - A long Metal envelope with a short Noise tick gives a ride.
- It has no pitch envelope, so it follows Tune but not the played note. Key Track and Pitch Depth are greyed out.

### Envelopes
- Breakpoint envelopes with up to 32 nodes each and adjustable curvature per segment
- Pitch envelopes interpolate on a logarithmic scale
- A waveform preview of the hit is drawn under the curve as you edit

### Global
- **Tune:** ±24 semitones with fine steps
- **Key Track** (kick and snare): lands the tail of the pitch sweep on the played MIDI note, and the whole sweep follows. The **Tail** readout shows the note and frequency it settles on.
- **Length:** stretches every envelope in time (0.25× to 4×), and the Click and Snap decays with them.
- **Pitch Depth** (kick and snare): scales how far the pitch sweep reaches above the tail (0 to 200%)

### Drive, EQ & Clip
Shared by all three instruments:
- Drive with four shapes (Soft, Hard, Fold and Tube), 4× oversampled so it doesn't alias, with a dry/wet mix
- 3-band EQ: an 80 Hz low shelf, a sweepable peak from 100 Hz to 5 kHz and a 6 kHz high shelf
- Soft-knee clipper that pushes up to +18 dB into a 0 dBFS ceiling
- DC blocker and output gain

### Playback
- Sample-accurate MIDI triggering
- Click-free retriggering: the previous hit fades out over 3 ms while the next one starts. A closed hat therefore cuts off a ringing open hat.
- Switching instrument fades out any hit that's still ringing
- A Hit pad for auditioning without MIDI
- Mono output, copied to every output channel

### Presets
- Factory presets built into the plugin
- The browser and ◀ ▶ show only the current instrument's presets
- **Init Preset** in the ⋯ menu resets the sound to its defaults, keeping the instrument
- Save, rename and delete your own presets
- Presets store the envelope curves as well as the parameters
- The preset name and unsaved-changes marker are restored with your DAW session

## System Requirements

- **macOS** 12 or later: VST3, AU and Standalone
- **Windows** 10 or later, 64-bit: VST3 and Standalone

## Build Requirements

- CMake 3.25+
- A C++23-capable compiler:
  - **macOS**: Xcode or its Command Line Tools (the `xcode` preset needs the full Xcode app)
  - **Windows**: Visual Studio 2022 or later with the "Desktop development with C++" workload
- Git

On the first configure, CMake downloads JUCE, pluginval and Catch2 automatically via CPM, into `.cpm-cache/`. The wasm build keeps its patched JUCE there too, beside the clean one.

## Commands

| Task | Command |
|---|---|
| Build the plugin (Debug) | `cmake --workflow --preset debug` |
| Build the plugin (Release) | `cmake --workflow --preset release` |
| Run the unit tests | `cmake --workflow --preset unit` |
| Run the unit tests and pluginval | `cmake --workflow --preset test` |
| Run all tests against a Release build | `cmake --workflow --preset test-release` |
| Build the plugin with the WebView editor (Release) | `cmake --workflow --preset webview` |
| Launch the Standalone app (Debug) | `cmake --build --preset run` |
| Install the plugin for your DAW: **macOS** | Automatic on every build |
| Install the plugin for your DAW: **Windows** | `cmake --build --preset install-windows`, from an administrator prompt |
| Open in an IDE: **macOS** | `cmake --preset xcode`, then open `build-xcode/Drums.xcodeproj` |
| Open in an IDE: **Windows** | Open `build-debug\Drums.sln` in Visual Studio after a Debug workflow |

`run` and `install-windows` use the Debug build, so run `cmake --workflow --preset debug` once first.

## Manual

`Assets/Manual.md` is what **... → Manual** shows, in the native editor, the WebView editor and the
demo. Only this much Markdown is read:

- `# ` heading, `## ` subheading, `- ` bullet
- Any other line is text, joined to the paragraph or bullet above until a blank line
- No inline formatting, links or images

`Source/ui/Manual.cpp` and `webui/src/manual.ts` parse it; their tests share one sample, so change both together.

## Presets

Factory presets are `.srdrum` files in `Assets/Presets/`. They're embedded into the plugin at build time along with everything else in `Assets/`.

Each preset has a `type` attribute naming its instrument (`Kick`, `Snare` or `Cymbal`), and is only listed while that instrument is showing.

To add one:
1. Design the sound in the plugin and save it as a user preset. Its type is set from the instrument you saved it on.
2. Copy the file from the user folder into `Assets/Presets/`.
3. Reconfigure and rebuild.

User presets are saved to:
- **macOS**: `~/Library/Application Support/SampleRealm/Drums/Presets/`
- **Windows**: `%APPDATA%\SampleRealm\Drums\Presets\`

A preset is the plugin's full state, parameters plus envelope curves, stored as XML. If a preset doesn't include a parameter, that parameter loads at its default, so older presets keep working as parameters are added.

## Debugging in Xcode

To debug the plugin in Xcode with an executable:

### 1. Generate Xcode Project

```bash
cmake --preset xcode
open build-xcode/Drums.xcodeproj
```

### 2. Configure Debugging

1. Select your plugin target from the scheme dropdown
2. Go to **Product → Scheme → Edit Scheme**
3. Click **Run** on the left sidebar
4. Under **Executable**, choose **Other** and navigate to executable.

### 3. Build and Run

1. Press **Cmd+B** to build the plugin
2. Press **Cmd+R** to run with AudioPluginHost
3. Load your plugin in AudioPluginHost

## Validating the Plugin

[pluginval](https://github.com/Tracktion/pluginval) loads the built plugin as a host would and tests it for stability. It is built from source on demand, so there is nothing to install.

```bash
cmake --build --preset debug --target validate   # builds, then validates
```

Logs land in `build-debug/pluginval-logs/`. Strictness defaults to 10; use `-DPLUGINVAL_STRICTNESS=5` (range 1–10) for a faster run, or `-DENABLE_PLUGINVAL=OFF` to skip pluginval entirely.

The AU test validates the installed component in `~/Library/Audio/Plug-Ins/Components`. macOS resolves Audio Units through its registry rather than by path, so this test needs `PLUGIN_COPY_AFTER_BUILD` left on.

## Unit Tests

Catch2 tests in `Tests/` drive the processor directly: bus layouts, state save and restore, and finite output across block sizes and settings (see [Commands](#commands) to run them). Add a `.cpp` file to `Tests/` and reconfigure to pick it up.

- `ParameterSnapshotTests.cpp` pins every parameter's id, range, default and text, which saved sessions rely on. Change it only on purpose.
- `GoldenTests.cpp` checks rendered hits against `Tests/fixtures/golden.json`, which the wasm engine's test also checks. After a deliberate change to the sound, re-record it with `build-debug/Drums_Tests "[.write-golden]"`.

## CI

[`.github/workflows/ci.yml`](.github/workflows/ci.yml) builds the Release plugin on macOS and Windows and runs the unit tests. It only runs when started by hand: **Actions → CI → Run workflow**, or `gh workflow run ci.yml`. Tick **Also run pluginval** (or pass `-f pluginval=true`) to validate the plugin too.

Each run lists the plugins under **Artifacts** for 30 days: `Drums-macOS.tar.gz` (VST3 and AU) and `Drums-Windows.tar.gz` (VST3). They are unsigned.

## Web Demo and WebView Editor

Needs Emscripten (`brew install emscripten`) and Node 24+.

| Task | Command |
|---|---|
| Build and test the demo (engine and UI tests) | `cd wasm && cmake --workflow --preset wasm` |
| Serve the demo with live reload at http://localhost:8099 | `cd webui && npm run serve` |
| Typecheck and test the UI on their own | `cd webui && npm run typecheck && npm test` |
| Build the plugin with the WebView editor | `cmake --workflow --preset webview` |

- The demo builds to `wasm/build-wasm/SampleRealm-Drums-<version>/`, named from `COMPANY_NAME`, `project()` and `VERSION` in `CMakeLists.txt`. Upload that folder; the site embeds its `index.html` in an iframe. [`wasm.yml`](.github/workflows/wasm.yml) builds the same folder on GitHub as an artifact of that name, run by hand.
- The demo plays from the on-screen keyboard, the computer keyboard (A to ' from C1) or a MIDI keyboard, and the Hit pad. Its user presets stay in the browser.
- The `webview` build installs over the native one; run the `release` workflow to switch back.
- Delete `wasm/patches/juce-wasm.patch` once JUCE fixes its wasm support.

## Plugin Locations

**Build artefacts** (`Debug` or `Release`; on Windows the Standalone is `Drums.exe` and there is no AU):
- Standalone: `build-debug/Drums_artefacts/Debug/Standalone/Drums.app`
- AU: `build-debug/Drums_artefacts/Debug/AU/Drums.component`
- VST3: `build-debug/Drums_artefacts/Debug/VST3/Drums.vst3`

**Installed on macOS** (automatically after every build, unless `-DPLUGIN_COPY_AFTER_BUILD=OFF`):
- VST3: `~/Library/Audio/Plug-Ins/VST3/Drums.vst3`
- AU: `~/Library/Audio/Plug-Ins/Components/Drums.component`

**Installed on Windows** (with the `install-windows` preset):
- VST3: `C:\Program Files\Common Files\VST3\Drums.vst3`
