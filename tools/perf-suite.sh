#!/bin/bash
# 性能套件：在当前安装的驱动栈上跑同一套基准，输出可直接对比的表格。
# usage: ./tools/perf-suite.sh [label]
#   结果写到 ~/perf-<label>.txt，同时在终端打印摘要。
set -u
label=${1:-$(date +%Y%m%d-%H%M%S)}
OUT=$HOME/perf-$label.txt
BIN=${BIN_DIR:-$HOME/dev-bin}
ASSETS=${ASSETS_DIR:-$HOME/vp9-assets}
export DISPLAY=${DISPLAY:-:0}
export XAUTHORITY=${XAUTHORITY:-$HOME/.Xauthority}

: > "$OUT"
say() { echo "$@" | tee -a "$OUT"; }
row() { printf '%-28s %s\n' "$1" "$2" | tee -a "$OUT"; }

say "# perf-suite  label=$label  $(date '+%F %T')"
say ""

say "## 0. 栈信息"
row "active stack:" "$(./switch-mwv207.sh status 2>/dev/null | head -1)"
row "renderer:" "$(glxinfo -B 2>/dev/null | grep -m1 'OpenGL renderer' | cut -d: -f2-)"
row "GL version:" "$(glxinfo -B 2>/dev/null | grep -m1 'OpenGL version' | cut -d: -f2-)"
row "VAAPI:" "$(vainfo 2>/dev/null | grep -m1 'Driver version' | cut -d: -f2-)"
say ""

say "## 1. glmark2 全套（duration=2）"
score=$(timeout 400 glmark2 -b :duration=2 2>/dev/null | awk '/glmark2 Score/ {s=$NF} END {print s}')
row "glmark2 score:" "${score:-FAIL}"
say ""

say "## 2. glmark2 逐场景（duration=2，FPS）"
for sc in build texture shading bump conditionals jellyfish shadow terrain refract effect2d pulsar desktop buffer function loop; do
  v=$(timeout 90 glmark2 -b $sc:duration=2 2>/dev/null | grep -oE 'FPS: *[0-9]+' | tail -1 | grep -oE '[0-9]+')
  row "$sc:" "${v:-FAIL}"
done
say ""

say "## 3. glxgears（vblank off，5s）"
g=$(vblank_mode=0 timeout 8 glxgears 2>/dev/null | tail -1 | grep -oE '[0-9.]+' | tail -1)
row "glxgears FPS:" "${g:-FAIL}"
say ""

say "## 4. present（swap，无 vsync）"
for t in "300 300 300" "800 600 200" "1920 1080 80"; do
  set -- $t
  r=$(vblank_mode=0 timeout 90 "$BIN/pbench" "$1" "$2" "$3" 1 2>/dev/null | tail -1)
  row "present $1x$2:" "${r:-FAIL}"
done
say ""

say "## 5. 纹理上传"
row "tex 1024 64-255 ntex8:" "$(timeout 120 "$BIN/texbench" 1024 64 100 8 2>/dev/null | tail -1)"
row "tex 2048 64 ntex8:"     "$(timeout 180 "$BIN/texbench" 2048 64 40 8 2>/dev/null | tail -1)"
row "tex 1024 full:"          "$(timeout 120 "$BIN/texbench" 1024 1024 50 1 2>/dev/null | tail -1)"
say ""

say "## 6. buffer（4 MB）"
say "$(timeout 120 "$BIN/bufbench" 20 2>/dev/null | tail -4)"
say ""

say "## 7. 回读"
row "rbbench:" "$(timeout 120 "$BIN/rbbench" 20 2>/dev/null | tail -1)"
row "rbscale:" "$(timeout 180 "$BIN/rbscale" 2>/dev/null | tail -2)"
say ""

say "## 8. 解码 + VPP（1080p，120 帧，-f null）"
dec() { local tag="$1" file="$2" vf="${3:-}" s e; s=$(date +%s%N); if [ -n "$vf" ]; then timeout 120 ffmpeg -nostdin -v error -hwaccel vaapi -hwaccel_output_format vaapi -i "$ASSETS/$file" -vf "$vf" -f null /dev/null >/dev/null 2>&1; else timeout 120 ffmpeg -nostdin -v error -hwaccel vaapi -hwaccel_output_format vaapi -i "$ASSETS/$file" -f null /dev/null >/dev/null 2>&1; fi; e=$(date +%s%N); row "$tag:" "$(python3 -c "print(f'{120/(($e-$s)/1e9):.1f} fps')")"; }
dec "vp9 1080p decode" p1080.webm
dec "h264 1080p decode" p1080h264.mp4
dec "hevc 1080p decode" p1080h265.mp4
dec "vp9 1080p->720p vpp" p1080.webm "scale_vaapi=w=1280:h=720"
say ""

say "## 完成：$OUT"
