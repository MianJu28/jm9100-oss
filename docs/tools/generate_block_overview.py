#!/usr/bin/env python3
"""生成 docs/api/10-HARDWARE-BLOCKS.md —— 硬件功能块总览。

数据来源：docs/registers/register-db.json（由官方 KMD 头文件解析得到）。
所有"功能推断"列均为**推断**，依据是块名 + 该块寄存器名集合 + 驱动中的使用位置；
凡无驱动使用位置的块，其功能仅由寄存器名推断，标注 ⚠。
"""
import json
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..'))
DB = json.load(open(os.path.join(ROOT, 'registers', 'register-db.json'), encoding='utf-8'))
regs = list(DB['registers'].values())
driver_ids = set()
for r in DB['driver_join'].values():
    pass

# 功能推断（人工整理）。键为块名，值为 (中文功能, 分组)
FUNC = {
    # ---- 3D 图形管线 ----
    'FE':      ('前端：图元装配、索引/顶点抓取、图元重启', '3D'),
    'VS':      ('顶点着色器：程序/指令缓存、负载均衡、属性', '3D'),
    'TCS':     ('曲面细分控制着色器', '3D'),
    'TES':     ('曲面细分求值着色器', '3D'),
    'TPG':     ('曲面细分图元生成器', '3D'),
    'GS':      ('几何着色器', '3D'),
    'PS':      ('像素（片元）着色器', '3D'),
    'SH':      ('着色器公共资源：缓存控制、采样器仲裁、uniform/采样器/纹理配额', '3D'),
    'SHADER':  ('着色器全局配置（steering、L1C flush）', '3D'),
    'SHL1':    ('着色器 L1 缓存控制', '3D'),
    'RA':      ('光栅化器控制（控制字、mediump 掩码、超时）', '3D'),
    'PA':      ('图元装配/裁剪：线宽、裁剪 W、点大小', '3D'),
    'SE':      ('裁剪/建立引擎：各向裁剪比较、深度偏移钳制', '3D'),
    'VIEWPORT':('视口变换（X/Y/Z scale+offset 扩展）', '3D'),
    'CLIP':    ('裁剪距离寄存器对', '3D'),
    'CULL':    ('剔除距离寄存器对', '3D'),
    'PE':      ('像素引擎：渲染目标配置、Alpha/混合、层次化 Z、VAA、深度基址', '3D'),
    'MRT':     ('多渲染目标：显存布局、颜色基址/清除值', '3D'),
    'MRTMC':   ('多渲染目标内存清除', '3D'),
    'DEPTH':   ('深度缓冲：基址/末端/切片、near/far 扩展', '3D'),
    'STENCIL': ('模板缓冲：步长/基址/末端', '3D'),
    'HZ':      ('层次化 Z 缓冲基址与 tile status', '3D'),
    'TX':      ('纹理单元：步长/体积/模式/YUV 控制/ASTC 形状', '3D'),
    'TEXTURE': ('纹理描述符表地址', '3D'),
    'TX3':     ('纹理（扩展）', '3D'),
    'SAMPLER': ('采样器地址表 / 命令 / 失效', '3D'),
    'MC':      ('显存/缓存控制：缓存频率、颜色清除、纹理 buffer', '3D'),
    'TW':      ('计算负载分发：work group X/Y/Z、tile 信息 X/Y/Z', '3D'),
    'OCL':     ('OpenCL/计算特性控制', '3D'),
    'VARYING': ('varying 打包', '3D'),
    'SEMANTIC':('语义位置', '3D'),
    'POS':     ('PA 簇控制', '3D'),
    'FINAL':   ('PA 控制（末级）', '3D'),
    'PSCS':    ('像素着色器节流', '3D'),
    'MULTI':   ('多重采样：抖动、采样坐标、采样配置', '3D'),
    'COMPONENT':('组件类型（多实例）', '3D'),
    'CLUSTER': ('簇/棋盘格控制', '3D'),
    'CONFIG':  ('NN/TP 配置', 'NN'),
    'POWER':   ('簇电源控制', '3D'),
    'APP':     ('上下文 ID', '公共'),
    'TOGGLE':  ('软件开关', '公共'),
    'SW':      ('软件侧控制', '公共'),
    'USC':     ('统一着色器控制', '3D'),
    'SP':      ('标量处理器指令存储器访问', '3D'),
    'HW':      ('硬件内部控制', '公共'),
    'RESERVED':('保留', '公共'),
    'FEDE':    ('前端/显示超时', '3D'),
    'FA':      ('前端仲裁', '3D'),
    'VERTEX':  ('顶点着色器常量', '3D'),
    'PIXEL':   ('像素着色器常量 / uniform', '3D'),
    'GPIPE':   ('几何流水线公共：负载均衡、uniform', '3D'),
    'ON':      ('芯片缓冲重映射', '公共'),
    'SRAM':    ('SRAM 重映射与图像流', '公共'),
    'SP':      ('标量处理器指令存储器', '3D'),

    # ---- 绘制/查询/同步 ----
    'DRAW':    ('绘制时间戳与绘制 ID', '3D'),
    'TIME':    ('时间戳地址与触发', '3D'),
    'FENCE':   ('写内存围栏（地址/数据）', '同步'),
    'OCCLUSION':('遮挡查询（地址/数据/控制）', '查询'),
    'TFB':     ('变换反馈（命令/头/缓冲地址）', '查询'),
    'SO':      ('流输出（控制、收集、bin 原点/大小、流大小）', '3D'),
    'VERIF':   ('图元 ID 校验', '调试'),
    'STORE':   ('计数器存储', '调试'),
    'TRIGGER': ('NN/TP/缩放触发', 'NN'),
    'XDEEP':   ('深度睡眠控制', '电源'),
    'DEBUG':   ('调试计数器选择/取值', '调试'),
    'PROBE':   ('AXI 探针（地址/数据/选择）', '调试'),

    # ---- 2D / 显示 ----
    'BLT':     ('块传输（2D 引擎）：源/目标地址、tile status、光栅操作', '2D'),
    'DE':      ('显示/2D 引擎：多源叠加、plane 地址与步长、块大小', '2D'),
    'DEYUV':   ('YUV→RGB 转换控制', '2D'),
    'DEYUV2':  ('RGB→? 转换系数', '2D'),
    'DERGB2':  ('YUV 转换系数（扩展）', '2D'),
    'DEST':    ('目标配置扩展', '2D'),
    'DISPLAY': ('显示：抖动配置与抖动表', '显示'),
    'OVERLAY': ('叠加层配置/地址/步长/矩形', '显示'),
    'CURSOR':  ('硬件光标：配置/地址/位置/前景背景', '显示'),
    'GAMMA':   ('伽马 LUT 索引/数据', '显示'),
    'PANEL':   ('面板配置与时序', '显示'),
    'FRAME':   ('帧缓冲配置/地址/步长/原点', '显示'),
    'DPI':     ('DPI 配置', '显示'),
    'DBI':     ('DBI 显示接口', '显示'),
    'PATTERN': ('测试图案生成', '显示'),
    'RS':      ('渲染存储/回写：tile filler 触发、交织地址、源/目标地址', '内存'),
    'DRAM':    ('DRAM 控制', '内存'),
    'MEMORY':  ('内存诊断', '内存'),
    'REQ':     ('请求仲裁：bank/row 掩码、权重', '内存'),
    'RD':      ('读请求老化阈值', '内存'),
    'WR':      ('写请求老化阈值', '内存'),
    'FENCE':   ('围栏', '同步'),

    # ---- 视频编解码 ----
    'DEC':     ('解码器：读写配置、缓冲/缓存基址、控制', '视频'),
    'DEC400':  ('解码器 v4xx：控制/中断/状态（多实例）', '视频'),
    'AHBDEC':  ('AHB 侧解码器：控制/中断（多实例）', '视频'),
    'TPC':     ('视频编解码处理核：编码头/体地址、格式、尺寸、解码配置', '视频'),
    'TPCNV12': ('NV12 源码配置', '视频'),
    'WD':      ('看门狗配置', '视频'),
    'DENOISE': ('降噪后处理：系数、NN 输出缩放、钳制', '视频'),
    'PTN':     ('噪声/测试图案生成', '视频'),
    'OUT':     ('输出图像基址（R/G/B）与请求', '视频'),
    'ROI':     ('感兴趣区域：基址 Y/U/V、左上角', '视频'),
    'CLAMP':   ('输出钳制 R/G/B', '视频'),
    'SCALE':   ('输出缩放 X/Y', '视频'),
    'IN':      ('输入图像与初始化错误', '视频'),
    'CHECKSUM':('输出校验和期望值 R/G/B', '视频'),
    'HIST':    ('直方图基址（3 个）', '视频'),
    'YUV2':    ('RGB→YUV 系数', '视频'),
    'COMP':    ('解压控制', '视频'),
    'VIP':     ('视频输入处理：flush、时钟门控、tensor IO、SRAM 图像流', '视频'),
    'VIP40':   ('VIP 位宽/虚拟地址', '视频'),
    'XVIP':    ('VIP 调试', '视频'),
    'XDEEP':   ('深度睡眠', '视频'),
    'MCFE':    ('媒体/命令前端：标准与优先描述符环形缓冲、FIFO 指针', '视频'),

    # ---- 总线 / MMU ----
    'AHB':     ('AHB 总线电源控制（多实例）', '总线'),
    'AHBX':    ('AHB 扩展：tensor IO、AXI QoS、深度睡眠', '总线'),
    'AHBXDMA': ('AHB 扩展 DMA 中断/状态', '总线'),
    'AHBXDMA0':('AHB 扩展 DMA0 统计与调试', '总线'),
    'AHBXDMA1':('AHB 扩展 DMA1 调试', '总线'),
    'AHBXDMA2':('AHB 扩展 DMA2 统计与调试', '总线'),
    'AHBXDMA3':('AHB 扩展 DMA3 调试', '总线'),
    'AHBXDMA4':('AHB 扩展 DMA4 调试', '总线'),
    'AHBXDDR': ('AHB 扩展 DDR 读写时钟计数', '总线'),
    'AHBXVIP': ('AHB 扩展 VIP 调试/睡眠/完成', '总线'),
    'AHBXVIP0':('AHB 扩展 VIP0 请求统计', '总线'),
    'AHBXVIP1':('AHB 扩展 VIP1 请求统计', '总线'),
    'AHBXVIP2':('AHB 扩展 VIP2 请求统计', '总线'),
    'AHBXVIP3':('AHB 扩展 VIP3 请求统计', '总线'),
    'AHBVIP':  ('AHB VIP：tensor IO、时钟门控、调试', '总线'),
    'AHBVIPDMA':('AHB VIP DMA 状态/中断', '总线'),
    'AHBXTTD': ('AHB 扩展 TTD 杂项', '总线'),
    'AHBTT':   ('AHB TTD 同步', '总线'),
    'AHBTTD':  ('AHB TTD 杂项', '总线'),
    'AHBSW':   ('AHB 软件侧 VXSBI 控制', '总线'),
    'AXI':     ('AXI 配置与 SRAM 重映射', '总线'),
    'MMU':     ('IOMMU：安全地址、配置、状态、异常、屏障', '总线'),
    'MMUAHB':  ('AHB 侧 IOMMU：异常、页表基址、表大小', '总线'),
    'CMD':     ('命令缓冲 AHB 控制 / NN 循环区域', '总线'),
    'CE':      ('拷贝/命令引擎：异步触发、原点、尺寸、执行、程序', '2D'),
    'CLOCK':   ('时钟控制', '电源'),
    'DC':      ('芯片 ID/版本/日期/补丁版本', '公共'),
    'HI':      ('芯片补丁版本/AHB 控制', '公共'),

    # ---- 性能计数器 ----
    'PC':      ('性能计数器：绘制命令数、顶点数、索引/命令字节数、缓存命中', '性能'),
    'PCL1':    ('PC L1 缓存计数', '性能'),
    'PCC':     ('颜色缓存计数', '性能'),
    'PCHZ':    ('层次化 Z 缓存计数', '性能'),
    'PCZ':     ('深度缓存计数', '性能'),
    'PCEZ':    ('EZ 缓存计数', '性能'),
    'PCTX':    ('纹理缓存计数', '性能'),

    # ---- 未分类 ----
    'CHECKSUM':('校验和', '视频'),
    'COLOR':   ('颜色切片', '3D'),
    'PANEL':   ('面板', '显示'),
    'GLOBAL':  ('无块前缀的全局/显示时序寄存器（见第 6 节）', '公共'),
}
GROUP_ORDER = ['3D', '2D', '显示', '视频', 'NN', '内存', '总线', '同步', '查询', '性能', '电源', '调试', '公共', '视频', '其他']

