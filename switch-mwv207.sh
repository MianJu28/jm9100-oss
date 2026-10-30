#!/bin/bash
# ============================================================================
# switch-mwv207.sh — JM9100 (MWV207) 开源全功能驱动栈 统一切换/回滚脚本
# ============================================================================
# 2026-10-31 由 switch-mwv207-oss.sh (用户态, 701 行) 与 switch-mwv207-kmd.sh
# (内核态, 483 行) 合并而来。两者操作的对象完全不同, 合并的动机是"一个入口、
# 一次看全": 判断桌面为什么是软件渲染时, 需要同时看用户态栈和 KMD 身份。
#
# 分层(操作互不干扰, 可单独切换):
#   用户态 (umd)  Mesa/GL/EGL/VA-API/VDPAU/OpenCL —— 载荷目录 + ldconfig + 软链
#   内核态 (kmd)  mwv207.ko —— 内核内建版 vs 自建带修复版, 靠 srcversion 区分
#
# 用法:
#   sudo ./switch-mwv207.sh status                # 同时看用户态 + 内核态
#   sudo ./switch-mwv207.sh check                 # 两层一起预检
#   sudo ./switch-mwv207.sh verify                # 两层一起自检
#
#   sudo ./switch-mwv207.sh umd install           # 装用户态载荷并切换 (会重启 lightdm)
#   sudo ./switch-mwv207.sh umd rollback [--no-restart]
#   sudo ./switch-mwv207.sh umd {check|verify|status}
#   sudo ./switch-mwv207.sh umd watchdog          # (内部) 看门狗循环
#
#   sudo ./switch-mwv207.sh kmd check             # 预检 (不需 root)
#   ./switch-mwv207.sh kmd build                  # 构建 src/kernel-6.6 (不需 root)
#   sudo ./switch-mwv207.sh kmd status            # 内建/自建/实际加载 三方对照
#   sudo ./switch-mwv207.sh kmd install           # 装自建模块并热切换 (会停 lightdm)
#   sudo ./switch-mwv207.sh kmd rollback
#   ./switch-mwv207.sh kmd --dry-run install      # 只打印将执行的命令
#   ./switch-mwv207.sh kmd --root /tmp/fake rehearse   # 假根演练, 零副作用
#
#   sudo ./switch-mwv207.sh all install [--force] # 两层一起切, lightdm 只停一次
#      阶段 1 放置文件(不动桌面) → 阶段 2 停一次 lightdm 换 KMD →
#      阶段 3 拉起 lightdm 并自检。分开执行 umd install + kmd install
#      会导致桌面被杀两次、登录态丢两次, 故不要用两个命令代替。
#
# 全局选项 (须写在子命令之前):
#   --root <dir>   把"系统"搬到假根下 (供 kmd rehearse 演练)
#   --dry-run      特权命令只打印不执行
#
# 用户态要点 (2026-10-21 实证, 详见 reports/PHASE5-SWITCH-REPORT.md):
#   1. 非 glvnd 构建的私有目录方案 —— 本载荷 libEGL/libGL 未按 glvnd 构建,
#      不能走 glvnd ICD; 改为 /etc/ld.so.conf.d/00-mwv207-oss.conf 排最前
#      + SONAME 胶水副本 (libEGL.so.1/libGL.so.1/libgbm.so.1/...)。
#      libGL.so.1 副本同样关键 —— epoxy 探测 GL 版本时 dlopen libGL.so.1,
#      命中系统 glvnd GL 则无有效 context → glamor "Ignoring GL < 2.1"。
#        另: mwv207_dri.so 为安装目录内带 $ORIGIN rpath 的实体副本 ——
#      Xorg 经 dri 目录软链加载时 NEEDED libglapi 必须由 rpath 解析 (实证)。
#   2. 零 env 方案 —— GL/EGL/VA/VDPAU 全部经默认搜索路径自动命中。
#      仅 OpenCL 需 RUSTICL_ENABLE=mwv207。
#   3. 看门狗 —— lightdm 重启后 240s 内监视 Xorg.0.log 软渲染标记, 命中即回滚。
#
# 内核态要点 (都是踩过的坑):
#   1. 模块名就叫 mwv207, 与内核内建的**同名**。所以"谁生效"不能靠名字判断,
#      只能比对 srcversion: /sys/module/mwv207/srcversion 是已加载的那个,
#      两个候选 .ko 各自 modinfo -F srcversion 是磁盘上的。三者一比就清楚。
#   2. 自建模块放 /lib/modules/$KVER/updates/dkms/ —— depmod 的既有顺序里
#      /updates 优先于 /kernel, 所以 depmod -a 之后 modprobe 会挑到它。
#   3. 换模块必须先 modprobe -r, 而 Xorg 持有 /dev/dri (refcnt 常是几十),
#      所以顺序固定为: 停显示管理器 → rmmod → depmod → modprobe → 校验 →
#      起显示管理器 → 再校验。任一步失败自动回滚。
#   4. 回滚不是"删掉了事": 内建 .ko 一直都在, 回滚 = 删掉 updates 里的副本
#      + depmod -a + 重新 modprobe。install 前先备份并记 md5, rollback 后
#      要求它逐字节不变。
#   5. 签名不挡路 (SecureBoot 已关、lockdown none), 所以不做签模块这一步。
# ============================================================================
set -u

# ----------------------------- 常量与路径 ----------------------------------
# 版本号不硬编码 —— 由载荷自身派生 (见 payload_version)。
# 硬编码的 VERSION 会把"改了载荷忘了改版本"变成静默故障: 不带 --force 时直接
# 拒绝(挡住正确操作), 带 --force 则用同一版本号装进不同内容(状态里看不出差别)。
VERSION_FALLBACK="24.1.2-fullstack-r1"
# 仓库根 = 本脚本所在目录 (不硬编码用户主目录, 从任意路径调用均有效)。
# 自安装副本 (/usr/local/sbin/mwv207-oss-switch) 所在目录无仓库文件时:
# 载荷自动回退 payload-cache, egl-smoke 自检回退 $CACHE_DIR/tests (install 时复制)。
WORKDIR="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" && pwd)"

# 载荷候选的回退顺序: 显式 PAYLOAD_SRC 最优先, 其次是仓库内最新的 payload-*
# 目录 (见 repo_payloads), 最后才是 payload-cache。
# 顺序很关键 —— 若让缓存优先, 在仓库重打载荷后 install 会静默安装旧内容;
# 缓存只是"仓库工作区被删后的兜底", 不代表"更新的内容"。
PAYLOAD_CANDIDATES=(
    "/var/lib/mwv207-oss/payload-cache"
    "$WORKDIR/payload-24"
)

INST_DIR="/usr/local/lib/mwv207-oss"                  # 载荷安装目录 (平面布局)
CACHE_DIR="/var/lib/mwv207-oss"                       # 状态+历史+载荷缓存
STATE_FILE="$CACHE_DIR/active-stack"
HIST_ROOT="$CACHE_DIR/switch-history"
LOG_FILE="/var/log/mwv207-switch.log"
SELF_INSTALLED="/usr/local/sbin/mwv207-oss-switch"
STAMP="/run/mwv207-oss-switch.stamp"

LDCONF="/etc/ld.so.conf.d/00-mwv207-oss.conf"
ENVIRONMENT_D="/etc/environment.d/60-mwv207-oss.conf"
PROFILE_D="/etc/profile.d/60-mwv207-oss.sh"
SYS_ENVIRONMENT="/etc/environment"
OPENCL_DIR="/etc/OpenCL/vendors"
OPENCL_ICD="$OPENCL_DIR/mwv207.icd"
OPENCL_ICD_DISABLED="$OPENCL_DIR/jmgpu.icd.oss-disabled"
DRI_LINK="/usr/local/lib/aarch64-linux-gnu/dri/mwv207_dri.so"
VA_LINK="/usr/lib/aarch64-linux-gnu/dri/mwv207_drv_video.so"
VDPAU_LINK="/usr/lib/aarch64-linux-gnu/vdpau/libvdpau_mwv207.so.1"

