#!/bin/bash
# verify-overlay-present.sh —— 验证 mwv207oss DDX 的 WIN0 overlay 零拷贝 present 是否生效
#
# 背景见 docs/HANDOVER-ZEROCOPY-PRESENT.md §11。
# 用法: ./tools/verify-overlay-present.sh            # 全部检查
#       ./tools/verify-overlay-present.sh --quick    # 只查 Xorg 状态，不跑基准
#
# 判定依据(自上而下逐级):
#   1. 当前 Xorg 加载的是 mwv207oss 驱动(而不是 modesetting)
#   2. Xorg 日志里有 "using overlay plane N for zero-copy present"（DDX 已枚举到 WIN0）
#   3. present-bench 的 800x600 窗口态 swap 成本明显低于拷贝路径基线
#      (拷贝路径: glamor blit ≈0.49ms/帧; overlay 直扫预期 ≤0.20ms/帧)
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
LOG=${XORG_LOG:-/var/log/Xorg.0.log}
PASS=0
FAIL=0

ok()   { printf '[OK]   %s\n' "$*"; PASS=$((PASS+1)); }
bad()  { printf '[FAIL] %s\n' "$*"; FAIL=$((FAIL+1)); }
info() { printf '[INFO] %s\n' "$*"; }

quick=false
[ "${1:-}" = "--quick" ] && quick=true

echo "== 1. Xorg 状态 =="
pid=$(pgrep -x Xorg | head -1)
if [ -z "$pid" ]; then
    bad "Xorg 没有运行"
    echo; echo "结果: PASS=$PASS FAIL=$FAIL"
    exit 1
fi
info "Xorg pid=$pid 启动于 $(ps -o lstart= -p "$pid")"

if grep -q 'mwv207oss' "$LOG" 2>/dev/null; then
    ok "日志里出现 mwv207oss（自有 DDX 已加载）"
    grep -m3 'mwv207oss' "$LOG" | sed 's/^/       /'
else
    bad "日志里没有 mwv207oss —— 仍在用系统 modesetting"
    info "检查 $REPO/tools/build-xddx.sh --status 与 10-mwv207-oss.conf"
fi

echo
echo "== 2. overlay plane 枚举 =="
if grep -q 'using overlay plane' "$LOG" 2>/dev/null; then
    ok "DDX 已枚举到 WIN0 overlay 平面"
    grep -m4 'using overlay plane' "$LOG" | sed 's/^/       /'
else
    bad "日志里没有 'using overlay plane' —— KMD 可能没注册 overlay 平面"
    info "确认已加载的 mwv207.ko 是用 overlay=1 重载的:"
    if [ -r /sys/module/mwv207/parameters/overlay ]; then
        info "  /sys/module/mwv207/parameters/overlay = $(cat /sys/module/mwv207/parameters/overlay)"
    fi
fi

if [ "$quick" = true ]; then
    echo; echo "结果: PASS=$PASS FAIL=$FAIL(quick 模式，未跑基准)"
    [ "$FAIL" -eq 0 ] || exit 1
    exit 0
fi

echo
echo "== 3. present 成本(窗口态 800x600) =="
BENCH=$REPO/tests/present-bench/present-bench
if [ ! -x "$BENCH" ]; then
    info "编译 present-bench"
    cc -O2 -o "$BENCH" "$REPO/tests/present-bench/present-bench.c" \
        -lepoxy -lX11 2>/dev/null || {
        bad "present-bench 编译失败"
        echo; echo "结果: PASS=$PASS FAIL=$FAIL"
        exit 1
    }
fi

out=$(vblank_mode=0 DISPLAY=${DISPLAY:-:0} "$BENCH" 800 600 200 1 2>/dev/null | tail -1)
info "800x600 clear+swap: $out"
ms=$(printf '%s' "$out" | sed -n 's/.*: *\([0-9.]*\) ms\/frame.*/\1/p')
if [ -z "$ms" ]; then
    bad "解析不到耗时"
else
    # 拷贝路径基线 0.49ms（glamor blit），overlay 直扫应显著低于此
    if awk -v v="$ms" 'BEGIN{exit !(v < 0.30)}'; then
        ok "耗时 ${ms}ms < 0.30ms —— overlay 零拷贝通路命中"
    else
        bad "耗时 ${ms}ms ≥ 0.30ms —— 仍在走拷贝路径(glamor blit 基线 ≈0.49ms)"
        info "排查: 窗口是否被遮挡/是否居中(overlay 只在 clipList 等于窗口矩形时启用)"
    fi
fi

echo
echo "== 4. 轻载渲染(glmark2 build) =="
if command -v glmark2 >/dev/null; then
    fps=$(vblank_mode=0 DISPLAY=${DISPLAY:-:0} glmark2 -b build:duration=3 2>/dev/null \
          | sed -n 's/.*glmark2 Score: *\([0-9]*\).*/\1/p' | tail -1)
    [ -n "$fps" ] && info "glmark2 build: ${fps} FPS"
    info "参考: 拷贝路径 ≈1477 FPS；厂商栈 5982 FPS"
else
    info "glmark2 未安装，跳过"
fi

echo
echo "结果: PASS=$PASS FAIL=$FAIL"
[ "$FAIL" -eq 0 ] || exit 1