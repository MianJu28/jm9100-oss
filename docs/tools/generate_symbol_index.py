#!/usr/bin/env python3
"""生成 docs/api/09-SYMBOL-INDEX.md —— 驱动符号索引。

从 drivers/mwv207 + winsys/mwv207 的 .c/.h 中提取:
  - 函数定义（Mesa 风格：返回类型单独一行，函数名顶格，参数列表以 ")" 结束）
  - 结构体/联合体/枚举定义
  - 函数宏定义
输出按文件分组的索引表，便于按符号名检索。
"""
import os, re, collections

# 2026-11-02 起 UMD 适配代码独立存放在 src/mwv207-umd/；路径按脚本位置推导。
# 输出的相对路径仍是 drivers/mwv207/... 与 winsys/mwv207/...（以新 SRC 为基准）。
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..'))            # docs/
REPO = os.path.normpath(os.path.join(ROOT, '..'))            # 仓库根
SRC = os.path.join(REPO, 'src', 'mwv207-umd')
SUBS = ('drivers/mwv207', 'winsys/mwv207')
OUT = os.path.join(ROOT, 'api', '09-SYMBOL-INDEX.md')

TYPE_RE = re.compile(r'^(?:static\s+|inline\s+|extern\s+|const\s+)*'
                     r'(?:struct\s+\w+|union\s+\w+|enum\s+\w+|unsigned|signed|void|char|short|int|long|'
                     r'float|double|bool|uint\w*_t|int\w*_t|size_t|uint|enum|struct)\b')
NAME_RE = re.compile(r'^([A-Za-z_][A-Za-z0-9_]*)\s*\(')

funcs, structs, macros = [], [], []
for sub in SUBS:
    base = os.path.join(SRC, sub)
    for dp, dn, fn in os.walk(base):
        for f in sorted(fn):
            if not f.endswith(('.c', '.h')):
                continue
            p = os.path.join(dp, f)
            rel = os.path.relpath(p, SRC)
            lines = open(p, encoding='utf-8', errors='replace').read().split('\n')
            for i, ln in enumerate(lines):
                # 结构体/联合体/枚举
                m = re.match(r'^(?:static\s+)?(struct|union|enum)\s+([A-Za-z_]\w*)\s*\{', ln)
                if m:
                    # 排除 "typedef struct {" 匿名
                    structs.append((rel, i + 1, f'{m.group(1)} {m.group(2)}', ''))
                # 函数宏
                m = re.match(r'^#define\s+([A-Za-z_]\w*)\(', ln)
                if m:
                    macros.append((rel, i + 1, m.group(1), ln.strip()[:110]))
                # 函数定义
                m = NAME_RE.match(ln)
                if not m:
                    continue
                name = m.group(1)
                if name in ('if', 'for', 'while', 'switch', 'return', 'sizeof', 'ELSE', 'else'):
                    continue
                # 参数列表可能跨行：向下找 ")" 结尾
                j = i
                depth = ln.count('(') - ln.count(')')
                while depth > 0 and j + 1 < len(lines) and j - i < 25:
                    j += 1
                    depth += lines[j].count('(') - lines[j].count(')')
                sig_end = lines[j].rstrip()
                if not sig_end.endswith(')') and not sig_end.endswith(');'):
                    continue
                # 必须紧跟 "{"
                k = j + 1
                while k < len(lines) and lines[k].strip() == '':
                    k += 1
                if k >= len(lines) or not lines[k].lstrip().startswith('{'):
                    continue
                # 返回类型：向上取最近的非空行，且必须像类型
                t = i - 1
                rtype = ''
                while t >= 0 and lines[t].strip() == '':
                    t -= 1
                if t >= 0:
                    cand = lines[t].strip()
                    if TYPE_RE.match(cand):
                        rtype = cand
                sig = ' '.join(x.strip() for x in lines[i:j + 1])
                funcs.append((rel, i + 1, name, rtype, sig))

lines = []
lines.append('# 09 · 驱动符号索引')
lines.append('')
lines.append('本文件由 `docs/tools/generate_symbol_index.py` 自动生成，便于按符号名检索。')
lines.append('')
lines.append(f'- 函数定义：**{len(funcs)}** 个')
lines.append(f'- 结构体/联合体/枚举定义：**{len(structs)}** 个')
lines.append(f'- 函数宏：**{len(macros)}** 个')
lines.append('')
lines.append('各符号的语义请查对应模块文档（见 [../README.md](../README.md) 索引）；')
lines.append('本索引只提供"符号在哪"与签名。')
lines.append('')

lines.append('## 函数定义（按文件）')
lines.append('')
byfile = collections.defaultdict(list)
for rel, ln, name, rtype, sig in funcs:
    byfile[rel].append((ln, name, rtype, sig))
for rel in sorted(byfile):
    lines.append(f'### `{rel}`  ({len(byfile[rel])} 个)')
    lines.append('')
    lines.append('| 行 | 返回类型 | 函数 | 签名 |')
    lines.append('|---:|---|---|---|')
    for ln, name, rtype, sig in sorted(byfile[rel]):
        s = sig.replace('|', '\\|')
        lines.append(f'| {ln} | `{rtype}` | **{name}** | `{s}` |')
    lines.append('')

lines.append('## 结构体 / 联合体 / 枚举定义')
lines.append('')
lines.append('| 文件 | 行 | 定义 |')
lines.append('|---|---:|---|')
for rel, ln, kind, _ in sorted(structs):
    lines.append(f'| `{rel}` | {ln} | `{kind}` |')
lines.append('')

lines.append('## 函数宏')
lines.append('')
lines.append('| 文件 | 行 | 宏 | 定义 |')
lines.append('|---|---:|---|---|')
for rel, ln, name, txt in sorted(macros):
    lines.append(f'| `{rel}` | {ln} | `{name}` | `{txt.replace("|", "\\|")}` |')
lines.append('')

open(OUT, 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
print('wrote', OUT, len(lines), 'lines')
print('funcs', len(funcs), 'structs', len(structs), 'macros', len(macros))
