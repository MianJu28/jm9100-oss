#!/bin/bash
set -u
LOG=/home/admin/vendor-boot.log
ASSETS=/home/admin/vp9-assets
exec >>"$LOG" 2>&1
echo "=== vendor-probe start $(date) ==="

rm -f /etc/modprobe.d/99-vendor-only.conf
rm -f /etc/modules-load.d/jmgpu.conf
systemctl disable vendor-probe.service >/dev/null 2>&1
update-initramfs -u >/dev/null 2>&1
echo "[restore] blacklist removed, initramfs refreshed"

echo "--- uname: $(uname -r)"
lsmod | grep -E "^(mwv207|jmgpu)" || echo "(no mwv207/jmgpu in lsmod)"
ls -l /dev/jmgpu /dev/dri/ 2>&1 | head -20
echo "--- dmesg (gpu) ---"
dmesg | grep -iE "jmgpu|mwv207" | tail -40

for i in $(seq 1 60); do [ -e /dev/jmgpu ] && break; sleep 1; done
if [ ! -e /dev/jmgpu ]; then
    echo "[FATAL] /dev/jmgpu not present"
    echo "=== done $(date) ==="; sync; systemctl reboot; exit 0
fi
echo "[ok] /dev/jmgpu exists"

echo "--- vainfo (jmgpu) ---"
LIBVA_DRIVER_NAME=jmgpu timeout 30 vainfo 2>&1 | head -20

DEV=/dev/dri/renderD128
[ -e "$DEV" ] || DEV=/dev/jmgpu
echo "--- decode trb.webm via vendor stack (dev=$DEV) ---"
rm -f /tmp/trb_vendor.yuv
LIBVA_DRIVER_NAME=jmgpu timeout 120 ffmpeg -v info -y -hwaccel vaapi -hwaccel_device "$DEV" \
    -hwaccel_output_format vaapi -i "$ASSETS/trb.webm" -vf hwdownload,format=nv12 \
    -pix_fmt yuv420p -f rawvideo /tmp/trb_vendor.yuv 2>&1 | tail -30
ls -l /tmp/trb_vendor.yuv 2>&1
python3 - <<'EOF'
try:
    a=open('/tmp/trb_vendor.yuv','rb').read(); w,h=640,256; n=w*h
    if len(a) < n:
        print("vendor yuv too small:", len(a)); raise SystemExit
    top=sum(a[:w*(h//2)])/(w*h//2); bot=sum(a[w*(h//2):n])/(w*h//2)
    print("vendor means: top=%.1f (expect 235) bottom=%.1f (expect 16)" % (top,bot))
except Exception as e:
    print("analysis failed:", e)
EOF

echo "--- gdb register trace on trb.webm ---"
cat >/tmp/vendor-regs.gdb <<'GDB'
set pagination off
set breakpoint pending on
set confirm off
break SetDecRegister
commands
silent
printf "REG %d %u\n", $w1, $w2
continue
end
run
quit
GDB
LIBVA_DRIVER_NAME=jmgpu timeout 180 gdb -q -batch -x /tmp/vendor-regs.gdb --args \
    ffmpeg -v error -y -hwaccel vaapi -hwaccel_device "$DEV" -hwaccel_output_format vaapi \
    -i "$ASSETS/trb.webm" -vf hwdownload,format=nv12 -pix_fmt yuv420p \
    -f rawvideo /tmp/trb_gdb.yuv 2>&1 | grep -E "^REG |Program received|Cannot" | head -400

echo "[done] reboot back to OSS"
echo "=== done $(date) ==="
sync
sleep 3
systemctl reboot
