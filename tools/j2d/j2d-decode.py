#!/usr/bin/env python3
"""把 j2dcap 抓到的厂商 2D 命令流拆成可读的寄存器写序列。

命令字（来自厂商 KMD 源码 jmgpu_accessible.c / jmgpu_pipe2d 的落环逻辑）：
  0x40000000 | reg   → 写寄存器 reg（后一个 u32 是值）
  0x80000000         → NOP
  0x8xxxxxxx（其它）  → 链接/等待描述符（后 3 个 u32 是地址/参数）
首字是头部 byte16_cnt（命令流按 16 字节计数的长度-1），与 KMD 的 ring 描述符一致。

用法：j2d-decode.py <NNNNN-size.bin> [...] 或 j2d-decode.py --dir /tmp/j2d-cap
"""
import glob
import os
import struct
import sys

NOP = 0x80000000
REGWRITE = 0x40000000


def words(data):
    n = len(data) // 4
    return struct.unpack("<%dI" % n, data[: n * 4])


def decode(path):
    data = open(path, "rb").read()
    w = words(data)
    out = []
    out.append("%s (%d bytes, %d words)" % (os.path.basename(path), len(data), len(w)))
    i = 0
    step = 0
    while i < len(w):
        x = w[i]
        if x == NOP:
            out.append("  [%3d] NOP" % i)
            i += 1
            continue
        if (x & 0xF0000000) == 0x40000000:
            # 寄存器写：低 28 位是寄存器偏移；0x6000 段是 2D 槽寄存器
            reg = x & 0x0FFFFFFF
            val = w[i + 1] if i + 1 < len(w) else None
            out.append("  [%3d] WREG 0x%05x = 0x%08x%s" % (i, reg, val or 0,
                        "  (slot %d)" % ((reg - 0x6000) // 4) if 0x6000 <= reg < 0x6080 else ""))
            i += 2
            continue
        if (x & 0xF0000000) == 0x80000000:
            out.append("  [%3d] DESC 0x%08x %s" % (i, x,
                       " ".join("0x%08x" % v for v in w[i + 1:i + 4])))
            i += 4
            continue
        out.append("  [%3d] 0x%08x" % (i, x))
        i += 1
        step += 1
        if step > 4000:
            out.append("  ... (截断)")
            break
    return "\n".join(out) + "\n"


def main():
    args = sys.argv[1:]
    if args[:1] == ["--dir"]:
        files = sorted(glob.glob(os.path.join(args[1], "*.bin")))
    else:
        files = args
    if not files:
        print("用法: j2d-decode.py <bin>... | --dir <capture dir>", file=sys.stderr)
        return 2
    for f in files:
        sys.stdout.write(decode(f) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
