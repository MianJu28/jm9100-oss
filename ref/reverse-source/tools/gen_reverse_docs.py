#!/usr/bin/env python3
"""生成《闭源驱动反编译》文档集：分层架构 + HAL API + 寄存器映射。

数据来源全部是本机可复现的 objdump/nm/readelf 输出，每条结论带 file:line 或
符号名，不含厂商头文件原文（只引用"地址/字段名"这类事实，不复制受许可约束的实现）。
"""
import re, subprocess, struct, os, collections, json

SO = '/tmp/v180/usr/lib/aarch64-linux-gnu/libdrm_jmgpu.so.1.0.0'
DRI = '/tmp/v180/usr/lib/aarch64-linux-gnu/dri/jmgpu_dri.so'
VENDOR = '/tmp/dpkg/gg/usr/src/mwv207-1.6.1-v25.u1'
OUT = '/home/admin/jm9100-oss/docs/reverse'
os.makedirs(OUT, exist_ok=True)

# ---------- 厂商库 ----------
vendor = {}; vfields = collections.defaultdict(dict); vvals = collections.defaultdict(dict)
for fn in sorted(os.listdir(VENDOR)):
    if not fn.endswith('.h'): continue
    txt = open(os.path.join(VENDOR, fn), encoding='utf-8', errors='replace').read()
    for m in re.finditer(r'#define\s+MWV207REG_(\w+?)_Address\s+0x([0-9A-Fa-f]+)', txt):
        vendor.setdefault(int(m.group(2),16), set()).add(m.group(1))
for fn in sorted(os.listdir(VENDOR)):
    if not fn.endswith('.h'): continue
    txt = open(os.path.join(VENDOR, fn), encoding='utf-8', errors='replace').read()
    for m in re.finditer(r'#define\s+MWV207REG_(\w+?)_Address\s+0x([0-9A-Fa-f]+)', txt):
        pass
    # 字段位域
    for m in re.finditer(r'#define\s+MWV207REG_(\w+)_(\w+)\s+(\d+):(\d+)', txt):
        reg, fld, msb, lsb = m.group(1), m.group(2), int(m.group(3)), int(m.group(4))
        for a, ns in vendor.items():
            if reg in ns: vfields[a][fld] = (msb, lsb)
    # 命名值
    for m in re.finditer(r'#define\s+MWV207REG_(\w+)_(\w+)_(\w+)\s+0x([0-9A-Fa-f]+)\s*$', txt):
        reg, fld, val, v = m.group(1), m.group(2), m.group(3), m.group(4)
        for a, ns in vendor.items():
            if reg in ns: vvals[a].setdefault(fld, {})[val] = v

def nm_of(so):
    r = subprocess.run(['nm', so], capture_output=True, text=True).stdout
    d = {}
    for l in r.split('\n'):
        p = l.split()
        if len(p) >= 3: d.setdefault(p[2], []).append((p[0], p[1]))
    return d

def disasm_funcs(so):
    out = subprocess.run(['objdump','-d',so], capture_output=True, text=True).stdout
    funcs = {}; cur = None
    INS = re.compile(r'\s+([0-9a-f]+):\s+([0-9a-f]{8})\s+(.*)$')
    for line in out.split('\n'):
        m = re.match(r'^[0-9a-f]+ <(.+)>:$', line.strip())
        if m: cur = m.group(1); funcs[cur] = []
        elif cur:
            m2 = INS.match(line)
            if m2: funcs[cur].append((int(m2.group(1),16), m2.group(3).split('  ')[0].strip()))
    return funcs

dri_syms = nm_of(DRI); hal_syms = nm_of(SO)
hal_f = disasm_funcs(SO)

# ---------- A. 分层结构 ----------
BINS = [
 ('jmgpu_dri.so',      '/tmp/v180/usr/lib/aarch64-linux-gnu/dri/jmgpu_dri.so',            'GL/Dri 入口层'),
 ('libGLX_mwv207.so.0','/tmp/v180/usr/lib/aarch64-linux-gnu/libGLX_mwv207.so.0',          'GLX（X11 绑定）'),
 ('libEGL_mwv207.so.0','/tmp/v180/usr/lib/aarch64-linux-gnu/libEGL_mwv207.so.0',          'EGL'),
 ('libGLESv2_mwv207.so','/tmp/v180/usr/lib/aarch64-linux-gnu/libGLESv2_mwv207.so',        'GLES 2.0/3.x'),
 ('libGLESv1_CM_mwv207.so','/tmp/v180/usr/lib/aarch64-linux-gnu/libGLESv1_CM_mwv207.so', 'GLES 1.1'),
 ('libJMC.so',         '/tmp/v180/usr/lib/aarch64-linux-gnu/libJMC.so',                   '中间层 (JMC)'),
 ('libGLSLC.so',       '/tmp/v180/usr/lib/aarch64-linux-gnu/libGLSLC.so',                 'GLSL 编译器前端'),
 ('libCLC.so',         '/tmp/v180/usr/lib/aarch64-linux-gnu/libCLC.so',                   'OpenCL 编译器'),
 ('libSPIRV_jm.so',    '/tmp/v180/usr/lib/aarch64-linux-gnu/libSPIRV_jm.so',              'SPIR-V→JMIR'),
 ('libdrm_jmgpu.so',   SO,                                                                 'HAL / 寄存器写入层'),
 ('mwv207_drv.so',     '/tmp/v180/usr/lib/xorg/modules/drivers/mwv207_drv.so',            'Xorg DDX'),
 ('libOpenCL.so.1',    '/tmp/v180/usr/lib/aarch64-linux-gnu/mwv207/libOpenCL.so.1',       'OpenCL ICD 装载器'),
 ('libgbm_jm.so',      '/tmp/v180/usr/lib/aarch64-linux-gnu/libgbm_jm.so',                'GBM'),
]
rows = []
for name, path, role in BINS:
    if not os.path.exists(path): continue
    try:
        dyn = len(subprocess.run(['nm','-D',path],capture_output=True,text=True).stdout.strip().split('\n'))
        sta = len(subprocess.run(['nm',path],capture_output=True,text=True).stdout.strip().split('\n'))
        sz  = os.path.getsize(path)
    except Exception: continue
    rows.append((name, role, sz, dyn, sta))
json.dump({'bins':rows}, open('/tmp/_bins.json','w'))
print("A 分层:", len(rows))
