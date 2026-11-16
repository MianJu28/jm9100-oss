#!/bin/sh
# 在厂商栈（jmgpu 模块）上抓 2D 引擎的寄存器写序列。
#
# 思路：厂商 KMD 里所有引擎寄存器写都经过 engine_write_reg(engine, off, val)，
# 用 kprobe 记下 (off, val)，跑一段 XCopyArea 触发 EXA 的 2D 拷贝，就能看到
# 0x968000 段（J9MATHS_BANDCUTTER）从复位、配置到 0x4200/0x4c00 的完整序列。
#
# 用法：sudo ./tools/j2d/capture-vendor-engregs.sh [输出目录]
# 前提：当前必须是厂商栈（lsmod 有 jmgpu），且 X 在 :0 上跑着。
set -eu

OUT=${1:-/home/admin/j2d-vendor-regs}
XCOPY=${XCOPY:-/home/admin/dev-bin/xcopy}
SIZE=${SIZE:-256}
COUNT=${COUNT:-40}
TR=/sys/kernel/debug/tracing

if ! lsmod | grep -q '^jmgpu'; then
	echo "错误：当前不是厂商栈（lsmod 里没有 jmgpu）" >&2
	exit 1
fi

mount -t debugfs none /sys/kernel/debug 2>/dev/null || true
[ -d "$TR/kprobe_events" ] || { echo "错误：内核没有 kprobe_events" >&2; exit 1; }

cleanup() {
	echo 0 > "$TR/events/kprobes/enable" 2>/dev/null || true
	echo > "$TR/kprobe_events" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

cleanup
echo 'p:ewreg engine_write_reg off=%x1 val=%x2' >> "$TR/kprobe_events"
echo 'p:eread engine_read_reg off=%x1' >> "$TR/kprobe_events"
echo 1 > "$TR/events/kprobes/enable"
: > "$TR/trace"

echo "跑 $COUNT 次 XCopyArea(${SIZE}x${SIZE}) 触发 2D 拷贝..."
DISPLAY="${DISPLAY:-:0}" XAUTHORITY="${XAUTHORITY:-/home/admin/.Xauthority}" \
	"$XCOPY" :0 "$SIZE" "$COUNT" >/dev/null 2>&1 || \
	echo "警告：xcopy 失败（可能没装 X 客户端依赖），继续抓已产生的写入"

# 再补几次窗口搬运，覆盖 fill/copy 两种命令
DISPLAY="${DISPLAY:-:0}" XAUTHORITY="${XAUTHORITY:-/home/admin/.Xauthority}" \
	"$XCOPY" :0 512 20 win >/dev/null 2>&1 || true

sleep 0.3
echo 0 > "$TR/events/kprobes/enable"

mkdir -p "$OUT"
cp "$TR/trace" "$OUT/trace"
grep -c 'ewreg' "$OUT/trace" >/dev/null 2>&1 || true

echo "原始 trace: $OUT/trace"
echo "2D 窗口（0x968xxx）写入："
awk '/ewreg/ && $0 ~ /off=0x9[0-9a-f]*[0-9a-f]/ {print}' "$OUT/trace" | \
	sed -n '1,40p'
echo
echo "统计（off 前缀计数）："
awk '/ewreg/ {for(i=1;i<=NF;i++) if ($i ~ /^off=/) {o=$i; sub("off=","",o); print substr(o,1,7)}}' \
	"$OUT/trace" | sort | uniq -c | sort -rn | sed -n '1,20p'
