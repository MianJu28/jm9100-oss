#!/usr/bin/env python3
"""Try to map the driver's emitted 16-bit command register IDs onto the vendor
register database's _Address values, by testing simple linear transforms."""
import os, re, json, glob
from collections import defaultdict

VENDOR = "/tmp/dpkg/gg/usr/src/mwv207-1.6.1-v25.u1"
addr = {}
for p in glob.glob(os.path.join(VENDOR, "*.h")):
    for line in open(p, encoding="utf-8", errors="replace"):
        m = re.match(r"#define\s+(MWV207REG_[A-Z0-9_]+)_Address\s+0x([0-9a-fA-F]+)\s*$", line)
        if m:
            addr[m.group(1)] = int(m.group(2), 16)

drv = json.load(open("/home/admin/jm9100-oss/docs/registers/command-registers.json"))
drv_regs = {int(k, 16) for k in drv if re.match(r"^0x[0-9a-f]+$", k)}
print("vendor registers with _Address :", len(addr))
print("driver emitted register IDs   :", len(drv_regs))
print()

# Build reverse index of vendor addresses (multiple names may share an address)
by_addr = defaultdict(list)
for name, a in addr.items():
    by_addr[a].append(name)

for shift in (0, 1, 2, 3, 4, 5, 6):
    hit = {}
    for r in drv_regs:
        cand = r << shift
        if cand in by_addr:
            hit[r] = by_addr[cand]
    print("shift %d : %3d / %d driver regs matched (%.0f%%)" %
          (shift, len(hit), len(drv_regs), 100.0 * len(hit) / len(drv_regs)))
    if hit and shift in (2, 3):
        for r in sorted(hit)[:14]:
            print("    0x%04x << %d = 0x%05x -> %s" % (r, shift, r << shift, ", ".join(hit[r][:3])))
