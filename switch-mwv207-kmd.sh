#!/bin/bash
# switch-mwv207-kmd.sh — 在内核内建的开源 mwv207 KMD 与"自建带修复版"之间切换
#
# 与 switch-mwv207-oss.sh 的分工：
#   那个脚本只管用户态（Mesa/VA/VDPAU/OpenCL），**故意不含 KMD 段**，
#   因为内核内建的 mwv207 已是 0731:9100 的 modules.alias 默认。
#   本脚本补上 KMD 那一层，且与用户态完全解耦：换 KMD 不影响 libGL 选型。
#
# 用法：
#   ./switch-mwv207-kmd.sh check            预检（不需要 root）
#   ./switch-mwv207-kmd.sh build            构建 src/kernel-6.6（不需要 root）
#   ./switch-mwv207-kmd.sh status           看现状：内建/自建/实际加载 三者对照
#   ./switch-mwv207-kmd.sh install          安装自建模块并热切换（需 root）
#   ./switch-mwv207-kmd.sh rollback         恢复内核内建模块（需 root）
#   ./switch-mwv207-kmd.sh verify           校验驱动是否健康
#
# 演练（零副作用，不需要 root）：
#   ./switch-mwv207-kmd.sh --root /tmp/fake rehearse
#     在假根里跑一遍 install → rollback 的文件操作，用 md5 证明可完全还原。
#   ./switch-mwv207-kmd.sh --dry-run install
#     连真根也只打印命令不执行（privileged ops 一律 echo）。
#
# 设计要点（都是踩过的坑）：
#  1. 模块名叫 mwv207，与内核内建的**同名**。所以"谁生效"不能靠名字判断，
#     只能比对 **srcversion**：/sys/module/mwv207/srcversion 是已加载的那个，
#     两个候选 .ko 各自 modinfo -F srcversion 是磁盘上的。三者一比就清楚。
#  2. 自建模块放 /lib/modules/$KVER/updates/dkms/ —— depmod 的既有顺序里
#     /updates 优先于 /kernel，所以 depmod -a 之后 modprobe 会挑到它。
#  3. 换模块必须先 `modprobe -r`，而 Xorg 持有 /dev/dri（refcnt 常是几十），
#     所以顺序固定为：停显示管理器 → 确认无 /dev/dri 占用 → rmmod →
#     depmod → modprobe → 校验 → 起显示管理器 → 再校验。任一步失败自动回滚。
#  4. 回滚不是"删掉了事"：内建 .ko 一直都在，回滚 = 删掉 updates 里的副本 +
#     depmod -a + 重新 modprobe。但为防内建 .ko 被人动过，install 前先备份并
#     记 md5，rollback 后要求它逐字节不变。
#  5. 签名不挡路（SecureBoot 已关、lockdown none），所以不做签模块这一步。

set -u

KVER=$(uname -r)
WORKDIR=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
SRC_DIR="$WORKDIR/src/kernel-6.6"
KOBJ="/lib/modules/$KVER/build"

# 内建（内核自带）与自建（放 updates）两个位置
IN_TREE_REL="kernel/drivers/gpu/drm/mwv207/mwv207.ko"
DKMS_REL="updates/dkms/mwv207.ko"

# --root 把整个"系统"搬到假根下，用来演练；--dry-run 只打印特权命令
R=""
DRY_RUN=0

MODROOT="$R/lib/modules/$KVER"
STATE="$R/var/lib/mwv207-oss/kmd"
LOG_FILE="$R/var/log/mwv207-oss-kmd.log"

log()  { printf '[%s] %s\n' "$(date '+%F %T')" "$*"; }
warn() { printf '[%s] [WARN] %s\n' "$(date '+%F %T')" "$*"; }
die()  { printf '[%s] [FAIL] %s\n' "$(date '+%F %T')" "$*" >&2; exit 1; }

