#!/usr/bin/env python3
"""从景美闭源 HAL (libdrm_jmgpu.so) 提取「命令寄存器号 -> 厂商寄存器名」映射。

两条提取路径：
  A) 立即数路径：函数内 `mov/movk wN,#imm` 后 30 条内调用 LoadState* 写入口
  B) 字面量池路径：只读节里每个 u32 v，若 v*4 命中厂商库地址则采纳
     （闭源栈与开源栈同样以 addr>>2 作为命令字寄存器号，
       证据：jmo_HARDWARE_LoadState32 入口 `lsr w1, w1, #2`）

再与开源驱动里「未映射」的寄存器号做交叉，输出可回填的语义。
"""
import re, subprocess, struct, os, collections, json, sys

SO = os.environ.get('JM_SO', '/tmp/v180/usr/lib/aarch64-linux-gnu/libdrm_jmgpu.so.1.0.0')
VENDOR = os.environ.get('JM_VENDOR', '/tmp/dpkg/gg/usr/src/mwv207-1.6.1-v25.u1')
OPEN_JSON = os.environ.get('JM_OPEN_JSON', '/home/admin/jm9100-oss/docs/registers/command-registers.json')

# ---------- 1. 厂商寄存器库 ----------
vendor = {}          # addr -> set(name)
fields = collections.defaultdict(dict)   # addr -> {field: (msb,lsb,type)}
for fn in os.listdir(VENDOR):
    if not fn.endswith('.h'): continue
    txt = open(os.path.join(VENDOR, fn), encoding='utf-8', errors='replace').read()
    for m in re.finditer(r'#define\s+MWV207REG_(\w+?)_Address\s+0x([0-9A-Fa-f]+)', txt):
        vendor.setdefault(int(m.group(2), 16), set()).add(m.group(1))
    for m in re.finditer(r'#define\s+MWV207REG_(\w+)_(\w+)\s+(\d+):(\d+)', txt):
        reg, fld, msb, lsb = m.group(1), m.group(2), int(m.group(3)), int(m.group(4))
        if reg in vendor:
            for a in [k for k, v in vendor.items() if reg in v]:
                fields[a][fld] = (msb, lsb)

def names_at(addr):
    return '/'.join(sorted(vendor.get(addr, ()))) or None

# ---------- 2. 反汇编 ----------
out = subprocess.run(['objdump','-d',SO], capture_output=True, text=True).stdout
funcs = {}; cur = None
INS = re.compile(r'\s+([0-9a-f]+):\s+([0-9a-f]{8})\s+(.*)$')
for line in out.split('\n'):
    m = re.match(r'^[0-9a-f]+ <(.+)>:$', line.strip())
    if m: cur = m.group(1); funcs[cur] = []
    elif cur:
        m2 = INS.match(line)
        if m2: funcs[cur].append((int(m2.group(1),16), m2.group(3).split('  ')[0].strip()))

WRITERS = ('HARDWARE_LoadState','HARDWARE_LoadCtrlState','HARDWARE_WriteBuffer',
           'HARDWARE_LoadProgram','HARDWARE_Load2DState')
res = collections.defaultdict(set)      # cmd_id -> {func}
for name, body in funcs.items():
    if name.endswith('@plt'): continue
    consts = collections.defaultdict(list)
    for i,(a,t) in enumerate(body):
        m = re.match(r'mov\s+(w\d+), #0x([0-9a-f]+)', t)
        if m: consts[m.group(1)].append((int(m.group(2),16), i))
        m = re.match(r'movk\s+(w\d+), #0x([0-9a-f]+), lsl #(\d+)', t)
        if m and consts[m.group(1)]:
            pv, pi = consts[m.group(1)][-1]
            consts[m.group(1)].append((pv | (int(m.group(2),16) << int(m.group(3))), i))
    for i,(a,t) in enumerate(body):
        m = re.search(r'bl\s+[0-9a-f]+ <(\S+)>', t)
        if m and any(w in m.group(1) for w in WRITERS):
            for reg in ('w1','w2','w3','w4'):
                for v, ci in consts.get(reg, []):
                    if 0 <= i - ci <= 30:
                        res[v].add(name)

# ---------- 3. 字面量池 ----------
elf = open(SO,'rb').read()
rout = subprocess.run(['readelf','-S','-W',SO], capture_output=True, text=True).stdout
pool = {}
for m in re.finditer(r'\[\s*\d+\]\s+(\.\S+)\s+PROGBITS\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)', rout):
    n, va, off, sz = m.group(1), int(m.group(2),16), int(m.group(3),16), int(m.group(4),16)
    if n in ('.rodata','.rodata1','.data.rel.ro'):
        d = elf[off:off+sz]
        for i in range(0, len(d)-4, 4):
            pool.setdefault(va+i, struct.unpack_from('<I', d, i)[0])

# ---------- 4. 合并 ----------
mapping = {}   # cmd -> {addr, name, src, funcs}
for v, fs in res.items():
    a = v*4
    if a in vendor:
        mapping[v] = dict(addr=a, name=names_at(a), src='immediate', funcs=sorted(fs))
for va, v in pool.items():
    a = v*4
    if a in vendor:
        e = mapping.setdefault(v, dict(addr=a, name=names_at(a), src='literal-pool', funcs=[]))
        if e['src'] == 'literal-pool': pass
        elif 'immediate' not in e['src']: e['src'] += '+literal-pool'

print(f"厂商寄存器地址总数: {len(vendor)}")
print(f"  立即数命中: {sum(1 for e in mapping.values() if 'immediate' in e['src'])}")
print(f"  字面量池命中: {sum(1 for e in mapping.values() if 'literal-pool' in e['src'])}")
print(f"  合并去重后: {len(mapping)}")

# ---------- 5. 与开源未映射寄存器交叉 ----------
try:
    openreg = json.load(open(OPEN_JSON))
except Exception as ex:
    openreg = None; print(f"(无法读取开源寄存器表: {ex})")

if openreg:
    # 提取开源里所有命令寄存器号
    ids = set()
    if isinstance(openreg, dict):
        for k in openreg:
            m = re.match(r'^0x([0-9a-fA-F]+)$', k)
            if m: ids.add(int(m.group(1),16))
        items = openreg
    else:
        items = openreg
        for it in openreg:
            if isinstance(it, dict) and 'reg' in it: ids.add(int(str(it['reg']),16))
    print(f"\n开源驱动命令寄存器号: {len(ids)} 个")
    unmapped = sorted(i for i in ids if i*4 not in vendor)
    print(f"开源里厂商库查不到的: {len(unmapped)}")
    now = [i for i in unmapped if i in mapping]
    print(f"  >>> 其中被闭源反编译解出的: {len(now)}")
    for i in now:
        e = mapping[i]
        print(f"    0x{i:X} -> addr 0x{e['addr']:X}  {e['name']}   [{e['src']}]")
        if e['funcs']:
            print(f"        调用者: {', '.join(e['funcs'])[:100]}")
    json.dump({f"0x{k:X}": v for k, v in sorted(mapping.items())},
              open('/home/admin/jm9100-oss/reverse/jm-reg-map.json','w'), indent=1)
    print("\n已写出 /home/admin/jm9100-oss/reverse/jm-reg-map.json")