BLOCK_RE = re.compile(r'MWV207REG_([A-Z0-9]+)_')


def block_of(name):
    m = BLOCK_RE.match(name)
    if m:
        return m.group(1)
    # 少数寄存器直接写成 MWV207REG_<NAME>（无再一层前缀）→ 归入 GLOBAL 伪块
    return 'GLOBAL'


globals_rs = []
blocks = {}
for r in regs:
    b = block_of(r['name'])
    if b == 'GLOBAL':
        globals_rs.append(r)
    blocks.setdefault(b, []).append(r)

# driver_join 的键是十六进制字符串（如 '0x530'）；寄存器条目的 driver_ids 同格式
used_ids = {k.lower() for k in DB['driver_join'].keys()}
_vendor_used = set()
for e in DB['driver_join'].values():
    for v in e.get('vendor', []):
        _vendor_used.add(v)

L = []
L.append('# 硬件功能块总览')
L.append('')
L.append('> 数据来源：`../registers/register-db.json`（由官方 KMD 释放头文件解析）+ 驱动源码。')
L.append('> 生成方式：`../tools/generate_block_overview.py`（可重复执行）。')
L.append('>')
L.append('> **"功能"列是推断**，依据为块名、该块寄存器名集合、以及驱动中的使用位置。')
L.append('> 带 ⚠ 的块在驱动中**没有任何使用位置**，其功能完全由寄存器名推断，未经任何验证。')
L.append('')
L.append('## 1. 总表')
L.append('')
L.append('| 块 | 寄存器数 | 地址范围 | 驱动使用 | 推断功能 | 分组 |')
L.append('|---|---:|---|---:|---|---|')
for b in sorted(blocks, key=lambda b: (-len(blocks[b]), b)):
    rs = sorted(blocks[b], key=lambda r: r['address'])
    lo, hi = rs[0]['address'], rs[-1]['address']
    u = sum(1 for r in rs if r['name'] in _vendor_used)
    fn, grp = FUNC.get(b, ('**未推断**', '其他'))
    warn = ' ⚠' if u == 0 else ''
    L.append(f'| `{b}` | {len(rs)} | `0x{lo:05X}`–`0x{hi:05X}` | {u} 个{warn} | {fn} | {grp} |')
