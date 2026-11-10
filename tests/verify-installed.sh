#!/bin/bash
# 已安装栈一键验收：KMD 版本 + GL/GLES + VAAPI 逐位一致 + 引擎计数 + TDR 探针 + dmesg
#
# 用法:
#   ./tests/verify-installed.sh                 # 只报告，不比对期望 srcversion
#   ./tests/verify-installed.sh <srcversion>    # 额外比对运行中模块版本（如 0EFB7F4433EA5311B08E88C）
#   ./tests/verify-installed.sh --no-n14        # 跳过需要 root 的 TDR 探针
#
# 设计约束（踩过的坑）:
#   * tests/query/run-query-tests.sh 自己会 export LIBGL_DRIVERS_PATH="$DRI_DIR"，
#     必须用 DRI_DIR=... 调用；只设 LIBGL_DRIVERS_PATH 会用默认路径并假失败。
#   * VAAPI 走自包含的 mwv207_drv_video.so，不关 mwv207_dri.so 的事。
#   * hwdownload 必须配 -hwaccel_output_format vaapi，否则报 ENOSYS（与驱动无关）。
#   * 比对用 cmp -s（逐位）；不要用 ffmpeg 的哈希日志去比，格式细节会骗人。
set -u

W="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/.." && pwd)"
DRI_DIR=${DRI_DIR:-/usr/local/lib/mwv207-oss}
VADIR=${VADIR:-/usr/local/lib/mwv207-oss}
TMP=${TMP:-/tmp/mwv207-verify}
EXPECT_SRC=""
DO_N14=1

for arg in "$@"; do
	case "$arg" in
	--no-n14) DO_N14=0 ;;
	-*) echo "未知参数: $arg" >&2; exit 2 ;;
	*) EXPECT_SRC="$arg" ;;
	esac
done

mkdir -p "$TMP"
# root 跑过一次之后 $TMP 属 root，普通用户再跑会 Permission denied（VP9 样片生成失败）。
chmod 0777 "$TMP" 2>/dev/null || true

# dmesg 基线：只检查本次运行之后新增的行，避免把历史告警算到这次头上。
# 有些系统开了 kernel.dmesg_restrict，非 root 读不到 —— 这种情况必须明确 SKIP，
# 不能因为输出为空就当成"没有异常"。
DMESG_BASE=0
DMESG_OK=0
if dmesg >/dev/null 2>&1; then
	DMESG_BASE=$(dmesg | wc -l)
	DMESG_OK=1
fi
newdmesg() { dmesg 2>/dev/null | tail -n +$((DMESG_BASE + 1)); }

pass=0; fail=0; skip=0
ok()   { pass=$((pass+1)); printf '  [PASS] %s\n' "$1"; }
bad()  { fail=$((fail+1)); printf '  [FAIL] %s\n' "$1"; }
skp()  { skip=$((skip+1)); printf '  [SKIP] %s\n' "$1"; }
head1() { printf '\n===== %s =====\n' "$1"; }

need() { command -v "$1" >/dev/null 2>&1; }

# ---------------------------------------------------------------- 1. KMD 版本
head1 "1. KMD"
SRC=$(cat /sys/module/mwv207/srcversion 2>/dev/null || true)
if [ -n "$SRC" ]; then
	ok "运行中 srcversion = $SRC"
	if [ -n "$EXPECT_SRC" ]; then
		if [ "$SRC" = "$EXPECT_SRC" ]; then
			ok "与期望一致（$EXPECT_SRC）"
		else
			bad "期望 $EXPECT_SRC，实际 $SRC（是否忘了重启？）"
		fi
	fi
else
	bad "读不到 /sys/module/mwv207/srcversion"
