#!/usr/bin/env python3
"""Builds worship-station/Samples/Orchestra from VS Chamber Orchestra 2: Community Edition.

  VSCO 2 CE by Versilian Studios (Sam Gossner, Simon Dalzell, Elan Hickler) - CC0 1.0 (public domain).
  https://github.com/sgossner/VSCO-2-CE

Reads each instrument's .sfz (SFZ branch) for key / velocity / round-robin mapping, downloads only
the WAVs we keep, trims + fades them, peak-normalises to -1 dBFS and converts to 16-bit 44.1 kHz FLAC.
The normalisation gain is undone at playback (map.txt "gainDb"), so the sections keep their natural
balance while the 16-bit files use their full resolution.

Output per layer:  Samples/Orchestra/<Layer>/<n>.flac  +  map.txt
  map.txt line:     <file> <root> <lokey> <hikey> <lovel> <hivel> <rr> <gainDb>
Needs: python 3, ffmpeg on PATH.
"""
import concurrent.futures as cf
import os
import re
import subprocess
import sys
import urllib.parse
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DL = ROOT / "_download" / "vsco"
OUT = ROOT / "Samples" / "Orchestra"
RAW = "https://raw.githubusercontent.com/sgossner/VSCO-2-CE"

# layer name -> (sfz, max round robins, seconds kept, sample filter)
LAYERS = {
    "VlnStac": ("ViolinEnsSpic", 2, 2.5, None),
    "VlaStac": ("ViolaEnsSpic", 2, 2.5, None),
    "VcStac":  ("CelloEnsSpic", 2, 2.5, None),
    "CbStac":  ("ContrabassSpic", 2, 2.5, None),
    "VlnSus":  ("ViolinEnsSusVib", 1, 8.0, None),
    "VlaSus":  ("ViolaEnsSusVib", 1, 8.0, None),
    "VcSus":   ("CelloEnsSusVib", 1, 8.0, None),
    "CbSus":   ("ContrabassSusVB", 1, 8.0, None),
    "TptStac": ("TrumpetStac", 2, 2.5, None),
    "HrnStac": ("FHornStac", 2, 2.5, None),
    "TbnStac": ("TromboneStac", 2, 2.5, None),
    "TubStac": ("TubaStac", 2, 2.5, None),
    "TptSus":  ("TrumpetSus", 1, 8.0, None),
    "HrnSus":  ("FHornSus", 1, 8.0, None),
    "TbnSus":  ("TromboneSus", 1, 8.0, None),
    "TubSus":  ("TubaSus", 1, 8.0, None),
    "Timp":    ("Timpani", 2, 6.0, None),
    "Crash":   ("GM-StylePerc", 2, 7.0, r"^cymbal-crash1_(mf|ff)_"),
    "Boom":    ("GM-StylePerc", 2, 4.0, r"^BDrumNewhit_v[4-7]_"),
}


def fetch(url: str, dest: Path):
    if dest.exists() and dest.stat().st_size > 0:
        return dest
    dest.parent.mkdir(parents=True, exist_ok=True)
    tmp = dest.with_suffix(dest.suffix + ".part")
    with urllib.request.urlopen(url, timeout=120) as r, open(tmp, "wb") as f:
        f.write(r.read())
    tmp.replace(dest)
    return dest


def raw_url(branch: str, path: str) -> str:
    return f"{RAW}/{branch}/" + urllib.parse.quote(path)


def parse_sfz(text: str):
    """Minimal SFZ reader: <control>/<global>/<group>/<region> with opcode inheritance."""
    control, glob, group, regions = {}, {}, {}, []
    current, header = None, None
    text = re.sub(r"//.*", "", text)
    for tok in re.findall(r"<\w+>|[a-z_]+=[^<\n]*?(?=\s+[a-z_]+=|\s*<|\s*$)", text, re.M):
        if tok.startswith("<"):
            header = tok
            if header == "<group>":
                group = {}
            elif header == "<region>":
                current = {}
                regions.append(current)
            continue
        k, v = tok.split("=", 1)
        v = v.strip()
        {"<control>": control, "<global>": glob, "<group>": group}.get(header, current if header == "<region>" else {})[k] = v
        if header == "<region>":
            current["_group"] = dict(group)
    out = []
    for r in regions:
        merged = {**glob, **r.pop("_group"), **r}
        merged["_path"] = control.get("default_path", "").replace("\\", "/")
        out.append(merged)
    return out


