# Anthem Synth

A worship / anthem virtual-analog synthesizer for Windows (VST3 and standalone). It is voiced for modern worship and anthem synth sounds.

**Engine (16 voices):** OSC A (7-saw supersaw / saw / square / triangle) + OSC B + sub + noise → stereo 4-pole ladder filter with drive → filter and amp envelopes. It also has an LFO (vibrato / filter / PWM), poly, mono and legato modes with glide, and the mod wheel opens the filter ("swell").
**Motion:** tempo-synced pump (sidechain feel) or rhythmic gate (1/8, 1/16, trance pattern). It follows host tempo, or the Tempo knob when there is no host.
**Arp:** up, down, up/down, random, or chord stabs. The sustain pedal latches held notes.
**FX:** low cut, brightness, Juno-style chorus, ping-pong delay (dotted-eighth by default), FDN reverb with shimmer, and a limiter.

## Presets
| Preset | Idea |
|---|---|
| Ambient Supersaw Pad | Wide supersaw bed with an octave layer, slow swell, big shimmer hall |
| 16th Pulse Pad | 16th-note gated pad for builds |
| Soaring Lead | Legato supersaw lead with vibrato and dotted delay |
| Round Analog Bass | Round mono bass |
| Analog Bloom Pad | Jupiter-style two-saw pad with a slow filter bloom |
| Dotted Echo Pluck | Plucks that turn into dotted-eighth grooves |
| Pop Pump Chords | Bright pop supersaw chords pumping on the quarter note |
| String Machine Swell | Soft string-machine swell with long shimmer |
| Anthem Pump Supersaw | Pumping stacked supersaw chords |
| Bright Hook Pluck | Bright supersaw pluck for hook lines |
| Hype Saw Lead | Aggressive detuned lead with an octave layer |
| Chopped Chord Stabs | Hold a chord and the arp chops it in 1/8 notes |
| Festival Arp | 1/16 two-octave arp |
| Driven Saw Bass | Driven mono saw bass that ducks with the beat |
| Instant Trance Saw | Zero-attack epic trance supersaw for fast upbeat songs (no pump, so notes hit on the beat) |
| Electro Hook Synth | Bouncy, driven supersaw hook for upbeat intros; instant attack |
| Build-Up Riser | Noise and saws that open up over about 8 bars |
| Shimmer Cloud | Soft ambient cloud for ambient moments |
| Init - Basic Saw | Starting point |

The voicings are educated estimates, not copies of the bands' patches.

## Build
Requires VS 2022 Build Tools (C++), CMake 3.22+, and Python with numpy and Pillow plus ffmpeg (for the promo only).
```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DFETCHCONTENT_SOURCE_DIR_JUCE=../worship-keys/build/_deps/juce-src
cmake --build build --config Release
./install.ps1                                             # VST3 -> Common Files\VST3 (UAC) + Start-menu shortcut
build/AnthemRender_artefacts/Release/AnthemRender.exe     # renders each preset to renders/ and runs a stability test
AnthemRender.exe --snapshot ui.png [preset]               # UI screenshot
```

## Promo video
```
python promo/compose.py        # writes the parts, renders them through the presets, mixes out/soundtrack.wav
python promo/render_video.py   # 9:16 1080x1920 -> promo/out/anthem-synth-ad.mp4 (--preview for stills)
```
