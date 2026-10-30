# Anthem Synth

A worship / anthem virtual-analog synthesizer for Windows (VST3 and standalone). It is voiced after the synth sounds of Elevation Worship, Hillsong and Planetshakers.

**Engine (16 voices):** OSC A (7-saw supersaw / saw / square / triangle) + OSC B + sub + noise → stereo 4-pole ladder filter with drive → filter and amp envelopes. It also has an LFO (vibrato / filter / PWM), poly, mono and legato modes with glide, and the mod wheel opens the filter ("swell").
**Motion:** tempo-synced pump (sidechain feel) or rhythmic gate (1/8, 1/16, trance pattern). It follows host tempo, or the Tempo knob when there is no host.
**Arp:** up, down, up/down, random, or chord stabs. The sustain pedal latches held notes.
**FX:** low cut, brightness, Juno-style chorus, ping-pong delay (dotted-eighth by default), FDN reverb with shimmer, and a limiter.

## Presets
| Preset | Idea |
|---|---|
| Elevation - Ambient Pad | Wide supersaw bed with an octave layer, slow swell, big shimmer hall |
| Elevation - Pulse Pad | 16th-note gated pad for builds |
| Elevation - Soaring Lead | Legato supersaw lead with vibrato and dotted delay |
| Elevation - Moog Bass | Round mono bass |
| Hillsong - Analog V Pad | Jupiter-style two-saw pad with a slow filter bloom |
| Hillsong - United Pluck | Plucks that turn into dotted-eighth grooves |
| Hillsong - Young & Free Pump | Bright pop supersaw chords pumping on the quarter note |
| Hillsong - Oceans Swell | Soft string-machine swell with long shimmer |
| Planetshakers - Our Time Anthem | "This Is Our Time"-style pumping stacked supersaw chords |
| Planetshakers - Our Time Pluck | Bright supersaw pluck for hook lines |
| Planetshakers - Turn It Up Lead | "Turn It Up"-style aggressive detuned lead with an octave layer |
| Planetshakers - Turn It Up Stabs | Hold a chord and the arp chops it in 1/8 notes |
| Planetshakers - Festival Arp | 1/16 two-octave arp |
| Planetshakers - Drive Bass | Driven mono saw bass that ducks with the beat |
| Build-Up Riser | Noise and saws that open up over about 8 bars |
| Prayer - Shimmer Cloud | Soft ambient cloud for prayer moments |
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
