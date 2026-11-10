#!/bin/bash
# run-n14-reset.sh —— 实测 N14：让 3D 作业超时，触发 pipe->reset()，
# 用 kprobe 证明 pipe_3d_force_complete()（N14 的 -EIO signal 实现）确实被执行，
# 并检查用户态 fence 没有被永久挂住、复位后 GPU 仍可用。
#
# 风险：本脚本会故意制造 GPU 超时，触发 6 轮 3D 管线硬复位（每轮 2 s）。
# 期间桌面会卡顿；若硬件复位不能恢复，可能整机无响应 —— 请先保存工作，
# 并准备好 SSH（本机 22 端口已开）或电源键重启。
#
# 用法（需要 root 操作 tracefs；探针本身在 root 下亦可连 X）：
#   sudo DISPLAY=:0 XAUTHORITY=/home/admin/.Xauthority ./run-n14-reset.sh
#
# 退出码：0 = 全部 PASS；2 = 探针判定失败；3 = 环境/符号问题；4 = 看门狗。

set -u
DIR=$(cd "$(dirname "$0")" && pwd)
TRACE=/sys/kernel/tracing
DISPLAY=${DISPLAY:-:0}
XAUTHORITY=${XAUTHORITY:-/home/admin/.Xauthority}
ITERS=${ITERS:-2000000000}
export DISPLAY XAUTHORITY

if [ "$(id -u)" != 0 ]; then
	echo "必须以 root 运行（tracefs/kprobe/dmesg）" >&2
	exit 3
fi

echo "== 编译探针 =="
gcc -O2 -Wall -o "$DIR/hangprobe" "$DIR/hangprobe.c" -lGL -lX11 || exit 3
echo "ok: $DIR/hangprobe"

echo
echo "== 安装 kprobe =="
cd "$TRACE" || exit 3
echo 0 > tracing_on
echo 0 > events/enable
echo > trace
echo > kprobe_events
# TDR 入口（证明超时通路被走到）
echo 'p:n14_tdr mwv207_sched_timedout_job' > kprobe_events || {
	echo "!! kprobe mwv207_sched_timedout_job 失败：" >&2
	cat error_log >&2
	exit 3
}
# N14 实现在此函数里：对在飞 fence set_error(-EIO)+signal
echo 'p:n14_fc pipe_3d_force_complete' >> kprobe_events || {
	echo "!! kprobe pipe_3d_force_complete 失败：" >&2
	cat error_log >&2
	exit 3
}
echo 0 > events/enable
echo 1 > events/kprobes/n14_tdr/enable
echo 1 > events/kprobes/n14_fc/enable
echo 1 > tracing_on
cat kprobe_events

cleanup() {
	echo 0 > "$TRACE/tracing_on" 2>/dev/null || true
	echo 0 > "$TRACE/events/enable" 2>/dev/null || true
	echo > "$TRACE/kprobe_events" 2>/dev/null || true
}
trap cleanup EXIT

DMESG_BEFORE=$(dmesg | wc -l)
echo
echo "== 运行探针（预计 12-15 s，期间可能卡顿）=="
#
# 探针只需 GLX；root 连接 X 用 admin 的 Xauthority 即可。
#
timeout 150 "$DIR/hangprobe" "$ITERS"
PROBE_RC=$?
echo "probe exit code: $PROBE_RC"

sleep 1
echo 0 > tracing_on
sleep 0.3
echo
echo "== kprobe 命中 =="
TDR_HITS=$(grep -c 'n14_tdr:' trace 2>/dev/null || true)
FC_HITS=$(grep -c 'n14_fc:' trace 2>/dev/null || true)
TDR_HITS=${TDR_HITS:-0}
FC_HITS=${FC_HITS:-0}
echo "mwv207_sched_timedout_job : $TDR_HITS"
echo "pipe_3d_force_complete    : $FC_HITS"

echo
echo "== 本次新增 dmesg =="
dmesg | tail -n +$((DMESG_BEFORE + 1)) | grep -E "mwv207|timed out|BUG|WARNING|Oops|watchdog" | head -40 || true

echo
echo "== 清理 kprobe =="
echo 0 > events/enable
echo > kprobe_events
cd - > /dev/null || true

echo
if [ "$PROBE_RC" -ne 0 ]; then
	echo "RESULT: FAIL（探针退出码 $PROBE_RC）"
	exit 2
fi
if [ "$TDR_HITS" -eq 0 ]; then
	echo "RESULT: FAIL（TDR 未触发，没走到 reset 通路）"
	exit 2
fi
if [ "$FC_HITS" -eq 0 ]; then
	echo "RESULT: FAIL（reset 走了但没有调用 pipe_3d_force_complete）"
	exit 2
fi
echo "RESULT: PASS（TDR $TDR_HITS 次，force_complete $FC_HITS 次，探针未阻塞）"