#!/bin/bash
# 生成 10-bit / 12-bit VP9 样片（VP9 profile 2）
set -eu
OUT=${1:-$HOME/vp9-assets}
mkdir -p "$OUT"
ffmpeg -hide_banner -loglevel error -f lavfi -i testsrc2=size=320x180:rate=30 -frames:v 12 \
	-pix_fmt yuv420p10le -f rawvideo "$OUT/t10_in.yuv" -y
ffmpeg -hide_banner -loglevel error -f lavfi -i testsrc2=size=320x180:rate=30 -frames:v 12 \
	-pix_fmt yuv420p12le -f rawvideo "$OUT/t12_in.yuv" -y
vpxenc --codec=vp9 --profile=2 --bit-depth=10 --input-bit-depth=10 --width=320 --height=180 \
	--fps=30/1 --limit=12 --good --cpu-used=4 --output="$OUT/t10.webm" "$OUT/t10_in.yuv"
vpxenc --codec=vp9 --profile=2 --bit-depth=12 --input-bit-depth=12 --width=320 --height=180 \
	--fps=30/1 --limit=12 --good --cpu-used=4 --output="$OUT/t12.webm" "$OUT/t12_in.yuv"
echo "samples in $OUT"