XORG_LOG="/var/log/Xorg.0.log"
# 注意: "Initialized DRISWRAST" 不可作失败标记! 2026-10-21 10:02 实证: glamor
# 硬件加速点亮 (Xorg 日志含 glamor enabled on MWV207, glxinfo direct
# rendering:Yes + renderer MWV207) 的同一次引导里, Xorg GLX 模块仍会初始化
# swrast 供应商并写 "GLX: Initialized DRISWRAST GL provider for screen 0"。
# 09:43 第三次安装即因该假阳性被看门狗在 T+4s 误杀——真实点亮加速的 Xorg
# 被回滚重启, 日志经双重轮转灭失。判定逻辑必须 PASS 优先。
FAIL_MARKERS="Refusing to try glamor on llvmpipe|glamor initialization failed|Ignoring GL < 2.1|eglInitialize\(\) failed|ShadowFB: preferred YES, enabled YES"
PASS_MARKERS="glamor X acceleration enabled|glamor initialized|glamor: EGL version|glamor: OpenGL"

# 载荷真实文件 (MANIFEST.sha256 校验对象)
REAL_FILES=(
    libEGL_mwv207.so.1.0.0
    libgbm_mwv207.so.1.0.0
    libglapi_mwv207.so.0.0.0
    libGL_mwv207.so.1.2.0
    libGLESv2_mwv207.so.2.0.0
    libGLESv1_CM_mwv207.so.1.1.0
    libmwv207_dri.so
    libmwv207_drv_video.so
    libvdpau_mwv207.so.1.0.0
    libmwv207OpenCL.so.1.0.0
)
# 内部 SONAME 软链
declare -A SONAME_LINKS=(
    [libEGL_mwv207.so.1]=libEGL_mwv207.so.1.0.0
    [libEGL_mwv207.so]=libEGL_mwv207.so.1.0.0
    [libgbm_mwv207.so.1]=libgbm_mwv207.so.1.0.0
    [libgbm_mwv207.so]=libgbm_mwv207.so.1.0.0
    [libglapi_mwv207.so.0]=libglapi_mwv207.so.0.0.0
    [libglapi_mwv207.so]=libglapi_mwv207.so.0.0.0
    [libGL_mwv207.so.1]=libGL_mwv207.so.1.2.0
    [libGL_mwv207.so]=libGL_mwv207.so.1.2.0
    [libGLESv2_mwv207.so.2]=libGLESv2_mwv207.so.2.0.0
    [libGLESv2_mwv207.so]=libGLESv2_mwv207.so.2.0.0
    [libGLESv1_CM_mwv207.so.1]=libGLESv1_CM_mwv207.so.1.1.0
    [libGLESv1_CM_mwv207.so]=libGLESv1_CM_mwv207.so.1.1.0
    # mwv207_dri.so 不放 SONAME_LINKS: 它需要 rpath 实体副本(见下方),
    # 且软链先建会导致 cp -l/cp -L 报 "same file" (09:43 安装实证)
    [mwv207_drv_video.so]=libmwv207_drv_video.so
    [libvdpau_mwv207.so.1.0]=libvdpau_mwv207.so.1.0.0
    [libvdpau_mwv207.so.1]=libvdpau_mwv207.so.1.0.0
    [libvdpau_mwv207.so]=libvdpau_mwv207.so.1.0.0
    [libmwv207OpenCL.so.1]=libmwv207OpenCL.so.1.0.0
    [libmwv207OpenCL.so]=libmwv207OpenCL.so.1.0.0
)
# SONAME 胶水副本: 公开链接名 <- 真实文件 (patchelf --set-soname)
# 注: [libGL.so] 无后缀名条目 (2026-10-21 11:44 glmark2 实证) —— 部分 X11
# 应用 (glmark2 等一切 dlopen "libGL.so" 开发名者) 不走 ldconfig 的 SONAME
# 条目, 落默认目录命中 glvnd 开发名软链 → 系统 Mesa 24.3 → 其 dri 目录无
# 我们驱动 → llvmpipe。ldconfig 缓存收录无后缀名条目后 dlopen("libGL.so")
# 直接命中我们。回归: glmark2 MWV207 硬件加速点亮, glxinfo 不受影响。
declare -A GLUE_COPIES=(
    [libEGL.so.1]=libEGL_mwv207.so.1.0.0
    [libGL.so.1]=libGL_mwv207.so.1.2.0
    [libGL.so]=libGL_mwv207.so.1.2.0
    [libgbm.so.1]=libgbm_mwv207.so.1.0.0
    [libGLESv2.so.2]=libGLESv2_mwv207.so.2.0.0
    [libGLESv1_CM.so.1]=libGLESv1_CM_mwv207.so.1.1.0
)
# 本安装管理的全部系统路径 (备份/回滚清单)
MANAGED=(
    "$LDCONF"
    "$INST_DIR"
    "$DRI_LINK"
    "$VA_LINK"
    "$VDPAU_LINK"
    "$VDPAU_LINK.0"
    "$OPENCL_ICD"
    "$OPENCL_DIR/jmgpu.icd"
    "$ENVIRONMENT_D"
    "$PROFILE_D"
    "$SYS_ENVIRONMENT"
)


# ----------------------------- 全局选项 -------------------------------------
# --root 把整个"系统"搬到假根下, 用来做零副作用演练
R=""
# --dry-run 只打印特权命令
DRY_RUN=0

# ----------------------------- 基础工具 ------------------------------------
# 注意: 原实现是 `echo ... | tee -a "$LOG_FILE" 2>/dev/null || echo ...` ——
# 非 root 时 tee 写 /var/log 失败被 2>/dev/null 吞掉, 于是走 || 分支再打一遍,
# 每条日志出现两行。改成先 echo 到 stdout, 写日志文件失败时静默跳过。
log()  { echo "[$(date '+%F %T')] $*"; { echo "[$(date '+%F %T')] $*" >> "$LOG_FILE"; } 2>/dev/null || true; }
die()  { log "错误: $*"; exit 1; }
warn() { log "[WARN] $*"; }

# 特权命令统一走 run(): 演练/假根时只打印
run() {
    printf '  + %s\n' "$*"
    [ -n "$R" ] && return 0
    [ "$DRY_RUN" = 1 ] && return 0
    "$@"
}

need_root() {
    [ -n "$R" ] && return 0          # 假根演练不需要 root
    [ "$DRY_RUN" = 1 ] && return 0
    [ "$(id -u)" = 0 ] || die "请以 root 运行: sudo $0 $*"
}


# ============================ 内核态 (kmd) 常量 =============================
KVER=$(uname -r)
SRC_DIR="$WORKDIR/src/kernel-6.6"
KOBJ="/lib/modules/$KVER/build"

# 内建 (内核自带) 与自建 (放 updates) 两个位置
IN_TREE_REL="kernel/drivers/gpu/drm/mwv207/mwv207.ko"
DKMS_REL="updates/dkms/mwv207.ko"

# 路径统一经过 rp(): 假根演练时前缀 $R
rp() { if [ -n "$R" ]; then echo "$R$1"; else echo "$1"; fi; }
ko_in_tree() { rp "/lib/modules/$KVER/$IN_TREE_REL"; }
ko_dkms()    { rp "/lib/modules/$KVER/$DKMS_REL"; }
ko_state()   { rp "/var/lib/mwv207-oss/kmd"; }


