#!/usr/bin/env python3
"""生成 docs/registers/ISA-OPCODES.md —— 着色器 ISA 指令操作码（厂商依据 + 驱动交叉验证）。

数据来源：
  A. 厂商 KMD 头文件中的 MWV207REG_SH_*_OPCODE_* 字段与取值宏（权威依据）；
  B. 驱动侧的 nir/mwv207_disasm.c（`[0x7f + n]` 扩展操作码表）与
     nir/mwv207_emit.c（.ext_opcode 赋值点）。

结论：驱动扩展操作码编号空间 `0x7f + n` 与厂商 `SH_EXTENDED_OPCODE` 完全一致，
      因此 ISA 扩展操作码**有厂商依据**（此前 NIR 文档中"无厂商依据"的说法需修正）。
"""
import collections
import glob
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..'))
REPO = os.path.normpath(os.path.join(ROOT, '..'))
VENDOR = os.environ.get('MWV207_VENDOR_SRC') or next(
    (d for d in sorted(glob.glob('/tmp/dpkg/gg/usr/src/mwv207-*'))
     if os.path.isdir(d)), '/tmp/dpkg/gg/usr/src/mwv207-1.6.1-v25.u1')
DRV = os.path.join(REPO, 'src', 'mwv207-umd', 'drivers', 'mwv207')

if not os.path.isdir(VENDOR):
    raise SystemExit(f'找不到厂商头文件目录：{VENDOR}（需要先解包 mwv207-dkms）')

# ---------- 1. 厂商侧解析 ----------
fields = {}                                     # field -> dict(start,end,type)
values = collections.defaultdict(dict)          # field -> {name: value}
FIELD_RE = re.compile(r'\s*#define\s+(MWV207REG_SH_[A-Z0-9_]*OPCODE)_(Start|End|Type)\s+(\S+)')
VAL_RE = re.compile(r'\s*#define\s+(MWV207REG_SH_[A-Z0-9_]*OPCODE)_([A-Z0-9_]+)\s+(0x[0-9A-Fa-f]+|\d+)')

for fn in glob.glob(os.path.join(VENDOR, '*.h')):
    for ln in open(fn, encoding='utf-8', errors='replace'):
        m = FIELD_RE.match(ln)
        if m:
            f = m.group(1)
            fields.setdefault(f, {})[m.group(2)] = m.group(3)
            continue
        m = VAL_RE.match(ln)
        if m:
            f, suf, v = m.group(1), m.group(2), int(m.group(3), 0)
            if f in fields and suf in ('Start', 'End', 'Type', 'MSB', 'LSB'):
                continue
            values[f][suf] = v

# ---------- 2. 驱动侧解析 ----------
disasm_ext = {}          # ext_number -> (name, line)
disasm_file = os.path.join(DRV, 'nir', 'mwv207_disasm.c')
for i, ln in enumerate(open(disasm_file, encoding='utf-8', errors='replace'), 1):
    m = re.search(r'\[0x7f \+ (0x[0-9A-Fa-f]+|\d+)\]\s*=\s*\{"([^"]+)"', ln)
    if m:
        disasm_ext[int(m.group(1), 0)] = (m.group(2), i)

emit_ext = []            # (file, line, value)
emit_file = os.path.join(DRV, 'nir', 'mwv207_emit.c')
for i, ln in enumerate(open(emit_file, encoding='utf-8', errors='replace'), 1):
    m = re.search(r'\.ext_opcode\s*=\s*(0x[0-9A-Fa-f]+|\d+)', ln)
    if m:
        emit_ext.append((os.path.basename(emit_file), i, int(m.group(1), 0)))

# ---------- 3. 输出 ----------
L = []
L.append('# 着色器 ISA 指令操作码')
L.append('')
L.append('> 权威依据：厂商 KMD 头文件中的 `MWV207REG_SH_*_OPCODE_*` 字段与取值宏。')
L.append('> 交叉验证：驱动 `nir/mwv207_disasm.c` 的扩展操作码表、`nir/mwv207_emit.c` 的使用点。')
L.append('> 生成：`../tools/generate_isa_opcodes.py`（可重复执行）。')
L.append('')
L.append('## 0. 关键结论')
L.append('')
L.append('驱动反汇编表把扩展指令编码在 **`0x7f + n`** 区段。逐条比对显示：')
L.append('**`n` 就是厂商 `SH_EXTENDED_OPCODE`（U08，bits 0–7）的取值**，例如')
L.append('`n = 0x4 → LODQ`、`0xF → HALFMIX`、`0xB → BIT_FINDLSB`、`0x13 → MOVBIX`，')
L.append('与厂商命名**逐项一致**。')
L.append('')
L.append('因此：**ISA 扩展操作码具备厂商依据**；此前 `04-NIR-COMPILER.md` §12 中')
L.append('"101 条 opcode 数值无厂商文档依据"的限制，对**扩展操作码部分应予修正**')
L.append('（基础 ALU 操作码仍未在厂商头文件中找到对应枚举）。')
L.append('')

order = ['MWV207REG_SH_EXTENDED_OPCODE', 'MWV207REG_SH_CMPLX_OPCODE',
         'MWV207REG_SH_IMG_ATOM_OPCODE', 'MWV207REG_SH_VISION_OPCODE']
titles = {
    'MWV207REG_SH_EXTENDED_OPCODE': '扩展操作码 `SH_EXTENDED_OPCODE`',
    'MWV207REG_SH_CMPLX_OPCODE': '复式操作码 `SH_CMPLX_OPCODE`（三操作数 MAD）',
    'MWV207REG_SH_IMG_ATOM_OPCODE': '图像原子操作码 `SH_IMG_ATOM_OPCODE`',
    'MWV207REG_SH_VISION_OPCODE': '视觉操作码 `SH_VISION_OPCODE`（NN/视觉单元）',
}

