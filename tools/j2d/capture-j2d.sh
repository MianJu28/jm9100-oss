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

# 本脚本要停 lightdm；若自身就跑在 lightdm 的 cgroup 里（从桌面终端启动），
# 会在 "systemctl stop lightdm" 那一刻被一起杀掉 —— 之前就是这么中断的。
# 因此：① 先把自己搬进独立 systemd scope；② 无论如何退出都重启 lightdm。
if [ "${J2D_SCOPE:-0}" != "1" ]; then
	echo "== 转到独立 systemd scope 运行（避免被 lightdm 的 stop 杀掉） =="
	exec sudo systemd-run --scope --collect --unit="j2d-capture-$$" \
		--description="J2D command capture" \
		env J2D_SCOPE=1 bash "$0" "$@"
fi

lightdm_back() {
	systemctl reset-failed lightdm 2>/dev/null || true
	systemctl start lightdm 2>/dev/null || true
}
trap lightdm_back EXIT INT TERM
# 兜底：即便脚本异常退出，90 秒后也把 lightdm 拉起来
( sleep 90; systemctl is-active --quiet lightdm || {
	systemctl reset-failed lightdm 2>/dev/null
	systemctl start lightdm 2>/dev/null
  } ) &

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

echo "== 停 lightdm（腾出 X 与 DRM master） =="
systemctl stop lightdm 2>/dev/null || true
for i in $(seq 1 40); do
	pgrep -f "/usr/lib/xorg/Xorg .*:0" >/dev/null 2>&1 || break
	sleep 1
done
pkill -f "/usr/lib/xorg/Xorg .*:0" 2>/dev/null || true
sleep 1

echo "== 用厂商 DDX 起一个 X（带 LD_PRELOAD） =="
rm -f /tmp/.X11-unix/X0
J2D_CAPTURE_DIR="$OUT" LD_PRELOAD="$SO" \
	/usr/lib/xorg/Xorg :0 -noreset -nolisten tcp -novtswitch >"$OUT/xorg.log" 2>&1 &
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

# J2D_SIZES: 要跑的尺寸；J2D_COUNT=0 表示"只做一次 pixmap→pixmap 拷贝"（最干净的样本）
J2D_SIZES="${J2D_SIZES:-64 256 1024}"
J2D_COUNT="${J2D_COUNT:-20}"
echo "== 制造 XCopyArea 序列（尺寸: $J2D_SIZES，次数: $J2D_COUNT） =="
for sz in $J2D_SIZES; do
	"$BIN" :0 "$sz" "$J2D_COUNT" >/dev/null 2>&1 || echo "  (xcopy $sz 失败)"
done

# 可选矩阵：J2D_MATRIX="256:24:pix 256:32:pix …"（尺寸:深度:目标），
# 用来把"描述字"里的 pitch/格式位对照出来。
if [ -n "${J2D_MATRIX:-}" ]; then
	echo "== 矩阵样本: $J2D_MATRIX =="
	for item in $J2D_MATRIX; do
		sz="${item%%:*}"; rest="${item#*:}"
		dep="${rest%%:*}"; dst="${rest##*:}"
		echo "   - $sz x $sz, depth=$dep, dest=$dst"
		"$BIN" :0 "$sz" 0 "$dep" "$dst" >/dev/null 2>&1 || echo "     (失败: $item)"
	done
fi

kill $XPID 2>/dev/null || true
wait $XPID 2>/dev/null || true

echo "== 反解 =="
python3 "$HERE/j2d-decode.py" --dir "$OUT" >"$OUT/decode.txt" 2>&1 || true
echo "捕获到的命令流: $(ls "$OUT"/*.bin 2>/dev/null | wc -l) 个"
echo "反解结果:       $OUT/decode.txt"
echo "原始命令流:     $OUT/*.bin"
echo
echo "提示: 把 $OUT/decode.txt 与 bin 文件留好，重启回开源栈后再分析。"

echo "== 恢复 lightdm =="
lightdm_back
for i in $(seq 1 60); do
	systemctl is-active --quiet lightdm && break
	sleep 1
done
systemctl is-active --quiet lightdm && echo "lightdm: active" || echo "lightdm: 仍未起来，请手动 systemctl start lightdm"
