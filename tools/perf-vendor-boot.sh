#!/bin/bash
# 开机后（厂商栈）自动跑一遍性能套件，跑完切回 oss 栈（会再重启一次）。
# 由 mwv207-perf-vendor.service 调用；只在 /var/lib/mwv207-oss/perf-vendor-request 存在时执行。
set -u
REQ=/var/lib/mwv207-oss/perf-vendor-request
LOG=/home/admin/perf-vendor-run.log
[ -f "$REQ" ] || exit 0
rm -f "$REQ"
exec >>"$LOG" 2>&1
echo "==== perf-vendor-boot $(date '+%F %T') ===="

# 等 X 起来（厂商 DDX）
for i in $(seq 90); do
    DISPLAY=:0 XAUTHORITY=/home/admin/.Xauthority xdpyinfo >/dev/null 2>&1 && break
    sleep 2
done
echo "X ready: $(DISPLAY=:0 XAUTHORITY=/home/admin/.Xauthority xdpyinfo 2>/dev/null | head -1)"

cd /home/admin/jm9100-oss || exit 1
runuser -u admin -- env \
    DISPLAY=:0 XAUTHORITY=/home/admin/.Xauthority \
    MESA_LOADER_DRIVER_OVERRIDE=jmgpu \
    LIBVA_DRIVER_NAME=jmgpu \
    bash -lc 'cd /home/admin/jm9100-oss && ./tools/perf-suite.sh vendor'
echo "---- suite rc=$? ----"
touch /home/admin/perf-vendor.done

# 切回开源栈（重启释放路径）
./switch-mwv207.sh stack oss
echo "==== perf-vendor-boot end $(date '+%F %T') ===="
