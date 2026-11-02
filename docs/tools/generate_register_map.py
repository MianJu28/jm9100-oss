#!/usr/bin/env python3
"""从 register-db.json 生成厂商视角的完整寄存器地址表 docs/registers/REGISTER-MAP.md。

输出: 全部 1392 个寄存器的 address / reg_id(addr>>2) / 块 / 字段数 / 驱动是否使用，
按地址排序，并按块前缀分组给出地址区间。
"""
import json, os, re, collections

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', 'registers'))
db = json.load(open(os.path.join(ROOT, 'register-db.json'), encoding='utf-8'))
regs = db['registers']
join = db['driver_join']

# 驱动使用过的寄存器名（driver_join: vendor_name -> {driver_id, uses, sites...}）
used_names = set()
used_by_name = {}
for vname, info in join.items():
    used_names.add(vname)
    used_by_name[vname] = info

BLOCK_RE = re.compile(r'^MWV207REG_([A-Z0-9]+?)_')

def block_of(name):
    m = BLOCK_RE.match(name)
    return m.group(1) if m else '?'

items = sorted(regs.values(), key=lambda r: r['address'])

# 统计值解析质量
bad_values = 0
total_values = 0
for r in items:
    for f in r.get('fields', {}).values():
        for k, v in (f.get('values') or {}).items():
            total_values += 1
            if isinstance(v, str) and re.match(r'^\d+:\d+$', v):
                bad_values += 1

blocks = collections.defaultdict(list)
for r in items:
    blocks[block_of(r['name'])].append(r)

L = []
L.append('# 寄存器地址总表（厂商视角）')
L.append('')
L.append('本表由 `docs/tools/generate_register_map.py` 从 `register-db.json` 生成，')
L.append('数据源为厂商 KMD 头文件中的 `MWV207REG_*` 定义（1392 个寄存器）。')
L.append('')
L.append('**驱动寄存器号** = 厂商寄存器地址 `>> 2`（即下表的 `reg_id` 列）。')
L.append('凡是 `reg_id` 出现在驱动命令流中的寄存器，`驱动` 列标为 ✅，并可交叉查阅')
L.append('[REGISTERS-FIELDS.md](REGISTERS-FIELDS.md) 中的字段位域与驱动写入点。')
L.append('')
L.append(f'- 寄存器总数：**{len(items)}**')
L.append(f'- 驱动实际使用：**{len(join)}**（{100.0*len(join)/len(items):.1f}%）')
L.append(f'- 字段总数：**{sum(len(r.get("fields",{})) for r in items)}**')
L.append(f'- 命名取值总数：{total_values}（其中 {bad_values} 条为解析噪声，值为 `N:N` 形式，已在字段表中忽略）')
L.append('')
L.append('## 块分布')
L.append('')
L.append('| 块 | 寄存器数 | 地址区间 (min–max) | 驱动使用 |')
L.append('|---|---:|---|---:|')
for b in sorted(blocks, key=lambda b: blocks[b][0]['address']):
    rs = blocks[b]
    lo = min(r['address'] for r in rs)
    hi = max(r['address'] for r in rs)
    u = sum(1 for r in rs if r['name'] in used_names)
    L.append(f'| `{b}` | {len(rs)} | `0x{lo:05X}`–`0x{hi:05X}` | {u} |')
L.append('')
L.append('## 全部寄存器（按地址升序）')
L.append('')
L.append('| 地址 | reg_id | 寄存器名 | 块 | 字段数 | 驱动 | 定义位置 |')
L.append('|---|---:|---|---|---:|---|---|')
for r in items:
    b = block_of(r['name'])
    nf = len(r.get('fields', {}))
    u = '✅' if r['name'] in used_names else ''
    L.append(f"| `0x{r['address']:05X}` | `0x{r['address']>>2:04X}` | {r['name']} | {b} | {nf} | {u} | {r.get('src','')} |")


# ---- 按块索引（便于按功能块查找）----
L.append('')
L.append('## 按块索引')
L.append('')
L.append('每个块的寄存器按地址升序。`reg_id` 即驱动侧使用的寄存器号。')
L.append('')
for b in sorted(blocks, key=lambda b: blocks[b][0]['address']):
    rs = blocks[b]
    u = sum(1 for r in rs if r['name'] in used_names)
    L.append(f'### `{b}` — {len(rs)} 个（驱动使用 {u} 个）')
    L.append('')
    L.append('| reg_id | 地址 | 寄存器 | 字段 | 驱动 |')
    L.append('|---|---:|---|---:|---|')
    for r in rs:
        uu = '✅' if r['name'] in used_names else ''
        L.append(f"| `0x{r['address']>>2:04X}` | `0x{r['address']:05X}` | {r['name']} | {len(r.get('fields',{}))} | {uu} |")
    L.append('')

out = os.path.join(ROOT, 'REGISTER-MAP.md')
open(out, 'w', encoding='utf-8').write('\n'.join(L) + '\n')
print('wrote', out, len(L), 'lines')
print('registers', len(items), 'used', len(join), 'bad_values', bad_values, '/', total_values)
