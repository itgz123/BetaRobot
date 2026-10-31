#!/bin/sh
# 把 lib_music_simple 的真实曲谱源码 + host 包装编成共享库，供 render_music.py 用 ctypes 调用。
# 产物：lib/lib_music_simple/host/build/libmusic_host.so（build/ 已被根 .gitignore 忽略）
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../../.." && pwd)

CC=${CC:-gcc}
STD=${STD:--std=gnu11}
CFLAGS=${CFLAGS:--O2 -Wall -Wextra -Werror}

OUT_DIR="$SCRIPT_DIR/build"
OUT="$OUT_DIR/libmusic_host.so"
mkdir -p "$OUT_DIR"

# -I "$SCRIPT_DIR" 必须靠前：命中桩 app_cfg.h，避免误用固件的 app_cfg.h。
# 曲谱只用 drv_buzzer_note.h 里的编译期宏，不需要编译 drv_buzzer_note.c。
# shellcheck disable=SC2086
"$CC" $CFLAGS $STD -fPIC -shared \
    -I "$SCRIPT_DIR" \
    -I "$ROOT/lib/lib_music_simple" \
    -I "$ROOT/lib/lib_music_simple/music_custom" \
    -I "$ROOT/lib/lib_music_simple/twinkle_star" \
    -I "$ROOT/lib/lib_music_simple/happy_birthday" \
    -I "$ROOT/lib/lib_music_simple/see_you_again/see_you_again_guitar" \
    -I "$ROOT/drv/drv_buzzer_simple" \
    -o "$OUT" \
    "$SCRIPT_DIR/music_host.c" \
    "$ROOT/lib/lib_music_simple/lib_music_simple.c" \
    "$ROOT/lib/lib_music_simple/music_custom/music_custom.c" \
    "$ROOT/lib/lib_music_simple/twinkle_star/twinkle_star.c" \
    "$ROOT/lib/lib_music_simple/happy_birthday/happy_birthday.c" \
    "$ROOT/lib/lib_music_simple/see_you_again/see_you_again_guitar/see_you_again_guitar.c"

echo "built: $OUT"