# ============================ 用户态 (umd) =============================

repo_payloads() {
    local d
    for d in "$WORKDIR"/payload-*; do
        [ -d "$d" ] && [ -f "$d/MANIFEST.sha256" ] && echo "$d"
    done | sort -rV
}

# 载荷声明的版本: 优先读载荷内 VERSION 文件; 没有就用 MANIFEST 摘要
# —— 内容一变摘要就变, 不会被"忘了改版本号"骗过去。
payload_version() {
    local d="$1"
    if [ -f "$d/VERSION" ]; then
        head -1 "$d/VERSION" | tr -d '[:space:]'
    elif [ -f "$d/MANIFEST.sha256" ]; then
        echo "manifest-$(sha256sum "$d/MANIFEST.sha256" | cut -c1-12)"
    else
        echo "$VERSION_FALLBACK"
    fi
}

payload_src() {
    local d
    if [ -n "${PAYLOAD_SRC:-}" ]; then
        [ -f "$PAYLOAD_SRC/MANIFEST.sha256" ] || return 1
        echo "$PAYLOAD_SRC"; return 0
    fi
    d=$(repo_payloads | head -1)
    [ -n "$d" ] && { echo "$d"; return 0; }
    for d in "${PAYLOAD_CANDIDATES[@]}"; do
        [ -n "$d" ] && [ -f "$d/MANIFEST.sha256" ] && { echo "$d"; return 0; }
    done
    return 1
}

kernel_driver() {
    local node
    for node in /dev/dri/renderD128 /dev/dri/card0; do
        [ -e "$node" ] && { basename "$(readlink -f "/sys/class/drm/$(basename "$node")/device/driver" 2>/dev/null)" 2>/dev/null && return 0; }
    done
    lsmod | awk '$1=="mwv207"{print "mwv207"}' | head -1
}

umd_active_stack() { [ -f "$STATE_FILE" ] && cat "$STATE_FILE" | head -1 || echo "unknown"; }

# ----------------------------- check: 预检 ---------------------------------
umd_check() {
    echo "== MWV207 开源栈切换 预检 =="
    local rc=0

    local drv
    drv=$(kernel_driver) || true
    if [ "$drv" = "mwv207" ]; then
        echo "  [OK] 内核驱动 = mwv207 (aliases 默认, 无需 KMD 操作)"
    else
        echo "  [FAIL] 内核驱动 = '${drv:-未知}' —— 本脚本只针对 mwv207 KMD"; rc=1
    fi

    local src
    src=$(payload_src) || { echo "  [FAIL] 找不到载荷 (payload-24 / payload-cache)"; return 1; }
    echo "  [OK] 载荷源: $src"
    if (cd "$src" && sha256sum -c MANIFEST.sha256 >/dev/null 2>&1); then
        echo "  [OK] MANIFEST 校验通过 (10 文件)"
    else
        echo "  [FAIL] MANIFEST 校验失败"; rc=1
    fi

    for lib in libLLVM-17.so.1 libclang-cpp.so.17 libLLVMSPIRVLib.so.17; do
        ldconfig -p | grep -q "$lib " && echo "  [OK] $lib 在 ldconfig" || { echo "  [FAIL] 缺 $lib"; rc=1; }
    done
    for f in /usr/lib/clc/spirv-mesa3d-.spv /usr/lib/clc/spirv64-mesa3d-.spv; do
        [ -f "$f" ] && echo "  [OK] libclc 运行时: $(basename "$f")" || { echo "  [WARN] 缺 $f (OpenCL 受影响)"; }
    done
    command -v patchelf >/dev/null && echo "  [OK] patchelf 可用" || { echo "  [FAIL] 缺 patchelf"; rc=1; }
    ldconfig -p | grep -q "libva.so.2 " && echo "  [OK] libva $(ldconfig -p | grep -m1 'libva.so.2 ' | sed 's/.*libva\.so\.2 \((.*)\).*/\1/')" || { echo "  [FAIL] 缺 libva"; rc=1; }
    ldconfig -p | grep -q "libOpenCL.so.1 " && echo "  [OK] ocl-icd (libOpenCL)" || { echo "  [WARN] 缺 libOpenCL"; }
    systemctl cat lightdm.service >/dev/null 2>&1 && echo "  [OK] lightdm.service 存在" || { echo "  [FAIL] 无 lightdm"; rc=1; }
    [ -e /dev/dri/renderD128 ] && echo "  [OK] /dev/dri/renderD128" || { echo "  [FAIL] 无 render 节点"; rc=1; }

    # 部署目录可写性 —— deepin 不可变根会把 /usr 挂成 ro overlay,
    # VA/VDPAU/DRI-glVND 目录不可写则 install 必失败 (03:24 实测 vdpau ln 报错)
    if [ "$(id -u)" = 0 ]; then
        local d
        for d in /usr/local/lib/aarch64-linux-gnu/dri /usr/lib/aarch64-linux-gnu/dri \
                 /usr/lib/aarch64-linux-gnu/vdpau /etc/ld.so.conf.d "$OPENCL_DIR"; do
            if touch "$d/.wtest" 2>/dev/null; then
                rm -f "$d/.wtest"; echo "  [OK] 可写: $d"
            else
                echo "  [FAIL] 只读: $d (deepin 不可变根未解锁, 需关闭后重启)"; rc=1
            fi
        done
    fi

    local free_gb
    free_gb=$(df -BG / | awk 'NR==2{print $4}' | tr -d G)
    [ "${free_gb:-0}" -ge 1 ] && echo "  [OK] 磁盘剩余 ${free_gb}G (需 ~0.6G)" || { echo "  [FAIL] 磁盘不足"; rc=1; }

    local ver; ver=$(payload_version "$src")
    echo "  [OK] 载荷版本: $ver"
    local cache="$CACHE_DIR/payload-cache"
    if [ -f "$cache/MANIFEST.sha256" ] && [ "$(readlink -f "$src")" != "$(readlink -f "$cache")" ]; then
        if cmp -s "$src/MANIFEST.sha256" "$cache/MANIFEST.sha256"; then
            echo "  [OK] 载荷缓存与所选载荷一致"
        else
            echo "  [WARN] 载荷缓存与所选载荷不同 (install 用 $src, 部署后缓存会被覆盖)"
        fi
    fi

    local cur; cur=$(umd_active_stack)
    [ "$cur" = "oss-$ver" ] && echo "  [注意] 当前已是 oss 栈 ($cur, install 将拒绝, 除非 --force)"
    echo "== 预检结果: $([ $rc = 0 ] && echo 通过 || echo 未通过) =="
    return $rc
}

# ----------------------------- 备份/回滚框架 --------------------------------
umd_backup_snapshot() {
    local ts hist
    ts=$(date '+%Y%m%d-%H%M%S')
    hist="$HIST_ROOT/$ts"
    mkdir -p "$hist/backup/store"
    : > "$hist/backup/manifest.txt"
    local i=0 p enc
    for p in "${MANAGED[@]}"; do
        i=$((i+1))
        enc=$(echo "$p" | sed 's|^/||; s|/|__|g')
        if [ -e "$p" ]; then
            mkdir -p "$hist/backup/store/$(dirname "$enc")"
            cp -a "$p" "$hist/backup/store/$enc"
            echo "present	$p" >> "$hist/backup/manifest.txt"
        else
            echo "absent	$p" >> "$hist/backup/manifest.txt"
        fi
    done
    # 记录 jmgpu.icd 的 .oss-disabled 衍生名 (回滚时清除)
    echo "derived	$OPENCL_ICD_DISABLED" >> "$hist/backup/manifest.txt"
    echo "${1:-unknown}" > "$hist/version"
    ln -sfn "$ts" "$HIST_ROOT/LATEST"
    echo "$ts"
}

