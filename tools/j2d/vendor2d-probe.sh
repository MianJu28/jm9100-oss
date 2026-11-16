#!/bin/bash
# 厂商栈 2D 取证：在 jmgpu 驱动 + 厂商 UMD 下，跑一次 XCopyArea 触发的 2D 拷贝，
# 并抓取"命令流之外"的状态（Xorg 映射、2D 全局寄存器、环链路、TOP 门控）。
#
# 用法：sudo ./tools/j2d/vendor2d-probe.sh [outdir]
# 前提：当前栈是厂商栈（lsmod 里有 jmgpu），且已安装 xcopy（~/dev-bin/xcopy）。
set -u

OUT=${1:-$HOME/j2d-vendor-$(date +%Y%m%d-%H%M%S)}
BARMAP=${BARMAP:-/home/admin/dev-bin/barmap}
VRAM=/sys/bus/pci/devices/0000:07:00.0/resource2

mkdir -p "$OUT"
echo "输出目录: $OUT"

echo "== 栈 ==" | tee "$OUT/summary.txt"
lsmod | grep -E '^(jmgpu|mwv207)' | tee -a "$OUT/summary.txt"
echo "j2dmode: $(cat /sys/module/jmgpu/parameters/j2dmode 2>/dev/null)" | tee -a "$OUT/summary.txt"

# 1) Xorg 是否映射了 2D 窗口（0x968000 附近）或设备 BAR
XPID=$(pgrep -x Xorg | head -1)
echo "== Xorg pid=$XPID 的映射（/dev/ 相关，>4K） ==" | tee -a "$OUT/summary.txt"
if [ -n "$XPID" ]; then
    sudo awk '$6 ~ /^\// && $0 ~ /^\S+\s+\S+\s+\S+\s+\S+\s+\S+\s+\// {print}' "/proc/$XPID/maps" \
        | grep -E 'dri|jmgpu|resource|mem' | tee "$OUT/xorg-maps.txt"
    # 映射大小 >= 0x1000 且落在 BAR 的，单独列出方便对齐
    sudo awk -v pid="$XPID" '{print}' "/proc/$XPID/maps" > "$OUT/xorg-maps-all.txt"
fi

# 2) 2D 全局/TOP/MMU 寄存器快照（只读）
snap() {
    local tag=$1
    {
        echo "--- $tag"
        for off in 0x96C000 0x96C100 0x96C200 0x96C800 0x96CC00 0x961100 \
                   0x9E83B4 0x9E8388 0x9B0000 0x9B0008 0x9B0014 0x9B0018 \
                   0x9B0048 0x9B004C; do
            v=$(sudo "$BARMAP" /sys/bus/pci/devices/0000:07:00.0/resource1 "$off" 2>/dev/null | tail -1)
            printf '  %-10s %s\n' "$off" "$v"
        done
    }
}

snap idle-before | tee "$OUT/regs.txt"

# 3) 后台持续刷新 2D 负载（XCopyArea），抓忙时快照
if [ -x "$HOME/dev-bin/xcopy" ]; then
    DISPLAY=${DISPLAY:-:0} XAUTHORITY=${XAUTHORITY:-/home/admin/.Xauthority} \
        "$HOME/dev-bin/xcopy" :0 256 400 >/dev/null 2>&1 &
    XCPID=$!
    sleep 1
    snap busy-1 | tee -a "$OUT/regs.txt"
    sleep 0.5
    snap busy-2 | tee -a "$OUT/regs.txt"

    # 4) 环内容：CMDDADDR 来自 0x96C200 的 bits27:0（字节地址 = 值 << 4）
    G=$(sudo "$BARMAP" /sys/bus/pci/devices/0000:07:00.0/resource1 0x96C200 2>/dev/null | tail -1)
    echo "0x96C200 = $G" | tee -a "$OUT/regs.txt"

    # 5) VRAM 全景里找 2D 写的像素：抓 xcopy 前后两块屏幕区域
    sudo dd if="$VRAM" of="$OUT/vram-head.bin" bs=1M count=64 status=none || true

    kill $XCPID 2>/dev/null
    wait $XCPID 2>/dev/null
else
    echo "缺 ~/dev-bin/xcopy（tools/j2d/xcopy.c 编译：cc -O2 -o ~/dev-bin/xcopy tools/j2d/xcopy.c -lX11）"
fi

snap idle-after | tee -a "$OUT/regs.txt"
echo "完成。关键文件: $OUT/regs.txt $OUT/xorg-maps.txt $OUT/summary.txt"