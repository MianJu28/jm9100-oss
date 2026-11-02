#!/bin/bash
# 构建我们维护的 MWV207 用户态驱动（源码在 src/mwv207-umd）
#
# 为什么要这个脚本：
#   驱动源码 2026-11-02 起从上游 oh-mesa 树里搬出来了，改由 src/mwv207-umd 维护，
#   上游树里只留符号链接。上游树一旦被重新克隆/覆盖，链接就会丢，构建会莫名其妙
#   地在“旧代码”上跑（或直接找不到驱动目录）。这里先保证链接正确再编译。
set -euo pipefail

W=$(cd "$(dirname "$0")/.." && pwd)
MESA=${MESA_SRC:-$W/src/oh-mesa/User_source_code/mesa-24.1.2}
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
   *)
      sed -n '2,9p' "$0" | sed 's/^# \{0,1\}//'
      echo "用法: $(basename "$0") [build|relink|check]"
      exit 1 ;;
esac