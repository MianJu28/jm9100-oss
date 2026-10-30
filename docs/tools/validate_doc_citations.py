#!/usr/bin/env python3
"""校验文档中的 `文件:行号` 引用是否可解析。

扫描 docs/ 下所有 .md，提取形如
    mwv207_draw.c:123        mwv207_draw.c:123-145
    include/mwv207_cmd.h:88  ../src/kernel/.../mwv207_bo.c:172
的引用，检查：
  1. 该文件能否在三棵源码树中找到（UMD、winsys、KMD）；
  2. 行号是否落在文件行数内。

只读。退出码 0 = 无硬错误（无法解析的引用按文件粒度汇总为"提示"，
因为文档允许引用本仓库其他文档/补丁中的文件名）。
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..'))
REPO = os.path.normpath(os.path.join(ROOT, '..'))

SRC_DIRS = [
    os.path.join(REPO, 'src', 'oh-mesa', 'User_source_code', 'mesa-24.1.2', 'src', 'gallium'),
    os.path.join(REPO, 'src', 'kernel'),
    os.path.join(REPO, 'src', 'mesa'),
]
# 厂商发布包内的 KMD 寄存器头（只读、外部数据，可能不存在）：存在时纳入校验，
# 用于核对 REGISTERS-FIELDS.md / REGISTER-MAP.md 里的 `jmgpu_*.h:行号` 引用。
VENDOR_DIRS = [
    '/tmp/dpkg/gg/usr/src/mwv207-1.6.1-v25.u1',
    '/tmp/v180/usr/src/mwv207-1.8.0.uos',
]
for d in VENDOR_DIRS:
    if os.path.isdir(d):
        SRC_DIRS.append(d)
INDEX = {}
for d in SRC_DIRS:
    if not os.path.isdir(d):
        continue
    for dirpath, _dirs, files in os.walk(d):
        if '/.git' in dirpath:
            continue
        for f in files:
            if f.endswith(('.c', '.h')):
                INDEX.setdefault(f, []).append(os.path.join(dirpath, f))

CITE = re.compile(r'([A-Za-z0-9_][A-Za-z0-9_./-]*\.(?:c|h))[:：](\d+)(?:\s*[-–]\s*(\d+))?')
_linecount = {}


def nlines(path):
    if path not in _linecount:
        with open(path, 'rb') as f:
            _linecount[path] = f.read().count(b'\n') + 1
    return _linecount[path]


docs = []
for dirpath, _dirs, files in os.walk(ROOT):
    if any(x in dirpath for x in ('tools', '.git')):
        continue
    for f in files:
        if f.endswith('.md'):
            docs.append(os.path.join(dirpath, f))
docs.sort()

tot = resolved = unresolved = oob = 0
unresolved_names = {}
oob_list = []
per_doc = {}

for doc in docs:
    txt = open(doc, encoding='utf-8', errors='replace').read()
    rel = os.path.relpath(doc, ROOT)
    d_tot = d_ok = d_bad = 0
    for m in CITE.finditer(txt):
        name, l1, l2 = m.group(1), int(m.group(2)), m.group(3)
        base = os.path.basename(name)
        tot += 1
        d_tot += 1
        cands = list(INDEX.get(base) or [])
        # 系统头文件（文档常写 usr/include/drm/drm.h 指 /usr/include/drm/drm.h）：按真实路径解析。
        # 命中后必须跳过下面的 mwv207 缩写优先，否则会被 mwv207_drm.h 这类同后缀文件覆盖。
        sys_hit = False
        for cand in (os.path.join('/', name.lstrip('/')), os.path.join('/', name)):
            if os.path.isfile(cand):
                cands = [cand]
                sys_hit = True
                break
        # 文档常用缩写名（blit.c 代指 mwv207_blit.c）：并上 mwv207 两棵树的后缀匹配，
        # 且当存在 mwv207 候选时优先采用它们（否则像 src/mesa/main/blit.c 这样的
        # 同名无关文件会把范围误判为越界）。
        if not sys_hit:
            for f, paths in INDEX.items():
                if f.endswith('_' + base):
                    cands += [q for q in paths if 'mwv207' in q]
            mwv = [c for c in cands if 'mwv207' in c]
            if mwv:
                cands = mwv
        if not cands:
            unresolved += 1
            d_bad += 1
            unresolved_names[base] = unresolved_names.get(base, 0) + 1
            continue
        top = max(l1, int(l2) if l2 else 0)
        if any(top <= nlines(c) for c in cands):
            resolved += 1
            d_ok += 1
        else:
            best = max(nlines(c) for c in cands)
            oob += 1
            d_bad += 1
            oob_list.append(f'{rel}: {name}:{l1}{"-" + l2 if l2 else ""} 超出（最长 {best} 行）')
    per_doc[rel] = (d_ok, d_bad)

print('== 文档引用校验 ==\n')
print(f'{"文档":52s} {"可解析":>7s} {"异常":>6s}')
for rel, (ok, bad) in sorted(per_doc.items()):
    if ok or bad:
        print(f'{rel:52s} {ok:7d} {bad:6d}')
print()
print(f'引用总数：{tot}   可解析：{resolved}   越界：{oob}   找不到文件：{unresolved}')

if oob_list:
    print('\n== 行号越界 ==')
    for x in oob_list[:40]:
        print(' !', x)
    if len(oob_list) > 40:
        print(f' ...（共 {len(oob_list)} 条）')

if unresolved_names:
    print('\n== 找不到同名源文件（多为只写基名或引用非源码文件，属提示）==')
    for n, c in sorted(unresolved_names.items(), key=lambda kv: -kv[1])[:30]:
        print(f' ? {n} × {c}')

print('\n结论：', '无行号越界' if not oob else f'{oob} 处行号越界需修正')
sys.exit(1 if oob else 0)