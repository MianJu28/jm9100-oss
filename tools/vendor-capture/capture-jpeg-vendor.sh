#!/bin/bash
# 在厂商驱动栈上抓一次 JPEG 硬解的 BO 内容（配合 vmapdump.so）。
#
# 用法：tools/vendor-capture/capture-jpeg-vendor.sh [jpeg文件]
# 产物：/tmp/vmd603/{*.bin,index.txt}
set -u
JPEG="${1:-$HOME/vp9-assets/t1080.jpg}"
OUT="/tmp/vmd$(id -u)"
SO="$HOME/dev-bin/vmapdump.so"

if [ ! -f "$SO" ]; then
   echo "缺少 $SO，先编译："
   echo "  cc -O2 -shared -fPIC -o $SO tools/vendor-capture/vmapdump.c -ldl"
   exit 1
fi
if ! vainfo 2>/dev/null | grep -q "JPEGBaseline"; then
   echo "[!] 当前 VA-API 没有 JPEGBaseline（不是厂商栈？）"
   vainfo 2>/dev/null | head -5
   exit 1
fi

rm -rf "$OUT"; mkdir -p "$OUT"
export VMAPDUMP_DIR="$OUT" VMAPDUMP_MIN=256 VMAPDUMP_MAX=4194304
export LD_PRELOAD="$SO"

echo "== 抓一帧 JPEG 硬解（$JPEG）"
ffmpeg -v info -y -hwaccel vaapi -hwaccel_output_format vaapi \
   -vaapi_device /dev/dri/renderD128 -i "$JPEG" -f null /dev/null 2>&1 |
   grep -E "Stream|frame=|error|Failed" | tail -3

echo "== 已抓到 $(find "$OUT" -name '*.bin' | wc -l) 个映射，分析："
python3 "$(dirname "$0")/analyze-vmapdump.py" "$OUT" "$JPEG"
echo "== 原始产物在 $OUT（index.txt + *.bin）"