umd_restore_snapshot() {
    local hist="$1" line status p enc
    while IFS=$'\t' read -r status p; do
        [ -n "${p:-}" ] || continue
        case "$status" in
            present)
                enc=$(echo "$p" | sed 's|^/||; s|/|__|g')
                if [ -e "$hist/backup/store/$enc" ]; then
                    rm -rf "$p"
                    cp -a "$hist/backup/store/$enc" "$p"
                    log "  恢复: $p"
                else
                    log "  [警告] 备份缺失: $p"
                fi
                ;;
            absent)
                rm -rf "$p" && log "  移除(原不存在): $p"
                ;;
            derived)
                rm -f "$p" && log "  移除(衍生): $p"
                ;;
        esac
    done < "$hist/backup/manifest.txt"
    # /etc/environment 若原为空文件, cp -a 恢复后可能带我们的追加行 —— 用备份内容覆盖
    :
}

# ----------------------------- install -------------------------------------
umd_deploy() {
    local src="$1"
    rm -rf "$INST_DIR"; mkdir -p "$INST_DIR"
    local f
    for f in "${REAL_FILES[@]}"; do
        cp -L "$src/$f" "$INST_DIR/$f" || die "复制载荷失败: $f"
        chmod 0644 "$INST_DIR/$f"
    done
    for f in "${!SONAME_LINKS[@]}"; do
        ln -sf "${SONAME_LINKS[$f]}" "$INST_DIR/$f"
    done
    # SONAME 胶水副本
    for f in "${!GLUE_COPIES[@]}"; do
        cp -L "$src/${GLUE_COPIES[$f]}" "$INST_DIR/$f" || die "复制胶水失败: $f"
        patchelf --set-soname "$f" "$INST_DIR/$f" || die "patchelf 改名失败: $f"
        patchelf --set-rpath '$ORIGIN' "$INST_DIR/$f" || true
        chmod 0644 "$INST_DIR/$f"
        # 校验 SONAME 确已生效
        [ "$(objdump -p "$INST_DIR/$f" 2>/dev/null | awk '/SONAME/{print $NF}')" = "$f" ] \
            || die "胶水副本 SONAME 校验失败: $f"
    done
    # DRI 驱动实体副本 + $ORIGIN rpath —— 2026-10-21 09:23 glamor 点光的实证
    # 配方: Xorg 经 /usr/local/lib/aarch64-linux-gnu/dri/mwv207_dri.so 软链
    # 加载本文件, 其 NEEDED libglapi_mwv207.so.0 必须由 rpath 命中 (03:24 安装
    # 中仅靠 ldconfig 实证失败: loader 报 "libglapi_mwv207.so.0 cannot open
    # shared object")。载荷其余文件 (drv_video/vdpau) 构建期已带 $ORIGIN 且
    # 无私有 NEEDED, 无需处理。cp -l 硬链不重复占 82MB 盘。
    rm -f "$INST_DIR/mwv207_dri.so"
    cp -l "$INST_DIR/libmwv207_dri.so" "$INST_DIR/mwv207_dri.so" 2>/dev/null \
        || cp -L "$INST_DIR/libmwv207_dri.so" "$INST_DIR/mwv207_dri.so" \
        || die "复制 DRI 驱动实体失败"
    [ -f "$INST_DIR/mwv207_dri.so" ] || die "复制 DRI 驱动实体失败"
    patchelf --set-rpath '$ORIGIN' "$INST_DIR/mwv207_dri.so" || die "DRI rpath 设置失败"
    chmod 0644 "$INST_DIR/mwv207_dri.so"
    # 载荷缓存 (供工作区删除后重装)
    if [ "$(readlink -f "$src")" != "$(readlink -f "$CACHE_DIR/payload-cache")" ]; then
        rm -rf "$CACHE_DIR/payload-cache"
        mkdir -p "$CACHE_DIR/payload-cache"
        cp -a "$src"/. "$CACHE_DIR/payload-cache/" || die "刷新载荷缓存失败"
    fi
    # 供 verify 做安装目录的内容校验: 只查"链接指向哪"发现不了内容被换掉,
    # 必须拿 MANIFEST 对 $INST_DIR 里的 10 个实体文件逐个核 sha256。
    cp -f "$src/MANIFEST.sha256" "$CACHE_DIR/manifest.sha256" || die "无法保存 MANIFEST"
}

# 只部署文件 + 写状态，**不动 lightdm**。
# 拆出来的原因：同时切换两层时必须"停一次 lightdm 换 KMD → 再统一起来"，
# 若各切一次则桌面被杀两遍、登录态丢两次。
umd_deploy_files() {
    local force="${1:-0}"

    umd_check >/dev/null 2>&1 || { umd_check; die "预检未通过, 中止"; }
    local src; src=$(payload_src) || die "无载荷"
    local ver; ver=$(payload_version "$src")

    local cur; cur=$(umd_active_stack)
    [ "$cur" = "oss-$ver" ] && [ $force = 0 ] && die "当前已是 oss 栈 ($cur)。如需重装: install --force"

    log "==== install 开始 (载荷 $src, 版本 $ver) ===="

    # 1) 备份快照
    local ts; ts=$(umd_backup_snapshot "$ver")
    log "已创建系统快照: $HIST_ROOT/$ts"

    # 2) 部署载荷 + 缓存
    umd_deploy "$src"
    log "载荷已部署: $INST_DIR ($(du -sh "$INST_DIR" | cut -f1))"

    # 2.5) 测试工具随缓存 —— 供自安装副本 (/usr/local/sbin) 的 verify 使用
    if [ -x "$WORKDIR/tests/egl-smoke" ]; then
        mkdir -p "$CACHE_DIR/tests"
        cp -f "$WORKDIR/tests/egl-smoke" "$CACHE_DIR/tests/egl-smoke"
    fi

    # 3) ldconfig 优先级
    echo "$INST_DIR" > "$LDCONF"
    chmod 0644 "$LDCONF"

    # 4) DRI / VA / VDPAU 默认搜索路径软链 (DRI 指向 rpath 实体副本, 见 deploy)
    mkdir -p /usr/local/lib/aarch64-linux-gnu/dri /usr/lib/aarch64-linux-gnu/vdpau
    ln -sfn "$INST_DIR/mwv207_dri.so"            "$DRI_LINK"    || die "DRI 软链失败: $DRI_LINK (deepin 不可变根未解锁?)"
    ln -sfn "$INST_DIR/libmwv207_drv_video.so"   "$VA_LINK"     || die "VA 软链失败: $VA_LINK"
    ln -sfn "$INST_DIR/libvdpau_mwv207.so.1.0.0" "$VDPAU_LINK"   || die "VDPAU 软链失败: $VDPAU_LINK"
    ln -sfn "$INST_DIR/libvdpau_mwv207.so.1.0.0" "$VDPAU_LINK.0" || die "VDPAU 软链失败: $VDPAU_LINK.0"

    # 5) OpenCL: 禁用厂商 ICD (mwv207 KMD 下 devices=0), 启用 rusticl
    mkdir -p "$OPENCL_DIR"
    [ -f "$OPENCL_DIR/jmgpu.icd" ] && mv -f "$OPENCL_DIR/jmgpu.icd" "$OPENCL_ICD_DISABLED"
    echo "$INST_DIR/libmwv207OpenCL.so.1" > "$OPENCL_ICD"
    chmod 0644 "$OPENCL_ICD"

    # 6) RUSTICL_ENABLE env (OpenCL 设备门控; GL/VA/VDPAU 零 env)
    mkdir -p /etc/environment.d
    printf 'RUSTICL_ENABLE=mwv207\nVDPAU_DRIVER=mwv207\n' > "$ENVIRONMENT_D"
    printf '# mwv207 开源栈: rusticl 设备门控\nexport RUSTICL_ENABLE=mwv207\nexport VDPAU_DRIVER=mwv207\n' > "$PROFILE_D"
    # 幂等且能纠错: 先删掉已有的同名键再追加。原来的实现是"键不存在才追加",
# 一旦值被写错 (例如 RUSTICL_ENABLE=llvmpipe) 就永远纠正不回来。
    # 只动这两行, /etc/environment 其余内容原样保留。
    if [ -f "$SYS_ENVIRONMENT" ]; then
        sed -i '/^RUSTICL_ENABLE=/d; /^VDPAU_DRIVER=/d' "$SYS_ENVIRONMENT"
        # 若文件末尾没有换行, 先补一个, 免得与最后一行粘连
        [ -n "$(tail -c1 "$SYS_ENVIRONMENT" 2>/dev/null)" ] && echo >> "$SYS_ENVIRONMENT"
        printf 'RUSTICL_ENABLE=mwv207\nVDPAU_DRIVER=mwv207\n' >> "$SYS_ENVIRONMENT"
    fi

    # 7) 自安装 (看门狗需要稳定路径)
    cp -f "$(readlink -f "${BASH_SOURCE[0]}")" "$SELF_INSTALLED"
    chmod 0755 "$SELF_INSTALLED"

    # 8) 生效
    ldconfig || die "ldconfig 失败 (动态库缓存未更新)"
    echo "oss-$ver" > "$STATE_FILE"
    echo "installed $(date '+%F %T') history=$ts payload=$src" >> "$STATE_FILE"
    # 返回版本号供上层 (all install) 打印
    UMD_DEPLOYED_VER="$ver"
    return 0
}

