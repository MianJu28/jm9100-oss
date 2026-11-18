#!/bin/bash
# overlay-postrestart-watch.sh —— 重启 X 之后自动跑验证, 结果落到日志文件
#
# 为什么需要它: 重启 lightdm 会经 pty 挂断把 agent/终端里的会话杀掉, 而验证
# 必须在**新**的 Xorg 起来之后才能做。所以本脚本在重启**之前**以脱离会话的
# 方式挂着(detached), 等新的 Xorg 出现后自动把该跑的都跑完并写报告。
#
# 用法(重启前):
#   setsid nohup ./tools/overlay-postrestart-watch.sh >/tmp/overlay-watch.out 2>&1 &
#   sudo systemctl restart lightdm
#
# 重启后看:
#   cat /tmp/overlay-watch.out          # 等着脚本自己输出
#   ls -t /tmp/overlay-report-*.log | head -1
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
STAMP=$(date '+%Y%m%d-%H%M%S')
REPORT=/tmp/overlay-report-$STAMP.log
LOG=/var/log/Xorg.0.log
WAIT_NEW_XORG=${WAIT_NEW_XORG:-90}
WAIT_DPY=${WAIT_DPY:-60}
DRY=false
[ "${1:-}" = "--dry-run" ] && DRY=true

say() { printf '%s %s\n' "$(date '+%H:%M:%S')" "$*"; }

say "watch 启动 (report: $REPORT)"
if [ "$DRY" = true ]; then
    say "[dry-run] 逻辑:"
    say "  1) 记录当前 Xorg pid"
    say "  2) 等新 Xorg pid 出现(最多 ${WAIT_NEW_XORG}s), 再观察 10s 存活"
    say "  3) 等 DISPLAY 可用(最多 ${WAIT_DPY}s)"
    say "  4) 跑 tools/verify-overlay-present.sh 全量"
    say "  5) 跑 present-bench 300/800x600/1080p, glmark2 build + 全套"
    say "  6) 跑 GL_DURATION=1 ./tools/gl-regression.sh"
    say "  7) 抓 Xorg.0.log 尾部与 overlay 统计, 汇总到 $REPORT"
    exit 0
fi

OLD_PID=$(pgrep -x Xorg | head -1)
say "旧 Xorg pid=${OLD_PID:-none}(等重启)"

NEW_PID=""
deadline=$(( $(date +%s) + WAIT_NEW_XORG ))
while [ "$(date +%s)" -lt "$deadline" ]; do
    pid=$(pgrep -x Xorg | head -1)
    if [ -n "$pid" ] && [ "$pid" != "$OLD_PID" ]; then
        sleep 10
        if kill -0 "$pid" 2>/dev/null; then
            NEW_PID=$pid
            break
        fi
        say "新 Xorg $pid 起来后又退出了, 继续等"
        OLD_PID=$pid
    fi
    sleep 2
done

if [ -z "$NEW_PID" ]; then
    say "!! 没有等到新的 Xorg —— X 可能起不来"
    {
        echo "== overlay 重启后验证报告 $STAMP =="
        echo "结果: 新 Xorg 未出现(等待 ${WAIT_NEW_XORG}s)"
        echo
        echo "== systemctl status lightdm =="; systemctl status lightdm --no-pager -l 2>&1 | tail -20
        echo; echo "== Xorg.0.log 尾部 =="; tail -60 "$LOG" 2>/dev/null
        echo; echo "== (EE) 行 =="; grep -n '(EE)' "$LOG" 2>/dev/null | tail -30
        echo; echo "回滚: cd $REPO && sudo ./tools/build-xddx.sh --rollback && sudo systemctl restart lightdm"
    } > "$REPORT"
    say "报告写好了: $REPORT"
    exit 3
fi

say "新 Xorg pid=$NEW_PID, 等 DISPLAY"

export DISPLAY=${DISPLAY:-:0}
export XAUTHORITY=${XAUTHORITY:-/home/$USER/.Xauthority}
deadline=$(( $(date +%s) + WAIT_DPY ))
until xdpyinfo >/dev/null 2>&1; do
    [ "$(date +%s)" -ge "$deadline" ] && { say "DISPLAY 一直不可用"; break; }
    sleep 1
done

{
    echo "== overlay 重启后验证报告 $STAMP =="
    echo "新 Xorg pid=$NEW_PID 启动于 $(ps -o lstart= -p "$NEW_PID")"
    echo "DISPLAY=$DISPLAY  XAUTHORITY=$XAUTHORITY"
    echo
    echo "== 1. overlay 判定 =="
    "$REPO/tools/verify-overlay-present.sh" 2>&1
    echo "   (verify 退出码: $?)"
    echo
    echo "== 2. glamor blit 快路径 =="
    "$REPO/tools/verify-glamor-patch.sh" 2>&1
    echo
    echo "== 3. present 分尺寸(窗口态, vblank_mode=0) =="
    B=$REPO/tests/present-bench/present-bench
    [ -x "$B" ] || cc -O2 -o "$B" "$REPO/tests/present-bench/present-bench.c" -lepoxy -lX11
    for spec in "300 300 200 1" "800 600 200 1" "1920 1080 100 1"; do
        vblank_mode=0 "$B" $spec 2>&1 | tail -1
    done
    echo
    echo "== 4. glmark2 =="
    vblank_mode=0 glmark2 -b build:duration=3 2>&1 | grep -E 'FPS|Score' | tail -2
    vblank_mode=0 glmark2 -b :duration=3 2>&1 | grep -E 'Score' | tail -1
    echo
    echo "== 5. 回归 =="
    ( cd "$REPO" && GL_DURATION=1 ./tools/gl-regression.sh 2>&1 | tail -5 )
    echo
    echo "== 6. Xorg 日志 =="
    grep -nE 'overlay|mwv207oss|using overlay plane' "$LOG" 2>/dev/null | tail -15
    echo "-- overlay 统计(CloseScreen 时打印, 当前进程还没退出所以通常看不到):"
    grep -n 'overlay present: flips' "$LOG" 2>/dev/null | tail -3
    echo "-- (EE)/(WW) 尾部:"
    grep -nE '\((EE|WW)\)' "$LOG" 2>/dev/null | tail -20
} > "$REPORT" 2>&1

say "报告: $REPORT"
grep -E '^\[(OK|FAIL)\]|结果: PASS|Score:|ms/frame|pass=' "$REPORT" | tail -30
exit 0