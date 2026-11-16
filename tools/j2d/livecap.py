#!/usr/bin/env python3
"""在厂商栈上"边跑边抓" 2D 拷贝命令：触发 XCopyArea → 立刻 dump 2D 环 → 解出拷贝命令。

需要 root（读 BAR1/BAR2）。用法：sudo python3 tools/j2d/livecap.py [xcopy 次数]
"""
import mmap, os, struct, subprocess, sys, time

BAR1 = '/sys/bus/pci/devices/0000:07:00.0/resource1'
BAR2 = '/sys/bus/pci/devices/0000:07:00.0/resource2'

f1 = os.open(BAR1, os.O_RDONLY)
m1 = mmap.mmap(f1, 32 << 20, prot=mmap.PROT_READ)
def rd(o): return struct.unpack_from('<I', m1, o)[0]

g = rd(0x96C200)
ring = (g & 0x0fffffff) << 4
print('globals=0x%08x ring=0x%x adstat=0x%08x sync0=0x%08x'
      % (g, ring, rd(0x96C100), rd(0x96E004)))

n = int(sys.argv[1]) if len(sys.argv) > 1 else 30
xcopy = '/home/admin/dev-bin/xcopy'
p = subprocess.Popen([xcopy, ':0', '256', str(n)],
                     stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                     env={**os.environ, 'DISPLAY': ':0',
                          'XAUTHORITY': '/home/admin/.Xauthority'})
time.sleep(0.05)
f2 = os.open(BAR2, os.O_RDONLY)
m2 = mmap.mmap(f2, 256 << 20, prot=mmap.PROT_READ)
ring_bytes = bytes(m2[ring:ring + 0x100000])
p.wait()

# 找每个 4KB 槽里的命令：链头 + 0x8200001c
slots = []
for s in range(0, 0x100000, 0x1000):
    w = struct.unpack_from('<1024I', ring_bytes, s)
    if 0x8200001c not in w:
        continue
    slots.append(s)
print('slots with copy opcode:', len(slots))

def dump_cmd(w):
    idx = [i for i, x in enumerate(w) if x == 0x8200001c]
    for i in idx:
        ops = w[i + 1:i + 8]
        print('  copy@0x%03x: %s' % (i * 4, ' '.join('%08x' % o for o in ops)))
        # 往前找 0x01010/0x01018 寄存器写
        for j in range(max(0, i - 16), i):
            if w[j] in (0x40001010, 0x40001018, 0x40001054, 0x40001008,
                        0x4000100c, 0x40001014, 0x40001020):
                print('    wreg 0x%03x: %08x -> %08x' % (j * 4, w[j], w[j + 1]))

for s in slots[:4]:
    print('slot 0x%x' % (ring + s))
    dump_cmd(struct.unpack_from('<1024I', ring_bytes, s))