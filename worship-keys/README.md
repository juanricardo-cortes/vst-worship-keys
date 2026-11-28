# Worship Keys

A Nord-style worship keys instrument (VST3 and standalone) for Windows. It has four layers (sampled grand piano, FM electric piano, analog pad, FM bells) feeding a built-in chain: EQ → compressor → soft clip → chorus → dotted-1/8 delay → shimmer reverb → limiter.

## Easy view and MIDI sliders
The plugin opens on the **EASY** page: eight big faders (Piano, E.Piano, Pad, Bells, Tone, Reverb, Delay, Master). **FULL** shows every knob.

To put a fader on a hardware slider, click **LEARN** under it and move the slider. On the Full page, right-click any knob and choose *MIDI Learn*. Right-click also offers *Clear MIDI mapping* and *Clear ALL MIDI mappings*. Mappings are saved to `%APPDATA%\WorshipKeys\midi-map.xml` and apply in every project and the standalone. The sustain pedal (CC 64), soft and sostenuto pedals (CC 66/67), and channel-mode messages (CC 120+) are never learned. Double-click a fader or knob to reset it.

Piano **Tone** works like a stage piano's "soft/dynamic" timbre. Lower settings are darker, harder playing opens the sound up, and the top notes stay clear. 100% switches the filter off.

## Presets
Voiced darker than the raw samples. Pad and piano levels are comparable, so a Pad level of -14 dB plays about 14 dB under the keys.

| Preset | Idea |
|---|---|
| Warm Stage Grand | The default: warm compressed grand, low pad bed, medium hall |
| Grand + Low Pad | Same grand with the pad tucked further down |
| Grand + Whisper Pad | Pad you feel more than hear, mostly in the sustain |
| Big Room Grand | A little brighter and wider, more reverb, dotted-1/8 delay, a touch of shimmer |
| Felt Night Piano | Very dark, soft touch, long reverb |
| Soft Prayer Keys | Dark piano, slow-swelling pad, long reverb |
| Soft Prayer - Wide Chorus | Soft Prayer Keys with heavy slow chorus and a wide stereo image |
| Soft Prayer - Huge Chorus Wall | Maximum chorus and width, octave-below string, louder pad, second hall |
| Soft Prayer - Deep | Adds the octave-below string and low body for weight |
| Soft Prayer - Cathedral | Long second hall and more shimmer |
| Soft Prayer - Close | Less reverb and delay, for talking or praying over the keys |
| Soft Prayer - Pad Forward | Pad up front, piano just under it |
| Soft Prayer - Octave Halo | Pad an octave up, more shimmer |
| Soft Prayer - EP Blend | A soft tine EP layered under the piano |
| Octave Halo (pad under) | Pad an octave up, quiet, shimmery |
| Modern Pop Grand | The brightest grand: more compression and presence |
| Upbeat Bell Grand | For upbeat songs: brighter grand, glassy bells an octave up, bright fast pad under it, dotted-1/8 delay |
| Upbeat Sparkle (bells forward) | Bells two octaves up and louder, bright octave-up pad, more delay and shimmer |
| Deep Mellow Grand | The settings from `nord-piano-patch-guide.html`, no pad |
| Piano Only - Warm Room | Warm grand, small room, no pad |
| Warm Tine EP | Mellow FM electric piano with chorus |
| Grand + EP Blend | Piano with the EP layered underneath |
| Ambient Pad Wash (pad forward) | The one pad-forward patch, for transitions |
| Studio Grand (dry, open) | No processing, tone filter off: the raw samples |
| Anthem Ballad Grand | Concert grand with a second grand stacked under it, wide pad, long hall: the big modern worship ballad piano |
| Concert Grand + Pad / Big Hall | The 9 ft concert grand, with a pad or on its own in a big hall |
| Wall of Grands | Concert + session grand stacked, the octave-down string and a pad: as big as it gets |
| Clear Grand + Warm Pad / Session Grand Ballad | The other two new pianos with a pad |

## Pianos
The **Piano** menu (Full page) picks the sampled piano; **Stack** plays a second piano on top of it; **Low Oct** adds an octave-down string.
The original grand loads at startup; the others load in the background the first time a patch uses one (about 250 MB of RAM each).

| Piano | Samples |
|---|---|
| Classic Grand | Salamander Grand Piano V3 (Yamaha C5) - `Samples/` |
| Concert Grand | bitKlavier Grand (Steinway D), Lip Cardioid mics - `Samples/Piano-ConcertD` |
| Clear Grand | Accurate-Salamander V6.2e - `Samples/Piano-Clear` |
| Session Grand | Headroom Piano (Yamaha C3), close + Decca mics - `Samples/Piano-StudioC3` |

## Build
Requires VS 2022 Build Tools (C++), CMake 3.22+, ffmpeg and Git Bash.

```
bash Tools/prepare_samples.sh                 # downloads Salamander (~400 MB) -> Samples/ (42 MB FLAC)
python Tools/prepare_pianos.py                # the other pianos, from their libraries in _download/ -> Samples/Piano-*/
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
./install.ps1                                 # samples -> ProgramData, VST3 -> Common Files\VST3 (UAC prompt)
build/WKRender_artefacts/Release/WKRender.exe # renders every preset to renders/*.wav, prints brightness + pad balance, stability test
build/WKRender_artefacts/Release/WKRender.exe --ui shots # editor screenshots + MIDI learn self-test
```

The plugin looks for samples in `C:\ProgramData\WorshipKeys\Samples`, then in a `Samples` folder next to the plugin, then in this repo's `Samples/` folder. It uses about 250 MB of RAM, shared across all instances.

## Credits
Piano samples (if you distribute the plugin, keep these credits; each is also in its folder's `CREDITS.txt`):
- **Salamander Grand Piano V3** by Alexander Holm, [CC-BY 3.0](http://creativecommons.org/licenses/by/3.0/). Retuning by Markus Fiedler.
- **bitKlavier Grand** (Steinway D, Lip Cardioid mic image), Princeton University: recorded by Andrés Villalta, played by Matt Wang, produced by Dan Trueman. [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/), https://doi.org/10.34770/xm18-yr83
- **Accurate-Salamander Grand Piano V6.2e** by Chisato Yamauchi, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
- **Headroom Piano** (Yamaha C3) by Bengt Nilsson, SFZ conversion by kinwie, [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
