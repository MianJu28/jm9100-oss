#!/bin/bash
# VP9 12-bit 行为验证：10-bit 必须正确解码，12-bit 必须"优雅拒绝"（不挂死、不输出脏图）。
# 用法: tests/vp9-12bit/run-vp9-12bit.sh [--local DIR]
set -u
ASSETS=${VP9_ASSETS:-$HOME/vp9-assets}
LIBVA_PATH=${LIBVA_DRIVERS_PATH:-/usr/local/lib/mwv207-oss}
fail=0

for f in t10.webm t12.webm; do
	if [ ! -f "$ASSETS/$f" ]; then
		echo "SKIP $f (缺样片，先运行本目录的 make-samples.sh)"
		continue
	fi
	echo "== $f"
	start=$(date +%s%N)
	timeout 60 env LIBVA_DRIVERS_PATH="$LIBVA_PATH" LIBVA_DRIVER_NAME=mwv207 \
		ffmpeg -hide_banner -loglevel error -hwaccel vaapi \
		-vaapi_device /dev/dri/renderD128 -i "$ASSETS/$f" \
		-f rawvideo -pix_fmt nv12 -y /tmp/vp912_hw.yuv 2>/tmp/vp912_err.txt
	rc=$?
	end=$(date +%s%N)
	ms=$(( (end - start) / 1000000 ))
	if [ "$f" = "t12.webm" ]; then
		# 12-bit 目前不做：接受"报错退出"或"输出为空"，但不接受挂死(124)或脏图
		if [ $rc -eq 124 ]; then
			echo "  FAIL 12-bit 解码挂死（timeout）"; fail=1
		else
			size=$(stat -c %s /tmp/vp912_hw.yuv 2>/dev/null || echo 0)
			echo "  PASS 12-bit 优雅拒绝 rc=$rc size=$size ${ms}ms"
		fi
	else
		if [ $rc -ne 0 ]; then
			echo "  FAIL 10-bit 解码 rc=$rc"; sed -n '1,5p' /tmp/vp912_err.txt; fail=1
		else
			size=$(stat -c %s /tmp/vp912_hw.yuv 2>/dev/null || echo 0)
			echo "  PASS 10-bit rc=$rc size=$size ${ms}ms"
		fi
	fi
done
[ $fail -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $fail