# 特权命令统一走 run()：演练或 dry-run 时只打印
run() {
    if [ -n "$R" ] || [ "$DRY_RUN" = 1 ]; then
        printf '  + %s\n' "$*"
        return 0
    fi
    printf '  + %s\n' "$*"
    "$@"
}

need_root() {
    [ -n "$R" ] && return 0          # 演练不需要 root
    [ "$DRY_RUN" = 1 ] && return 0
    [ "$(id -u)" = 0 ] || die "需要 root：sudo $0 $*"
}

# ---- 模块身份 ----

# 已加载模块的 srcversion（未加载则空）
loaded_srcver() {
    cat /sys/module/mwv207/srcversion 2>/dev/null
}
loaded_refcnt() {
    awk '$1=="mwv207"{print $3}' /proc/modules 2>/dev/null
}
ko_srcver() {
    [ -f "$1" ] || { echo ""; return; }
    modinfo -F srcversion "$1" 2>/dev/null
}
ko_field() {
    [ -f "$1" ] || { echo ""; return; }
    modinfo -F "$2" "$1" 2>/dev/null
}
# modprobe 下次会挑哪个文件
next_module_path() {
    modinfo -n mwv207 2>/dev/null
}

# ---- check ----

cmd_check() {
    local rc=0
    log "预检（内核 $KVER）"

    if [ -d "$R$KOBJ" ] || [ -d "$KOBJ" ]; then
        log "  [OK] 内核构建目录: $KOBJ"
    else
        warn "  缺少内核构建目录 $KOBJ（需要 linux-headers-$KVER）"; rc=1
    fi

    if command -v make >/dev/null && command -v modinfo >/dev/null; then
        log "  [OK] make / modinfo 可用"
    else
        warn "  缺少 make 或 modinfo"; rc=1
    fi

    if [ -f "$R$MODROOT/$IN_TREE_REL" ] || [ -f "$MODROOT/$IN_TREE_REL" ]; then
        log "  [OK] 内核内建模块存在: $MODROOT/$IN_TREE_REL"
    else
        warn "  找不到内核内建模块，rollback 将无从恢复"; rc=1
    fi

    # SecureBoot / lockdown
    if command -v mokutil >/dev/null 2>&1; then
        case "$(mokutil --sb-state 2>/dev/null)" in
            *enabled*) warn "  SecureBoot 已开启：自建模块需要签名，否则加载失败"; rc=1;;
            *)         log "  [OK] SecureBoot 未开启";;
        esac
    fi
    [ -r "$R/sys/kernel/security/lockdown" ] && \
        log "  lockdown: $(cat "$R/sys/kernel/security/lockdown")"

    # 当前加载情况
    local lv; lv=$(loaded_srcver)
    if [ -n "$lv" ]; then
        log "  当前已加载 mwv207，srcversion=$lv，refcnt=$(loaded_refcnt)"
        log "  下次 modprobe 会挑: $(next_module_path)"
    else
        warn "  mwv207 当前未加载"
    fi

    # /dev/dri 占用（决定能不能 rmmod）
    if command -v fuser >/dev/null 2>&1 && [ -z "$R" ]; then
        local users
        users=$(fuser /dev/dri/* 2>/dev/null | tr -s ' ')
        [ -n "${users// /}" ] && warn "  /dev/dri 正被占用（停显示管理器后才可能 rmmod）: $users"
    fi

    # 磁盘空间（模块 5.6MB，备份一次也就 6MB 级）
    local avail; avail=$(df -Pk / 2>/dev/null | awk 'NR==2{print $4}')
    if [ -n "$avail" ] && [ "$avail" -lt 102400 ]; then
        warn "  / 剩余不足 100MB"; rc=1
    fi

    [ "$rc" = 0 ] && log "预检通过" || warn "预检有问题（见上）"
    return "$rc"
}

# ---- build ----

cmd_build() {
    [ -d "$SRC_DIR" ] || die "源码目录不存在: $SRC_DIR"
    log "构建 $SRC_DIR"
    make -C "$KOBJ" M="$SRC_DIR" CONFIG_DRM_MWV207=m || die "构建失败"
    [ -f "$SRC_DIR/mwv207.ko" ] || die "没有产出 mwv207.ko"

    local vm kvm
    vm=$(ko_field "$SRC_DIR/mwv207.ko" vermagic)
    kvm=$(cat "$KOBJ/include/config/kernel.release" 2>/dev/null || echo "$KVER")
    log "  产物: $(stat -c %s "$SRC_DIR/mwv207.ko") B  srcversion=$(ko_srcver "$SRC_DIR/mwv207.ko")"
    log "  vermagic: $vm"
    case "$vm" in
        "$kvm"*) log "  [OK] vermagic 与目标内核一致";;
        *)       die "vermagic 不匹配（期望 $kvm）";;
    esac
}

# ---- status ----

cmd_status() {
    log "KMD 状态（内核 $KVER）"
    local it="$MODROOT/$IN_TREE_REL" dk="$MODROOT/$DKMS_REL"
    printf '  %-28s %s\n' "内建模块:"   "$it"
    printf '  %-28s %s\n' "  存在/srcversion:" "$([ -f "$it" ] && echo "是 / $(ko_srcver "$it")" || echo 否)"
    printf '  %-28s %s\n' "自建模块:"   "$dk"
    printf '  %-28s %s\n' "  存在/srcversion:" "$([ -f "$dk" ] && echo "是 / $(ko_srcver "$dk")" || echo 否)"
    printf '  %-28s %s\n' "实际加载 srcversion:" "$(loaded_srcver)"
    printf '  %-28s %s\n' "refcnt:" "$(loaded_refcnt)"
    printf '  %-28s %s\n' "下次 modprobe 挑:" "$(next_module_path)"

    local lv is dkv; lv=$(loaded_srcver); is=$(ko_srcver "$it"); dkv=$(ko_srcver "$dk")
    if [ -z "$lv" ]; then
        echo "  → 未加载"
    elif [ -n "$dkv" ] && [ "$lv" = "$dkv" ]; then
        echo "  → 正在跑【自建版】"
    elif [ "$lv" = "$is" ]; then
        echo "  → 正在跑【内核内建版】"
    else
        echo "  → 正在跑一个两边都对不上的版本（源码/sysfs 不一致？）"
    fi

    [ -f "$STATE/backup.md5" ] && log "  上次备份: $(cat "$STATE/backup.md5")"
}

# ---- 备份 / 恢复 ----

do_backup() {
    local it="$MODROOT/$IN_TREE_REL"
    [ -f "$it" ] || die "找不到内建模块，拒绝继续: $it"
    run mkdir -p "$STATE"
    run cp -a "$it" "$STATE/mwv207.ko.in-tree"
    local m; m=$(md5sum "$it" | awk '{print $1}')
    if [ -z "$R" ] && [ "$DRY_RUN" = 0 ]; then echo "$m" > "$STATE/backup.md5"; fi
    log "  已备份内建模块 → $STATE/mwv207.ko.in-tree (md5 $m)"
    echo "$m"
}

# ---- 显示管理器 ----

stop_display() {
    if systemctl list-unit-files 2>/dev/null | grep -q '^lightdm\.service'; then
        log "  停止 lightdm（释放 /dev/dri）"
        run systemctl stop lightdm
        sleep 2
    else
        warn "  没有 lightdm，需自行确保 X/合成器已退出"
    fi
}

start_display() {
    if systemctl list-unit-files 2>/dev/null | grep -q '^lightdm\.service'; then
        run systemctl start lightdm
    fi
}

wait_x_ok() {
    local i
    [ -n "$R" ] && return 0
    for i in $(seq 1 20); do
        if grep -qE 'glamor X acceleration enabled|glamor initialized' /var/log/Xorg.0.log 2>/dev/null; then
            log "  [OK] Xorg glamor 已点亮"
            return 0
        fi
        sleep 1
    done
    warn "  Xorg 日志里没看到 glamor 标记"
    return 1
}

# ---- install ----

cmd_install() {
    need_root install
    cmd_build
    local src="$SRC_DIR/mwv207.ko" it="$MODROOT/$IN_TREE_REL" dk="$MODROOT/$DKMS_REL"

    log "安装自建模块"
    local backup_md5; backup_md5=$(do_backup)
    [ -n "$backup_md5" ] || die "备份失败"

    # 装到 updates（depmod 顺序里优先）
    run install -D -m 0644 "$src" "$dk"
    run depmod -a "$KVER"

    local installed; installed=$(ko_srcver "$dk")
    log "  已放置: $dk (srcversion $installed)"

    stop_display
    log "  卸载旧模块并加载新模块"
    if ! run modprobe -r mwv207; then
        warn "  modprobe -r 失败（仍被占用）"
        do_rollback
        die "无法卸载旧模块，已回滚"
    fi
    if ! run modprobe mwv207; then
        warn "  modprobe 新模块失败"
        do_rollback
        die "新模块加载失败，已回滚"
    fi

    sleep 1
    local lv; lv=$(loaded_srcver)
    if [ "$lv" != "$installed" ]; then
        warn "  加载后 srcversion=$lv，期望 $installed"
        do_rollback
        die "加载的不是新模块，已回滚"
    fi

    start_display
    wait_x_ok || { warn "  X 未点亮"; do_rollback; die "显示未恢复，已回滚"; }

    log "安装完成：正在跑【自建版】(srcversion $lv)"
    cmd_verify
}

# ---- rollback ----

do_rollback() {
    local it="$MODROOT/$IN_TREE_REL" dk="$MODROOT/$DKMS_REL"
    log "回滚到内核内建模块"
    stop_display
    [ -f "$dk" ] && run rm -f "$dk"
    run depmod -a "$KVER"
    run modprobe -r mwv207
    run modprobe mwv207
    sleep 1
    local lv is; lv=$(loaded_srcver); is=$(ko_srcver "$it")
    log "  回滚后 srcversion=$lv（内建应为 $is）"

    # 内建 .ko 必须逐字节没被我们动过
    if [ -f "$STATE/backup.md5" ] && [ -z "$R" ] && [ "$DRY_RUN" = 0 ]; then
        local want now
        want=$(cat "$STATE/backup.md5"); now=$(md5sum "$it" | awk '{print $1}')
        [ "$want" = "$now" ] && log "  [OK] 内建模块 md5 未变 ($now)" \
                             || warn "  内建模块 md5 变了: $want → $now"
    fi
    start_display
}

cmd_rollback() {
    need_root rollback
    do_rollback
    log "已回滚"
}

# ---- verify ----

cmd_verify() {
    local rc=0 it="$MODROOT/$IN_TREE_REL" dk="$MODROOT/$DKMS_REL"
    log "校验"
    local lv is dkv
    lv=$(loaded_srcver); is=$(ko_srcver "$it"); dkv=$(ko_srcver "$dk")

    if [ -z "$lv" ]; then warn "  mwv207 未加载"; rc=1
    elif [ -n "$dkv" ] && [ "$lv" = "$dkv" ]; then log "  [OK] 正在跑自建版 ($lv)"
    elif [ "$lv" = "$is" ]; then log "  [OK] 正在跑内建版 ($lv)"
    else warn "  srcversion=$lv 与两个候选都不符"; rc=1; fi

    [ -e /dev/dri/renderD128 ] && log "  [OK] /dev/dri/renderD128 在" || { warn "  没有 renderD128"; rc=1; }
    [ -e /dev/dri/card0 ]      && log "  [OK] /dev/dri/card0 在"      || { warn "  没有 card0"; rc=1; }

    # DRM 版本（内核给的 name/date，能区分是否真是 mwv207）
    if [ -x /tmp/drmver ]; then
        local out; out=$(/tmp/drmver 2>/dev/null | tr '\n' ' ')
        [ -n "$out" ] && log "  DRM: $out"
    fi

    # 连到显示器的头像
    for c in /sys/class/drm/card*-*; do
        [ -e "$c/status" ] || continue
        local st; st=$(cat "$c/status" 2>/dev/null)
        [ "$st" = "connected" ] && log "  显示器: $(basename "$c") $st"
    done

    # 用户态是否还能画（要 X）
    if [ -n "${DISPLAY:-}" ] && command -v glxinfo >/dev/null 2>&1; then
        glxinfo -B 2>/dev/null | grep -E 'OpenGL renderer|OpenGL version' | sed 's/^/  /'
    fi

    [ "$rc" = 0 ] && log "校验通过" || warn "校验有问题"
    return "$rc"
}

# ---- 演练：假根里 install → rollback ----

cmd_rehearse() {
    [ -n "$R" ] || die "rehearse 必须配合 --root 使用，例如: $0 --root /tmp/kmd-rehearse rehearse"
    local it="$MODROOT/$IN_TREE_REL" dk="$MODROOT/$DKMS_REL"
    log "演练（假根 $R，零副作用）"

    [ -f "$it" ] || die "假根里没有内建模块，先把一个 .ko 拷到 $it"
    local before; before=$(md5sum "$it" | awk '{print $1}')
    log "  演练前 内建 md5=$before"

    log "—— 第 1 步：模拟 install 的文件操作 ——"
    local bm; bm=$(do_backup)
    mkdir -p "$STATE"
    cp -a "$it" "$STATE/mwv207.ko.in-tree"
    mkdir -p "$(dirname "$dk")" && cp -a "$SRC_DIR/mwv207.ko" "$dk"
    local dkv; dkv=$(ko_srcver "$dk")
    log "  假根内自建模块 srcversion=$dkv"
    [ "$dkv" = "$(ko_srcver "$SRC_DIR/mwv207.ko")" ] \
        && log "  [OK] 自建模块已就位" || die "自建模块没就位"

    log "—— 第 2 步：模拟 rollback ——"
    rm -f "$dk"
    [ -f "$dk" ] && die "rollback 没删掉自建模块"
    log "  [OK] updates 里的副本已删除"

    local after; after=$(md5sum "$it" | awk '{print $1}')
    log "  演练后 内建 md5=$after"
    if [ "$before" = "$after" ]; then
        log "  [OK] 内建模块逐字节未变 → 回滚可完全还原"
    else
        die "内建模块被改动了！$before → $after"
    fi
    if [ -f "$STATE/mwv207.ko.in-tree" ]; then
        local bmd; bmd=$(md5sum "$STATE/mwv207.ko.in-tree" | awk '{print $1}')
        [ "$bmd" = "$before" ] && log "  [OK] 备份副本与原件一致 ($bmd)" \
                               || warn "  备份副本与原件不一致: $bmd vs $before"
    fi
    # 关键一步：证明 depmod 会优先挑 updates 里的那份（否则装了也白装）
    log "—— 第 3 步：验证 depmod 的挑选顺序（两边同名时谁赢）——"
    if command -v depmod >/dev/null 2>&1; then
        mkdir -p "$(dirname "$dk")" && cp -a "$SRC_DIR/mwv207.ko" "$dk"
        local base all entry
        base=$(cd "$R" && pwd)
        all=$(depmod -b "$base" -n "$KVER" 2>/dev/null)
        entry=$(printf '%s\n' "$all" | grep -m1 '^.*mwv207\.ko:$')
        if [ -z "$entry" ]; then
            warn "  depmod -n 无输出（假根缺内核符号表，属正常）——此项未验证"
        else
            printf '  %s\n' "$entry"
            printf '%s\n' "$all" | grep '^alias pci:.* mwv207$' | sed 's/^/  /'
            case "$entry" in
                updates/dkms/mwv207.ko:*)
                    log "  [OK] modules.dep 选的是 updates 里的自建模块（/updates 优先于 /kernel）";;
                *)
                    warn "  modules.dep 没按预期选 updates，需要复核";;
            esac
            if printf '%s\n' "$all" | grep -q '^softdep mwv207 pre: governor_performance'; then
                log "  [OK] 自建模块带 patch 0008 的 softdep"
            else
                warn "  softdep 缺失（检查 mwv207_drv.c 的 MODULE_SOFTDEP）"
            fi
        fi
        rm -f "$dk"
        [ -f "$dk" ] && warn "  清理失败：$dk 还在"
    else
        warn "  没有 depmod，跳过顺序验证"
    fi

    log "演练通过：install 的落点 / rollback 的清除 / 内建模块的不变性 / depmod 的优先级 均已验证"
    log "（真实加载/卸载/显示恢复只能在真机上验，见 --dry-run install 打印的命令序列）"
}

# ---- dry-run 打印 ----

cmd_dryrun() {
    local src="$SRC_DIR/mwv207.ko" it="$MODROOT/$IN_TREE_REL" dk="$MODROOT/$DKMS_REL"
    log "dry-run：计划在真机上执行的命令序列（本次不执行任何特权操作）"
    log "当前状态"
    printf '  内建模块      %s  srcversion=%s\n' "$it" "$(ko_srcver "$it")"
    printf '  自建模块      %s  srcversion=%s\n' "$src" "$(ko_srcver "$src")"
    printf '  已加载        srcversion=%s  refcnt=%s\n' "$(loaded_srcver)" "$(loaded_refcnt)"
    printf '  下次 modprobe %s\n' "$(next_module_path)"
    log "计划步骤（共 9 步，任一步失败自动回滚）"
    cat <<EOF
  1) 备份内建模块     cp -a $it $STATE/mwv207.ko.in-tree
  2) 放置自建模块     install -D -m 0644 $src $dk
  3) 刷新模块索引     depmod -a $KVER
  4) 停显示管理器     systemctl stop lightdm
  5) 卸载旧模块       modprobe -r mwv207          # 现 refcnt=$(loaded_refcnt)
  6) 加载新模块       modprobe mwv207
  7) 校验身份         cat /sys/module/mwv207/srcversion   # 期望 $(ko_srcver "$src")
  8) 起显示管理器     systemctl start lightdm
  9) 等 glamor 点亮   grep -E 'glamor X acceleration enabled|glamor initialized' /var/log/Xorg.0.log
  回滚 = 删 $dk + depmod -a + 重加载内建 + 起 lightdm + 核对内建 md5 未变
EOF
}

usage() {
    sed -n '2,40p' "$0" | sed 's/^# \{0,1\}//'
}

main() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --root)   R="$2"; shift 2; MODROOT="$R/lib/modules/$KVER"; STATE="$R/var/lib/mwv207-oss/kmd"; LOG_FILE="$R/var/log/mwv207-oss-kmd.log";;
            --root=*) R="${1#*=}"; shift; MODROOT="$R/lib/modules/$KVER"; STATE="$R/var/lib/mwv207-oss/kmd";;
            --dry-run) DRY_RUN=1; shift;;
            check)    cmd_check; exit $?;;
            build)    cmd_build; exit 0;;
            status)   cmd_status; exit 0;;
            install)  if [ "$DRY_RUN" = 1 ]; then cmd_dryrun; else cmd_install; fi; exit $?;;
            rollback) cmd_rollback; exit 0;;
            verify)   cmd_verify; exit $?;;
            rehearse) cmd_rehearse; exit 0;;
            *)        usage; exit 1;;
        esac
    done
    usage
}

main "$@"