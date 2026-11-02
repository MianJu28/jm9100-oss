#!/usr/bin/env bash
# ============================================================================
# make-payload.sh —— 从 Mesa 构建树组装一份新的驱动载荷 (payload-XX)
#
# 为什么需要它:
#   切换脚本假定 "payload-XX 已存在且正确", 但载荷一直是手工拼出来的
#   (10 个实体文件 + MANIFEST + 一堆手写软链)。手工拼装必然漂移 ——
#   2026-10-30 复审就发现 payload-24 仍是 10-21 的二进制, 不含后来落地的
#   GLES 3.2 / 鲁棒访问 / 16 位格式修复。此脚本把这件事变成可复现的一步。
#
# 载荷由两部分组成:
#   * 随 Mesa 重建而变的: libmwv207_dri.so (GL/GLES/DRI) 与
#     libmwv207_drv_video.so (VA-API) —— 从构建树取;
#   * 与 Mesa 版本无关的: libEGL/libGL/libGLESv1_CM/libGLESv2/libgbm/
#     libglapi/libvdpau/libmwv207OpenCL —— 沿用上一份载荷。
#
# 用法:
#   tools/make-payload.sh -v <版本号> [-b <构建目录>] [-f <上游载荷>] [-o <输出目录>]
#
#   默认 -b = ref/oh-mesa/User_source_code/mesa-24.1.2/build-full
#        -f = 仓库内最新 payload-*
#        -o = payload-<上一份序号 + 1>
#   -v 必须显式给出 (不设默认, 逼你不要"忘了改版本号")
#
# 产物: 目标目录下的 10 个实体文件 + SONAME 软链 + MANIFEST.sha256 + VERSION
#       同时打印与上一份载荷的 md5 差异, 便于确认"这次到底换掉了什么"。
# ============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/.." && pwd)"
BUILD="$ROOT/ref/oh-mesa/User_source_code/mesa-24.1.2/build-full"
FROM=""
OUT=""
VER=""

# 10 个实体文件 (MANIFEST 的校验对象) —— 必须与 switch-mwv207.sh 的
# REAL_FILES 完全一致, 否则安装侧会 cp 失败。
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
# 全部从构建树取 —— 不要"沿用上一份载荷"。
# 原因(2026-11-02 实测): Mesa 把版本串 "24.1.2 (git-<sha>)" 编译进每个库。
# 只重建 DRI 时, 构建树里的 libGL/libEGL/libgbm 仍是旧 sha; 若载荷混用
# "新 DRI + 旧 libGL", 加载即报
#   DRI driver not from this Mesa build ('…git-54993602f4' vs '…git-6cb20bd761')
# 随后 fallback 到 swrast/zink → 桌面黑屏或软件渲染。
# 故要求所有产物同源, 并由下方的一致性检查兜底。
FROM_BUILD=(
    "libGL_mwv207.so.1.2.0:src/glx/libGL_mwv207.so.1.2.0"
    "libEGL_mwv207.so.1.0.0:src/egl/libEGL_mwv207.so.1.0.0"
    "libgbm_mwv207.so.1.0.0:src/gbm/libgbm_mwv207.so.1.0.0"
    "libGLESv2_mwv207.so.2.0.0:src/mapi/es2api/libGLESv2_mwv207.so.2.0.0"
    "libGLESv1_CM_mwv207.so.1.1.0:src/mapi/es1api/libGLESv1_CM_mwv207.so.1.1.0"
    "libglapi_mwv207.so.0.0.0:src/mapi/shared-glapi/libglapi_mwv207.so.0.0.0"
    "libmwv207_dri.so:src/gallium/targets/dri/libmwv207_dri.so"
    "libmwv207_drv_video.so:src/gallium/targets/va/libmwv207_drv_video.so"
    "libvdpau_mwv207.so.1.0.0:src/gallium/targets/vdpau/libvdpau_mwv207.so.1.0.0"
    "libmwv207OpenCL.so.1.0.0:src/gallium/targets/rusticl/libmwv207OpenCL.so.1.0.0"
)
# 内部 SONAME 软链 (与 switch-mwv207.sh 的 SONAME_LINKS 保持一致;
# mwv207_dri.so 不在此列 —— 它由安装侧用 cp -l 建实体副本并打 $ORIGIN rpath)
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
    [mwv207_drv_video.so]=libmwv207_drv_video.so
    [libvdpau_mwv207.so.1.0]=libvdpau_mwv207.so.1.0.0
    [libvdpau_mwv207.so.1]=libvdpau_mwv207.so.1.0.0
    [libvdpau_mwv207.so]=libvdpau_mwv207.so.1.0.0
    [libmwv207OpenCL.so.1]=libmwv207OpenCL.so.1.0.0
    [libmwv207OpenCL.so]=libmwv207OpenCL.so.1.0.0
)