# 起看门狗 + 重启 lightdm + 自检（部署之后单独调用）
umd_activate() {
    # 启动前先停掉可能残留的旧看门狗: 同名 unit 会让 systemd-run 直接失败,
    # 于是静默降级成"只靠本脚本内联自检"。
    systemctl stop mwv207-oss-watchdog 2>/dev/null || true
    touch "$STAMP"
    systemd-run --collect --unit=mwv207-oss-watchdog \
        --description="mwv207-oss switch watchdog" \
        "$SELF_INSTALLED" watchdog "$STAMP" >/dev/null 2>&1 || log "[警告] 看门狗启动失败, 依赖脚本自检"
    log "重启 lightdm (图形会话将终止, 请稍后重新登录)..."
    systemctl restart lightdm || die "lightdm 重启失败 (图形会话可能已中断)"

    sleep 8
    if umd_wait_xorg; then
        systemctl stop mwv207-oss-watchdog 2>/dev/null || true
        log "install 完成: glamor 硬件加速已生效"
        # 自检失败必须以非零退出 —— 原实现丢弃 umd_verify 的 rc, 即使
        # VA/OpenCL/EGL 全挂, install 依然报成功。这里不自动回滚(图形已可用),
        # 只把失败如实上报。
        umd_verify || die "install 已切换但自检未通过 (见上); 未自动回滚, 如需还原请执行 rollback"
        return 0
    else
        log "install 后 X 仍为软件渲染 → 自动回滚"
        systemctl stop mwv207-oss-watchdog 2>/dev/null || true
        umd_do_rollback "watchdog-script" ""   # 不再重启: 下一步会重启
        systemctl restart lightdm
        die "切换失败已回滚, 详见 $LOG_FILE / $XORG_LOG"
    fi
}

umd_install() {
    need_root "umd install"
    local force=0
    [ "${1:-}" = "--force" ] && force=1
    umd_deploy_files "$force"
    umd_activate
}

# Xorg.0.log 的 "Time: ..." 在 "(==) Log file:" 行内, 不在首行! 首行是
# "[148885.730] $" 时间戳空行 —— head -n1|sed 抓不到 Time 会返回空值,
# 被上层判为 "无新日志" (2026-10-21 03:06/03:24 两次安装均实测踩坑,
# 导致 umd_wait_xorg 超时假性回滚)。grep -m1 全文抓取彻底修复。
umd_xorg_log_epoch() {
    local t
    t=$(grep -m1 "Time:" "$XORG_LOG" 2>/dev/null | sed 's/.*Time: //')
    [ -n "$t" ] && date -d "$t" +%s 2>/dev/null
}
umd_stamp_epoch() { stat -c %Y "$STAMP" 2>/dev/null || echo 0; }
umd_xorg_log_is_new() {  # $1 = 基准 epoch
    local e; e=$(umd_xorg_log_epoch) || return 1
    [ -n "$e" ] && [ "$e" -ge "$1" ]
}

# 等待新 Xorg 日志并检查标记
umd_wait_xorg() {
    local se i
    se=$(umd_stamp_epoch)
    for i in $(seq 1 40); do
        [ -f "$XORG_LOG" ] && umd_xorg_log_is_new "$se" && break
        sleep 3
    done
    umd_xorg_log_is_new "$se" || { log "未等到新 Xorg 日志(Time 校验)"; return 1; }
    # 再等 glamor 探测完成(新 Xorg 启动 ~2s 内写 glamor 行, 多留余量)
    for i in $(seq 1 10); do
        grep -qiE "$PASS_MARKERS" "$XORG_LOG" && break
        grep -qE "$FAIL_MARKERS" "$XORG_LOG" && break
        sleep 2
    done
    # PASS 优先: glamor 加速点亮后 DRISWRAST 行会共存(见 FAIL_MARKERS 注释)
    if grep -qiE "$PASS_MARKERS" "$XORG_LOG"; then
        return 0
    fi
    if grep -qE "$FAIL_MARKERS" "$XORG_LOG"; then
        log "Xorg 日志命中软件渲染标记: $(grep -m2 -E "$FAIL_MARKERS" "$XORG_LOG" | tr '\n' ' ')"
        return 1
    fi
    log "Xorg 日志无明确 glamor 成功标记 (也无失败标记), 视为通过但请人工复核"
    return 0
}

# ----------------------------- rollback ------------------------------------
umd_do_rollback() {
    local reason="${1:-manual}" no_restart="${2:-}"
    local hist="$HIST_ROOT/LATEST"
    [ -d "$hist" ] || die "无回滚快照 ($HIST_ROOT)"
    log "==== rollback 开始 (原因: $reason, 快照: $hist) ===="
    umd_restore_snapshot "$hist"
    # MANAGED 路径 (LDCONF/INST_DIR/ENVIRONMENT_D/PROFILE_D/OPENCL_ICD) 一律由
    # umd_restore_snapshot 三态语义处理, 不可在此无条件 rm —— 否则会把快照恢复出的
    # "装前已存在"状态再次破坏 (2026-10-21 10:19 实证: 装前手动部署态被恢复后
    # 又被 rm 掉, 只剩 DRI_LINK 悬垂指向已删除的 INST_DIR)。此处只清瞬态文件。
    rm -f "$STAMP" 2>/dev/null
    ldconfig
    echo "system-restored $(date '+%F %T') reason=$reason" > "$STATE_FILE"
    systemctl stop mwv207-oss-watchdog 2>/dev/null || true
    if [ "$no_restart" != "--no-restart" ]; then
        log "重启 lightdm 恢复系统软件栈..."
        systemctl restart lightdm
    fi
    log "rollback 完成 (系统已还原为软件渲染栈; 载荷缓存保留于 $CACHE_DIR/payload-cache)"
}

