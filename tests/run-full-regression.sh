#!/bin/bash
# 全功能回归：GL + VA硬解 + OpenCL 一键验证（env 非侵入, 构建树定向）
# 用法: ./tests/run-full-regression.sh   (仓库内任意位置调用均可, 路径相对脚本自身)
# 前置: 先运行 ./tests/run-all.sh 生成 /tmp/{fbo-test,tri-test,cl-test} 二进制
R="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/.." && pwd)"
W="$R/src/oh-mesa/User_source_code/mesa-24.1.2/build-full"
export LIBGL_DRIVERS_PATH=$W/src/gallium/targets/dri
export LD_LIBRARY_PATH=$W/src/egl:$W/src/gbm:$W/src/mapi/shared-glapi
export MESA_LOADER_DRIVER_OVERRIDE=mwv207
VA="$R/compat-env/va222-root/usr/lib/aarch64-linux-gnu"

# OpenCL ICD 平坦目录 (rusticl 构建产物)
mkdir -p /tmp/ocl-icd
echo "$W/src/gallium/targets/rusticl/libmwv207OpenCL.so.1" > /tmp/ocl-icd/mwv207.icd

echo "===== A. GL 硬件渲染（GL 4.x caps: DOUBLES/INT64） ====="
D=$W
env LD_LIBRARY_PATH=$W/src/egl:$W/src/gbm:$W/src/mapi/shared-glapi EGL_PLATFORM=surfaceless /tmp/fbo-test 2>/dev/null | tail -2

echo "===== B. 完整渲染管线 ====="
env LD_LIBRARY_PATH=$W/src/egl:$W/src/gbm:$W/src/mapi/shared-glapi EGL_PLATFORM=surfaceless /tmp/tri-test 2>/dev/null | tail -1

echo "===== C. VA 硬件解码（H264+HEVC） ====="
env LD_LIBRARY_PATH=$VA LIBVA_DRIVERS_PATH=$W/src/gallium/targets/va LIBVA_DRIVER_NAME=mwv207 vainfo --display drm 2>/dev/null | grep -cE "VLD" | xargs -I{} echo "VLD entrypoints: {} (H264x4 + HEVCx2)"
env LD_LIBRARY_PATH=$VA LIBVA_DRIVERS_PATH=$W/src/gallium/targets/va LIBVA_DRIVER_NAME=mwv207 ffmpeg -hide_banner -loglevel error -hwaccel vaapi -hwaccel_device /dev/dri/renderD128 -i /tmp/test-1080p.mp4 -frames:v 1 -y /tmp/reg-hw.png && echo "H264 1080p hw frame: OK"

echo "===== D. OpenCL GPU 计算 ====="
env RUSTICL_ENABLE=mwv207 OCL_ICD_VENDORS=/tmp/ocl-icd LD_LIBRARY_PATH=$W/src/gallium/targets/rusticl "$R/tests/cl-test" 2>/dev/null | tail -1

echo "===== ALL DONE ====="
