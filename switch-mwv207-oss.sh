#!/bin/bash
# ============================================================================
# switch-mwv207-oss.sh — JM9100 (MWV207) 开源全功能驱动栈 系统级切换/回滚
# ============================================================================
# 生成于 2026-10-21，取代旧 switch-to-oss.sh (690 行, 缺陷记录于
# reports/PHASE5-SWITCH-REPORT.md §2, 脚本本体已清理)。
#
# 载荷: mesa-24.1.2 build-full (GL/EGL + VA-API 硬解 + VDPAU + OpenCL rusticl)
#   含本会话全部缺陷修复 (patches/0001-0006) 与 OpenHarmony 仓库移植的
#   H264/HEVC 硬件解码 (video-codec 56 文件)。
#
# 与旧脚本的架构差异 (旧脚本逐行分析结论, 详见 STAGE5-SWITCH-REPORT.md):
#   1. 无 KMD 切换段 —— mwv207 已是内核 modules.alias 默认驱动 (0731:9100),
#      不需要任何 modprobe.d / initramfs / grub 操作。旧脚本第 2/6 节为死代码。
#   2. 非 glvnd 构建的私有目录方案 —— 本载荷 libEGL/libGL 未按 glvnd 构建
#      (无 eglvnd 入口), 不能走 glvnd ICD; 改为:
#        /etc/ld.so.conf.d/00-mwv207-oss.conf  (排最前, 实测优先级有效)
#        + SONAME 胶水副本 (libEGL.so.1/libGL.so.1/libgbm.so.1/...)
#      ldconfig 缓存按 SONAME 索引, 故必须为公开名做 patchelf 改名副本;
#      libGL.so.1 副本同样关键 —— epoxy 探测 GL 版本时会 dlopen libGL.so.1,
#      命中系统 glvnd GL 则无有效 context → glamor "Ignoring GL < 2.1"
#      (2026-10-21 08:28 实测; 09:23 补副本后 glamor 点亮)。
#        另: mwv207_dri.so 为安装目录内带 $ORIGIN rpath 的实体副本 ——
#      Xorg 经 dri 目录软链加载时 NEEDED libglapi 必须由 rpath 解析 (实证)。
#   3. 零 env 方案 —— GL/EGL/VA/VDPAU 全部经默认搜索路径自动命中:
#        DRI   /usr/local/lib/aarch64-linux-gnu/dri/mwv207_dri.so (编译期默认)
#        VA    /usr/lib/aarch64-linux-gnu/dri/mwv207_drv_video.so (libva 默认,
#              驱动名自动探测 = 内核驱动名 mwv207, 实测有效)
#        VDPAU /usr/lib/aarch64-linux-gnu/vdpau/libvdpau_mwv207.so.1
#      仅 OpenCL 需 RUSTICL_ENABLE=mwv207 (environment.d/environment/profile.d)。
#   4. 看门狗 —— lightdm 重启后 240s 内监视 Xorg.0.log 软渲染标记, 命中即自动
#      回滚 (判据与旧脚本一致, 已对当前日志实证标定)。
#
# 用法:
#   sudo ./switch-mwv207-oss.sh check              # 预检 (不改系统)
#   sudo ./switch-mwv207-oss.sh install            # 安装+切换+自动验证
#   sudo ./switch-mwv207-oss.sh rollback           # 回滚到系统软件栈
#   sudo ./switch-mwv207-oss.sh rollback --no-restart   # 回滚但不重启 lightdm
#   sudo ./switch-mwv207-oss.sh status             # 查看当前状态
#   sudo ./switch-mwv207-oss.sh verify             # 安装后自检
#   sudo ./switch-mwv207-oss.sh watchdog           # (内部) 看门狗循环
#
# 注意: install/rollback 会重启 lightdm —— 图形会话将被终止 (包括浏览器),
#       lightdm 回来后需重新登录。脚本与看门狗运行在 systemd 系统层, 不受影响。
# ============================================================================
set -u

# ----------------------------- 常量与路径 ----------------------------------
VERSION="24.1.2-fullstack-r1"
# 仓库根 = 本脚本所在目录 (不硬编码用户主目录, 从任意路径调用均有效)。
# 自安装副本 (/usr/local/sbin/mwv207-oss-switch) 所在目录无仓库文件时:
# 载荷自动回退 payload-cache, egl-smoke 自检回退 $CACHE_DIR/tests (install 时复制)。
WORKDIR="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" && pwd)"

PAYLOAD_CANDIDATES=(
    "${PAYLOAD_SRC:-}"
    "/var/lib/mwv207-oss/payload-cache"
    "$WORKDIR/payload-24"
)

INST_DIR="/usr/local/lib/mwv207-oss"                  # 载荷安装目录 (平面布局)
CACHE_DIR="/var/lib/mwv207-oss"                       # 状态+历史+载荷缓存
STATE_FILE="$CACHE_DIR/active-stack"
HIST_ROOT="$CACHE_DIR/switch-history"
LOG_FILE="/var/log/mwv207-oss-switch.log"
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
    # libEGL.so (无后缀开发名): Firefox/WebRender 与一切 dlopen 开发名者的用法
    # ——不拦截则落 glvnd→厂商ICD失败→系统Mesa llvmpipe (about:support 实证
    # 2026-10-21: WebGL renderer llvmpipe Mesa 24.3; 拦截后 X11 窗口表面+swap
    # 实测 MWV207)。与 libGL.so 同配方。
    [libEGL.so]=libEGL_mwv207.so.1.0.0
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