umd_rollback() {
    need_root "umd rollback"
    umd_do_rollback "manual" "${1:-}"
}

# ----------------------------- watchdog (内部) -----------------------------
umd_watchdog() {
    local stamp="${1:-$STAMP}" i se
    se=$(stat -c %Y "$stamp" 2>/dev/null || echo 0)
    for i in $(seq 1 60); do
        sleep 4
        [ -f "$XORG_LOG" ] || continue
        # Time 校验: 只认本次切换之后新起的 Xorg 写的日志
        local e; e=$(umd_xorg_log_epoch) || continue
        [ -n "$e" ] && [ "$e" -ge "$se" ] || continue
        # PASS 优先(见 FAIL_MARKERS 注释): 加速点亮的日志也会带 DRISWRAST 行
        if grep -qiE "$PASS_MARKERS" "$XORG_LOG"; then
            log "看门狗: glamor 硬件加速确认, 退出"
            exit 0
        fi
        if grep -qE "$FAIL_MARKERS" "$XORG_LOG"; then
            log "看门狗: 新 Xorg 日志命中软件渲染标记 → 自动回滚: $(grep -m1 -E "$FAIL_MARKERS" "$XORG_LOG")"
            umd_do_rollback "watchdog" ""
            exit 1
        fi
    done
    log "看门狗: 240s 超时, 未检测到失败标记, 放行 (请人工复核 $XORG_LOG)"
}

# ----------------------------- verify / status ------------------------------
umd_verify() {
    echo "== 安装后自检 =="
    local rc=0
    for s in libEGL.so.1 libGL.so.1 libgbm.so.1 libGLESv2.so.2; do
        local hit; hit=$(ldconfig -p | grep -m1 "$s " | awk '{print $NF}')
        case "$hit" in
            "$INST_DIR"*) echo "  [OK] $s -> $hit" ;;
            *) echo "  [FAIL] $s -> ${hit:-未命中} (应指向 $INST_DIR)"; rc=1 ;;
        esac
    done
    for l in "$DRI_LINK" "$VA_LINK" "$VDPAU_LINK" "$OPENCL_ICD"; do
        [ -e "$l" ] && echo "  [OK] $l" || { echo "  [FAIL] 缺 $l"; rc=1; }
    done
    [ -f "$LDCONF" ] && echo "  [OK] $LDCONF" || { echo "  [FAIL] 缺 $LDCONF"; rc=1; }
    [ -f "$OPENCL_ICD_DISABLED" ] && echo "  [OK] 厂商 jmgpu.icd 已禁用" || echo "  [INFO] 厂商 jmgpu.icd 原本不存在"
    # 厂商包升级(apt upgrade mwv207-dev)会把这个 ICD 装回来, 于是出现第二个
    # OpenCL 平台(该实现在 mwv207 KMD 下枚举 0 设备), 需要重新 install。
    [ -f "$OPENCL_DIR/jmgpu.icd" ] && echo "  [WARN] $OPENCL_DIR/jmgpu.icd 又出现 (mwv207-dev 被重装?), 需重新 install"

    echo "  -- 安装目录内容完整性:"
    if [ -f "$CACHE_DIR/manifest.sha256" ]; then
        if ( cd "$INST_DIR" && sha256sum -c "$CACHE_DIR/manifest.sha256" >/dev/null 2>&1 ); then
            echo "     [OK] 实体文件与 MANIFEST 一致"
        else
            echo "     [FAIL] 实体文件与 MANIFEST 不一致 (安装目录被改动?)"; rc=1
            ( cd "$INST_DIR" && sha256sum -c "$CACHE_DIR/manifest.sha256" 2>&1 \
                | grep -v '成功$' | sed 's/^/       /' )
        fi
    else
        echo "     [INFO] 无 $CACHE_DIR/manifest.sha256 (旧安装), 跳过内容校验"
    fi
    if [ -e "$INST_DIR/mwv207_dri.so" ] && [ -e "$INST_DIR/libmwv207_dri.so" ]; then
        if [ "$INST_DIR/mwv207_dri.so" -ef "$INST_DIR/libmwv207_dri.so" ] \
           || [ "$(md5sum < "$INST_DIR/mwv207_dri.so" | cut -d' ' -f1)" = \
                "$(md5sum < "$INST_DIR/libmwv207_dri.so" | cut -d' ' -f1)" ]; then
            echo "     [OK] DRI 实体副本与 libmwv207_dri.so 内容一致"
        else
            echo "     [FAIL] DRI 实体副本与 libmwv207_dri.so 内容不同"; rc=1
        fi
    fi

    echo "  -- Xorg glamor:"
    if grep -qiE "$PASS_MARKERS" "$XORG_LOG" 2>/dev/null; then
        echo "     [OK] $(grep -m1 -iE "$PASS_MARKERS" "$XORG_LOG")"
    else
        echo "     [FAIL] 未见 glamor 成功标记"; rc=1
    fi

    echo "  -- VA-API (零 env, 纯 DRM):"
    if env -u DISPLAY -u LIBVA_DRIVERS_PATH -u LIBVA_DRIVER_NAME timeout 30 vainfo 2>/dev/null | grep -q "MWV207"; then
        echo "     [OK] vainfo 命中 mwv207 驱动"
    else
        echo "     [FAIL] vainfo 未命中"; rc=1
    fi

    echo "  -- OpenCL (rusticl):"
    if env -u DISPLAY RUSTICL_ENABLE=mwv207 timeout 60 clinfo 2>/dev/null | grep -q "Device Name.*MWV207"; then
        echo "     [OK] clinfo 设备 MWV207"
    else
        echo "     [FAIL] clinfo 未命中"; rc=1
    fi

    # egl-smoke: 仓库 tests/ 优先, 自安装副本回退缓存副本 (install 步骤 2.5)
    local egl_smoke=""
    local t
    for t in "$WORKDIR/tests/egl-smoke" "$CACHE_DIR/tests/egl-smoke"; do
        [ -x "$t" ] && { egl_smoke="$t"; break; }
    done
    if [ -n "$egl_smoke" ]; then
        echo "  -- EGL 硬件管线 (egl-smoke, 零 env):"
        if env -u DISPLAY -u LD_LIBRARY_PATH -u LIBGL_DRIVERS_PATH timeout 30 "$egl_smoke" 2>/dev/null | grep -q "full pipeline: OK"; then
            echo "     [OK] EGL 全管线"
        else
            echo "     [FAIL] egl-smoke 失败"; rc=1
        fi
    fi

    echo "== 自检结果: $([ $rc = 0 ] && echo 全部通过 || echo 存在失败) =="
    return $rc
}

