#!/bin/bash
# build-xddx.sh —— 构建/安装 mwv207 自有 X DDX 模块(mwv207oss_drv.so)
#
# 背景见 docs/HANDOVER-ZEROCOPY-PRESENT.md §11。
# 该 DDX 从 xserver 的 modesetting 派生(src/xddx-mwv207/)，逐步加入
# WIN0 overlay 平面直扫路径以消除窗口态 Present 的整帧拷贝。
#
# 用法:
#   ./tools/build-xddx.sh --build      只构建(-j 并行)，产物在 xserver 构建树里
#   ./tools/build-xddx.sh --install    构建并安装到 /usr/lib/xorg/modules/drivers/
#   ./tools/build-xddx.sh --rollback   卸载自建模块，恢复系统 modesetting
#   ./tools/build-xddx.sh --status     查看当前安装状态
#   ./tools/build-xddx.sh --test       只看产物(A)与系统(B)差异，不改系统
#
# 说明: 安装/回滚后必须重启 X 才生效。重启会终止本会话(pty 挂断)，
#      所以本脚本不自动重启 —— 请用户在 SSH/TTY 里执行 systemctl restart lightdm。
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
XORG_VER=${XORG_VER:-21.1.16}
SRC=${XORG_SRC:-/tmp/xorg-server-$XORG_VER}
BUILD=${XORG_BUILD:-$SRC/build-glx}
PC_SHIM=${PC_SHIM:-/tmp/pkgconfig}
SRC_DIR=$REPO/src/xddx-mwv207
MODULE=mwv207oss_drv.so
MODULE_DIR=/usr/lib/xorg/modules/drivers
INSTALLED=$MODULE_DIR/$MODULE
BACKUP=$INSTALLED.mwv207-bak
CONF=/usr/share/X11/xorg.conf.d/10-mwv207-oss.conf
CONF_BACKUP=$CONF.bak
TARGET=hw/xfree86/drivers/mwv207oss/$MODULE
# Present 核心补丁: 让非全屏窗口也能走 overlay 直扫(present 的版本 2 钩子)
CORE_PATCH=$REPO/patches/xserver/0003-present-overlay-zero-copy.patch
CORE_TARGET=hw/xfree86/Xorg
CORE_OUT=$BUILD/$CORE_TARGET
INSTALLED_XORG=/usr/lib/xorg/Xorg
XORG_BACKUP=$INSTALLED_XORG.mwv207-bak

log() { printf '%s\n' "$*"; }

# 是不是我们自己编的模块(或核心)? 用来避免把自建产物当成"系统备份"。
is_our_module() {
    [ -f "$1" ] || return 1
    nm -D "$1" 2>/dev/null | grep -q 'mwv207ossModuleData'
}
is_our_core() {
    [ -f "$1" ] || return 1
    # 我们编的 Xorg 比系统的大且带 present overlay 的版本 2 结构;
    # 用大小 + 是否导出 present_screen_init 的兄弟符号难以区分,
    # 这里退一步: 与当前构建产物比对 md5。
    [ -f "$CORE_OUT" ] && [ "$(md5sum "$1" | cut -d' ' -f1)" = "$(md5sum "$CORE_OUT" | cut -d' ' -f1)" ]
}
die() { printf '[ERROR] %s\n' "$*" >&2; exit 1; }

need_sudo() {
    if [ "$(id -u)" -ne 0 ]; then
        command -v sudo >/dev/null || die "需要 root 或 sudo"
    fi
}
SUDO=""
[ "$(id -u)" -ne 0 ] && SUDO="sudo"

# 系统没有 dri.pc(mesa 24 起不再提供)，但 xserver 开 GLX 必需要它。
# 造一个最小的 shim：dri_dep 只用来取 DRI_DRIVER_PATH 与链接 dl。
ensure_dri_pc() {
    if pkg-config --exists dri 2>/dev/null; then
        return
    fi
    mkdir -p "$PC_SHIM"
    if [ ! -f "$PC_SHIM/dri.pc" ]; then
        cat > "$PC_SHIM/dri.pc" <<'EOF'
prefix=/usr
exec_prefix=${prefix}
libdir=/usr/lib/aarch64-linux-gnu
includedir=${prefix}/include
dridriverdir=/usr/lib/aarch64-linux-gnu/dri

Name: dri
Description: Direct Rendering Infrastructure (local build shim)
Version: 24.3.0
Libs:
Cflags:
EOF
    fi
    export PKG_CONFIG_PATH=$PC_SHIM${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}
}

# 构建目录必须是开了 GLX 的配置：系统 Xorg 加载 libglx.so，缺 glxServer
# 会导致 GLX 整个不可用。
ensure_configured() {
    ensure_dri_pc
    if [ -f "$BUILD/build.ninja" ]; then
        grep -q "glx *: *true" "$BUILD/meson-info/intro-buildoptions.json" 2>/dev/null || true
        return
    fi
    log "[INFO] 配置构建目录 $BUILD(GLX 打开)"
    meson setup "$BUILD" "$SRC" --prefix=/usr --libdir=lib/aarch64-linux-gnu \
        --buildtype=release -Dglamor=true -Dxorg=true -Dglx=true -Ddri1=false \
        -Ddri2=true -Ddri3=true -Dxnest=false -Dxvfb=false -Dxephyr=false \
        -Dxdmcp=false -Dsystemd_logind=false -Dudev=true \
        -Dxkb_dir=/usr/share/X11/xkb -Dxkb_output_dir=/var/lib/xkb \
        -Dsha1=libnettle || die "meson setup 失败"
}

