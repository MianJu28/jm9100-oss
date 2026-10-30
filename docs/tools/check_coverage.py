#!/usr/bin/env python3
"""文档覆盖率自检：驱动源文件是否在 docs/ 的某篇文档中被引用（按文件名提及）。

用法: python3 docs/tools/check_coverage.py
输出: 未覆盖率与未提及文件清单（按目录分组）。
"""
import os, re, collections

SRC = '/home/admin/jm9100-oss/src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium'
DOCS = '/home/admin/jm9100-oss/docs'
SUBS = ('drivers/mwv207', 'winsys/mwv207')

# 汇总所有文档正文
blob = []
for dp, dn, fn in os.walk(DOCS):
    if os.path.basename(dp) == 'tools':
        continue
    for f in fn:
        if f.endswith('.md'):
            try:
                blob.append(open(os.path.join(dp, f), encoding='utf-8').read())
            except Exception:
                pass
blob = '\n'.join(blob)

files = []
for sub in SUBS:
    base = os.path.join(SRC, sub)
    for dp, dn, fn in os.walk(base):
        for f in fn:
            p = os.path.join(dp, f)
            rel = os.path.relpath(p, SRC)
            n = sum(1 for _ in open(p, encoding='utf-8', errors='replace'))
            files.append((rel, n))

miss = [(rel, n) for rel, n in files if os.path.basename(rel) not in blob]
ok = len(files) - len(miss)
print(f'驱动源文件: {len(files)} 个（drivers/mwv207 + winsys/mwv207）')
print(f'已被文档提及: {ok} 个  ({100.0*ok/len(files):.1f}%)')
print(f'未被提及:     {len(miss)} 个')
print()
if miss:
    g = collections.defaultdict(list)
    for rel, n in miss:
        g[os.path.dirname(rel)].append((os.path.basename(rel), n))
    print('未提及文件（按目录）:')
    for d in sorted(g):
        tot = sum(n for _, n in g[d])
        print(f'  {d}/  ({len(g[d])} 个, {tot} 行)')
        for b, n in sorted(g[d]):
            print(f'     {b}  ({n} 行)')
