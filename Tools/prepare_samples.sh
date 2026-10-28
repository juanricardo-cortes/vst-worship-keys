#!/usr/bin/env bash
# Builds worship-keys/Samples from the Salamander Grand Piano V3 (44.1 kHz/16-bit) pack.
#   Salamander Grand Piano by Alexander Holm, CC-BY 3.0 (http://creativecommons.org/licenses/by/3.0/)
#   Retuning by Markus Fiedler.
# Keeps 5 of the 16 velocity layers, caps each note at 12 s with a fade, converts to FLAC,
# and writes tuning.txt ("<midi> <cents>") from the retuned .sfz.
# Needs: curl, tar/xz, ffmpeg.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DL="$ROOT/_download"
SRC="$DL/SalamanderGrandPianoV3_44.1khz16bit"
OUT="$ROOT/Samples"
LAYERS=(2 6 10 13 16) # Salamander layers kept, softest first

if [ ! -d "$SRC" ]; then
    mkdir -p "$DL"
    curl -L -o "$DL/salamander.tar.xz" \
        "https://freepats.zenvoid.org/Piano/SalamanderGrandPiano/SalamanderGrandPianoV3+20161209_44khz16bit.tar.xz"
    tar -xJf "$DL/salamander.tar.xz" -C "$DL"
fi

mkdir -p "$OUT"

note_to_midi() { # A0, C1, D#1, F#1 ...
    local name="${1%?}" oct="${1: -1}" idx
    case "$name" in C) idx=0 ;; D#) idx=3 ;; F#) idx=6 ;; A) idx=9 ;; *) echo "bad note $1" >&2; exit 1 ;; esac
    echo $(( 12 * (oct + 1) + idx ))
}

jobs_file="$(mktemp)"
for f in "$SRC"/44.1khz16bit/*v*.wav; do
    base="$(basename "$f" .wav)"
    [[ "$base" =~ ^(A|C|D#|F#)([0-9])v([0-9]+)$ ]] || continue
    vel="${BASH_REMATCH[3]}"
    for i in "${!LAYERS[@]}"; do
        if [ "${LAYERS[$i]}" = "$vel" ]; then
            midi=$(note_to_midi "${BASH_REMATCH[1]}${BASH_REMATCH[2]}")
            printf '%s\t%s\n' "$f" "$OUT/$(printf '%03d' "$midi")_v$((i + 1)).flac" >> "$jobs_file"
        fi
    done
done

echo "Converting $(wc -l < "$jobs_file") samples..."
while IFS=$'\t' read -r in out; do
    printf '%s\0%s\0' "$in" "$out"
done < "$jobs_file" | xargs -0 -n 2 -P 8 sh -c \
    'ffmpeg -v error -y -i "$0" -af "afade=t=out:st=9.5:d=2.5,atrim=0:12" -c:a flac -compression_level 8 "$1"'
rm -f "$jobs_file"

# Per-note tuning (cents) from the retuned sfz.
grep -oE 'sample=[^ ]*\\(A|C|D#|F#)[0-9]v1\.wav .*tune=-?[0-9]+' "$SRC/SalamanderGrandPianoV3Retuned.sfz" \
    | sed -E 's/.*\\((A|C|D#|F#)[0-9])v1\.wav.*tune=(-?[0-9]+)/\1 \3/' \
    | while read -r n cents; do echo "$(note_to_midi "$n") $cents"; done > "$OUT/tuning.txt"

cat > "$OUT/CREDITS.txt" <<'EOF'
Piano samples: Salamander Grand Piano V3 by Alexander Holm
License: Creative Commons Attribution 3.0 (http://creativecommons.org/licenses/by/3.0/)
Retuning by Markus Fiedler. Trimmed, faded and converted to FLAC for Worship Keys.
EOF

echo "Done: $(ls "$OUT"/*.flac | wc -l) files, $(du -sh "$OUT" | cut -f1)"
