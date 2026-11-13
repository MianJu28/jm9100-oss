#!/bin/bash
# 在"厂商栈"（jmgpu KMD + jmgpu UMD）上抓一次 JPEG 硬解的寄存器写入序列。
#
# 用法（先切到厂商栈）：
#   sudo STACK_WD_TIMEOUT=900 ./switch-mwv207.sh stack vendor
#   ./tools/vendor-capture/capture-jpeg.sh            # 默认解 1080p 样片
#   sudo ./switch-mwv207.sh stack oss                 # 抓完切回来
#
# 产物：$OUT（默认 ~/vcap-jpeg）/NNNNN-req*.bin + log.txt + summary.txt
set -u
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
OUT=${OUT:-$HOME/vcap-jpeg}
SRC=${1:-$HOME/vp9-assets/t1080.jpg}
DEV=${DEV:-/dev/dri/renderD128}

[ -f "$HERE/vcap.so" ] || { echo "缺 $HERE/vcap.so，先: cc -O2 -shared -fPIC -o vcap.so vcap.c -ldl"; exit 1; }
[ -d "$DEV" ] && DEV=/dev/dri/renderD128
[ -e "$SRC" ] || { echo "找不到样片 $SRC"; exit 1; }

rm -rf "$OUT"; mkdir -p "$OUT"
export VCAP_DIR="$OUT"
export LIBVA_DRIVER_NAME=jmgpu
export LIBVA_DRIVERS_PATH=/usr/lib/aarch64-linux-gnu/dri

echo "== 厂商栈信息 =="
timeout 20 vainfo 2>/dev/null | grep -iE "vainfo|driver version|JPEG" | head -5

echo "== 解码（vcap 抓 ioctl） =="
timeout 120 env LD_PRELOAD="$HERE/vcap.so" \
  ffmpeg -nostdin -v info -hwaccel vaapi -vaapi_device "$DEV" \
         -i "$SRC" -frames:v 1 -f rawvideo -pix_fmt nv12 "$OUT/out.yuv" 2>&1 | tail -5

echo "== 产物 =="
ls "$OUT" | wc -l
du -sh "$OUT"
python3 - "$OUT" <<'PY'
import os, sys, collections
d = sys.argv[1]
hist = collections.Counter()
for f in os.listdir(d):
    if f.endswith('.bin'):
        req = f.split('-req')[1].split('-')[0]
        sz = int(f.split('-sz')[1].split('.')[0])
        hist[(req, sz)] += 1
with open(os.path.join(d, 'summary.txt'), 'w') as fh:
    for (req, sz), n in sorted(hist.items(), key=lambda kv: -kv[1]):
        line = f"req={req} sz={sz} count={n}"
        print(line); fh.write(line + "\n")
PY
echo "完成: $OUT/summary.txt"
