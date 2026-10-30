#!/usr/bin/env python3
"""Build the full MWV207 register database from the vendor KMD headers, then
join it with the registers the open-source UMD driver actually programs.

Register identification
-----------------------
The vendor headers define, per register R (e.g. TX_CONFIG2):
    MWV207REG_R_Address    0x10780     <- hardware address
    MWV207REG_R_MSB/LSB/BLK/Count/FieldMask/ReadMask/WriteMask/ResetValue
    MWV207REG_R_<FIELD>    <msb>:<lsb> <- field bit range
    MWV207REG_R_<FIELD>_End/_Start/_Type  U02
    MWV207REG_R_<FIELD>_<VALUE> 0x<n>  <- named enum value

Driver mapping: the UMD's 16-bit command register ID equals Address >> 2.
"""
import os, re, json, glob, sys
from collections import defaultdict

VENDOR_DIR = "/tmp/dpkg/gg/usr/src/mwv207-1.6.1-v25.u1"
DRIVER_REGS = "/home/admin/jm9100-oss/docs/registers/command-registers.json"
OUT_JSON = "/home/admin/jm9100-oss/docs/registers/register-db.json"

define_re = re.compile(r"^\s*#define\s+(MWV207REG_[A-Za-z0-9_]+)\s+(.*?)\s*$")
reg_props = {"Address", "MSB", "LSB", "BLK", "Count", "FieldMask", "ReadMask",
             "WriteMask", "ResetValue"}

macros = []          # (name, value, file, line)
for path in sorted(glob.glob(os.path.join(VENDOR_DIR, "*.h"))):
    for i, line in enumerate(open(path, encoding="utf-8", errors="replace"), 1):
        m = define_re.match(line)
        if m:
            macros.append((m.group(1), m.group(2), os.path.basename(path), i))

# 1. register names = those declaring _Address
regs = {}
for name, val, f, ln in macros:
    if name.endswith("_Address"):
        r = name[:-len("_Address")]
        try:
            regs[r] = {"name": r, "address": int(val, 16), "src": "%s:%d" % (f, ln),
                       "fields": {}, "props": {}}
        except ValueError:
            pass

reg_names = sorted(regs, key=len, reverse=True)

def split_reg(name):
    """Longest register-name prefix of a macro name."""
    for r in reg_names:
        if name.startswith(r + "_"):
            return r, name[len(r) + 1:]
    return None, None

# 2. field bit ranges (unambiguous field names)
field_hint = defaultdict(set)     # reg -> {field}
for name, val, f, ln in macros:
    r, rest = split_reg(name)
    if r is None:
        continue
    m = re.match(r"^(\d+):(\d+)$", val)
    if m and re.match(r"^[A-Za-z0-9_]+$", rest):
        fld, msb, lsb = rest, int(m.group(1)), int(m.group(2))
        regs[r]["fields"][fld] = {"name": fld, "msb": msb, "lsb": lsb,
                                  "width": msb - lsb + 1, "values": {}}
        field_hint[r].add(fld)

# 3. properties, field attributes, enum values
for name, val, f, ln in macros:
    r, rest = split_reg(name)
    if r is None:
        continue
    if rest in reg_props:
        regs[r]["props"][rest] = val
        continue
    m = re.match(r"^([A-Za-z0-9_]+)_(End|Start|Type)$", rest)
    if m and m.group(1) in field_hint[r]:
        regs[r]["fields"][m.group(1)][m.group(2).lower()] = val
        continue
    hit = False
    for fld in sorted(field_hint[r], key=len, reverse=True):
        if rest.startswith(fld + "_"):
            enum = rest[len(fld) + 1:]
            try:
                regs[r]["fields"][fld]["values"][enum] = int(val, 0)
            except ValueError:
                regs[r]["fields"][fld]["values"][enum] = val
            hit = True
            break
    if hit:
        continue

# 4. join with driver usage (driver id == address >> 2)
drv = json.load(open(DRIVER_REGS))
by_id = defaultdict(list)
for r in regs.values():
    by_id[r["address"] >> 2].append(r["name"])

joined = {}
unmapped = []
for key, uses in drv.items():
    m = re.match(r"^0x([0-9a-f]+)$", key)
    if not m:
        continue
    rid = int(m.group(1), 16)
    names = by_id.get(rid)
    if names:
        joined[key] = {"id": rid, "vendor": names, "uses": uses}
        for n in names:
            regs[n].setdefault("driver_ids", []).append(key)
    else:
        unmapped.append((key, rid, len(uses)))

for r in regs.values():
    r.setdefault("driver_ids", [])

json.dump({"registers": {k: v for k, v in sorted(regs.items())},
           "driver_join": joined},
          open(OUT_JSON, "w"), indent=1, ensure_ascii=False, sort_keys=False)

nfields = sum(len(r["fields"]) for r in regs.values())
print("registers parsed        :", len(regs))
print("fields parsed           :", nfields)
print("driver IDs in usage JSON:", len([k for k in drv if re.match(r'^0x[0-9a-f]+$', k)]))
print("  mapped to a register  :", len(joined))
print("  unmatched             :", len(unmapped))
print()
print("unmatched driver IDs (no vendor Address>>2):")
for k, rid, n in sorted(unmapped, key=lambda x: -x[2])[:25]:
    print("   %-6s (addr would be 0x%x) uses=%d" % (k, rid << 2, n))
