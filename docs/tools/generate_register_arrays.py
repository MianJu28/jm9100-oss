#!/usr/bin/env python3
"""生成 docs/registers/REGISTER-ARRAYS.md

内容:
  1. 多实例（数组型）寄存器文件 —— 驱动用 "基址 + 索引" 写入的寄存器族，
     含 PE 渲染目标 8 槽寄存器文件、纹理单元寄存器文件、着色器 uniform 区。
  2. 未映射寄存器分析 —— 驱动写但厂商发布头文件中没有 _Address 定义的寄存器，
     按地址区域归类并给出归属推断（推断一律标注"未确认"）。

数据源: register-db.json (厂商寄存器定义) + command-registers.json (驱动写入点)
"""
import json, os, re, collections

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', 'registers'))
db = json.load(open(os.path.join(ROOT, 'register-db.json'), encoding='utf-8'))
regs, join = db['registers'], db['driver_join']
cmd = json.load(open(os.path.join(ROOT, 'command-registers.json'), encoding='utf-8'))
byaddr = {}
for n, r in regs.items():
    byaddr.setdefault(r['address'], []).append(n)

def look(rid):
    """驱动寄存器号 -> 厂商名（0 或多个）"""
    return byaddr.get(rid << 2, [])

def nm(rid):
    h = look(rid)
    return h[0] if h else '—'

L = []
L.append('# 多实例寄存器文件与未映射寄存器分析')
L.append('')
L.append('本表由 `docs/tools/generate_register_arrays.py` 生成。')
L.append('配合 [REGISTERS.md](REGISTERS.md)（驱动寄存器号映射总表）与')
L.append('[REGISTERS-FIELDS.md](REGISTERS-FIELDS.md)（字段位域）阅读。')
L.append('')
L.append('驱动写入寄存器的编号有两种来源：')
L.append('')
L.append('1. **字面量**（如 `0x530`）—— 可直接映射到厂商寄存器名，见 REGISTERS.md。')
L.append('2. **基址 + 索引**（如 `0x5240 + (render_num - 1)`）—— 属于"多实例寄存器文件"，')
L.append('   厂商头文件只定义了第 0 个实例的地址，其余实例靠地址递增访问。本节记录这类结构。')
L.append('')
L.append('驱动命令流中出现 12 个非字面量键（含 `<STALL>`、`<computed:reg>` 等占位符），')
L.append('分类如下：')
L.append('')
L.append('| 键 | 出现次数 | 位置 |')
L.append('|---|---:|---|')
for k, v in sorted(cmd.items()):
    if k.startswith('<'):
        files = sorted({s['file'] for s in v if isinstance(s, dict)})
        lines = [s['line'] for s in v if isinstance(s, dict)]
        L.append(f"| `{k}` | {len(v)} | {', '.join(files)}:{','.join(map(str,lines[:6]))} |")
L.append('')

# ---------------- 1. PE 渲染目标寄存器文件 ----------------
L.append('## 1. PE 渲染目标寄存器文件（8 个 RT 槽）')
L.append('')
L.append('`.640` 段：驱动按"基址 + RT 序号"写入。厂商头文件把第 0 个实例命名为')
L.append('`MWV207REG_PE_RENDER_TARGET_*`，第 N 个实例即 `基址 + N`。')
L.append('')
L.append('证据（`mwv207_draw.c`）：')
L.append('')
L.append('```c')
L.append('// mwv207_draw.c:959   render_num 从 1 开始')
L.append('MWV207_EMIT(cmd_buf, (0x5240 + (render_num - 1)),')
L.append('            surface->peInfo.pixel_config_ex | ctx->blend_state->blend_configex[i]);')
L.append('// mwv207_draw.c:652   RT0 单独处理，其余走基址+索引')
L.append('if (i == 0) MWV207_EMIT(cmd_buf, 0x508, ctx->zsa->alpha_config);')
L.append('else        MWV207_EMIT(cmd_buf, 0x5250 + (i - 1), ctx->zsa->alpha_config);')
L.append('```')
L.append('')
L.append('| 驱动基址 | 厂商寄存器（第 0 实例） | 地址 | 驱动用途 | 写入点 |')
L.append('|---|---|---|---|---|')
pe_rows = [
    (0x5200, '颜色缓冲起始地址'),
    (0x5240, 'pixel_config_ex（像素格式/α 等，与 blend_configex 或运算）'),
    (0x5248, 'colormask 与 dst_read 位域'),
    (0x5250, 'α 测试配置（zsa->alpha_config）'),
    (0x5258, 'blend_config（RT 级混合方程）'),
    (0x5260, '混合常量低 32 位'),
    (0x5268, '混合常量高 32 位'),
    (0x5270, '颜色缓冲结束地址'),
    (0x5278, '像素格式（_EX 变体）'),
]
for rid, use in pe_rows:
    L.append(f"| `0x{rid:04X}` | {nm(rid)} | `0x{(rid<<2):05X}` | {use} | mwv207_draw.c:959/963/715/717/652/776/777 |")
