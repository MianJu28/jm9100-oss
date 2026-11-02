#!/bin/bash
# 构建我们维护的 MWV207 用户态驱动（源码在 src/mwv207-umd）
#
# 为什么要这个脚本：
#   驱动源码 2026-11-02 起从上游 oh-mesa 树里搬出来了，改由 src/mwv207-umd 维护，
#   上游树里只留符号链接。上游树一旦被重新克隆/覆盖，链接就会丢，构建会莫名其妙
#   地在“旧代码”上跑（或直接找不到驱动目录）。这里先保证链接正确再编译。
set -euo pipefail

W=$(cd "$(dirname "$0")/.." && pwd)
MESA=${MESA_SRC:-$W/ref/oh-mesa/User_source_code/mesa-24.1.2}
BUILD=${MESA_BUILD:-$MESA/build-full}
OURS=$W/src/mwv207-umd

die() { echo "[FAIL] $*" >&2; exit 1; }
info() { echo "[ OK ] $*"; }

[ -d "$OURS/drivers/mwv207" ] || die "找不到源码: $OURS/drivers/mwv207"
[ -f "$MESA/meson.build" ] || die "找不到上游树: $MESA（用 MESA_SRC= 指路）"

relink() {
   local fixed=0
   for pair in "drivers/mwv207" "winsys/mwv207"; do
      local dst="$MESA/src/gallium/$pair" src="$OURS/$pair"
      if [ -L "$dst" ] && [ "$(readlink -f "$dst")" = "$(readlink -f "$src")" ]; then
         continue
      fi
      if [ -e "$dst" ] && [ ! -L "$dst" ]; then
         # 上游树里是一份真实目录：先确认它不是我们改过的那份，再替换
         if grep -rqs 'FIX(jm9100-oss)' "$dst"; then
            die "$dst 是含我们改动的真实目录，请先人工比对再搬";
         fi
         echo "  备份上游原始目录: $dst.orig"
         mv "$dst" "$dst.orig"
      fi
      ln -sfn "$(realpath --relative-to="$(dirname "$dst")" "$src")" "$dst"
      info "已链接 $pair"
      fixed=1
   done
   [ "$fixed" = 0 ] && info "链接均正确"
   return 0
}

verify_links() {
   for pair in "drivers/mwv207" "winsys/mwv207"; do
      local dst="$MESA/src/gallium/$pair"
      [ -L "$dst" ] || die "$dst 不是符号链接（构建会用到错误代码）"
      [ -d "$dst/" ] || die "$dst 断链"
   done
   # FIX 标记必须能在构建路径上看到，否则说明编译的不是我们的代码
   local n; n=$(grep -rl 'FIX(jm9100-oss)' "$MESA/src/gallium/drivers/mwv207" 2>/dev/null | wc -l)
   [ "$n" -gt 0 ] || die "构建路径上看不到 FIX(jm9100-oss) 标记，源码可能不是我们的"
   info "构建路径校验通过（含 FIX 标记的文件: $n）"
}