fi
MODPATH=$(modinfo -n mwv207 2>/dev/null || true)
case "$MODPATH" in
	*/updates/dkms/*) ok "模块来自 $MODPATH" ;;
	"") bad "modinfo 找不到 mwv207" ;;
	*) bad "模块路径不是自建版：$MODPATH" ;;
esac

# ---------------------------------------------------------------- 2. GL / GLES
head1 "2. GL / GLES"
if need glxinfo; then
	if DISPLAY=${DISPLAY:-:0} XAUTHORITY=${XAUTHORITY:-$HOME/.Xauthority} \
		glxinfo -B 2>/dev/null | grep -q "Device: MWV207"; then
		ok "glxinfo -B 识别 MWV207"
	else
		bad "glxinfo -B 未识别 MWV207（X 是否在跑？DISPLAY=$DISPLAY）"
	fi
else
	skp "没有 glxinfo（mesa-utils 未装？）"
fi

if [ -x "$W/tests/query/run-query-tests.sh" ]; then
	out=$(DRI_DIR="$DRI_DIR" "$W/tests/query/run-query-tests.sh" 2>&1 | tail -3)
	if printf '%s' "$out" | grep -qE "FAIL=0|FAIL: 0"; then
		ok "query 测试：$(printf '%s' "$out" | grep -oE 'PASS=[0-9]+ FAIL=[0-9]+' | tail -1)"
	else
		bad "query 测试未全过：$(printf '%s' "$out" | tr '\n' ' ')"
	fi
else
	skp "tests/query/run-query-tests.sh 不存在"
fi

if [ -x "$W/tests/engines/run-engines.sh" ]; then
	out=$("$W/tests/engines/run-engines.sh" 2>&1 | tail -2)
	if printf '%s' "$out" | grep -q "PASS"; then
		ok "引擎计数：$(printf '%s' "$out" | tr '\n' ' ')"
	else
		bad "引擎计数不符：$(printf '%s' "$out" | tr '\n' ' ')"
	fi
else
	skp "tests/engines/run-engines.sh 不存在"
fi

# ---------------------------------------------------------------- 3. VAAPI 能力
head1 "3. VAAPI profile"
if need vainfo; then
	v=$(LIBVA_DRIVERS_PATH="$VADIR" LIBVA_DRIVER_NAME=mwv207 vainfo 2>/dev/null)
	if [ -z "$v" ]; then
		bad "vainfo 无输出"
	else
		for p in H264ConstrainedBaseline H264Main H264High HEVCMain HEVCMain10 \
			VP9Profile0 VP9Profile2; do
			printf '%s' "$v" | grep -q "VAProfile$p" && ok "声明 $p" || bad "缺 $p"
		done
		for p in H264High10 H264High422 H264High444; do
			if printf '%s' "$v" | grep -q "VAProfile$p"; then
				bad "不该再声明 $p（见 UNRESOLVED §1.5 / I-4）"
			else
				ok "已收回 $p"
			fi
		done
	fi
else
	skp "没有 vainfo"
fi

# ---------------------------------------------------------------- 4. 解码逐位一致
head1 "4. 解码逐位一致"
if ! need ffmpeg; then
	skp "没有 ffmpeg"
else
	# 注意：$2 是选项字符串，必须让 shell 按空格再拆一次（不能整体引号化）
	mk() {
		[ -s "$TMP/$1" ] && return 0
		# shellcheck disable=SC2086
		ffmpeg -v error -f lavfi -i "testsrc2=size=640x360:rate=30" -frames:v 20 \
			$2 -y "$TMP/$1" >/dev/null 2>&1
	}

	mk h264_420.mp4 "-c:v libx264 -pix_fmt yuv420p -x264-params keyint=10:bframes=0"
	mk hevc_8.mp4   "-c:v libx265 -pix_fmt yuv420p -x265-params keyint=10:bframes=0"
	mk hevc_10.mp4  "-c:v libx265 -pix_fmt yuv420p10le -x265-params keyint=10:bframes=0"

	bitcmp() { # $1=样片 $2=名字 $3=hwdownload 格式 $4=输出像素格式
		local src="$TMP/$1" name="$2" hwf="$3" pix="$4"
		[ -s "$src" ] || { skp "$name 样片生成失败"; return; }
		ffmpeg -v error -y -i "$src" -f rawvideo -pix_fmt "$pix" "$TMP/$name.sw.yuv" >/dev/null 2>&1
		LIBVA_DRIVERS_PATH="$VADIR" LIBVA_DRIVER_NAME=mwv207 \
		ffmpeg -v error -y -hwaccel vaapi -hwaccel_output_format vaapi -i "$src" \
			-vf "hwdownload,format=$hwf" -f rawvideo -pix_fmt "$pix" \
			"$TMP/$name.hw.yuv" >/dev/null 2>&1
		if [ -s "$TMP/$name.sw.yuv" ] && [ -s "$TMP/$name.hw.yuv" ] && \
			cmp -s "$TMP/$name.sw.yuv" "$TMP/$name.hw.yuv"; then
			ok "$name 逐位一致（$(stat -c %s "$TMP/$name.hw.yuv") B）"
		else
			bad "$name 不一致或硬件解码失败"
		fi
	}

	bitcmp h264_420.mp4 h264-420 nv12 yuv420p
	bitcmp hevc_8.mp4 hevc-8bit nv12 yuv420p
	bitcmp hevc_10.mp4 hevc-10bit p010 yuv420p10le

	# A5：VP9 Profile 2（10bit 4:2:0）
	ffmpeg -v error -y -f lavfi -i "testsrc2=size=320x240:rate=30" -frames:v 10 \
		-pix_fmt yuv420p10le -c:v libvpx-vp9 -profile:v 2 "$TMP/vp9_10.webm" >/dev/null 2>&1
	if [ -s "$TMP/vp9_10.webm" ]; then
		bitcmp vp9_10.webm vp9-profile2-10bit p010 yuv420p10le
	else
		skp "vp9 profile2 样片生成失败（ffmpeg 无 libvpx-vp9？）"
	fi

	# H.264 High10 必须走软件回退（不能把 DEC 打挂）
	ffmpeg -v error -y -f lavfi -i "testsrc2=size=352x288:rate=30" -frames:v 10 \
		-c:v libx264 -pix_fmt yuv420p10le -profile:v high10 "$TMP/h264_10.mp4" >/dev/null 2>&1
	if [ "$DMESG_OK" = 0 ]; then
		before=0; after=0
	else
		before=$(dmesg 2>/dev/null | grep -c "mwv207_dec0.*timed out" || true)
	fi
	out=$(LIBVA_DRIVERS_PATH="$VADIR" LIBVA_DRIVER_NAME=mwv207 ffmpeg -hide_banner \
		-hwaccel vaapi -hwaccel_output_format vaapi -i "$TMP/h264_10.mp4" -f null - 2>&1)
	[ "$DMESG_OK" = 0 ] || after=$(dmesg 2>/dev/null | grep -c "mwv207_dec0.*timed out" || true)
	[ "$DMESG_OK" = 0 ] && after=$before
	if printf '%s' "$out" | grep -qE "Failed setup for format vaapi|hwaccel initialisation returned error"; then
		ok "H.264 High10 正确回退到软件解码"
	else
		bad "H.264 High10 没有回退（输出：$(printf '%s' "$out" | tail -2 | tr '\n' ' ')）"
	fi
	if [ "$DMESG_OK" = 0 ]; then
		skp "无法读 dmesg，无法确认 High10 期间是否超时（用 sudo 跑本脚本）"
	elif [ "$before" = "$after" ]; then
		ok "High10 期间 DEC 管线零超时"
	else
		bad "High10 让 DEC 管线超时 $((after-before)) 次"
	fi
fi

# ---------------------------------------------------------------- 5. N14 探针
head1 "5. TDR / reset 通路"
if [ "$DO_N14" = 0 ]; then
	skp "按参数跳过"
elif [ ! -x "$W/tests/n14-reset/run-n14-reset.sh" ]; then
	skp "tests/n14-reset/run-n14-reset.sh 不存在"
elif [ "$(id -u)" != 0 ]; then
	skp "需要 root：printf '666\\n' | sudo -S $W/tests/n14-reset/run-n14-reset.sh"
else
	out=$("$W/tests/n14-reset/run-n14-reset.sh" 2>&1 | tail -3)
	if printf '%s' "$out" | grep -q "PASS"; then
		ok "N14 探针：$(printf '%s' "$out" | tr '\n' ' ')"
	else
		bad "N14 探针：$(printf '%s' "$out" | tr '\n' ' ')"
	fi
fi

# ---------------------------------------------------------------- 6. dmesg
head1 "6. dmesg（只看本次运行新增的行）"
if [ "$DMESG_OK" = 0 ]; then
	skp "读不到 dmesg（kernel.dmesg_restrict=1）；请用 sudo 跑本脚本"
else
	f=$(newdmesg | grep -iE "BUG:|WARNING:|Oops|DMA timeout|busy_count|nobody cared" || true)
	if [ -z "$f" ]; then
		ok "本次运行期间无 BUG/WARNING/Oops/DMA timeout/nobody cared"
	else
		bad "本次运行期间 dmesg 有异常：$(printf '%s' "$f" | head -5 | tr '\n' ' ')"
	fi
	t=$(newdmesg | grep -c "timed out" || true)
	if [ "$t" = 0 ]; then
		ok "本次运行期间无 DEC/DMA 超时记录"
	else
		skp "本次运行期间 $t 条 timed out（TDR 探针之后属预期，需人工确认）"
	fi
fi

# 以 root 跑时，query/engines 会把测试二进制写在 /tmp 且属 root，
# 让之后的普通用户运行无法覆盖编译 —— 收尾时清掉它们。
if [ "$(id -u)" = 0 ]; then
	rm -f /tmp/qt-occlusion /tmp/qt-occlusion-scale /tmp/qt-condrender /tmp/mwv207-engines
fi

# ---------------------------------------------------------------- 汇总
printf '\n===== 总结：PASS=%d FAIL=%d SKIP=%d =====\n' "$pass" "$fail" "$skip"
[ "$fail" = 0 ] || exit 1