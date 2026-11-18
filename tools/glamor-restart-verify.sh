#!/bin/sh
# 带自动回滚的"重启 X 并验证 glamor blit 快路径"脚本。
#
# 背景：tools/build-xglamor.sh 装上的 libglamoregl.so 只有在 X 启动时才会被加载，
# 所以装完必须重启 lightdm 才能生效。如果新模块有问题，X 会起不来，桌面会话
# 就没了。本脚本把这件事做成一次性、可自恢复的操作：
#
#   1. 重启 lightdm；
#   2. 等新的 Xorg 起来并稳定存活若干秒；
#   3. 起得来 -> 跑 tools/verify-glamor-patch.sh 报告实测数字；
#      起不来 -> 自动 --rollback 还原原模块再重启一次，并明确告知结果；
#   4. 全程写日志到 /tmp/glamor-restart-<时间>.log。
#
# 注意：重启 lightdm 会关掉当前桌面会话里的所有程序（浏览器、终端等），
# 正在跑的本 agent 若挂在桌面终端的 pty 上也会随之结束。请在本地 TTY 或
# SSH 里执行本脚本，或者在确认可以断开当前会话后再执行。
#
# 用法：
#   sudo ./tools/glamor-restart-verify.sh            # 重启 + 验证（推荐的入口）
#   sudo ./tools/glamor-restart-verify.sh --dry-run  # 只检查前置条件，不动 X
#   sudo ./tools/glamor-restart-verify.sh --no-verify # 只重启，不跑基准
#
# 退出码：0 = X 已用新模块起来；1 = 前置条件不满足；2 = 回滚后恢复；3 = 回滚也没起来

set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
MODULE=/usr/lib/xorg/modules/libglamoregl.so
BACKUP=$MODULE.mwv207-orig
XAUTH=${XAUTH:-/home/admin/.Xauthority}
LOG=/tmp/glamor-restart-$(date +%Y%m%d-%H%M%S).log

DRY=0
RUN_VERIFY=1
case "${1:-}" in
    --dry-run)   DRY=1 ;;
    --no-verify) RUN_VERIFY=0 ;;
    "")          ;;
    *) echo "用法: $0 [--dry-run|--no-verify]" >&2; exit 1 ;;
esac

say() { printf '%s\n' "$*" | tee -a "$LOG"; }
die() { say "错误: $*"; exit 1; }

start_log() { : > "$LOG"; say "log: $LOG"; }

[ "$(id -u)" -eq 0 ] || die "需要 root（会重启 lightdm、读写 /usr/lib/xorg/modules）"

start_log

say "== 前置检查 =="
if [ -e "$MODULE" ]; then
    MOD_MD5=$(md5sum "$MODULE" | awk '{print $1}')
    say "当前模块 md5: $MOD_MD5"
else
    die "$MODULE 不存在"
fi
if [ -f "$BACKUP" ]; then
    say "回滚备份:    $BACKUP ($(md5sum "$BACKUP" | awk '{print $1}'))"
else
    say "警告: 没有 $BACKUP，回滚将退化为「保留当前模块」"
fi

if strings "$MODULE" | grep -q GLAMOR_NO_BLIT; then
    say "模块含 GLAMOR_NO_BLIT 快路径: 是"
    HAVE_PATCH=1
else
    say "模块含 GLAMOR_NO_BLIT 快路径: 否 —— 重启也不会有收益，请先跑 ./tools/build-xglamor.sh"
    HAVE_PATCH=0
fi

if ! systemctl is-active --quiet lightdm; then
    say "lightdm 当前不是 active（可能用其它 DM），重启方式需要人工确认"
    DM_OK=0
else
    say "lightdm: active"
    DM_OK=1
fi

if [ "$DRY" = 1 ]; then
    say "== --dry-run：不做任何修改 =="
    exit 0
fi

[ "$DM_OK" = 1 ] || die "lightdm 未运行，未做任何修改"
[ "$HAVE_PATCH" = 1 ] || die "模块不含快路径补丁，未做任何修改"

say ""
say "== 重启 lightdm（会关闭当前桌面会话） =="
BEFORE_XORG=$(pgrep -x Xorg | head -1 || true)
T0=$(date +%s)
say "重启前 Xorg pid: ${BEFORE_XORG:-none}"
systemctl restart lightdm || die "systemctl restart lightdm 失败"

# 等新的 Xorg 进程出现（旧的 pid 消失、新的 pid 出现）
NEW_XORG=""
i=0
while [ $i -lt 30 ]; do
    sleep 1
    i=$((i + 1))
    CUR=$(pgrep -x Xorg | head -1 || true)
    if [ -n "$CUR" ] && [ "$CUR" != "$BEFORE_XORG" ]; then
        NEW_XORG=$CUR
        say "新的 Xorg 起来了: pid=$NEW_XORG (等待 ${i}s)"
        break
    fi
done

if [ -z "$NEW_XORG" ]; then
    say "!! 30s 内没有看到新的 Xorg 进程"
    ROLLBACK=1
else
    # 再观察 12s，确认不是起来就崩（闪烁重启）
    say "观察 12s 确认 Xorg 稳定..."
    sleep 12
    if kill -0 "$NEW_XORG" 2>/dev/null; then
        say "Xorg pid=$NEW_XORG 仍然存活"
        ROLLBACK=0
    else
        say "!! Xorg pid=$NEW_XORG 已经退出（模块可能有问题）"
        ROLLBACK=1
    fi
fi

if [ "$ROLLBACK" = 1 ]; then
    say ""
    say "== 回滚 =="
    if [ -f "$BACKUP" ]; then
        cp -a "$BACKUP" "$MODULE" && say "已还原 $MODULE"
    else
        say "没有备份，无法回滚；请手动处理"
    fi
    systemctl restart lightdm || true
    say "已再次重启 lightdm"
    sleep 5
    if pgrep -x Xorg >/dev/null 2>&1; then
        say "结果: 回滚后 X 已恢复（当前仍是原模块，present 会慢 ~2.6×）"
        exit 2
    fi
    say "结果: 回滚后 X 仍未起来，需要人工介入（本地 TTY 登录排查）"
    exit 3
fi

say ""
say "== X 已用新模块启动 =="
if [ -f /var/log/Xorg.0.log ]; then
    say "Xorg.0.log 中 glamor 相关行:"
    grep -iE 'glamor|AIGLX' /var/log/Xorg.0.log | tail -5 | tee -a "$LOG" || true
fi

if [ "$RUN_VERIFY" = 1 ] && [ -x "$REPO/tools/verify-glamor-patch.sh" ]; then
    say ""
    say "== 等待桌面会话可用（最多 30s） =="
    i=0
    while [ $i -lt 30 ]; do
        if sudo -u admin env DISPLAY=:0 XAUTHORITY="$XAUTH" xdpyinfo >/dev/null 2>&1; then
            say "xdpyinfo OK（${i}s）"
            break
        fi
        sleep 1
        i=$((i + 1))
    done
    say ""
    say "== 实测 =="
    if command -v sudo >/dev/null 2>&1 && id -u admin >/dev/null 2>&1; then
        sudo -u admin env DISPLAY=:0 XAUTHORITY="$XAUTH" \
            "$REPO/tools/verify-glamor-patch.sh" 2>&1 | tee -a "$LOG"
    else
        env DISPLAY=:0 XAUTHORITY="$XAUTH" "$REPO/tools/verify-glamor-patch.sh" 2>&1 | tee -a "$LOG"
    fi
fi

say ""
say "完成。日志: $LOG"
exit 0