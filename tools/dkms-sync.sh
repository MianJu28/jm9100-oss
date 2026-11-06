#!/usr/bin/env bash
# 把仓库里的开源 KMD 源码同步到 DKMS 源目录，并用 DKMS 重建/安装 mwv207 模块。
#
# 为什么要这个脚本：src/kernel-6.6 只是仓库里的源码树，内核升级后 dkms 不会
# 自动编译它。注册成 DKMS 包（mwv207-oss）后，内核升级时 dkms autoinstall
# 会重新编译，避免退回发行版内建（或厂商 jmgpu）模块。
#
# 用法（需要 root）：
#   sudo ./tools/dkms-sync.sh              # 默认版本 1.0.0
#   sudo ./tools/dkms-sync.sh 1.0.1        # 改版本号（源码有改动时递增）
#
# 安装位置：/lib/modules/$(uname -r)/updates/dkms/mwv207.ko
# 因为文件名为 mwv207，depmod 会把 updates/dkms 里的版本排在
# kernel/drivers/gpu/drm/mwv207/ 的内建版之前，重启后即生效。
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
src="$here/src/kernel-6.6"
pkg="mwv207-oss"
ver="${1:-1.0.0}"
dst="/usr/src/${pkg}-${ver}"

if [ "$(id -u)" -ne 0 ]; then
   echo "需要 root，请用 sudo $0 $*" >&2
   exit 1
fi
if [ ! -d "$src" ]; then
   echo "缺少源码目录 $src" >&2
   exit 1
fi

# 模块编译依赖 <generated/autoconf.h>。Deepin 的 headers 包里带这个文件；如果它
# 被 "make M=" 之类的误操作删掉（kbuild 会尝试 syncconfig，而 headers 包没有
# scripts/kconfig），编译会报 generated/autoconf.h 找不到。可从 ostree 部署里还原：
#   sudo rsync -a /ostree/deploy/deepin/deploy/<当前部署>/usr/src/linux-headers-$(uname -r)/ \
#        /usr/src/linux-headers-$(uname -r)/
KDIR="/lib/modules/$(uname -r)/build"
if [ ! -f "$KDIR/include/generated/autoconf.h" ]; then
   echo "错误: $KDIR/include/generated/autoconf.h 缺失，内核头文件不完整。" >&2
   echo "      可从 ostree 部署快照还原（见脚本内注释），再重跑本脚本。" >&2
   exit 1
fi

echo "== 同步 $src -> $dst"
rm -rf "$dst"
mkdir -p "$dst"
rsync -a --exclude '*.o' --exclude '*.ko' --exclude '*.mod' \
      --exclude '*.mod.c' --exclude '*.cmd' --exclude '.*.cmd' \
      --exclude 'modules.order' --exclude 'Module.symvers' \
      --exclude '.tmp_versions' --exclude '*.d' \
      "$src"/ "$dst"/

# dkms 会把源码拷贝到 /var/lib/dkms/<pkg>/<ver>/build，必须用 ${build_dir} 编译，
# 否则产物不在 build 根目录、dkms 会报 "Build of mwv207.ko failed"。
cat > "$dst/dkms.conf" <<EOF
PACKAGE_NAME="$pkg"
PACKAGE_VERSION="$ver"
MAKE[0]="make -C /lib/modules/\${kernelver}/build M=\${build_dir} CONFIG_DRM_MWV207=m"
CLEAN="make -C /lib/modules/\${kernelver}/build M=\${build_dir} clean"
BUILT_MODULE_NAME[0]="mwv207"
DEST_MODULE_LOCATION[0]="/updates/dkms"
AUTOINSTALL="YES"
EOF

if dkms status -m "$pkg" -v "$ver" >/dev/null 2>&1; then
   echo "== 已注册，先移除旧条目以便重新读取 dkms.conf"
   dkms remove -m "$pkg" -v "$ver" --all >/dev/null 2>&1 || true
fi
echo "== dkms add $pkg/$ver"
dkms add -m "$pkg" -v "$ver"
echo "== dkms build $pkg/$ver --force"
dkms build -m "$pkg" -v "$ver" --force
echo "== dkms install $pkg/$ver --force"
dkms install -m "$pkg" -v "$ver" --force
depmod -a

ko="$(modinfo -n mwv207 2>/dev/null || true)"
echo "== 完成"
echo "   模块路径: ${ko:-<modinfo 失败>}"
if [ -n "$ko" ]; then
   echo "   srcversion: $(modinfo -F srcversion "$ko")"
fi
if [ -r /sys/module/mwv207/srcversion ]; then
   echo "   运行中:     $(cat /sys/module/mwv207/srcversion)（重启后才会换成新构建）"
fi
echo "   dkms status: $(dkms status -m "$pkg" -v "$ver" || echo 无)"