#!/usr/bin/env python3
"""2D 拷贝模板验证（raw BAR 写，需要 root）。

按厂商 ring 里捕获到的真实拷贝命令形状发一条链，验证"raw 地址进 0x01010/0x01018 +
7 个操作数"这套编码在自研栈上是否出像素。

厂商捕获（0x213000 环）：
    40001010 <dst>            40001018 <src>            40001054 ffffffff
    8200001c 000200f0 ((pitch/16)<<16)|off (x|y<<16) (h<<16|w) <dst> <src> (h<<16|w)
    81000000

用法：sudo python3 tools/j2d/copytest.py [src_addr] [dst_addr] [size]
"""
import mmap, os, struct, sys, time

BAR1 = '/sys/bus/pci/devices/0000:07:00.0/resource1'
BAR2 = '/sys/bus/pci/devices/0000:07:00.0/resource2'
f1 = os.open(BAR1, os.O_RDWR | os.O_SYNC)
f2 = os.open(BAR2, os.O_RDWR | os.O_SYNC)
m1 = mmap.mmap(f1, 32 << 20, prot=mmap.PROT_READ | mmap.PROT_WRITE)
m2 = mmap.mmap(f2, 256 << 20, prot=mmap.PROT_READ | mmap.PROT_WRITE)

def rd(o): return struct.unpack_from('<I', m1, o)[0]
def wr(o, v):
    struct.pack_into('<I', m1, o, v)
    m1.flush(o & ~0xfff, 0x1000)
def vwr(o, v): struct.pack_into('<I', m2, o, v)
def vrd(o): return struct.unpack_from('<I', m2, o)[0]

src = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0x0c000000
dst = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x0c100000
n   = int(sys.argv[3], 10) if len(sys.argv) > 3 else 256
pitch = n * 4

# 源面：每行一个可识别的行号图案
for y in range(n):
    for x in range(0, n, 16):
        vwr(src + y * pitch + x * 4, (0x11000000 | (y << 8) | (x & 0xff)))
m2.flush(src & ~0xfff, pitch * n)

# 目标面清 0
for y in range(n):
    for x in range(n):
        vwr(dst + y * pitch + x * 4, 0)
m2.flush(dst & ~0xfff, pitch * n)

ring = (rd(0x96C200) & 0x0fffffff) << 4
chain = ring + 0x1000
wh = (n << 16) | n
cmds = [
    0x80000000, 0, 0x80000000, 0x80000000,
    0x40001010, dst,
    0x40001018, src,
    0x40001054, 0xffffffff,
    0x8200001c, 0x000200f0, ((pitch // 16) << 16) | 0, 0, wh, dst, src, wh,
    0x81000000,
    0x80000000, 0x80000000,
    0x40006004, 0x4321, 0x80000000, 0x80000000,
    0x88000000, 0x80000000, 0x80000000, 0x80000000,
]
cmds[1] = len(cmds) // 4 - 1
for i, w in enumerate(cmds):
    vwr(chain + 4 * i, w)
vwr(ring + 0x20, 0x89000000)
vwr(ring + 0x24, chain)
m2.flush(chain & ~0xfff, 0x1000)
m2.flush(ring & ~0xfff, 0x1000)

wr(0x96CC00, 1)
time.sleep(0.2)
ok = 0
first_bad = None
for y in range(n):
    for x in range(0, n, 16):
        want = 0x11000000 | (y << 8) | (x & 0xff)
        if vrd(dst + y * pitch + x * 4) == want:
            ok += 1
        elif first_bad is None:
            first_bad = (x, y, vrd(dst + y * pitch + x * 4))
print('sync=0x%08x dst=0x%x matched_rows_samples=%d/%d first_bad=%s'
      % (rd(0x96E004), dst, ok, n * (n // 16), first_bad))