L.append('')
L.append('各数组的槽位数可从"与下一个具名寄存器的地址间隔"推断，但厂商头文件未给出明确')
L.append('数组长度，**槽位上限为未确认**；本硬件 `PIPE_MAX_COLOR_BUFS = 4`，驱动实际只用到')
L.append('槽 0–3。')
L.append('')
L.append('### 同一地址空间的另两个变体（驱动未使用）')
L.append('')
L.append('| 族 | 基址 | 第一个寄存器 |')
L.append('|---|---|---|')
L.append(f"| 非 EX（单 RT） | `0x0532` | {nm(0x532)} |")
L.append(f"| `_EX`（本节主用） | `0x5200` | {nm(0x5200)} |")
L.append(f"| `_EX2` | `0x5F00` | {nm(0x5F00)} |")
L.append('')
L.append('结论：同一硬件里存在 3 套渲染目标寄存器编码，驱动统一使用 `_EX` 族；')
L.append('排布原因未确认（推断与多引擎/多客户端有关）。')
L.append('')

# ---------------- 2. 纹理单元寄存器文件 ----------------
L.append('## 2. 纹理单元寄存器文件（2 个纹理单元）')
L.append('')
L.append('`.640` 段：`mwv207_sampler.c:78-90` 的 `static const struct mwv207_sampler_desc sampler_regs[]`')
L.append('给出两个纹理单元各自的 8 个子寄存器基址；写入时再加上采样器槽号 `index`。')
L.append('')
L.append('`struct mwv207_sampler_desc`（`include/mwv207_surface.h:76-85`）字段顺序：')
L.append('`sampler_ctrl0`、`sampler_ctrl1`、`sampler_lod_config`、`sampler_lod_bias`、')
L.append('`sampler_anisval`、`tex_desc_address`、`tex_control`、`tex_command`。')
L.append('')
L.append('| 单元 | 字段 | 驱动基址 | 厂商寄存器 | 地址 |')
L.append('|---|---|---|---|---|')
unit_tbl = [
    ('GPIPE（VS/GS/TCS/TES）', [
        ('sampler_ctrl0', 0x5800), ('sampler_ctrl1', 0x5880),
        ('sampler_lod_config', 0x5900), ('sampler_lod_bias', 0x5980),
        ('sampler_anisval', 0x5A00), ('tex_desc_address', 0x5600),
        ('tex_control', 0x5680), ('tex_command', 0x5311),
    ]),
    ('PIXEL（FS）', [
        ('sampler_ctrl0', 0x5B00), ('sampler_ctrl1', 0x5B80),
        ('sampler_lod_config', 0x5C00), ('sampler_lod_bias', 0x5C80),
        ('sampler_anisval', 0x5D00), ('tex_desc_address', 0x5700),
        ('tex_control', 0x5780), ('tex_command', 0x5312),
    ]),
]
for unit, fields in unit_tbl:
    for f, rid in fields:
        L.append(f"| {unit} | `{f}` | `0x{rid:04X}` | {nm(rid)} | `0x{(rid<<2):05X}` |")
L.append('')
L.append('驱动写入时统一体现在 `sampler_regs[config_id].<field> + index`')
L.append('（`mwv207_sampler.c:567-590`），`index` 即采样器槽号。')
L.append('')
L.append('注意纹理描述符表通过 **DAT**（而非 EMIT）写入：')
L.append('`mwv207_add_dat(ctx, &cmd_buf, sampler_regs[config_id].tex_desc_address + index, &tex_desc->dat)`')
L.append('（`mwv207_sampler.c:578`）。')
L.append('')

# ---------------- 3. 着色器 uniform 区 ----------------
L.append('## 3. 着色器 uniform / 立即数区')
L.append('')
L.append('`mwv207_vertex.c:305-344` `mwv207_config_uniform()` 按 shader stage 选择基址，')
L.append('随后每 4 个字用一次 `MWV207_EMITS(cmd_buf, base, 4, unifrom)` 写入：')
L.append('')
L.append('| stage | 驱动基址 | 厂商寄存器 | 地址 |')
L.append('|---|---|---|---|')
for st, rid in [('VERTEX / TESS_CTRL / TESS_EVAL / GEOMETRY', 0xD000),
                ('FRAGMENT', 0xD800 + 0x400),
                ('COMPUTE', 0xD800)]:
    hit = look(rid)
    L.append(f"| {st} | `0x{rid:04X}` | {hit[0] if hit else '—（未定义，未确认）'} | `0x{(rid<<2):05X}` |")