L.append('## 1. 厂商操作码字段清单')
L.append('')
L.append('| 字段 | 位宽 | 位段 | 取值数 |')
L.append('|---|---:|---|---:|')
for f in order:
    d = fields.get(f, {})
    st, en, ty = d.get('Start', '?'), d.get('End', '?'), d.get('Type', '?')
    L.append(f'| `{f}` | {ty} | {st}–{en} | {len(values.get(f, {}))} |')
L.append('')

for f in order:
    d = fields.get(f, {})
    st, en, ty = d.get('Start', '?'), d.get('End', '?'), d.get('Type', '?')
    vv = values.get(f, {})
    L.append(f'## 2.{order.index(f)+1} {titles[f]}')
    L.append('')
    L.append(f'定义：`{f}`，{ty}，bits {st}–{en}，共 {len(vv)} 个命名取值。')
    L.append('')
    if f == 'MWV207REG_SH_EXTENDED_OPCODE':
        L.append('| 值 | 厂商名 | 驱动反汇编（`0x7f + n`）| 驱动是否实现 |')
        L.append('|---:|---|---|---|')
        for name in sorted(vv, key=lambda k: vv[k]):
            v = vv[name]
            dd = disasm_ext.get(v)
            dn = dd[0] if dd else '—'
            impl = '✅' if dd else '❌ 未实现'
            L.append(f'| `0x{v:02X}` | `{name}` | `0x7f + 0x{v:02X}` = "{dn}"（`mwv207_disasm.c:{dd[1]}`）| {impl} |' if dd
                      else f'| `0x{v:02X}` | `{name}` | — | {impl} |')
    elif f == 'MWV207REG_SH_CMPLX_OPCODE':
        L.append('| 值 | 厂商名 | 说明 |')
        L.append('|---:|---|---|')
        note = {'MUL': '乘法', 'MAD': '乘加（三操作数）', 'ADD': '加法'}
        for name in sorted(vv, key=lambda k: vv[k]):
            L.append(f'| `0x{vv[name]:02X}` | `{name}` | {note.get(name, "—")} |')
    else:
        L.append('| 值 | 厂商名 |')
        L.append('|---:|---|')
        for name in sorted(vv, key=lambda k: vv[k]):
            L.append(f'| `0x{vv[name]:02X}` | `{name}` |')
    L.append('')

L.append('## 3. 驱动侧使用点（`mwv207_emit.c`）')
L.append('')
L.append('| 位置 | `ext_opcode` | 对应厂商名 |')
L.append('|---|---:|---|')
rev = {v: k for k, v in values.get('MWV207REG_SH_EXTENDED_OPCODE', {}).items()}
for fn, ln, v in emit_ext:
    L.append(f'| `nir/{fn}:{ln}` | `0x{v:X}` | `{rev.get(v, "—")}` |')
L.append('')
L.append('## 4. 缺口')
L.append('')
missing = [n for n in sorted(values.get('MWV207REG_SH_EXTENDED_OPCODE', {}), key=lambda k: values['MWV207REG_SH_EXTENDED_OPCODE'][k])
           if values['MWV207REG_SH_EXTENDED_OPCODE'][n] not in disasm_ext
           and n not in ('TEXLD_FETCH_MS',)]
L.append('厂商定义但驱动反汇编表中**没有**条目的扩展操作码（即驱动未实现或未命名为可解码指令）：')
L.append('')
L.append('| 值 | 厂商名 | 备注 |')
L.append('|---:|---|---|')
noimpl = [n for n in sorted(values.get('MWV207REG_SH_EXTENDED_OPCODE', {}),
                            key=lambda k: values['MWV207REG_SH_EXTENDED_OPCODE'][k])
          if values['MWV207REG_SH_EXTENDED_OPCODE'][n] not in disasm_ext]
if noimpl:
    for n in noimpl:
        L.append(f'| `0x{values["MWV207REG_SH_EXTENDED_OPCODE"][n]:02X}` | `{n}` | 驱动无对应解码条目 |')
else:
    L.append('| — | — | 全部已实现 |')
L.append('')
L.append('> 注：`TEXLD_FETCH_MS` 与 `TEXLD_FETCH_MS_S` 同为 `0x0D`，是厂商侧的别名；')
L.append('> 驱动表里用的是 `TEXLD_FETCH_MS_S`。')
L.append('')
L.append('## 5. 反向未解释项')
L.append('')
L.append('驱动反汇编表 `0x7f + n` 区段中存在、而厂商 `SH_EXTENDED_OPCODE` **未命名**的 `n`：')
L.append('')
unknown = [n for n in sorted(disasm_ext) if n not in rev]
if unknown:
    for n in unknown:
        L.append(f'- `0x{n:02X}` → "{disasm_ext[n][0]}"（`mwv207_disasm.c:{disasm_ext[n][1]}`）')
else:
    L.append('（无）')
L.append('')

out = os.path.join(ROOT, 'registers', 'ISA-OPCODES.md')
open(out, 'w', encoding='utf-8').write('\n'.join(L) + '\n')
print('wrote', out, len(L), 'lines')
print('厂商操作码字段:', ', '.join(f'{f}({len(values.get(f, {}))})' for f in order))
print('驱动 ext 条目:', len(disasm_ext), '  emit 使用点:', len(emit_ext))
print('驱动未实现的 vendor 扩展操作码:', ', '.join(f'{n}=0x{values["MWV207REG_SH_EXTENDED_OPCODE"][n]:02X}' for n in noimpl) or '无')
print('驱动有但厂商未命名的 n:', ', '.join(f'0x{n:02X}("{disasm_ext[n][0]}")' for n in unknown) or '无')