#!/bin/sh
# Reproduce docs/media from the real Phosphor Field render path.
# Needs: the build dependencies, ffmpeg. Runs headless (SDL dummy driver),
# uses a private temporary data directory and never touches your library,
# settings or desktop theme. Usage: tools/showcase/capture.sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
OUT="$ROOT/docs/media"
WORK="$ROOT/build/showcase"
mkdir -p "$OUT" "$WORK"
DATA=$(mktemp -d "${TMPDIR:-/tmp}/phosphor-showcase-XXXXXX")
trap 'rm -rf "$DATA"' EXIT INT TERM

command -v ffmpeg >/dev/null || { echo "ffmpeg is required" >&2; exit 1; }
FLAGS=$(pkg-config --cflags sdl2 SDL2_ttf)
LIBS=$(pkg-config --libs sdl2 SDL2_ttf)
# observe.c gets a virtual monotonic clock; everything else is the real source.
cc -O2 -std=c11 -c "$ROOT/src/observe.c" -Dclock_gettime=showcase_clock_gettime $FLAGS -o "$WORK/observe.o"
cc -O2 -std=c11 "$ROOT/tools/showcase/capture.c" "$ROOT/src/theme.c" "$WORK/observe.o" -o "$WORK/capture" $FLAGS $LIBS -lm

run() { # scene seconds size_w size_h skip ffmpeg-output-args [app args...]
    scene=$1 seconds=$2 width=$3 height=$4 skip=$5 out=$6; shift 6
    env SDL_VIDEODRIVER=dummy XDG_DATA_HOME="$DATA" SHOWCASE_SCENE="$scene" \
        SHOWCASE_SKIP="$skip" SHOWCASE_FFMPEG_OUT="$out" \
        "$WORK/capture" --size "$width" "$height" --seconds "$seconds" "$@" >/dev/null
    rm -rf "${DATA:?}"/*
}
gif() { # input.mp4 output.gif width fps colors
    ffmpeg -loglevel error -y -i "$1" -vf "fps=$4,scale=$3:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=$5:stats_mode=full[p];[b][p]paletteuse=dither=sierra2_4a" "$2"
}
still() { echo "-f image2 -update 1 '$1'"; }

# Hero: two swim beats of PF-MEDUSA, default 10 000 points.
run hero 6.2 1280 800 24 "-c:v libx264 -crf 14 -pix_fmt yuv420p '$WORK/hero.mp4'" --theme phosphor
gif "$WORK/hero.mp4" "$OUT/demo.gif" 720 20 64
# Tour: Creation Lab parameter edits applied live, then Aquarium cross-fades.
run tour 23.6 1280 800 15 "-c:v libx264 -crf 14 -pix_fmt yuv420p '$WORK/tour.mp4'" --theme phosphor
gif "$WORK/tour.mp4" "$OUT/tour.gif" 600 12 40
# Stills (last frame of each run).
run hero 3.1 1600 1000 90 "$(still "$OUT/medusa.png")" --theme phosphor
run lab 3.0 1600 1000 80 "$(still "$OUT/lab.png")" --theme phosphor
run library 2.0 1600 1000 50 "$(still "$OUT/library.png")" --theme phosphor
run hero 4.4 1600 1000 120 "$(still "$OUT/theme.png")" --theme-file "$ROOT/tests/fixtures/sulfur-night.toml"
ls -l "$OUT"