L.append('')
L.append(f'合计 **{len(regs)}** 个寄存器、**{len(blocks)}** 个块前缀、其中 **{len(used_ids)}** 个驱动寄存器号被驱动使用（映射到 **{len(_vendor_used)}** 个厂商寄存器）。')
L.append('')

L.append('## 2. 主 3D 管线的寄存器块')
L.append('')
L.append('按驱动中的实际执行顺序：`FE`（前端装配）→ `VS`/`TCS`/`TES`/`GS`/`PS`（各着色阶段)'
         '→ `RA`（光栅化）→ `SE`（裁剪/建立）→ `VIEWPORT` → `PE`/`MRT`（像素引擎与渲染目标）'
         '→ `DEPTH`/`STENCIL`/`HZ`（深度模板）→ `TX`/`SAMPLER`/`TEXTURE`（纹理）→ `MC`/`RS`（显存写入）。')
L.append('')
L.append('驱动侧对照：`PIPE_SHADER_*` 阶段 → `include/mwv207_state.h` 的 `mwv207_shader_state`；')
L.append('发射函数见 `mwv207_draw.c` 的 `mwv207_config_*` 系列（详见 `03-DRAW-PIPELINE.md` §3）。')
L.append('')

L.append('## 3. 地址空间分布')
L.append('')
L.append('同一块名可能出现在**多个不相邻的地址段**（多实例/多版本寄存器），例如 `PE` 同时存在于 '
         '`0x01400` 区、`0x14800` 区（`_EX` 渲染目标实例）与 `0x17F00` 区。')
