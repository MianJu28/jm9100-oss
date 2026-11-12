#!/usr/bin/env bash
# 抓厂商 2D 引擎的命令流（需要在**厂商栈**上跑：jmgpu 内核模块在、X 用 mwv207 DDX）。
#
# 背景：厂商的 2D 加速走 DRM_IOCTL_JM_J2D_SEND_CMD（_IOWR('d',0x4B,16)=0xc010644b），
# 命令流由闭源 userspace 拼好、内核只落环。要复刻这条路，必须先看清命令编码，
# 本脚本用 LD_PRELOAD 截获命令流，并用 xcopy 制造已知的 XCopyArea 序列。
#
# 用法：
#   sudo ./tools/j2d/capture-j2d.sh [输出目录]
# 产物：<输出目录>/*.bin（原始命令流）、log.txt、decode.txt（反解结果）
set -u

DIR="${1:-/tmp/j2d-cap}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SO="$DIR/j2dcap.so"
BIN="$DIR/xcopy"
OUT="$DIR"

if [ ! -d /sys/module/jmgpu ]; then
	echo "错误: 当前内核没有加载 jmgpu —— 本脚本必须在厂商栈上跑" >&2
	echo "      (切到厂商栈: sudo $ROOT/switch-mwv207.sh stack vendor 然后重启)" >&2
	exit 1
fi

mkdir -p "$OUT"
rm -f "$OUT"/*.bin "$OUT/log.txt" "$OUT/decode.txt"

gcc -O2 -Wall -shared -fPIC -o "$SO" "$HERE/j2dcap.c" -ldl || exit 1
gcc -O2 -Wall -o "$BIN" "$HERE/xcopy.c" -lX11 || exit 1

echo "== 停 lightdm（腾出 X） =="
systemctl stop lightdm 2>/dev/null || true
sleep 1

echo "== 用厂商 DDX 起一个 X（带 LD_PRELOAD） =="
rm -f /tmp/.X11-unix/X0
J2D_CAPTURE_DIR="$OUT" LD_PRELOAD="$SO" \
	/usr/lib/xorg/Xorg :0 -noreset -nolisten tcp vts >"$OUT/xorg.log" 2>&1 &
XPID=$!
for i in $(seq 1 50); do
	[ -e /tmp/.X11-unix/X0 ] && break
	sleep 0.2
done
if [ ! -e /tmp/.X11-unix/X0 ]; then
	echo "错误: X 没起来，见 $OUT/xorg.log" >&2
	kill $XPID 2>/dev/null
	exit 1
fi

echo "== 制造 XCopyArea 序列（64/256/1024 三种尺寸） =="
for sz in 64 256 1024; do
	"$BIN" :0 "$sz" 20 >/dev/null 2>&1 || echo "  (xcopy $sz 失败)"
done

kill $XPID 2>/dev/null || true
wait $XPID 2>/dev/null || true

echo "== 反解 =="
python3 "$HERE/j2d-decode.py" --dir "$OUT" >"$OUT/decode.txt" 2>&1 || true
echo "捕获到的命令流: $(ls "$OUT"/*.bin 2>/dev/null | wc -l) 个"
echo "反解结果:       $OUT/decode.txt"
echo "原始命令流:     $OUT/*.bin"
echo
echo "提示: 把 $OUT/decode.txt 与 bin 文件留好，重启回开源栈后再分析。"
