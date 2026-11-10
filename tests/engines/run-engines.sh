#!/bin/bash
# 引擎数量回归（N1）：读 KMD 经 DRM_IOCTL_MWV207_INFO 上报的引擎计数。
#
# lite 版 JM9100（`jdev->lite` 由 0x12c0000 寄存器是否等于 0x21 决定）只有
# 3d0/dec0/2d0/dma0 四个引擎，没有 3d1/dec1/enc0；非 lite 的 MWV207 才有
# 2×3D、2×DEC、1×ENC。期望值可用 $1 覆盖，默认按 lite 判。
set -u
cd "$(dirname "$0")" || exit 1
cc -O2 -I/usr/include/libdrm -o /tmp/mwv207-engines engines.c -ldrm 2>&1 || {
   echo "编译失败"; exit 1; }
exec /tmp/mwv207-engines "${1:-3d=1,dec=1,enc=0,2d=1,dma=1}"
