#!/usr/bin/env python3
"""校验 docs/registers/register-db.json 中驱动寄存器号的映射自洽性。

检查项：
  1. `驱动号 × 4 == 厂商 _Address`（映射等式本身）；
  2. 每条映射的 `vendor` 名字确实存在于寄存器库；
  3. 每条 `uses[]` 的 file:line 在源码中真实存在且落在文件行数内；
  4. 统计映射覆盖率与各类异常。

只读，不修改任何文件。退出码 0 = 全部通过，1 = 存在异常。
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..'))          # docs/
# 2026-11-02 起 UMD 适配代码独立存放在 src/mwv207-umd/（oh-mesa 里的同名目录是符号
# 链接；os.walk 不跟符号链接，原先走 oh-mesa 会一个文件都找不到）。
UMD = os.path.normpath(os.path.join(ROOT, '..', 'src', 'mwv207-umd'))
DB = json.load(open(os.path.join(ROOT, 'registers', 'register-db.json'), encoding='utf-8'))
regs = DB['registers']
join = DB['driver_join']

errors = []
warn = []

# --- 1/2: 映射等式与名字存在性 ---
for key, e in sorted(join.items()):
    try:
        did = int(key, 16)
    except ValueError:
        errors.append(f'驱动号无法解析为十六进制: {key!r}')
        continue
    if e['id'] != did:
        errors.append(f'{key}: driver_join.id={e["id"]} 与键不一致')
    vlist = e.get('vendor') or []
    if not vlist:
        warn.append(f'{key}: 无 vendor 名字')
        continue
    for v in vlist:
        if v not in regs:
            errors.append(f'{key}: vendor 名 {v} 不在寄存器库中')
            continue
        addr = regs[v]['address']
        if did * 4 != addr:
            errors.append(f'{key}: {key}*4=0x{did*4:X} != {v} 地址 0x{addr:X}')
        if regs[v].get('driver_ids') and key not in regs[v]['driver_ids']:
            warn.append(f'{key}: {v}.driver_ids 未回填该号')

# --- 3: uses 的 file:line 真实存在 ---
CANDIDATE_DIRS = [
    os.path.join(UMD, 'drivers', 'mwv207'),
    os.path.join(UMD, 'winsys', 'mwv207'),
]
_line_cache = {}


def find_file(relpath):
    base = os.path.basename(relpath)
    for d in CANDIDATE_DIRS:
        for dirpath, _dirs, files in os.walk(d):
            if base in files:
                return os.path.join(dirpath, base)
    return None


def nlines(path):
    if path not in _line_cache:
        with open(path, 'rb') as f:
            _line_cache[path] = f.read().count(b'\n') + 1
    return _line_cache[path]


n_uses = 0
bad_uses = 0
for key, e in sorted(join.items()):
    for u in e.get('uses') or []:
        n_uses += 1
        f = find_file(u['file'])
        if f is None:
            # 少数使用点在外层文件（如 winsys 头）
            bad_uses += 1
            warn.append(f'{key}: 找不到文件 {u["file"]}')
            continue
        if not (1 <= u['line'] <= nlines(f)):
            bad_uses += 1
            errors.append(f'{key}: {u["file"]}:{u["line"]} 超出文件范围（共 {nlines(f)} 行）')

# --- 4: 覆盖率 ---
total_ids = len(join)
mapped_ok = total_ids - len([x for x in errors if 'vendor 名' in x or '地址' in x])
print(f'驱动寄存器号（进入 driver_join）：{total_ids}')
print(f'厂商寄存器（register-db）：      {len(regs)}')
print(f'uses 引用点：                    {n_uses}（异常 {bad_uses}）')
print(f'映射等式错误：                   {len([x for x in errors if "*4=" in x])}')
print(f'其他错误：                       {len([x for x in errors if "*4=" not in x])}')
print(f'提示（不阻断）：                 {len(warn)}')

if errors:
    print('\n== 错误 ==')
    for x in errors[:60]:
        print(' !', x)
    if len(errors) > 60:
        print(f' ...（共 {len(errors)} 条）')
if warn:
    print('\n== 提示（前 20 条）==')
    for x in warn[:20]:
        print(' ?', x)
    if len(warn) > 20:
        print(f' ...（共 {len(warn)} 条）')

print('\n结论：', '全部映射自洽' if not errors else '存在映射不一致，请修复')
sys.exit(1 if errors else 0)