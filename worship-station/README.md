# Worship Station

One instrument with three sound packs:

| Pack | What it is |
|---|---|
| **KEYS** | Worship Keys (stage grand, pad, EP, bells): the same engine and presets as the Worship Keys plugin |
| **SYNTH** | Anthem Synth (supersaw pads, plucks, arps, pumping leads): the same engine and presets |
| **ORCHESTRA** | Praise Orchestra (new): heavy strings + brass stabs, swelling sustains, low brass, timpani, cymbal hits |

Builds a VST3 (FL Studio) and a standalone app.

## Switching packs from your keyboard

Open **CONTROLLER SETUP** (top right):

- **Pack selector wheel / knob**: press LEARN and turn the Arturia center wheel. One click moves one pack
  (Keys → Synth → Orchestra → Keys). The wheel type is detected automatically; change it in the box if needed:
  *Endless wheel (Arturia, 64 +/-)*, *Endless wheel (1 / 127)*, or *Knob / fader* (3 zones: low = Keys, middle = Synth, high = Orchestra).
- **Next / Previous / Keys / Synth / Orchestra buttons**: learn any button (CC), pad (note), or program change.
  Pad notes you learn here never play a sound.
- Arturia keyboards: use **User / Arturia mode, not DAW mode**, so the wheel and buttons send plain MIDI.
  If a button only works every other press, set it to *momentary / gate* in Arturia MIDI Control Center.
- The green "Last MIDI in" line shows exactly what your controller sends.

Controller settings are saved once for all projects in `%APPDATA%\WorshipStation\controller.xml`.

## Routing

- **One keyboard - switch packs** (default): the selected pack plays. Notes you're holding when you switch
  finish on the pack they started on. The sustain pedal reaches every pack.
- **Each pack on its own MIDI**: every pack listens to its own MIDI channel (default Keys = ch 1,
  Synth = ch 2, Orchestra = ch 3) and all three can play together. In the **standalone app** you can also
  give each pack its own MIDI **device** (e.g. Arturia → Keys, second keyboard → Orchestra).
  In FL Studio all keyboards arrive merged, so give each keyboard its own MIDI channel instead.

The small knob next to each pack button is that pack's volume. The bar under each button shows what's
sounding, including tails from a pack you switched away from.

## Orchestra pack

- Right hand: violins, violas, trumpets, horns, plus celli and trombones an octave below (OCT DOWN).
- Left hand, below the **L.H. SPLIT** key (default G2, the G below middle C): celli, basses, trombones, tuba and timpani.
- Velocity at or above **ACCENT AT** adds a cymbal crash and bass drum.
- Articulations: **Stabs**; **Sustain**; **Stab + Hold**, where a tap is a stab and holding the key brings the
  sustain in; **Marcato**, where stab and sustain start together for big held hits.
- Mod wheel swells the sustains (up to +6 dB). Expression (CC 11) scales the whole orchestra.
- Ten presets: Praise Stabs, Brass Hits, String Stabs, Full Orchestra Hit, Celebration Hits, Big Marcato,
  Anthem Swell, Strings Pad, Brass Section Swell, Timpani + Low Strings.

## Build / install

```
python Tools/prepare_orchestra.py          # once: downloads + converts the orchestra samples (~550 MB download, 159 MB FLAC)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DFETCHCONTENT_SOURCE_DIR_JUCE=../worship-keys/build/_deps/juce-src
cmake --build build --config Release
build\WSRender_artefacts\Release\WSRender.exe [--ui renders\ui]   # renders presets + runs switching/routing tests
powershell -ExecutionPolicy Bypass -File install.ps1
```

Keys and Synth are compiled directly from `../worship-keys` and `../anthem-synth`, so changes there show up
here on the next build. RAM: about 250 MB for the piano and 380 MB for the orchestra.

## Credits

- Orchestra samples: VS Chamber Orchestra 2 Community Edition, Versilian Studios, CC0 (public domain).
- Piano samples: Salamander Grand Piano V3 by Alexander Holm, CC-BY 3.0.
