#!/bin/bash
# 只读扫描显示窗口寄存器（0x990000 与 0x991000 两块）。
# 用法: sudo ./tools/scan-dc/dcdump.sh [label]
# 依赖: ~/dev-bin/barmap（或 BARMAP= 指定），读 BAR1。
set -u
BAR1=/sys/bus/pci/devices/0000:07:00.0/resource1
if [ -z "${BARMAP:-}" ]; then
  ub=$(getent passwd "${SUDO_USER:-$USER}" | cut -d: -f6)
  BARMAP=${ub:-$HOME}/dev-bin/barmap
fi
label=${1:-$(date +%H%M%S)}
[ -x "$BARMAP" ] || { echo "缺少 $BARMAP"; exit 1; }
[ -r "$BAR1" ] || { echo "读不到 $BAR1（要 root）"; exit 1; }

printf '%s\n' "$label"
for base in 0x990000 0x991000; do
  echo "== base $base"
  for page in 0 0x100 0x200 0x300 0x400 0x500 0x600 0x700 0x800 0x900 0xa00 0xb00 0xc00 0xd00 0xe00 0xf00; do
    off=$((base + page))
    for i in $(seq 0 4 0x3c); do
      a=$(printf '0x%x' $((off + i)))
      v=$($BARMAP "$BAR1" "$a" 2>/dev/null | awk '{print $NF}')
      [ -n "${v:-}" ] || continue
      case "$v" in 0x00000000|0xffffffff) continue;; esac
      printf '  %05x = %s\n' $((page + i)) "$v"
    done
  done
done