prepare_tree() {
    [ -d "$SRC" ] || die "找不到 xserver 源码树 $SRC(先跑 ./tools/build-xglamor.sh --build)"
    # 把仓库里的 DDX 源码挂进 xserver 树，meson 才能 subdir 进来
    local link=$SRC/hw/xfree86/drivers/mwv207oss
    if [ ! -e "$link" ] && [ ! -L "$link" ]; then
        ln -sfn "$SRC_DIR" "$link"
    fi
    # 保证 hw/xfree86/meson.build 里包含 subdir
    if ! grep -q "drivers/mwv207oss" "$SRC/hw/xfree86/meson.build"; then
        log "[INFO] 在 $SRC/hw/xfree86/meson.build 里注册子目录"
        python3 - "$SRC/hw/xfree86/meson.build" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
old = "   subdir('drivers/modesetting')"
new = "   subdir('drivers/modesetting')\n   subdir('drivers/mwv207oss')"
if 'drivers/mwv207oss' not in s:
    assert old in s, "找不到 modesetting subdir 行"
    s = s.replace(old, new, 1)
    open(p, 'w').write(s)
PY
    fi
    # 打 Present 核心补丁(幂等)。已打过时 present.h 里的版本号已是 2。
    if grep -q 'PRESENT_SCREEN_INFO_VERSION        2' "$SRC/present/present.h" 2>/dev/null; then
        log "[INFO] Present 核心补丁已在树上"
    else
        log "[INFO] 应用 $CORE_PATCH"
        ( cd "$SRC" && patch -p1 < "$CORE_PATCH" ) || die "补丁应用失败"
    fi
}

do_build() {
    prepare_tree
    ensure_configured
    log "[INFO] 构建 $TARGET 与 $CORE_TARGET"
    ( cd "$BUILD" && ninja "$TARGET" "$CORE_TARGET" ) || die "构建失败"
    local out=$BUILD/$TARGET
    [ -f "$out" ] || die "产物不存在: $out"
    [ -f "$CORE_OUT" ] || die "核心产物不存在: $CORE_OUT"
    log "[OK] 产物: $out"
    ls -l "$out"
    log "[OK] 核心: $CORE_OUT"
    ls -l "$CORE_OUT"
    log "[INFO] 导出的模块数据符号:"
    nm -D "$out" | grep -E 'ModuleData| D mwv207oss' || true
}

do_status() {
    log "== 构建产物 =="
    local out=$BUILD/$TARGET
    if [ -f "$out" ]; then
        log "  $out"
        log "  md5=$(md5sum "$out" | cut -d' ' -f1)  mtime=$(date -r "$out" '+%F %T')"
    else
        log "  (未构建)"
    fi
    log "== 核心产物(打了 present overlay 补丁的 Xorg) =="
    if [ -f "$CORE_OUT" ]; then
        log "  $CORE_OUT"
        log "  md5=$(md5sum "$CORE_OUT" | cut -d' ' -f1)  mtime=$(date -r "$CORE_OUT" '+%F %T')"
    else
        log "  (未构建)"
    fi
    log "== 系统安装 =="
    if [ -f "$INSTALLED" ]; then
        log "  $INSTALLED"
        log "  md5=$(md5sum "$INSTALLED" | cut -d' ' -f1)  mtime=$(date -r "$INSTALLED" '+%F %T')"
    else
        log "  (未安装 —— 使用系统 modesetting)"
    fi
    if [ -f "$INSTALLED_XORG" ]; then
        log "  $INSTALLED_XORG"
        log "  md5=$(md5sum "$INSTALLED_XORG" | cut -d' ' -f1)  mtime=$(date -r "$INSTALLED_XORG" '+%F %T')"
    fi
    log "== 配置文件 =="
    if [ -f "$CONF" ]; then
        log "  $CONF 存在"
        grep -E 'Driver|MatchDriver|Identifier' "$CONF" | sed 's/^/    /'
    else
        log "  $CONF 不存在(不会选中 mwv207oss)"
    fi
    log "== 当前 Xorg =="
    local pid
    pid=$(pgrep -x Xorg | head -1)
    if [ -n "$pid" ]; then
        log "  pid=$pid 启动于 $(ps -o lstart= -p "$pid")"
        local drv
        drv=$(grep -o 'Loading /usr/lib/xorg/modules/drivers/[a-z0-9_]*\.so' /var/log/Xorg.0.log 2>/dev/null | tail -3)
        log "  日志里加载的驱动:"; printf '%s\n' "$drv" | sed 's/^/    /'
        if grep -q 'mwv207oss' /var/log/Xorg.0.log 2>/dev/null; then
            log "  => 当前 Xorg 已加载 mwv207oss"
        else
            log "  => 当前 Xorg 未加载 mwv207oss(仍是 modesetting)"
        fi
    else
        log "  Xorg 未运行"
    fi
}