die() { echo "错误: $*" >&2; exit 1; }
info() { echo "[make-payload] $*"; }

while getopts ":b:f:o:v:h" opt; do
    case "$opt" in
        b) BUILD="$OPTARG" ;;
        f) FROM="$OPTARG" ;;
        o) OUT="$OPTARG" ;;
        v) VER="$OPTARG" ;;
        h) sed -n '2,40p' "$0"; exit 0 ;;
        *) die "未知参数 -$OPTARG (见 -h)" ;;
    esac
done
[ -n "$VER" ] || die "必须用 -v 显式给出版本号 (例如 -v 24.1.2-fullstack-r2)"
[ -d "$BUILD" ] || die "构建目录不存在: $BUILD"

# 默认上游载荷 = 仓库内最新的 payload-*
if [ -z "$FROM" ]; then
    # 只认"完整"的载荷 (有 MANIFEST + VERSION)。半成品目录(上次中途退出)
    # 否则会被选成上游, 导致 cp 失败或照着残缺清单做差异对比。
    for d in $(ls -d "$ROOT"/payload-* 2>/dev/null | sort -rV); do
        if [ -f "$d/MANIFEST.sha256" ] && [ -f "$d/VERSION" ]; then FROM="$d"; break; fi
    done
fi
[ -n "$FROM" ] && [ -f "$FROM/MANIFEST.sha256" ] || die "找不到上游载荷 (用 -f 指定)"

# 默认输出 = 上一份序号 + 1
if [ -z "$OUT" ]; then
    last=$(basename "$FROM" | sed 's/^payload-//')
    case "$last" in
        ''|*[!0-9]*) die "无法从 $(basename "$FROM") 推断序号, 请用 -o 指定输出目录" ;;
    esac
    OUT="$ROOT/payload-$((last + 1))"
fi
[ -e "$OUT" ] && die "输出目录已存在: $OUT (先删掉或换 -o)"

info "构建目录 : $BUILD"
info "上游载荷 : $FROM"
info "输出目录 : $OUT"
info "版本号   : $VER"

mkdir -p "$OUT"

# 1) 全部从构建树取
for pair in "${FROM_BUILD[@]}"; do
    dst="${pair%%:*}"
    rel="${pair#*:}"
    src="$BUILD/$rel"
    [ -f "$src" ] || die "构建产物缺失: $src (先 ninja)"
    cp -L "$src" "$OUT/$dst"
    info "取自构建树: $dst  <- $rel"
done

# 2) 兜底：FROM_BUILD 没覆盖到的 REAL_FILES 才沿用上游载荷（正常情况下应为空）
for f in "${REAL_FILES[@]}"; do
    [ -f "$OUT/$f" ] && continue
    [ -f "$FROM/$f" ] || die "上游载荷缺文件: $FROM/$f"
    warn "沿用上游载荷（构建树未提供）: $f"
    cp -L "$FROM/$f" "$OUT/$f"
done