def rr_index(r) -> int:
    if "seq_position" in r:
        return int(r["seq_position"])
    if "lorand" in r:
        return 1 + int(float(r["lorand"]) * 2 + 1e-6)  # 0.0 -> 1, 0.5 -> 2
    return 1


def peak_db(path: Path) -> float:
    res = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", str(path), "-af", "volumedetect", "-f", "null", "-"],
                         capture_output=True, text=True)
    m = re.search(r"max_volume:\s*(-?[\d.]+) dB", res.stderr)
    return float(m.group(1)) if m else 0.0


def convert(src: Path, dst: Path, seconds: float) -> float:
    """Trim + fade + normalise to -1 dBFS. Returns the gain (dB) applied."""
    gain = -1.0 - peak_db(src)
    fade = min(1.5, seconds * 0.25)
    af = f"atrim=0:{seconds},afade=t=out:st={seconds - fade}:d={fade},volume={gain}dB"
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", str(src), "-af", af, "-ar", "44100", "-ac", "2",
                    "-sample_fmt", "s16", "-c:a", "flac", "-compression_level", "8", str(dst)], check=True)
    return gain


def main():
    jobs = []  # (layer, region, src path)
    for layer, (sfz, max_rr, seconds, flt) in LAYERS.items():
        sfz_file = fetch(raw_url("SFZ", sfz + ".sfz"), DL / "sfz" / (sfz + ".sfz"))
        for r in parse_sfz(sfz_file.read_text(errors="replace")):
            name = r["sample"].replace("\\", "/")
            if flt and not re.search(flt, name.split("/")[-1]):
                continue
            if rr_index(r) > max_rr:
                continue
            path = (r["_path"] + name).replace("//", "/")
            jobs.append((layer, r, path, seconds))
    print(f"{len(jobs)} samples to fetch")

    def get(job):
        layer, r, path, seconds = job
        return fetch(raw_url("master", path), DL / "wav" / path)

    with cf.ThreadPoolExecutor(12) as pool:
        srcs = list(pool.map(get, jobs))

    OUT.mkdir(parents=True, exist_ok=True)
    maps = {}

    def work(i):
        layer, r, path, seconds = jobs[i]
        d = OUT / layer
        d.mkdir(parents=True, exist_ok=True)
        idx = sum(1 for j in range(i) if jobs[j][0] == layer)
        dst = d / f"{idx:03d}.flac"
        gain = convert(srcs[i], dst, seconds)
        vol = float(r.get("volume", 0))
        key = int(r.get("pitch_keycenter", r.get("key", 60)))
        line = (f"{dst.name} {key} {r.get('lokey', key)} {r.get('hikey', key)} {r.get('lovel', 0)} {r.get('hivel', 127)} "
                f"{rr_index(r)} {vol - gain:.2f}")
        return layer, line

    with cf.ThreadPoolExecutor(os.cpu_count() or 4) as pool:
        for layer, line in pool.map(work, range(len(jobs))):
            maps.setdefault(layer, []).append(line)

    for layer, lines in maps.items():
        (OUT / layer / "map.txt").write_text("\n".join(sorted(lines)) + "\n")
    (OUT / "CREDITS.txt").write_text(
        "Orchestra samples: VS Chamber Orchestra 2 Community Edition by Versilian Studios\n"
        "(Sam Gossner, Simon Dalzell; sample cutting by Elan Hickler / Soundemote)\n"
        "License: CC0 1.0 Universal (public domain) - https://github.com/sgossner/VSCO-2-CE\n"
        "Trimmed, faded, normalised and converted to FLAC for Worship Station.\n")
    print("done")


if __name__ == "__main__":
    sys.exit(main())
