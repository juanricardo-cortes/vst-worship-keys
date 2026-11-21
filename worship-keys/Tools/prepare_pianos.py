#!/usr/bin/env python3
"""Builds the extra pianos in worship-keys/Samples/<folder>/ from their source libraries in _download/.

  Piano-ConcertD  Concert Grand  bitKlavier Grand (Steinway D), Lip Cardioid mics, Princeton University, CC BY 4.0
  Piano-Clear     Clear Grand    Accurate-Salamander Grand Piano V6.2e by Chisato Yamauchi, CC BY 4.0
  Piano-StudioC3  Session Grand  Headroom Piano (Yamaha C3) by Bengt Nilsson, CC BY 4.0 (close + Decca mics mixed)

Each piano keeps 5 velocity layers, at every minor third, as 44.1 kHz/16-bit FLAC capped at 12 s, named
"<midi>_v<layer>.flac" like the original grand. Every layer is then levelled so it plays as loud as the same layer
of the original grand (Samples/), so switching pianos never jumps in volume.
Needs: ffmpeg, numpy. Usage: python Tools/prepare_pianos.py [ConcertD|Clear|StudioC3 ...]
"""
import os, re, subprocess, sys, zipfile
from concurrent.futures import ThreadPoolExecutor
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DL = os.path.join(ROOT, "_download")
SAMPLES = os.path.join(ROOT, "Samples")
NOTE_INDEX = {"C": 0, "D#": 3, "F#": 6, "A": 9}
SALAMANDER_LAYERS = [2, 6, 10, 13, 16]   # the 16-layer libraries keep the same 5 layers the original grand does


def midi_of(name, octave):
    return 12 * (int(octave) + 1) + NOTE_INDEX[name]


def concert_d():
    src = os.path.join(DL, "bitKlavierGrand_LipCardioid_44k16b")
    jobs = []
    for f in os.listdir(src):
        clean = f.strip().rstrip("_")
        if clean != f:   # a CR from the download list can end up as a trailing "_" or whitespace
            os.replace(os.path.join(src, f), os.path.join(src, clean))
        # A0v1.wav, C4v16.wav, D_1v5(5).wav (= D#1), F_3v2(2).wav (= F#3)
        m = re.match(r"^(A|C|D_|F_)(\d)v(\d+)(\(\d+\))?\.wav$", clean)
        if not m:
            continue
        name = m.group(1).replace("_", "#")
        vel = int(m.group(3))
        if vel in SALAMANDER_LAYERS:
            jobs.append(([os.path.join(src, clean)], midi_of(name, m.group(2)), SALAMANDER_LAYERS.index(vel) + 1))
    return jobs, {}


def clear():
    folder = os.path.join(DL, "AccurateSalamander")
    if not os.path.isdir(folder):
        with zipfile.ZipFile(os.path.join(DL, "AccurateSalamander_flac.zip")) as z:
            z.extractall(folder)
    jobs, seen = [], {}
    for dirpath, _, files in os.walk(folder):
        for f in files:
            m = re.match(r"^(\d{3})_(A|C|D#|F#)\d+v(\d+)\.flac$", f)   # 021_A0v01.flac (its tune offsets are <= 4 cents)
            if m and int(m.group(3)) in SALAMANDER_LAYERS:
                key = (int(m.group(1)), SALAMANDER_LAYERS.index(int(m.group(3))) + 1)
                seen.setdefault(key, os.path.join(dirpath, f))   # first copy wins if the zip has duplicates
    for (midi, layer), path in seen.items():
        jobs.append(([path], midi, layer))
    return jobs, {}


def studio_c3():
    src = os.path.join(DL, "HeadroomPiano", "Samples")
    jobs = []
    for f in os.listdir(src):
        m = re.match(r"^HEADROOM PIANO LEVEL(\d) CLOSE (\d+)\.flac$", f)
        if m:
            decca = os.path.join(src, f.replace(" CLOSE ", " DECCA "))
            jobs.append(([os.path.join(src, f), decca], int(m.group(2)), int(m.group(1))))
    return jobs, {}


PIANOS = {
    "ConcertD": ("Piano-ConcertD", concert_d,
                 "Concert Grand: the bitKlavier Grand (Steinway D), Lip Cardioid mic image.\n"
                 "Recorded by Andres Villalta, played by Matt Wang, produced by Dan Trueman, Princeton University.\n"
                 "License: Creative Commons Attribution 4.0 (https://creativecommons.org/licenses/by/4.0/)\n"
                 "Source: https://doi.org/10.34770/xm18-yr83\n"),
    "Clear": ("Piano-Clear", clear,
              "Clear Grand: Accurate-Salamander Grand Piano V6.2e by Chisato Yamauchi,\n"
              "based on the Salamander Grand Piano by Alexander Holm.\n"
              "License: Creative Commons Attribution 4.0 (https://creativecommons.org/licenses/by/4.0/)\n"
              "Source: https://www.ir.isas.jaxa.jp/~cyamauch/AccurateSalamander/\n"),
    "StudioC3": ("Piano-StudioC3", studio_c3,
                 "Session Grand: Headroom Piano (Yamaha C3) by Bengt Nilsson, SFZ conversion by kinwie.\n"
                 "License: Creative Commons Attribution 4.0 (https://creativecommons.org/licenses/by/4.0/)\n"
                 "Source: https://github.com/sfzinstruments/BengtNilsson.HeadroomPiano\n"),
}