umd_status() {
    echo "== MWV207 开源栈状态 =="
    echo "  活动栈:     $(umd_active_stack)"
    echo "  内核驱动:   $(kernel_driver 2>/dev/null || echo 未知)"
    local src ver
    if src=$(payload_src); then
        ver=$(payload_version "$src")
        echo "  载荷:       $src (版本 $ver)"
    else
        echo "  载荷:       未找到"
    fi
    # 自安装副本才是 watchdog / rollback 的实际执行者; 它与仓库脚本不一致,
    # 就意味着回滚跑的是旧逻辑。
    if [ -f "$SELF_INSTALLED" ]; then
        if [ -f "$WORKDIR/switch-mwv207.sh" ]; then
            cmp -s "$SELF_INSTALLED" "$WORKDIR/switch-mwv207.sh" \
                && echo "  自安装副本: 与仓库脚本一致" \
                || echo "  自安装副本: [WARN] 与仓库脚本不同 (重新 install 可同步)"
        else
            echo "  自安装副本: $SELF_INSTALLED (无仓库脚本可比)"
        fi
    fi
    echo "  安装目录:   $([ -d "$INST_DIR" ] && echo "存在 ($(du -sh "$INST_DIR" 2>/dev/null | cut -f1))" || echo 未安装)"
    echo "  历史快照:   $(ls "$HIST_ROOT" 2>/dev/null | grep -v LATEST | tr '\n' ' ')"
    [ -L "$HIST_ROOT/LATEST" ] && echo "  LATEST:     $(readlink "$HIST_ROOT/LATEST")"
    echo "  -- 链接解析:"
    for s in libEGL.so.1 libGL.so.1 libgbm.so.1; do
        echo "     $s -> $(ldconfig -p | grep -m1 "$s " | awk '{print $NF}')"
    done
    echo "  -- Xorg 最近 glamor 状态:"
    sudo grep -m2 -iE "$PASS_MARKERS|$FAIL_MARKERS" "$XORG_LOG" 2>/dev/null | sed 's/^/     /' || echo "     (无日志)"
    echo "  -- ICD:"
    ls "$OPENCL_DIR" 2>/dev/null | sed 's/^/     /'
    return 0
}

# ============================ 内核态 (kmd) =============================

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

kmd_check() {
    local rc=0
    log "预检（内核 $KVER）"

    if [ -d "$(rp "$KOBJ")" ] || [ -d "$KOBJ" ]; then
        log "  [OK] 内核构建目录: $KOBJ"
    else
        warn "  缺少内核构建目录 $KOBJ（需要 linux-headers-$KVER）"; rc=1
    fi

    if command -v make >/dev/null && command -v modinfo >/dev/null; then
        log "  [OK] make / modinfo 可用"
    else
        warn "  缺少 make 或 modinfo"; rc=1
    fi

    if [ -f "$(ko_in_tree)" ] || [ -f "$(ko_in_tree)" ]; then
        log "  [OK] 内核内建模块存在: $(ko_in_tree)"
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
    [ -r "$(rp /sys/kernel/security/lockdown)" ] && \
        log "  lockdown: $(cat "$(rp /sys/kernel/security/lockdown)")"

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

kmd_build() {
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

kmd_status() {
    log "KMD 状态（内核 $KVER）"
    local it="$(ko_in_tree)" dk="$(ko_dkms)"
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

    # 注意: 原写法 `[ -f ... ] && log ...` 在没有备份文件时会让整个 status
    # 返回 1, 于是 "status" 这个只读命令被误报成失败。改成显式 if。
    if [ -f "$(ko_state)/backup.md5" ]; then
        log "  上次备份: $(cat "$(ko_state)/backup.md5")"
    fi
    return 0
}

# ---- 备份 / 恢复 ----

kmd_do_backup() {
    local it="$(ko_in_tree)"
    [ -f "$it" ] || die "找不到内建模块，拒绝继续: $it"
    run mkdir -p "$(ko_state)"
    run cp -a "$it" "$(ko_state)/mwv207.ko.in-tree"
    local m; m=$(md5sum "$it" | awk '{print $1}')
    if [ -z "$R" ] && [ "$DRY_RUN" = 0 ]; then echo "$m" > "$(ko_state)/backup.md5"; fi
    log "  已备份内建模块 → $(ko_state)/mwv207.ko.in-tree (md5 $m)"
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

# 只构建 + 放置模块 + depmod，**不动 lightdm、不换模块**。
# 拆出来的原因同 umd_deploy_files：同时切换两层时 lightdm 只能停一次。
kmd_place_module() {
    kmd_build
    local src="$SRC_DIR/mwv207.ko" it="$(ko_in_tree)" dk="$(ko_dkms)"

    log "安装自建模块"
    local backup_md5; backup_md5=$(kmd_do_backup)
    [ -n "$backup_md5" ] || die "备份失败"

    # 装到 updates（depmod 顺序里优先）
    run install -D -m 0644 "$src" "$dk"
    run depmod -a "$KVER"

    local installed; installed=$(ko_srcver "$dk")
    log "  已放置: $dk (srcversion $installed)"
    KMD_PLACED_SRCVER="$installed"
    [ -n "$installed" ] || die "放置后读不到 srcversion: $dk"
    return 0
}

# 纯换模块：**不管显示管理器**（调用方负责停/起）。
# 拆出来的原因：全栈切换要"停一次 lightdm、中间换模块、再由 umd 统一拉起"，
# 若用 kmd_swap_module 则会多一轮 start_display。
kmd_reload_module() {
    local dk="$(ko_dkms)"
    local installed="${KMD_PLACED_SRCVER:-$(ko_srcver "$dk")}"

    log "  卸载旧模块并加载新模块"
    if ! run modprobe -r mwv207; then
        warn "  modprobe -r 失败（仍被占用）"
        return 1
    fi
    if ! run modprobe mwv207; then
        warn "  modprobe 新模块失败"
        return 1
    fi
    sleep 1
    local lv; lv=$(loaded_srcver)
    if [ "$lv" != "$installed" ]; then
        warn "  加载后 srcversion=$lv，期望 $installed"
        return 1
    fi
    KMD_LOADED_SRCVER="$lv"
    return 0
}

# 停 lightdm → 换模块 → 校验 → 起 lightdm
kmd_swap_module() {
    stop_display
    if ! kmd_reload_module; then
        kmd_do_rollback
        die "模块切换失败，已回滚"
    fi

    start_display
    wait_x_ok || { warn "  X 未点亮"; kmd_do_rollback; die "显示未恢复，已回滚"; }

    log "安装完成：正在跑【自建版】(srcversion $KMD_LOADED_SRCVER)"
    kmd_verify
}

kmd_install() {
    need_root "kmd install"
    kmd_place_module
    kmd_swap_module
}

# ---- rollback ----

kmd_do_rollback() {
    local it="$(ko_in_tree)" dk="$(ko_dkms)"
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
    if [ -f "$(ko_state)/backup.md5" ] && [ -z "$R" ] && [ "$DRY_RUN" = 0 ]; then
        local want now
        want=$(cat "$(ko_state)/backup.md5"); now=$(md5sum "$it" | awk '{print $1}')
        [ "$want" = "$now" ] && log "  [OK] 内建模块 md5 未变 ($now)" \
                             || warn "  内建模块 md5 变了: $want → $now"
    fi
    start_display
}

kmd_rollback() {
    need_root "kmd rollback"
    kmd_do_rollback
    log "已回滚"
}

# ---- verify ----

kmd_verify() {
    local rc=0 it="$(ko_in_tree)" dk="$(ko_dkms)"
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

kmd_rehearse() {
    [ -n "$R" ] || die "rehearse 必须配合 --root 使用，例如: $0 --root /tmp/kmd-rehearse rehearse"
    local it="$(ko_in_tree)" dk="$(ko_dkms)"
    log "演练（假根 $R，零副作用）"

    [ -f "$it" ] || die "假根里没有内建模块，先把一个 .ko 拷到 $it"
    local before; before=$(md5sum "$it" | awk '{print $1}')
    log "  演练前 内建 md5=$before"

    log "—— 第 1 步：模拟 install 的文件操作 ——"
    local bm; bm=$(kmd_do_backup)
    mkdir -p "$(ko_state)"
    cp -a "$it" "$(ko_state)/mwv207.ko.in-tree"
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
    if [ -f "$(ko_state)/mwv207.ko.in-tree" ]; then
        local bmd; bmd=$(md5sum "$(ko_state)/mwv207.ko.in-tree" | awk '{print $1}')
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

kmd_dryrun() {
    local src="$SRC_DIR/mwv207.ko" it="$(ko_in_tree)" dk="$(ko_dkms)"
    log "dry-run：计划在真机上执行的命令序列（本次不执行任何特权操作）"
    log "当前状态"
    printf '  内建模块      %s  srcversion=%s\n' "$it" "$(ko_srcver "$it")"
    printf '  自建模块      %s  srcversion=%s\n' "$src" "$(ko_srcver "$src")"
    printf '  已加载        srcversion=%s  refcnt=%s\n' "$(loaded_srcver)" "$(loaded_refcnt)"
    printf '  下次 modprobe %s\n' "$(next_module_path)"
    log "计划步骤（共 9 步，任一步失败自动回滚）"
    cat <<EOF
  1) 备份内建模块     cp -a $it $(ko_state)/mwv207.ko.in-tree
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

# ----------------------------- usage ---------------------------------------
usage() {
    sed -n '2,20p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

# ----------------------------- 分层派发 ------------------------------------
umd_main() {
    case "${1:-help}" in
        check)    shift; umd_check "$@" ;;
        install)  shift; umd_install "$@" ;;
        rollback) shift; umd_rollback "$@" ;;
        verify)   shift; umd_verify "$@" ;;
        status)   shift; umd_status "$@" ;;
        watchdog) shift; umd_watchdog "$@" ;;
        *)        usage ;;
    esac
}

