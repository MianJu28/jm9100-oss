#!/bin/sh
# Firefox 渲染异常 A/B 启动器。
#
# 用途：把"火狐特定页面出乱码/条纹"的现场，按四种驱动配置复跑一次，
# 用来判定问题落在哪条路径上（不需要改系统、不需要重装驱动）。
#
# 用法：
#   tools/ff-ab.sh base      # 安装栈 + 默认配置（对照组）
#   tools/ff-ab.sh notile    # 客户端窗口缓冲保持线性（关闭"平铺 + 线性影子"）
#   tools/ff-ab.sh bufwait   # 开发驱动 + 恢复"映射写前一律等待"（N19 之前的行为）
#   tools/ff-ab.sh all       # 两者都开
#   tools/ff-ab.sh soft      # 火狐 GL 走 llvmpipe（软件渲染，完全绕开我们的驱动）
#   tools/ff-ab.sh wr0       # 关闭 WebRender（basic compositor）
#   tools/ff-ab.sh --clean   # 删除这四个 profile
#
# 每次使用独立 profile（/tmp/ff-ab-<mode>），互不干扰当前 Firefox。
# 判读：notile 下消失 ⇒ 影子拷贝路径；bufwait 下消失 ⇒ 常量环/映射写竞态；
#       都消失 ⇒ 两者叠加；都不消失 ⇒ 在 X 合成或 WebRender 自身。
set -e

MODE="${1:-base}"
DEV_DRI="${DEV_DRI:-$HOME/dev-dri}"

if [ "$MODE" = "--clean" ]; then
   rm -rf /tmp/ff-ab-base /tmp/ff-ab-notile /tmp/ff-ab-bufwait /tmp/ff-ab-all /tmp/ff-ab-soft /tmp/ff-ab-wr0
   echo "[ff-ab] 已清理 profile"
   exit 0
fi

PROFILE="/tmp/ff-ab-$MODE"
mkdir -p "$PROFILE"

ENV_EXTRA=""
case "$MODE" in
   base)    ;;
   notile)  ENV_EXTRA="MWV207_SCANOUT_TILE=0" ;;
   soft)    ENV_EXTRA="LIBGL_ALWAYS_SOFTWARE=1 MOZ_WEBRENDER=1" ;;
   wr0)     ENV_EXTRA="MOZ_WEBRENDER=0" ;;
   bufwait) ENV_EXTRA="LIBGL_DRIVERS_PATH=$DEV_DRI MESA_LOADER_DRIVER_OVERRIDE=mwv207 MWV207_BUFWAIT_ALL=1" ;;
   all)     ENV_EXTRA="MWV207_SCANOUT_TILE=0 LIBGL_DRIVERS_PATH=$DEV_DRI MESA_LOADER_DRIVER_OVERRIDE=mwv207 MWV207_BUFWAIT_ALL=1" ;;
   *) echo "用法: $0 {base|notile|bufwait|all|soft|wr0|--clean}" >&2; exit 2 ;;
esac

if [ "$MODE" = "bufwait" ] || [ "$MODE" = "all" ]; then
   if [ ! -f "$DEV_DRI/mwv207_dri.so" ]; then
      echo "[ff-ab] 缺少开发驱动: $DEV_DRI/mwv207_dri.so（先跑 ./tools/build-umd.sh 并拷到该目录）" >&2
      exit 1
   fi
fi

echo "[ff-ab] mode=$MODE profile=$PROFILE"
echo "[ff-ab] env: $ENV_EXTRA"
echo "[ff-ab] 用这个 profile 打开出问题的页面，观察是否复现；关闭窗口即结束。"

# shellcheck disable=SC2086
env $ENV_EXTRA firefox --profile "$PROFILE" --no-remote "$@" >/dev/null 2>&1 &
echo "[ff-ab] firefox pid=$!"