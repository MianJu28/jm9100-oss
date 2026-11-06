#!/usr/bin/env bash
# VP9 硬解回归：把样片目录里每个 <name>.webm 用 VAAPI 硬件解码，再与
# 同名的 <name>_sw.yuv（ffmpeg 软件解码结果）逐字节比较。
#
# 用法:
#   tools/vp9-regression.sh [样片目录] [额外文件...]
#
# 环境变量:
#   LIBVA_DRIVERS_PATH  指定驱动目录（例如 /tmp/gd3 跑开发构建）；
#                       不设则用系统安装的驱动。
#   VP9_TEST_TIMEOUT    单个样片超时秒数，默认 300。
set -u

DIR=${1:-/home/admin/vp9-assets}
shift || true
TIMEOUT=${VP9_TEST_TIMEOUT:-300}
FAIL=0
PASS=0
FAILED=()

decode() {
   # $1 = 输入, $2 = 输出
   ffmpeg -v error -y -hwaccel vaapi -hwaccel_device /dev/dri/renderD128 \
      -hwaccel_output_format vaapi -i "$1" \
      -vf hwdownload,format=nv12 -pix_fmt yuv420p -f rawvideo "$2"
}

check() {
   local in=$1 ref=$2 name
   name=$(basename "$in")
   if [ ! -f "$ref" ]; then
      echo "SKIP $name (缺少参考 $ref)"
      return
   fi
   if ! timeout "$TIMEOUT" bash -c "$(declare -f decode); decode '$in' /tmp/.vp9reg.yuv" ; then
      echo "FAIL $name (解码失败)"
      FAILED+=("$name"); FAIL=$((FAIL+1)); return
   fi
   if cmp -s /tmp/.vp9reg.yuv "$ref"; then
      echo "OK   $name"
      PASS=$((PASS+1))
   else
      echo "FAIL $name (与软件解码不一致)"
      FAILED+=("$name"); FAIL=$((FAIL+1))
   fi
   rm -f /tmp/.vp9reg.yuv
}

echo "== 驱动: ${LIBVA_DRIVERS_PATH:-<系统安装>} / ${LIBVA_DRIVER_NAME:-mwv207}"
for f in "$DIR"/*.webm; do
   [ -f "$f" ] || continue
   check "$f" "${f%.webm}_sw.yuv"
done
for f in "$@"; do
   [ -f "$f" ] || { echo "SKIP $f (不存在)"; continue; }
   check "$f" "${f%.*}_sw.yuv"
done

echo
echo "== 结果: pass=$PASS fail=$FAIL"
if [ "$FAIL" -ne 0 ]; then
   printf '   失败: %s\n' "${FAILED[*]}"
   exit 1
fi
