#!/bin/sh
# 构建"glamor 用 blit 做拷贝"的 libglamoregl.so，并替换系统模块。
#
# 背景：present 每帧 0.65 ms 的瓶颈是 X 的 glamor 用 3D 着色器把窗口 pixmap
# 合成到屏幕 pixmap；本驱动（mwv207）的 PE 写线性目标只有 ~3.3 GB/s，而 blit
# 引擎对同样的线性目标有 ~17 GB/s（快 2.6×）。glamor 21.1 的 glamor_copy.c 只
# 有着色器路径，这个补丁给它加一条 glBlitFramebuffer 快路径。
#
# 用法：
#   sudo ./tools/build-xglamor.sh              # 构建 + 安装（原文件自动备份）
#   sudo ./tools/build-xglamor.sh --build      # 只构建，不安装
#   sudo ./tools/build-xglamor.sh --rollback   # 还原系统原有的 libglamoregl.so
#   sudo ./tools/build-xglamor.sh --test       # 只打印判断，不改系统
#
# 构建依赖（Debian/Deepin，需先装）：
#   libxkbfile-dev libbsd-dev libepoxy-dev libgbm-dev libdrm-dev libxfont-dev
#   libpixman-1-dev libxshmfence-dev libx11-dev libxext-dev libxfixes-dev
#   libxrender-dev libxi-dev libxtst-dev libxau-dev libxdmcp-dev libpciaccess-dev
#   xtrans-dev x11proto-dev libxcb1-dev libunwind-dev libxcvt-dev nettle-dev
#
# 说明：本脚本不启用 GLX（-Dglx=false），避免引入 dri.pc 依赖；glamor 的结构体
# 布局只受 GLAMOR_HAS_GBM 影响（已启用），与发行版一致，因此模块 ABI 兼容。

set -eu

REPO=$(cd "$(dirname "$0")/.." && pwd)
XORG_VER=${XORG_VER:-21.1.16}
SRC=${XORG_SRC:-/tmp/xorg-server-$XORG_VER}
PATCH="$REPO/patches/xserver/0001-glamor-blit-fast-path.patch"
MODULE=/usr/lib/xorg/modules/libglamoregl.so
BACKUP=$MODULE.mwv207-orig

ACTION=install
case "${1:-}" in
    --build)    ACTION=build ;;
    --rollback) ACTION=rollback ;;
    --test)     ACTION=test ;;
    "")         ;;
    *) echo "用法: $0 [--build|--rollback|--test]" >&2; exit 1 ;;
esac

if [ "$ACTION" = rollback ]; then
    [ "$(id -u)" -eq 0 ] || { echo "需要 root" >&2; exit 1; }
    if [ ! -f "$BACKUP" ]; then echo "没有备份文件 $BACKUP" >&2; exit 1; fi
    cp -a "$BACKUP" "$MODULE"
    echo "已还原 $MODULE（来自 $BACKUP）"
    echo "重启图形会话后生效：sudo systemctl restart lightdm"
    exit 0
fi

if [ "$ACTION" = test ]; then
    echo "系统当前模块: $MODULE"
    md5sum "$MODULE" 2>/dev/null || true
    [ -f "$BACKUP" ] && { echo "备份: $BACKUP"; md5sum "$BACKUP"; } || echo "尚无备份（未替换过）"
    exit 0
fi

if [ "$ACTION" = install ] && [ "$(id -u)" -ne 0 ]; then
    echo "安装需要 root；只构建请用 --build" >&2
    exit 1
fi

if [ ! -d "$SRC" ]; then
    echo "== 下载 xorg-server $XORG_VER 源码到 $SRC"
    ARCHIVE=/tmp/xserver-xorg-server-$XORG_VER.tar.gz
    [ -f "$ARCHIVE" ] || curl -sSfL --max-time 900 -o "$ARCHIVE" \
        "https://gitlab.freedesktop.org/xorg/xserver/-/archive/xorg-server-$XORG_VER/xserver-xorg-server-$XORG_VER.tar.gz"
    rm -rf "$SRC" && mkdir -p "$SRC"
    tar xzf "$ARCHIVE" -C "$SRC" --strip-components=1
fi

if ! grep -q "glamor_copy_fbo_fbo_blit" "$SRC/glamor/glamor_copy.c"; then
    echo "== 应用补丁"
    ( cd "$SRC" && patch -p1 < "$PATCH" )
fi

echo "== 构建"
if [ ! -f "$SRC/build/build.ninja" ]; then
    meson setup "$SRC/build" "$SRC" --prefix=/usr --libdir=lib/aarch64-linux-gnu \
        --buildtype=release -Dglamor=true -Dxorg=true -Dglx=false \
        -Ddri1=false -Ddri2=false -Ddri3=false -Dxnest=false -Dxvfb=false \
        -Dxephyr=false -Dxdmcp=false -Dsystemd_logind=false -Dudev=true \
        -Dxkb_dir=/usr/share/X11/xkb -Dxkb_output_dir=/var/lib/xkb \
        -Dsha1=libnettle
fi
ninja -C "$SRC/build"

BUILT="$SRC/build/hw/xfree86/glamor_egl/libglamoregl.so"
[ -f "$BUILT" ] || { echo "构建产物缺失: $BUILT" >&2; exit 1; }
echo "== 产物: $BUILT"
ls -l "$BUILT"

if [ "$ACTION" = build ]; then
    echo "只构建完成。安装：sudo $0"
    exit 0
fi

echo "== 备份并按装"
[ -f "$BACKUP" ] || cp -a "$MODULE" "$BACKUP"
cp -a "$BUILT" "$MODULE"
ls -l "$MODULE"
echo
echo "已安装。验证："
echo "  1) GLAMOR_NO_BLIT=1 可关闭快路径（A/B 对照）"
echo "  2) 重启图形会话生效：sudo systemctl restart lightdm"
echo "  3) 撤销：sudo $0 --rollback"