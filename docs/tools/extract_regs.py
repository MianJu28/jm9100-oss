#!/usr/bin/env python3
"""Extract every command-register write the mwv207 UMD driver emits.

Scans for the MWV207_EMIT / EMITF / EMITS / EMITFS macros (and MWV207_STALL,
MWV207_SNAP_TO_PAGE), collects the register address and the value expression
(handling multi-line calls via balanced-parenthesis scanning) and prints a
per-register report.
"""
import os, re, sys, json
from collections import defaultdict

ROOTS = [
    "/home/admin/jm9100-oss/src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium/drivers/mwv207",
    "/home/admin/jm9100-oss/src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium/winsys/mwv207",
]
MACROS = ["MWV207_EMITFS", "MWV207_EMITS", "MWV207_EMITF", "MWV207_EMIT", "MWV207_STALL"]

def scan_call(text, start):
    """Return the full call text beginning at 'start' (index of macro name)."""
    i = text.find("(", start)
    if i < 0:
        return None
    depth, j = 0, i
    while j < len(text):
        if text[j] == "(":
            depth += 1
        elif text[j] == ")":
            depth -= 1
            if depth == 0:
                return text[start:j + 1]
        j += 1
    return None

def split_args(call):
    """Split the argument list of a call on top-level commas."""
    i = call.find("(")
    inner = call[i + 1:-1]
    out, depth, cur = [], 0, ""
    for ch in inner:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip()); cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out

regs = defaultdict(list)
files = 0
for root in ROOTS:
    for dirpath, _, names in os.walk(root):
        for n in sorted(names):
            if not n.endswith((".c", ".h")):
                continue
            path = os.path.join(dirpath, n)
            rel = os.path.relpath(path, ROOTS[0])
            files += 1
            text = open(path, encoding="utf-8", errors="replace").read()
            for m in re.finditer(r"\b(MWV207_EMITFS|MWV207_EMITS|MWV207_EMITF|MWV207_EMIT|MWV207_STALL)\b", text):
                macro = m.group(1)
                call = scan_call(text, m.start())
                if not call:
                    continue
                args = split_args(call)
                line = text.count("\n", 0, m.start()) + 1
                # MWV207_STALL(cmd, value) has no register; record it separately.
                if macro == "MWV207_STALL":
                    regs["<STALL>"].append({"file": rel, "line": line, "value": args[1] if len(args) > 1 else ""})
                    continue
                if len(args) < 3:
                    continue
                reg, value = args[1], args[2]
                if not reg.startswith("0x"):
                    regs["<computed:%s>" % reg].append({"file": rel, "line": line, "value": value})
                    continue
                regs[reg.lower()].append({"file": rel, "line": line, "macro": macro, "value": value})

out = {k: v for k, v in sorted(regs.items())}
json.dump(out, open("/home/admin/jm9100-oss/docs/registers/command-registers.json", "w"), indent=1, ensure_ascii=False)

print("scanned files:", files)
print("distinct registers:", len(out))
print()
print("%-10s %5s  %s" % ("reg", "uses", "files"))
for reg, uses in sorted(out.items(), key=lambda kv: (-len(kv[1]), kv[0])):
    fl = sorted({u["file"] for u in uses})
    print("%-10s %5d  %s" % (reg, len(uses), ", ".join(fl[:3]) + (" …" if len(fl) > 3 else "")))
