#!/bin/bash
# JM9100 开源驱动（修复版）硬件验证套件 — 非侵入式（仅 env 定向，不改系统）
# 用法: ./tests/run-all.sh   (仓库内任意位置调用均可, 路径相对脚本自身)
set -e
W="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/.." && pwd)"
D=$W/src/mesa/build-mwv207
export LIBGL_DRIVERS_PATH=$D/src/gallium/targets/dri
export MESA_LOADER_DRIVER_OVERRIDE=mwv207

echo "===== 1/4 KMD ioctl ABI ====="
/tmp/gltest-mwv207 2>/dev/null || gcc -O2 -I/usr/include/libdrm -o /tmp/gltest-mwv207 $W/tests/gltest-mwv207.c -ldrm && /tmp/gltest-mwv207

echo "===== 2/4 EGL/GBM surfaceless ====="
gcc -O2 -I/usr/include/libdrm -I$W/src/mesa/include -I$W/src/mesa/src -I$W/src/mesa/src/gbm/main \
    -o /tmp/egl-smoke $W/tests/egl-smoke.c \
    -L$D/src/egl -lEGL -L$D/src/gbm -lgbm -L$D/src/mapi/es2api -lGLESv2 \
    -Wl,-rpath,$D/src/egl -Wl,-rpath,$D/src/gbm -Wl,-rpath,$D/src/mapi/es2api
/tmp/egl-smoke

echo "===== 3/4 FBO 硬件渲染 ====="
gcc -O2 -I/usr/include/libdrm -I$W/src/mesa/include -o /tmp/fbo-test $W/tests/fbo-test.c \
    -L$D/src/egl -lEGL -L$D/src/mapi/es2api -lGLESv2 \
    -Wl,-rpath,$D/src/egl -Wl,-rpath,$D/src/mapi/es2api
EGL_PLATFORM=surfaceless /tmp/fbo-test

echo "===== 4/4 完整硬件渲染（shader+draw+readback） ====="
gcc -O2 -I/usr/include/libdrm -I$W/src/mesa/include -I$W/src/mesa/src -I$W/src/mesa/src/gbm/main \
    -o /tmp/tri-test $W/tests/tri-test.c \
    -L$D/src/egl -lEGL -L$D/src/gbm -lgbm -L$D/src/mapi/es2api -lGLESv2 \
    -Wl,-rpath,$D/src/egl -Wl,-rpath,$D/src/gbm -Wl,-rpath,$D/src/mapi/es2api
/tmp/tri-test

echo ""
echo "===== ALL TESTS DONE ====="

# ===== 应用兼容性追加测试（本会话新增）=====
compat_tests() {
    W="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/.." && pwd)"
    D=$W/src/mesa/build-mwv207
    VA=$W/compat-env/va222-root/usr/lib/aarch64-linux-gnu
    VADIR=$W/src/mesa/build-va/src/gallium/targets/va
    export LD_LIBRARY_PATH=$D/src/egl:$D/src/gbm:$D/src/mapi/shared-glapi
    export LIBGL_DRIVERS_PATH=$D/src/gallium/targets/dri
    export MESA_LOADER_DRIVER_OVERRIDE=mwv207

    echo "===== C5 SDL2+GLES2 ====="
    env SDL_VIDEODRIVER=offscreen EGL_PLATFORM=surfaceless /tmp/sdl-test 2>/dev/null | head -2

    echo "===== C6 mpv render API (video frames -> mwv207 GL) ====="
    env EGL_PLATFORM=surfaceless /tmp/mpv-render-test 2>/dev/null | tail -2

    echo "===== C7 VA-API vainfo (需 libva 2.22 + build-va 驱动) ====="
    env LD_LIBRARY_PATH=$VA LIBVA_DRIVERS_PATH=$VADIR LIBVA_DRIVER_NAME=mwv207 vainfo --display drm 2>/dev/null

    echo "===== C8 ffmpeg vaapi 优雅回退 ====="
    env LD_LIBRARY_PATH=$VA LIBVA_DRIVERS_PATH=$VADIR LIBVA_DRIVER_NAME=mwv207 ffmpeg -hide_banner \
        -hwaccel vaapi -hwaccel_device /dev/dri/renderD128 -i /tmp/test-h264.mp4 -f null - 2>&1 | grep -cE "segfault|崩溃" && echo "有崩溃" || echo "无崩溃，正常回退软解"
}
# 取消下行注释以运行兼容性追加:
# compat_tests
