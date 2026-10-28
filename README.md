# Worship Keys

A Nord-style worship keys instrument (VST3 and standalone) for Windows. It has three layers (sampled grand piano, FM electric piano, analog pad) feeding a built-in chain: EQ → compressor → soft clip → chorus → dotted-1/8 delay → shimmer reverb → limiter.

## Presets
| Preset | Idea |
|---|---|
| Elevation - Big Room Grand | Bright, compressed grand with a pad underneath, big hall, shimmer, dotted-1/8 delay |
| Hillsong - Bright & Compressed | Peter James-style bright/compressed piano with an "Analog V"-style detuned saw pad |
| Bethel - Intimate Felt | Darker, softer touch, warm low pad, long cloud-style reverb |
| Nord Deep Grand (FL chain) | The settings from `nord-piano-patch-guide.html`, built in |
| Ambient Shimmer Swell | Pad-forward, +1 octave pad, heavy shimmer for transitions and prayer moments |
| DX Worship EP | DX7-style bell EP with chorus |
| Grand + EP Layer | Piano with the EP tucked underneath for extra sparkle |
| Dry Grand | No processing, the raw samples |

The voicings are educated estimates from rig rundowns and worship-keys practice, not copies of the bands' actual patches.

## Build
Requires VS 2022 Build Tools (C++), CMake 3.22+, ffmpeg and Git Bash.

```
bash Tools/prepare_samples.sh                 # downloads Salamander (~400 MB) -> Samples/ (42 MB FLAC)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
./install.ps1                                 # samples -> ProgramData, VST3 -> Common Files\VST3 (UAC prompt)
build/WKRender_artefacts/Release/WKRender.exe # renders every preset to renders/*.wav + stability test
```

The plugin looks for samples in `C:\ProgramData\WorshipKeys\Samples`, then in a `Samples` folder next to the plugin, then in this repo's `Samples/` folder. It uses about 250 MB of RAM, shared across all instances.

## Credits
Piano samples: **Salamander Grand Piano V3** by Alexander Holm, [CC-BY 3.0](http://creativecommons.org/licenses/by/3.0/). Retuning by Markus Fiedler. If you distribute the plugin, keep this credit (it is also in `Samples/CREDITS.txt`).