def convert(job, out_dir):
    inputs, midi, layer = job
    out = os.path.join(out_dir, "%03d_v%d.flac" % (midi, layer))
    cmd = ["ffmpeg", "-v", "error", "-y"]
    for i in inputs:
        cmd += ["-i", i]
    trim = "aresample=44100,afade=t=out:st=9.5:d=2.5,atrim=0:12"
    if len(inputs) == 2:   # close mics + the Decca pair 6 dB down, for size without losing focus
        filt = "[0:a][1:a]amix=inputs=2:weights=1 0.5:normalize=0," + trim
    else:
        filt = trim
    cmd += ["-filter_complex", filt, "-ac", "2", "-sample_fmt", "s16", "-c:a", "flac", "-compression_level", "8", out]
    subprocess.run(cmd, check=True)
    return out


def loudness_db(path, seconds=1.5):
    """RMS of the first 1.5 s (the strike and early sustain), in dB."""
    raw = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-t", str(seconds), "-ac", "1", "-f", "f32le", "-"],
                         check=True, capture_output=True).stdout
    x = np.frombuffer(raw, dtype=np.float32)
    return 10 * np.log10(np.mean(x * x) + 1e-12)


def apply_gain(path, db):
    tmp = path + ".tmp.flac"
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", path, "-af", "volume=%.2fdB" % db,
                    "-sample_fmt", "s16", "-c:a", "flac", "-compression_level", "8", tmp], check=True)
    os.replace(tmp, path)


def peak_db(path):
    raw = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-f", "f32le", "-"], check=True, capture_output=True).stdout
    x = np.frombuffer(raw, dtype=np.float32)
    return 20 * np.log10(float(np.max(np.abs(x))) + 1e-12)


def level_match(out_dir):
    """Per layer: the median difference to the original grand over the middle of the keyboard, applied to the layer.
    If that would clip, the whole piano is stored lower and the rest goes in level.txt (the engine adds it back)."""
    ref_notes = [n for n in range(45, 85, 3)]
    gains = {}
    for layer in range(1, 6):
        diffs = []
        for n in ref_notes:
            ours, ref = os.path.join(out_dir, "%03d_v%d.flac" % (n, layer)), os.path.join(SAMPLES, "%03d_v%d.flac" % (n, layer))
            if os.path.exists(ours) and os.path.exists(ref):
                diffs.append(loudness_db(ref) - loudness_db(ours))
        gains[layer] = float(np.median(diffs)) if diffs else 0.0
    files = [f for f in os.listdir(out_dir) if f.endswith(".flac")]
    with ThreadPoolExecutor(8) as pool:
        peaks = dict(zip(files, pool.map(lambda f: peak_db(os.path.join(out_dir, f)), files)))
    loudest = max(peaks[f] + gains[int(f[-6])] for f in files)
    offset = min(0.0, -1.0 - loudest)   # keep 1 dB of headroom in the files
    with ThreadPoolExecutor(8) as pool:
        list(pool.map(lambda f: apply_gain(os.path.join(out_dir, f), gains[int(f[-6])] + offset), files))
    with open(os.path.join(out_dir, "level.txt"), "w") as f:
        f.write("%.2f\n" % -offset)
    return gains, -offset


def build(key):
    folder, collect, credits = PIANOS[key]
    out_dir = os.path.join(SAMPLES, folder)
    os.makedirs(out_dir, exist_ok=True)
    for f in os.listdir(out_dir):
        os.remove(os.path.join(out_dir, f))
    jobs, _ = collect()
    if not jobs:
        sys.exit("no source samples found for " + key)
    print("%s: converting %d samples..." % (key, len(jobs)), flush=True)
    with ThreadPoolExecutor(8) as pool:
        list(pool.map(lambda j: convert(j, out_dir), jobs))
    gains, playback = level_match(out_dir)
    print("  layer gains vs the original grand: " + ", ".join("v%d %+.1f dB" % (k, v) for k, v in gains.items())
          + "  (playback gain %+.1f dB in level.txt)" % playback)
    with open(os.path.join(out_dir, "CREDITS.txt"), "w") as f:
        f.write(credits + "Trimmed to 5 velocity layers, faded at 12 s, level-matched and converted to FLAC for Worship Keys.\n")
    size = sum(os.path.getsize(os.path.join(out_dir, f)) for f in os.listdir(out_dir))
    print("  done: %d samples, %.0f MB" % (len([f for f in os.listdir(out_dir) if f.endswith(".flac")]), size / 1e6))


if __name__ == "__main__":
    for key in sys.argv[1:] or list(PIANOS):
        build(key)