cmd=${1:-build}
case "$cmd" in
   relink)  relink; verify_links ;;
   check)   verify_links ;;
   build)
      relink; verify_links
      [ -d "$BUILD" ] || die "构建目录不存在: $BUILD（先按 docs/api/00-ARCHITECTURE.md 配置 meson）"
      ninja -C "$BUILD"
      info "产物: $BUILD/src/gallium/targets/dri/libmwv207_dri.so"
      ;;
   sim)
      # 把构建产物装配成"可直接跑的模拟安装目录"。
      #
      # 为什么需要它：Mesa 加载器会比对 DRI 驱动与 libGL 的版本戳
      # （`24.1.2 (git-<sha>)`，取自构建时上游树的 git HEAD）。一旦上游树有了新
      # 提交（例如“驱动迁出”那次），构建树里的戳就和系统里已安装的 libGL 不一致，
      # 直连 `LIBGL_DRIVERS_PATH=<构建树>` 会报
      #   DRI driver not from this Mesa build ('X' vs 'Y')
      # 而回退到软件渲染。把**所有**库都从同一棵构建树装配进一个目录、
      # 用 LD_LIBRARY_PATH 一起覆盖，戳就自洽了。
      SIM=${SIM_DIR:-/tmp/mwv207-sim}
      [ -f "$BUILD/src/gallium/targets/dri/libmwv207_dri.so" ] || die "还没构建: $BUILD（先跑 build）"
      rm -rf "$SIM"; mkdir -p "$SIM"
      # 名字 → 构建树里的真实文件（与 switch-mwv207.sh 的 REAL_FILES 同源）
      copy_one() {
         local dst="$1"; shift
         for cand in "$@"; do
            if [ -f "$cand" ]; then cp -a "$cand" "$SIM/$dst"; return 0; fi
         done
         die "构建树里找不到 $dst 的源文件（试过: $*）"
      }
      copy_one libmwv207_dri.so       "$BUILD/src/gallium/targets/dri/libmwv207_dri.so"
      copy_one libmwv207_drv_video.so "$BUILD/src/gallium/targets/va/libmwv207_drv_video.so"
      copy_one libvdpau_mwv207.so.1.0.0 "$BUILD/src/gallium/targets/vdpau/libvdpau_mwv207.so.1.0.0"
      copy_one libmwv207OpenCL.so.1.0.0 "$BUILD/src/gallium/targets/rusticl/libmwv207OpenCL.so.1.0.0"
      copy_one libGL_mwv207.so.1.2.0  "$BUILD/src/glx/libGL_mwv207.so.1.2.0" "$BUILD/src/glx/libGL.so.1.2.0"
      copy_one libEGL_mwv207.so.1.0.0 "$BUILD/src/egl/libEGL_mwv207.so.1.0.0" "$BUILD/src/egl/libEGL.so.1.0.0"
      copy_one libgbm_mwv207.so.1.0.0 "$BUILD/src/gbm/libgbm_mwv207.so.1.0.0" "$BUILD/src/gbm/libgbm.so.1.0.0"
      copy_one libGLESv2_mwv207.so.2.0.0  "$BUILD/src/mapi/es2api/libGLESv2_mwv207.so.2.0.0"
      copy_one libGLESv1_CM_mwv207.so.1.1.0 "$BUILD/src/mapi/es1api/libGLESv1_CM_mwv207.so.1.1.0"
      copy_one libglapi_mwv207.so.0.0.0 "$BUILD/src/mapi/shared-glapi/libglapi_mwv207.so.0.0.0"
      # SONAME 软链 + 安装时生成的 glvnd 胶水名
      for pair in libGL_mwv207.so.1:libGL_mwv207.so.1.2.0 libGL_mwv207.so:libGL_mwv207.so.1.2.0 \
                  libEGL_mwv207.so.1:libEGL_mwv207.so.1.0.0 libEGL_mwv207.so:libEGL_mwv207.so.1.0.0 \
                  libgbm_mwv207.so.1:libgbm_mwv207.so.1.0.0 libgbm_mwv207.so:libgbm_mwv207.so.1.0.0 \
                  libGLESv2_mwv207.so.2:libGLESv2_mwv207.so.2.0.0 libGLESv1_CM_mwv207.so.1:libGLESv1_CM_mwv207.so.1.1.0 \
                  libglapi_mwv207.so.0:libglapi_mwv207.so.0.0.0 libvdpau_mwv207.so.1:libvdpau_mwv207.so.1.0.0 \
                  libmwv207OpenCL.so.1:libmwv207OpenCL.so.1.0.0 \
                  mwv207_dri.so:libmwv207_dri.so libmwv207_drv_video.so:libmwv207_drv_video.so; do
         ln -sfn "${pair#*:}" "$SIM/${pair%%:*}"
      done
      for pair in libGL.so.1:libGL_mwv207.so.1.2.0 libGL.so:libGL_mwv207.so.1.2.0 \
                  libEGL.so.1:libEGL_mwv207.so.1.0.0 libgbm.so.1:libgbm_mwv207.so.1.0.0 \
                  libGLESv2.so.2:libGLESv2_mwv207.so.2.0.0 libGLESv1_CM.so.1:libGLESv1_CM_mwv207.so.1.1.0; do
         ln -sfn "${pair#*:}" "$SIM/${pair%%:*}"
      done
      # 版本戳自检：目录里出现多于一个 24.1.2 (git-xxxx) 就是装配错了
      stamps=$(strings "$SIM"/*.so* 2>/dev/null | grep -oE '24\.1\.2 \(git-[0-9a-f]+\)' | sort -u)
      n=$(printf '%s\n' "$stamps" | grep -c . || true)
      [ "$n" -eq 1 ] || die "模拟目录里版本戳不一致（$n 种）: $stamps"
      info "模拟目录: $SIM  版本戳: $stamps"
      info "跑探针: DISPLAY=:0 LD_LIBRARY_PATH=$SIM LIBGL_DRIVERS_PATH=$SIM MESA_LOADER_DRIVER_OVERRIDE=mwv207 <程序>"
      ;;
   *)
      sed -n '2,9p' "$0" | sed 's/^# \{0,1\}//'
      echo "用法: $(basename "$0") [build|relink|check|sim]"
      exit 1 ;;
esac