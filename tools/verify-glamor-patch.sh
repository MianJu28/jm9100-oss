#!/usr/bin/env bash
# 验证 "glamor blit 快路径" 补丁是否已在运行中的 X 里生效。
#
# 背景（2026-11-18 定位的性能回归）：present 的整帧拷贝由 glamor 的着色器采样
# 完成（线性目标 6.7 GB/s）；patches/xserver/0001-glamor-blit-fast-path.patch 把它
# 换成 glBlitFramebuffer（17.3 GB/s），800×600 present 0.58→0.22 ms。
# 该补丁装在 /usr/lib/xorg/modules/libglamoregl.so —— **不受 switch-mwv207.sh 管理**，
# ostree 部署切换会把文件换回发行版原版，性能静默退化。
#
# 关键：glamor 在 X 启动时加载，热替换文件**不会**生效，必须重启 X。
# 本脚本只做只读判定 + 实测，不做任何修改。
#
# 用法: ./tools/verify-glamor-patch.sh
set -uo pipefail

MOD=/usr/lib/xorg/modules/libglamoregl.so
export DISPLAY="${DISPLAY:-:0}"
export XAUTHORITY="${XAUTHORITY:-$HOME/.Xauthority}"

rc=0

echo "== glamor blit 快路径补丁 验证 =="
echo "  DISPLAY=$DISPLAY"

# 1) 文件里有没有补丁标记
if [ -f "$MOD" ] && strings "$MOD" 2>/dev/null | grep -q 'GLAMOR_NO_BLIT'; then
    echo "  [OK] $MOD 含 GLAMOR_NO_BLIT (md5 $(md5sum "$MOD" | cut -c1-12))"
else
    echo "  [FAIL] $MOD 不含补丁标记 —— 补丁丢失或未安装"
    echo "         修: ./tools/build-xglamor.sh --build && sudo ./tools/build-xglamor.sh"
    rc=1
fi

# 2) 运行中的 X 是否在补丁安装之后启动（glamor 只在启动时加载）
xpid=$(pgrep -x Xorg | head -1 || true)
if [ -n "${xpid:-}" ]; then
    xstart=$(stat -c %Y "/proc/$xpid" 2>/dev/null || echo 0)
    modmtime=$(stat -c %Y "$MOD" 2>/dev/null || echo 0)
    if [ "$xstart" -gt "$modmtime" ]; then
        echo "  [OK] Xorg(pid $xpid) 启动于补丁安装之后 —— 加载的是补丁版"
    else
        echo "  [WARN] Xorg(pid $xpid) 启动早于补丁安装时间 —— 需重启 X 才生效"
        rc=1
    fi
else
    echo "  [WARN] 没找到运行中的 Xorg"
    rc=1
fi

# 3) 实测（vblank 关闭）
echo "-- glmark2 build (vblank off, 3s) --"
score=$(vblank_mode=0 glmark2 -b build:duration=3 2>/dev/null \
        | awk '/glmark2 Score/{print $NF; exit}')
if [ -n "${score:-}" ]; then
    echo "  build = $score FPS"
    if [ "$score" -ge 1400 ] 2>/dev/null; then
        echo "  [OK] 落在补丁生效区间（~1600-2000）"
    else
        echo "  [WARN] 偏低（补丁生效预期 ~1600-2000；着色器路径约 900-1000）"
        rc=1
    fi
else
    echo "  [WARN] glmark2 未产出分数"
fi

echo "-- glxgears (vblank off, 5s) --"
gfps=$(vblank_mode=0 timeout 8 glxgears 2>/dev/null \
       | awk '/frames in/{print $(NF-1); exit}')
echo "  glxgears = ${gfps:-N/A} FPS  (着色器路径约 2300；补丁生效预期 >2800)"

echo "== 结果: $([ $rc -eq 0 ] && echo 通过 || echo 需处理) =="
exit $rc