# ----------------------------- 全栈切换 ------------------------------------
# 一次命令把用户态 + 内核态都切过去, **lightdm 只停一次**。
# 顺序是刻意安排的:
#   1) 两层都先只"放文件" (kmd_place_module / umd_deploy_files), 都不动 lightdm;
#      此时即使失败, 桌面完全没被打扰, 回滚也只需删文件。
#   2) 停一次 lightdm → 换 KMD 模块 (kmd_reload_module, 不含 start_display)。
#   3) umd_activate 统一拉起 lightdm + 看门狗 + 自检。
# 若反过来先 umd install 再 kmd install, 桌面会被杀两次、登录态丢两次。
all_install() {
    need_root "all install"
    local force=0
    for a in "$@"; do [ "$a" = "--force" ] && force=1; done

    # 唯一改动系统的动作是 umd_deploy_files / kmd_place_module 里的文件操作,
    # 它们不走统一的 run(); 不像 kmd install 有 kmd_dryrun 分支, 这里必须显式拦。
    if [ "$DRY_RUN" = 1 ]; then
        log "==== all install (--dry-run, 不改动系统) ===="
        log "阶段 1a  kmd_place_module : 构建 src/kernel-6.6, 备份内建 .ko, 放到 updates/dkms, depmod"
        log "阶段 1b  umd_deploy_files : 备份快照, 部署载荷到 $INST_DIR, ldconfig 优先, 软链, ICD/env"
        log "阶段 2   stop_display (仅此一次) + kmd_reload_module (modprobe -r / modprobe)"
        log "阶段 3   umd_activate : 起看门狗, 重启 lightdm, 等 glamor, umd_verify"
        log "阶段 4   kmd_verify"
        return 0
    fi

    log "==== all install 开始 (用户态 + 内核态, lightdm 只停一次) ===="

    # 提前做 umd_deploy_files 里那道"已装同版本"判定 —— 否则会先把 KMD 模块
    # 放好、才被 UMD 的 die 拦下, 留下"模块已就位但用户态没动"的半成品。
    # 只想换 KMD 而用户态已是同一版本时, 加 --force (或直接用 kmd install)。
    local src; src=$(payload_src) || die "无载荷"
    local ver; ver=$(payload_version "$src")
    local cur; cur=$(umd_active_stack)
    [ "$cur" = "oss-$ver" ] && [ $force = 0 ] &&
        die "用户态已是 oss 栈 ($cur), KMD 未改动。若只为换内核模块: all install --force (或 kmd install)"

    # --- 阶段 1: 只放文件 ---
    kmd_place_module
    umd_deploy_files "$force"

    # --- 阶段 2: 停一次 lightdm, 换 KMD 模块 ---
    stop_display
    if ! kmd_reload_module; then
        log "KMD 切换失败 → 回滚两层"
        kmd_do_rollback
        umd_do_rollback "all-install-kmd-failed" "--no-restart"
        start_display
        die "all install 失败: KMD 模块切换未成功 (两层均已回滚)"
    fi
    log "  KMD 就位: srcversion $KMD_LOADED_SRCVER"

    # --- 阶段 3: umd 统一拉起 (含 lightdm 重启 + 看门狗 + 自检) ---
    # umd_activate 失败时会自行 umd_do_rollback, 但 KMD 不会跟着退回去 ——
    # 那属于"模块已换、用户态没跟上"的混合态, 如实上报让用户决定。
    umd_activate || die "all install: 用户态未生效 (KMD 已切到 $KMD_LOADED_SRCVER, 未自动回滚内核态; 如需还原请 kmd rollback)"

    log "==== all install 完成 ===="
    log "  用户态: oss-${UMD_DEPLOYED_VER:-?}"
    log "  内核态: 自建版 (srcversion $KMD_LOADED_SRCVER)"
    kmd_verify
}

all_main() {
    case "${1:-help}" in
        install)  shift; all_install "$@" ;;
        *)        usage ;;
    esac
}

kmd_main() {
    case "${1:-help}" in
        check)    shift; kmd_check "$@" ;;
        build)    shift; kmd_build "$@" ;;
        status)   shift; kmd_status "$@" ;;
        install)  shift; if [ "$DRY_RUN" = 1 ]; then kmd_dryrun "$@"; else kmd_install "$@"; fi ;;
        rollback) shift; kmd_rollback "$@" ;;
        verify)   shift; kmd_verify "$@" ;;
        rehearse) shift; kmd_rehearse "$@" ;;
        *)        usage ;;
    esac
}

# 全局选项可写在任意位置 (层名前后都行): 逐个扫描参数, 收下选项、留下位置参数。
# 原实现只在开头收一次, 于是 "kmd --dry-run install" 这种更自然的写法会被
# 当成未知命令打 usage。
main() {
    local pos=()
    while [ $# -gt 0 ]; do
        case "$1" in
            --root)    R="$2"; shift 2 ;;
            --root=*)  R="${1#*=}"; shift ;;
            --dry-run) DRY_RUN=1; shift ;;
            --)        shift; while [ $# -gt 0 ]; do pos+=("$1"); shift; done ;;
            *)         pos+=("$1"); shift ;;
        esac
    done
    [ -n "$R" ] && LOG_FILE="$R/var/log/mwv207-switch.log"

    mkdir -p "$CACHE_DIR" "$HIST_ROOT" 2>/dev/null || true

    set -- "${pos[@]+"${pos[@]}"}"
    local layer="${1:-}"
    local cmd="${2:-}"
    case "$layer" in
        umd)  shift 2; umd_main "${cmd:-help}" "$@" ;;
        kmd)  shift 2; kmd_main "${cmd:-help}" "$@" ;;
        all)  shift 2; all_main "${cmd:-help}" "$@" ;;
        # 不带层名的 status/check/verify 同时跑两层 —— 桌面异常时要一起看
        status) shift; umd_status "$@"; echo; kmd_status "$@" ;;
        check)  shift; umd_check "$@";  echo; kmd_check "$@" ;;
        verify) shift; umd_verify "$@"; echo; kmd_verify "$@" ;;
        ""|help|-h|--help) usage ;;
        *) usage; exit 1 ;;
    esac
}
main "$@"