L.append('')
L.append('| 块 | 地址段（连续区间的起止） |')
L.append('|---|---|')
for b in sorted(blocks, key=lambda b: min(r['address'] for r in blocks[b])):
    rs = sorted(blocks[b], key=lambda r: r['address'])
    segs = []
    start = prev = rs[0]['address']
    for r in rs[1:]:
        if r['address'] - prev > 0x400:
            segs.append((start, prev))
            start = r['address']
        prev = r['address']
    segs.append((start, prev))
    txt = '、'.join(f'`0x{a:05X}`–`0x{c:05X}`' for a, c in segs[:4])
    if len(segs) > 4:
        txt += f' …（共 {len(segs)} 段）'
    L.append(f'| `{b}` | {txt} |')
L.append('')

L.append('## 4. 驱动号 ↔ 厂商地址')
L.append('')
L.append('`驱动寄存器号 = 厂商 _Address >> 2`（`07-WINSYS-KMD` 与 `REGISTERS.md` 有完整证据链）。')
L.append('因此上表的地址范围除以 4 即为驱动侧使用的号段，例如 `DE 0x01308-0x1D000` → 驱动号 `0x04C2-0x7400`。')
L.append('')
L.append('## 5. 未能推断的块')
L.append('')
unk = [b for b in blocks if b not in FUNC]
if unk:
    L.append('以下块没有足够的语义线索（无驱动使用位置、名字过于笼统或疑似混淆名），**未做功能推断**：')
    L.append('')
    L.append('`' + '`、`'.join(sorted(unk)) + '`')
