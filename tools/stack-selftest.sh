#!/usr/bin/env bash
# 三态切换自测: builtin → oss → vendor → oss
# 必须在 systemd 作用域运行(停 lightdm 会杀掉会话), 结果写 /var/log/mwv207-selftest.log
set -u
SELF=/home/admin/jm9100-oss/switch-mwv207.sh
RES=/run/mwv207-stack-result
LOG=/var/log/mwv207-selftest.log
W6=/home/admin/vp9-assets/w6.webm
W6_SW=/home/admin/vp9-assets/w6_sw.yuv

log() { echo "[$(date '+%F %T')] $*" >>"$LOG"; }
snap() { { "$SELF" stack status 2>&1 | sed -n '1,10p'; } >>"$LOG"; }
cur_state() { "$SELF" stack status 2>/dev/null | sed -n 's/.*\[\([a-z]*\)\].*/\1/p' | head -1; }

wait_result() { # $1=期望 target $2=超时秒数
    local want="$1" t="${2:-240}" i=0 line
    while [ $i -lt "$t" ]; do
        if [ -s "$RES" ]; then
            line=$(cat "$RES" 2>/dev/null)
            log "  切换结果: $line"
            case "$line" in *failed*) return 1 ;; esac
            case "$line" in *target="$want"*) return 0 ;; esac
        fi
        sleep 2; i=$((i+2))
    done
    log "  等待结果超时 (${t}s)"
    return 1
}

switch_to() {
    local target="$1" cur
    cur=$(cur_state)
    if [ "$cur" = "$target" ]; then log "  已是 $target, 跳过切换"; return 0; fi
    rm -f "$RES"
    "$SELF" stack "$target" >>"$LOG" 2>&1 || { log "  阶段1失败 (桌面未受影响)"; return 1; }
    wait_result "$target" 300
}

x_up()     { pgrep -x Xorg >/dev/null 2>&1; }
va_ok()    { timeout 40 vainfo 2>/dev/null | grep -q VAProfileVP9Profile0; }
decode_ok() {
    local out=/tmp/selftest_w6.yuv
    rm -f "$out"
    MWV207_VP9_CLEAR=1 LIBVA_DRIVER_NAME=mwv207 timeout 120 \
        ffmpeg -v error -y -hwaccel vaapi -hwaccel_device /dev/dri/renderD128 \
        -hwaccel_output_format vaapi -i "$W6" -vf hwdownload,format=nv12 \
        -pix_fmt yuv420p -f rawvideo "$out" >>"$LOG" 2>&1
    [ -s "$out" ] && cmp -s "$out" "$W6_SW"
}

check_common() {
    sleep 3; snap
    if x_up; then log "  Xorg: up"; else log "  Xorg: DOWN"; fi
    if va_ok; then log "  vainfo: VAProfileVP9Profile0 OK"; else log "  vainfo: FAIL"; fi
    if decode_ok; then log "  w6 硬解: 逐字节一致"; else log "  w6 硬解: 不一致/失败"; fi
}

main() {
    : >"$LOG"
    log "==== 三态切换自测开始 (由 $(id -un)@$(hostname) 触发) ===="
    snap

    log "---- [1/3] 切到 builtin (内核内建驱动) ----"
    if switch_to builtin; then check_common; else log "  builtin 切换失败"; fi

    log "---- [2/3] 切到 oss (自建修复驱动) ----"
    if switch_to oss; then check_common; else log "  oss 切换失败"; fi

    log "---- [3/3] 切到 vendor (厂商驱动) ----"
    if switch_to vendor; then
        sleep 3; snap
        if [ -e /dev/jmgpu ]; then log "  /dev/jmgpu: present"; else log "  /dev/jmgpu: absent"; fi
        if timeout 40 env LIBVA_DRIVER_NAME=jmgpu vainfo 2>/dev/null | grep -q VAProfileVP9Profile0; then
            log "  厂商 vainfo: OK"; else log "  厂商 vainfo: FAIL"; fi
        if x_up; then log "  Xorg: up (厂商栈下意外)"; else log "  Xorg: down (预期)"; fi
    else log "  vendor 切换失败"; fi

    log "---- 收尾: 回到 oss ----"
    if switch_to oss; then check_common; else log "  收尾切换失败, 系统停在 $(stack_detect 2>/dev/null || true)"; fi
    log "==== 自测结束; 最终: $(cur_state) ===="
    log "日志: $LOG"
    "$SELF" stack status >>"$LOG" 2>&1
}
main "$@"
