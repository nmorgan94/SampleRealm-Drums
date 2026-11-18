# SampleRealm: Drums

A drum synth with no samples. Each instance plays one instrument, and every MIDI note triggers it.

## Instrument

The switch at the top chooses Kick, Snare or Cymbal. Each instrument keeps its own settings, so you
can switch back and forth without losing anything. The instrument is saved with presets and sessions.

## Envelopes

The large display shows the instrument's envelopes over a preview of one hit.

- The kick has Pitch and Amp. The snare has Pitch, Body and Noise. The cymbal has Metal and Noise.
- Click a faded curve, or its dot in the legend, to edit it.
- Drag a node to move it. Double-click empty space to add one.
- Double-click or right-click a node to remove it.
- Drag the handle on a segment to bend it. Double-click the handle to straighten it.
- Cmd+Z undoes an edit and Cmd+Shift+Z redoes it. On Windows use Ctrl.

## Global

- Tune moves the pitch up or down by up to 24 semitones.
- Key Track lands the end of the pitch sweep on the played note.
- Length stretches every envelope in time, from 0.25x to 4x.
- Pitch Depth sets how far the pitch sweep reaches above where it ends.
- Vel Sens sets how much a note's velocity changes the hit. A softer note is quieter, and its click or snap is
  duller. At 0 % every note plays a full hit.

The cymbal has no pitch envelope, so Key Track and Pitch Depth are greyed out for it.

## Sound

The panels along the bottom change with the instrument.

- Kick: Sub is the body, with harmonics and a start phase. Click is the transient.
- Snare: Body is the tuned part, Noise is the wires and Snap is the transient.
- Cymbal: Metal is six detuned square waves through a band-pass. Tone sets the band and Ring narrows it.
  Noise adds the wash. Width, in Output, spreads the cymbal across left and right; the two still add up to the
  same sound in mono.
- Drive, EQ and Output are shared. Clip pushes the sound into a soft ceiling at 0 dB.

Drag a knob up or down to turn it. While you do, its name shows the value. Click the name to type a
value. Double-click the knob to reset it.

## Hit, Loop and Tail

The Hit pad plays the instrument's last note. Press it higher up for a harder hit: the top is velocity 127
and the bottom is 1.

The pad flashes on every hit, dimmer for a softer one. The number shows the last hit's velocity, whether it
came from the pad, the loop or a MIDI note. The bar on its left fills to the level the hit played at, after
Vel Sens, and the tick on it marks the velocity. At 100 % the bar meets the tick; at 0 % it is always full.

Loop plays the instrument's part of the pattern chosen above it, in time with the host.

- The patterns are Two-step, Halftime, Four-floor and Sixteenths. Each has a kick, a snare and a hat part; the cymbal
  plays the hat.
- The row of bars under the pad is the instrument's bar: sixteen steps in four beats. A bar fills to its step's
  velocity, an empty one is a rest, and the step playing is outlined.
- Click a step to turn it on or off. It comes back at the velocity it had, or at 127 if it was always a rest.
- Drag a step up or down to set its velocity, from 1 to 127. The number shows beside AUDITION while you drag.
- Each instrument keeps its own steps. Choosing a pattern replaces them all.
- To hear a kit, put a kick, a snare and a cymbal on three tracks and turn Loop on in each with the same pattern.
- While the host is stopped the loop keeps time by the computer's clock, at the host's tempo.
- Loop, its pattern and its steps aren't saved with presets or sessions.

TAIL at the top right shows the note and frequency the pitch settles on.

## Meter

The meter at the top right shows the output peak, left and right. The light above each turns on when the output goes
over 0 dB and stays on for a moment.

## Presets

- The arrows step through the presets, and the name opens the full list.
- Only the current instrument's presets are listed.
- A * after the name means the sound has changed since the preset was loaded.
- Save overwrites the current user preset. For a factory preset it asks for a name.
- The ... menu has Init Preset, Save As, Rename and Delete. In the plugin it also has Show Preset Folder.
- Init Preset keeps the instrument.
- Factory presets can't be renamed, deleted or overwritten.