else:
    L.append('（无）')
L.append('')


L.append('## 6. 无块前缀的 GLOBAL 寄存器')
L.append('')
L.append('这 32 个寄存器在厂商头文件里写成 `MWV207REG_<NAME>`（没有第二层块前缀），')
L.append('分为显示时序、全局控制/状态、以及若干 3D/视频/存储类单例：')
L.append('')
L.append('| 地址 | 驱动号 | 名称 | 驱动使用 | 推断作用 |')
L.append('|---|---:|---|---:|---|')
_g_note = {
    'HDISPLAY':'水平显示时序', 'HSYNC':'水平同步（**注意**：驱动把 0x508 用作 RT0 的 Alpha Test，见下）',
    'HCOUNTER1':'水平计数 1', 'HCOUNTER2':'水平计数 2', 'VDISPLAY':'垂直显示时序',
    'VSYNC':'垂直同步', 'COUNTER0-3':'通用计数器', 'CONTROL0-3':'通用控制',
    'STATUS0':'通用状态', 'ENDIANNESS0-2':'字节序控制', 'RESERVED/RESERVED1':'保留',
    'CENTROIDS':'多重采样质心坐标（驱动使用）', 'VARYINGS':'varying 打包（驱动使用）',
    'SEMANTICS':'语义位置（驱动使用）', 'CONTEXT':'上下文', 'PERFORMANCE':'性能控制',
    'ROI':'感兴趣区域', 'STRIDE':'步长', 'COMPRESSOR':'压缩器', 'DECOMPRESSOR':'解压器',
    'PATTERNS':'测试图案', 'XCACHE':'扩展缓存', 'CONTROL2':'全局控制 2',
}
for r in sorted(globals_rs, key=lambda r: r['address']):
    short = r['name'].replace('MWV207REG_', '')
    note = _g_note.get(short, _g_note.get(re.sub(r'\d+$', '', short), '未推断'))
    ids = '、'.join(r.get('driver_ids') or []) or '—'
    L.append(f"| `0x{r['address']:05X}` | {ids} | `{short}` | {'✅' if r['name'] in _vendor_used else ''} | {note} |")
L.append('')
L.append('> **交叉发现**：`0x508`（地址 `0x1420`）在厂商表里名为 `HSYNC`，但驱动在 `mwv207_draw.c` 中把它当作')
L.append('> **RT0 的 Alpha Test** 寄存器使用（`mwv207_sampler.c`/`mwv207_draw.c` 的 alpha_config 路径）。')
L.append('> 同一地址按功能复用，或厂商释放的命名有误导性——两种可能都未确认。')
L.append('')

out = os.path.join(ROOT, 'api', '10-HARDWARE-BLOCKS.md')
os.makedirs(os.path.dirname(out), exist_ok=True)
open(out, 'w', encoding='utf-8').write('\n'.join(L) + '\n')
print('wrote', out, len(L), 'lines')