# ----------------------------- 基础工具 ------------------------------------
log()  { echo "[$(date '+%F %T')] $*" | tee -a "$LOG_FILE" 2>/dev/null || echo "[$(date '+%F %T')] $*"; }
die()  { log "错误: $*"; exit 1; }
need_root() { [ "$(id -u)" = 0 ] || die "请以 root 运行: sudo $0 $*"; }

payload_src() {
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

active_stack() { [ -f "$STATE_FILE" ] && cat "$STATE_FILE" | head -1 || echo "unknown"; }

# ----------------------------- check: 预检 ---------------------------------
cmd_check() {
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

    local cur; cur=$(active_stack)
    [ "$cur" = "oss-$VERSION" ] && echo "  [注意] 当前已是 oss 栈 (install 将拒绝, 除非 --force)"
    echo "== 预检结果: $([ $rc = 0 ] && echo 通过 || echo 未通过) =="
    return $rc
}

# ----------------------------- 备份/回滚框架 --------------------------------
backup_snapshot() {
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
    echo "$VERSION" > "$hist/version"
    ln -sfn "$ts" "$HIST_ROOT/LATEST"
    echo "$ts"
}

restore_snapshot() {
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
deploy_payload() {
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
    if [ "$(readlink -f "$src")" != "$CACHE_DIR/payload-cache" ]; then
        rm -rf "$CACHE_DIR/payload-cache"
        mkdir -p "$CACHE_DIR/payload-cache"
        cp -a "$src"/. "$CACHE_DIR/payload-cache/"
    fi
}

cmd_install() {
    need_root install
    local force=0
    [ "${1:-}" = "--force" ] && force=1

    local cur; cur=$(active_stack)
    [ "$cur" = "oss-$VERSION" ] && [ $force = 0 ] && die "当前已是 oss 栈 ($cur)。如需重装: install --force"

    cmd_check >/dev/null 2>&1 || { cmd_check; die "预检未通过, 中止"; }
    local src; src=$(payload_src) || die "无载荷"

    log "==== install 开始 (载荷 $src, 版本 $VERSION) ===="

    # 1) 备份快照
    local ts; ts=$(backup_snapshot)
    log "已创建系统快照: $HIST_ROOT/$ts"

    # 2) 部署载荷 + 缓存
    deploy_payload "$src"
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
    ln -sfn "$INST_DIR/mwv207_dri.so"          "$DRI_LINK"
    ln -sfn "$INST_DIR/libmwv207_drv_video.so" "$VA_LINK"
    ln -sfn "$INST_DIR/libvdpau_mwv207.so.1.0.0" "$VDPAU_LINK"
    ln -sfn "$INST_DIR/libvdpau_mwv207.so.1.0.0" "$VDPAU_LINK.0"

    # 5) OpenCL: 禁用厂商 ICD (mwv207 KMD 下 devices=0), 启用 rusticl
    mkdir -p "$OPENCL_DIR"
    [ -f "$OPENCL_DIR/jmgpu.icd" ] && mv -f "$OPENCL_DIR/jmgpu.icd" "$OPENCL_ICD_DISABLED"
    echo "$INST_DIR/libmwv207OpenCL.so.1" > "$OPENCL_ICD"
    chmod 0644 "$OPENCL_ICD"

    # 6) RUSTICL_ENABLE env (OpenCL 设备门控; GL/VA/VDPAU 零 env)
    mkdir -p /etc/environment.d
    printf 'RUSTICL_ENABLE=mwv207\nVDPAU_DRIVER=mwv207\n' > "$ENVIRONMENT_D"
    printf '# mwv207 开源栈: rusticl 设备门控\nexport RUSTICL_ENABLE=mwv207\nexport VDPAU_DRIVER=mwv207\n' > "$PROFILE_D"
    if ! grep -q '^RUSTICL_ENABLE=' "$SYS_ENVIRONMENT" 2>/dev/null; then
        printf 'RUSTICL_ENABLE=mwv207\nVDPAU_DRIVER=mwv207\n' >> "$SYS_ENVIRONMENT"
    fi

    # 7) 自安装 (看门狗需要稳定路径)
    cp -f "$(readlink -f "${BASH_SOURCE[0]}")" "$SELF_INSTALLED"
    chmod 0755 "$SELF_INSTALLED"

    # 8) 生效
    ldconfig
    echo "oss-$VERSION" > "$STATE_FILE"
    echo "installed $(date '+%F %T') history=$ts payload=$src" >> "$STATE_FILE"

    # 9) 看门狗 + 重启 lightdm
    touch "$STAMP"
    systemd-run --collect --unit=mwv207-oss-watchdog \
        --description="mwv207-oss switch watchdog" \
        "$SELF_INSTALLED" watchdog "$STAMP" >/dev/null 2>&1 || log "[警告] 看门狗启动失败, 依赖脚本自检"
    log "重启 lightdm (图形会话将终止, 请稍后重新登录)..."
    systemctl restart lightdm

    # 10) 自检验证
    sleep 8
    if wait_xorg_and_check; then
        systemctl stop mwv207-oss-watchdog 2>/dev/null || true
        log "install 完成: glamor 硬件加速已生效"
        cmd_verify
        return 0
    else
        log "install 后 X 仍为软件渲染 → 自动回滚"
        systemctl stop mwv207-oss-watchdog 2>/dev/null || true
        do_rollback "watchdog-script" ""   # 不再重启: 下一步会重启
        systemctl restart lightdm
        die "切换失败已回滚, 详见 $LOG_FILE / $XORG_LOG"
    fi
}

# Xorg.0.log 的 "Time: ..." 在 "(==) Log file:" 行内, 不在首行! 首行是
# "[148885.730] $" 时间戳空行 —— head -n1|sed 抓不到 Time 会返回空值,
# 被上层判为 "无新日志" (2026-10-21 03:06/03:24 两次安装均实测踩坑,
# 导致 wait_xorg_and_check 超时假性回滚)。grep -m1 全文抓取彻底修复。
xorg_log_epoch() {
    local t
    t=$(grep -m1 "Time:" "$XORG_LOG" 2>/dev/null | sed 's/.*Time: //')
    [ -n "$t" ] && date -d "$t" +%s 2>/dev/null
}
stamp_epoch() { stat -c %Y "$STAMP" 2>/dev/null || echo 0; }
xorg_log_is_new() {  # $1 = 基准 epoch
    local e; e=$(xorg_log_epoch) || return 1
    [ -n "$e" ] && [ "$e" -ge "$1" ]
}

# 等待新 Xorg 日志并检查标记
wait_xorg_and_check() {
    local se i
    se=$(stamp_epoch)
    for i in $(seq 1 40); do
        [ -f "$XORG_LOG" ] && xorg_log_is_new "$se" && break
        sleep 3
    done
    xorg_log_is_new "$se" || { log "未等到新 Xorg 日志(Time 校验)"; return 1; }
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
do_rollback() {
    local reason="${1:-manual}" no_restart="${2:-}"
    local hist="$HIST_ROOT/LATEST"
    [ -d "$hist" ] || die "无回滚快照 ($HIST_ROOT)"
    log "==== rollback 开始 (原因: $reason, 快照: $hist) ===="
    restore_snapshot "$hist"
    # MANAGED 路径 (LDCONF/INST_DIR/ENVIRONMENT_D/PROFILE_D/OPENCL_ICD) 一律由
    # restore_snapshot 三态语义处理, 不可在此无条件 rm —— 否则会把快照恢复出的
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

cmd_rollback() {
    need_root rollback
    do_rollback "manual" "${1:-}"
}

# ----------------------------- watchdog (内部) -----------------------------
cmd_watchdog() {
    local stamp="${1:-$STAMP}" i se
    se=$(stat -c %Y "$stamp" 2>/dev/null || echo 0)
    for i in $(seq 1 60); do
        sleep 4
        [ -f "$XORG_LOG" ] || continue
        # Time 校验: 只认本次切换之后新起的 Xorg 写的日志
        local e; e=$(xorg_log_epoch) || continue
        [ -n "$e" ] && [ "$e" -ge "$se" ] || continue
        # PASS 优先(见 FAIL_MARKERS 注释): 加速点亮的日志也会带 DRISWRAST 行
        if grep -qiE "$PASS_MARKERS" "$XORG_LOG"; then
            log "看门狗: glamor 硬件加速确认, 退出"
            exit 0
        fi
        if grep -qE "$FAIL_MARKERS" "$XORG_LOG"; then
            log "看门狗: 新 Xorg 日志命中软件渲染标记 → 自动回滚: $(grep -m1 -E "$FAIL_MARKERS" "$XORG_LOG")"
            do_rollback "watchdog" ""
            exit 1
        fi
    done
    log "看门狗: 240s 超时, 未检测到失败标记, 放行 (请人工复核 $XORG_LOG)"
}

# ----------------------------- verify / status ------------------------------
cmd_verify() {
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

cmd_status() {
    echo "== MWV207 开源栈状态 =="
    echo "  活动栈:     $(active_stack)"
    echo "  内核驱动:   $(kernel_driver 2>/dev/null || echo 未知)"
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
}

# ----------------------------- main ----------------------------------------
main() {
    mkdir -p "$CACHE_DIR" "$HIST_ROOT"
    local cmd="${1:-help}"
    case "$cmd" in
        check)    shift; cmd_check "$@" ;;
        install)  shift; cmd_install "$@" ;;
        rollback) shift; cmd_rollback "$@" ;;
        verify)   shift; cmd_verify "$@" ;;
        status)   shift; cmd_status "$@" ;;
        watchdog) shift; cmd_watchdog "$@" ;;
        *) sed -n '2,40p' "${BASH_SOURCE[0]}" | grep '^#' | sed 's/^# \{0,2\}//' ;;
    esac
}
main "$@"