L.append('')
L.append('另有每 stage 的 uniform 基址/锁寄存器（驱动在 `mwv207_vertex.c:350-360` 写入 TCS/TES 的）：')
L.append('')
L.append('| 寄存器 | 地址 | reg_id | 用途 |')
L.append('|---|---|---|---|')
for rid in (0x0453, 0x0454, 0x5291, 0x5292, 0x52C9, 0x52CA):
    L.append(f"| {nm(rid)} | `0x{(rid<<2):05X}` | `0x{rid:04X}` | uniform 基址 / 锁 |")
L.append('')

# ---------------- 4. 未映射寄存器 ----------------
L.append('## 4. 未映射寄存器分析')
L.append('')
unmapped = sorted(k for k in cmd if k.startswith('0x') and k.lower() not in
                  {('0x%05x' % r['address']) for r in regs.values()}
                  and k.lower().lstrip('0x').lstrip('0') not in
                  {('%x' % (r['address'] >> 2)) for r in regs.values()})
# 上面的集合比较不可靠，改用数值比较
def mapped(key):
    try: rid = int(key, 16)
    except ValueError: return True
    return (rid << 2) in byaddr
unmapped = sorted((int(k, 16), k) for k in cmd if k.startswith('0x') and not mapped(k))
L.append(f'驱动写入了 **{len(unmapped)}** 个在厂商发布头文件中没有 `_Address` 定义的寄存器号')
L.append('（占 219 个键的 %.0f%%）。这不是映射规则的问题，而是发布出来的寄存器定义不完整。' % (100.0*len(unmapped)/max(1,len([k for k in cmd if k.startswith("0x")]))))
L.append('')
L.append('| reg_id | 地址 | 使用次数 | 相邻的已定义寄存器（地址） | 归属推断 |')
L.append('|---|---|---:|---|---|')
allsorted = sorted(r['address'] for r in regs.values())
import bisect
for rid, key in sorted(unmapped):
    a = rid << 2
    i = bisect.bisect_left(allsorted, a)
    prev = allsorted[i-1] if i > 0 else None
    nxt = allsorted[i] if i < len(allsorted) else None
    ctx_txt = (f"前 {byaddr[prev][0]} (0x{prev:05X})" if prev else '') + \
              (f" / 后 {byaddr[nxt][0]} (0x{nxt:05X})" if nxt else '')
    # 归属推断：取相邻寄存器的前缀
    def pref(addr):
        if addr is None: return None
        n = byaddr[addr][0]
        m = re.match(r'^MWV207REG_([A-Z0-9]+?)_', n)
        return m.group(1) if m else None
    p1, p2 = pref(prev), pref(nxt)
    guess = p1 if p1 == p2 else (f'{p1}/{p2}' if p1 and p2 else (p1 or p2 or '?'))
    if a > 0x30000:
        guess = 'uniform/立即数区（按 stage 分段的连续地址）'
    elif 0x01400 <= a <= 0x014FF:
        guess = 'PE 区（0x1400–0x14FF 有明显未定义空洞）'
    elif 0x00E00 <= a <= 0x00EFF:
        guess = 'RA/PS 引擎控制区（0xE00 段）'
    elif 0x03800 <= a <= 0x038FF:
        guess = 'CMD/SYNC 区（0x3800 段）'
    elif a == 0x3FFFC:
        guess = '命令流哨兵值（不是真实寄存器）'
    L.append(f"| `0x{rid:04X}` | `0x{a:05X}` | {len(cmd[key])} | {ctx_txt} | {guess}（未确认） |")
L.append('')
L.append('### 说明')
L.append('')
L.append('- `0xFFFF`（地址 `0x3FFFC`）不是寄存器：它是命令流里的终止/哨兵值。')
L.append('- 未映射寄存器集中在 `0x01400–0x014FF`（PE 区）、`0x00E00–0x00EFF`、')
L.append('  `0x03800–0x038FF`；这些地址区间在厂商发布头文件里恰好是空的，')
L.append('  说明发布版**有意或无意地省略**了这批定义。')
L.append('- 因此这 49 个寄存器**无法**从现有资料确定名称与字段，任何补全都属推断。')
L.append('')

out = os.path.join(ROOT, 'REGISTER-ARRAYS.md')
open(out, 'w', encoding='utf-8').write('\n'.join(L) + '\n')
print('wrote', out, len(L), 'lines; unmapped =', len(unmapped))