# 2.5) 一致性检查：所有带版本戳的库必须同源
#      Mesa 的版本串编译进每个库；混用不同 git sha 的 libGL 与 DRI,
#      加载时会报 "DRI driver not from this Mesa build" 并退回 swrast。
#      这是唯一能在部署前拦住"黑屏"的检查, 不要删。
STAMPS=$(for f in "$OUT"/*.so*; do
            [ -f "$f" ] || continue
            # 末尾 || true 必需: 无版本戳的库会让 grep 返回 1,
            # 在 set -euo pipefail 下会静默杀掉整个脚本
            strings -a "$f" 2>/dev/null | grep -oE '24\.1\.2 \(git-[0-9a-f]+\)' || true
         done | sort -u)
NSTAMP=$(printf '%s\n' "$STAMPS" | grep -c . || true)
if [ "$NSTAMP" -gt 1 ]; then
    printf '  发现 %s 个不同版本戳:\n' "$NSTAMP" >&2
    for f in "$OUT"/*.so*; do
        [ -f "$f" ] || continue
        s=$(strings -a "$f" 2>/dev/null | grep -oE '24\.1\.2 \(git-[0-9a-f]+\)' | sort -u | tr '\n' ' ')
        [ -n "$s" ] && printf '    %-34s %s\n' "$(basename "$f")" "$s" >&2
    done
    die "载荷内版本戳不一致 —— 部署会因 DRI/客户端版本握手失败而黑屏。请先在构建树跑完整 ninja"
fi
if [ "$NSTAMP" -eq 1 ]; then
    info "版本戳一致性: $STAMPS（全部同源）"
fi

# 3) DRI 的 RUNPATH 必须改成 $ORIGIN
#    构建树里它是 "$ORIGIN/../../../mapi/shared-glapi" (只在构建树成立),
#    装到 /usr/local/lib/mwv207-oss 后该路径不存在, 必须指回自己所在目录,
#    才能解析 NEEDED libglapi_mwv207.so.0。
command -v patchelf >/dev/null || die "缺 patchelf"
patchelf --set-rpath '$ORIGIN' "$OUT/libmwv207_dri.so"
[ "$(objdump -p "$OUT/libmwv207_dri.so" | awk '/RUNPATH|RPATH/{print $NF}')" = '$ORIGIN' ] \
    || die "DRI RUNPATH 设置失败"

# 4) SONAME 自检 —— 载荷文件名必须与 .so 内部 SONAME 一致, 否则 ldconfig 收录不到
[ "$(objdump -p "$OUT/libmwv207_dri.so" | awk '/SONAME/{print $NF}')" = "libmwv207_dri.so" ] \
    || die "DRI SONAME 不符"
[ "$(objdump -p "$OUT/libmwv207_drv_video.so" | awk '/SONAME/{print $NF}')" = "libmwv207_drv_video.so" ] \
    || die "VA SONAME 不符"

# 5) 内部软链 + 实体副本
for f in "${!SONAME_LINKS[@]}"; do
    ln -sfn "${SONAME_LINKS[$f]}" "$OUT/$f"
done
ln -f "$OUT/libmwv207_dri.so" "$OUT/mwv207_dri.so"

# 6) MANIFEST + VERSION
( cd "$OUT" && sha256sum "${REAL_FILES[@]}" > MANIFEST.sha256 )
echo "$VER" > "$OUT/VERSION"
( cd "$OUT" && sha256sum -c MANIFEST.sha256 >/dev/null ) || die "MANIFEST 自校验失败"

# 7) 与上游对比, 让"这次到底换了什么"一目了然
info "与 $(basename "$FROM") 的差异:"
for f in "${REAL_FILES[@]}"; do
    a=$(md5sum "$FROM/$f" 2>/dev/null | cut -d' ' -f1)
    b=$(md5sum "$OUT/$f"  2>/dev/null | cut -d' ' -f1)
    if [ "$a" = "$b" ]; then
        printf '  =  %s\n' "$f"
    else
        printf '  *  %s   %s -> %s\n' "$f" "${a:0:12}" "${b:0:12}"
    fi
done

# 8) 顺带把 DRI 里编进去的 GLSL 版本标记打出来 —— 用来确认这一版真带上了
#    GLES 3.2 之类的改动 (纯字符串, 不执行任何代码)
info "DRI 内 GLSL 版本标记:"
strings "$OUT/libmwv207_dri.so" | grep -oE 'OpenGL ES GLSL ES [0-9]+\.[0-9]+' | sort -u | sed 's/^/  /' || true

info "完成: $OUT"
info "下一步: 只读核对 ——  ./switch-mwv207.sh umd check   (不会动系统)"
info "        真正部署 ——  sudo ./switch-mwv207.sh umd install"