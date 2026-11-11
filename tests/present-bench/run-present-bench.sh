#!/bin/sh
# 测 present（glXSwapBuffers）单帧开销与 3D 渲染开销。
#
# 用法：
#   ./tests/present-bench/run-present-bench.sh            # 默认几组尺寸
#   ./tests/present-bench/run-present-bench.sh 800 600 500
#
# 判据（2026-11-11 的基线，r25 + 未打 glamor blit 补丁的 X）：
#   800x600 swap-only ≈ 0.65 ms；打了 tools/build-xglamor.sh 的补丁后
#   期望降到 ≈0.25–0.30 ms（见 reports/PRESENT-PATH-20261111.md）。
#
# 需要：DISPLAY（若 X 不在 :0，用 `DISPLAY=:1`）、epoxy 头文件、pkg-config。

set -eu
REPO=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${TMPDIR:-/tmp}/present-bench
mkdir -p "$OUT"

cc -O2 -o "$OUT/present-bench" \
    "$REPO/tests/present-bench/present-bench.c" \
    $(pkg-config --cflags --libs epoxy x11)

run() {
    w=$1; h=$2; n=$3
    "$OUT/present-bench" "$w" "$h" "$n" 0
    "$OUT/present-bench" "$w" "$h" "$n" 1
}

if [ $# -ge 3 ]; then
    run "$1" "$2" "$3"
    exit 0
fi

run 800 600 300
run 1280 720 200
run 1920 1080 120