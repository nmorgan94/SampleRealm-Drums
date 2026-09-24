# SampleRealm: Drums

A kick drum synthesizer for Drum & Bass producers

Every sound is synthesised, with no samples, so kicks can be tuned to the track's key and reshaped hit by hit.

## Features

### Sub
- Sine oscillator with a blend of 2nd and 3rd harmonics, which are dropped above Nyquist so they never alias
- Start phase from 0° to 90°, for a clean onset or a punchy click
- Level from −∞ to +6 dB

### Envelopes
- Breakpoint pitch and amp envelopes, with up to 32 nodes each and adjustable curvature per segment
- Pitch interpolated on a logarithmic scale, from 20 Hz to 12 kHz
- Length stretches both envelopes in time (0.25× to 4×)
- Pitch Depth scales how far the pitch sweep reaches above the tail (0 to 200%)

### Tuning
- Key Track, which lands the tail on the played MIDI note while the whole sweep follows
- Tune, ±24 semitones with fine steps
- Velocity sensitivity

### Click
- Synthesised transients in three modes: Noise (band-passed), Sweep (a falling sine) and Impulse (a single-cycle tick)
- Level, Tone, Decay and Pitch controls

### Drive, EQ & Clip
- Drive with four shapes (Soft, Hard, Fold and Tube), 4× oversampled so it doesn't alias, with a dry/wet mix
- 3-band EQ: an 80 Hz low shelf, a sweepable peak from 100 Hz to 5 kHz and a 6 kHz high shelf
- Soft-knee clipper that pushes up to +18 dB into a 0 dBFS ceiling
- DC blocker and output gain

### Playback
- Sample-accurate MIDI triggering
- Click-free retriggering: the previous hit fades out over 3 ms while the next one starts
- Trigger button for auditioning without MIDI

### Presets
- Factory presets built into the plugin
- Save, rename and delete your own presets
- Presets store the envelope curves as well as the parameters
- The preset name and unsaved-changes marker are restored with your DAW session

## Build Requirements

- CMake 3.25+
- A C++23-capable compiler
- Git
- macOS development environment for AU/Standalone/VST3 builds

On the first configure, CMake downloads JUCE and pluginval automatically via CPM.

**Debug Build:**
```bash
cmake --preset debug
cmake --build --preset debug
```

**Release Build:**
```bash
cmake --preset release
cmake --build --preset release
```

**Run the Standalone app:**
```bash
open build-debug/Drums_artefacts/Debug/Standalone/Drums.app
```

## Presets

Factory presets are `.srkick` files in `Assets/Presets/`. They're embedded into the plugin at build time along with everything else in `Assets/`.

To add one:
1. Design the sound in the plugin and save it as a user preset.
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
cmake -B build-xcode -G Xcode
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
ctest --preset debug                             # validates an existing build
```

Logs land in `build-debug/pluginval-logs/`. Strictness defaults to 10; use `-DPLUGINVAL_STRICTNESS=5` (range 1–10) for a faster run, or `-DENABLE_PLUGINVAL=OFF` to skip pluginval entirely.

The AU test validates the installed component in `~/Library/Audio/Plug-Ins/Components`. macOS resolves Audio Units through its registry rather than by path, so this test needs `COPY_PLUGIN_AFTER_BUILD` left on.

## Plugin Locations

**Build artefacts** (`Debug` or `Release`):
- Standalone: `build-debug/Drums_artefacts/Debug/Standalone/Drums.app`
- AU: `build-debug/Drums_artefacts/Debug/AU/Drums.component`
- VST3: `build-debug/Drums_artefacts/Debug/VST3/Drums.vst3`

**Installed** (macOS, via `COPY_PLUGIN_AFTER_BUILD`):
- VST3: `~/Library/Audio/Plug-Ins/VST3/Drums.vst3`
- AU: `~/Library/Audio/Plug-Ins/Components/Drums.component`
