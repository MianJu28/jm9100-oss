#!/usr/bin/env python3
"""文档覆盖率自检：驱动源文件是否在 docs/ 的某篇文档中被引用（按文件名提及）。

用法: python3 docs/tools/check_coverage.py
输出: 未覆盖率与未提及文件清单（按目录分组）。
"""
import os, collections, sys

# 2026-11-02 起 UMD 适配代码已迁出 oh-mesa，独立存放在 src/mwv207-umd/
# （上游树里的同名目录只是指向它的符号链接，os.walk 默认不跟符号链接，所以不能再走 oh-mesa）。
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..'))          # docs/
REPO = os.path.normpath(os.path.join(ROOT, '..'))          # 仓库根
SRC = os.path.join(REPO, 'src', 'mwv207-umd')
DOCS = ROOT
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

if not files:
    print(f'[FAIL] 在 {SRC}/{{drivers,winsys}}/mwv207 下没找到驱动源文件 —— 路径失效')
    sys.exit(1)

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