do_install() {
    do_build
    need_sudo
    local out=$BUILD/$TARGET

    if [ -f "$INSTALLED" ] && [ ! -f "$BACKUP" ]; then
        if is_our_module "$INSTALLED"; then
            log "[INFO] 系统里已是我们自建的 $MODULE, 不覆盖备份"
        else
            log "[INFO] 备份系统原有的 $MODULE -> $BACKUP"
            $SUDO cp -a "$INSTALLED" "$BACKUP" || die "备份失败"
        fi
    fi
    log "[INFO] 安装 $out -> $INSTALLED"
    $SUDO cp -a "$out" "$INSTALLED" || die "安装失败"

    # 核心也要换: 只有打了 present overlay 补丁的 Xorg 才会走到我们新增的
    # check_overlay/overlay_flip 钩子(版本 2)。
    if [ -f "$INSTALLED_XORG" ] && [ ! -f "$XORG_BACKUP" ]; then
        if is_our_core "$INSTALLED_XORG"; then
            log "[INFO] 系统 Xorg 已是我们自建的, 不覆盖备份"
        else
            log "[INFO] 备份系统原有的 Xorg -> $XORG_BACKUP"
            $SUDO cp -a "$INSTALLED_XORG" "$XORG_BACKUP" || die "备份 Xorg 失败"
        fi
    fi
    log "[INFO] 安装 $CORE_OUT -> $INSTALLED_XORG"
    $SUDO cp -a "$CORE_OUT" "$INSTALLED_XORG" || die "安装 Xorg 失败"

    # 安装 xorg 配置，让 X 选中 mwv207oss；保留原配置备份
    if [ ! -f "$CONF" ]; then
        log "[INFO] 写 $CONF"
        $SUDO tee "$CONF" >/dev/null <<'EOF'
# 由 tools/build-xddx.sh 生成。让 Xorg 使用 mwv207 自有 DDX 模块
# (从 modesetting 派生，见 docs/HANDOVER-ZEROCOPY-PRESENT.md §11)。
# 回滚: ./tools/build-xddx.sh --rollback
Section "OutputClass"
	Identifier "mwv207-oss"
	MatchDriver "mwv207"
	Driver "mwv207oss"
EndSection
EOF
    fi
    log "[OK] 安装完成。重启 X 后生效:"
    log "     sudo systemctl restart lightdm"
    log "     然后运行: ./tools/build-xddx.sh --status"
}

do_rollback() {
    need_sudo
    if [ -f "$CONF" ]; then
        log "[INFO] 删除 $CONF"
        $SUDO rm -f "$CONF"
    fi
    if [ -f "$CONF_BACKUP" ]; then
        $SUDO mv -f "$CONF_BACKUP" "${CONF%.bak}" 2>/dev/null || true
    fi
    if [ -f "$INSTALLED" ]; then
        log "[INFO] 删除 $INSTALLED"
        $SUDO rm -f "$INSTALLED"
    fi
    if [ -f "$BACKUP" ]; then
        if is_our_module "$BACKUP"; then
            log "[INFO] 备份其实是自建模块, 直接丢弃"
            $SUDO rm -f "$BACKUP"
        else
            log "[INFO] 恢复备份 $BACKUP -> $INSTALLED"
            $SUDO mv -f "$BACKUP" "$INSTALLED"
        fi
    fi
    if [ -f "$XORG_BACKUP" ]; then
        if is_our_core "$XORG_BACKUP"; then
            log "[INFO] Xorg 备份其实是自建的, 直接丢弃"
            $SUDO rm -f "$XORG_BACKUP"
        else
            log "[INFO] 恢复备份 $XORG_BACKUP -> $INSTALLED_XORG"
            $SUDO mv -f "$XORG_BACKUP" "$INSTALLED_XORG"
        fi
    fi
    log "[OK] 已回滚到系统 modesetting。重启 X 后生效:"
    log "     sudo systemctl restart lightdm"
}

do_test() {
    local out=$BUILD/$TARGET
    [ -f "$out" ] || die "先 --build"
    log "== 未定义符号对比(应与 modesetting 完全一致) =="
    local a b
    a=$(nm -D --undefined-only "$BUILD/hw/xfree86/drivers/modesetting/modesetting_drv.so" | awk '{print $2}' | sort)
    b=$(nm -D --undefined-only "$out" | awk '{print $2}' | sort)
    if [ "$a" = "$b" ]; then
        log "[OK] 两者未定义符号集合完全一致"
    else
        log "[WARN] 未定义符号集合不同:"
        diff <(printf '%s\n' "$a") <(printf '%s\n' "$b") | head -30
    fi
    log "== 依赖库 =="
    ldd "$out" | sed 's/^/  /'
}

case "${1:---status}" in
    --build)    do_build ;;
    --install)  do_install ;;
    --rollback) do_rollback ;;
    --status)   do_status ;;
    --test)     do_test ;;
    *) die "未知参数: $1(用 --build/--install/--rollback/--status/--test)" ;;
esac
