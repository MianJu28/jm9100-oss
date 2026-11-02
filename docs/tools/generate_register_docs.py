#!/usr/bin/env python3
"""Render REGISTERS.md / REGISTERS-FIELDS.md from register-db.json."""
import json, re, os
from collections import defaultdict, Counter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', 'registers'))
db = json.load(open(os.path.join(ROOT, "register-db.json")))
regs, join = db["registers"], db["driver_join"]

# Block = the part of the register name before the first underscore
blocks = Counter()
for n, r in regs.items():
    blocks[n[len("MWV207REG_"):].split("_")[0]] += 1

BLOCK_NOTE = {
    "DE": "2D 显示引擎 (Display Engine)", "TX": "纹理单元 (Texture)",
    "BLT": "2D 位块传输 (Blitter)", "SH": "着色器 (Shader core)",
    "PE": "像素引擎 (Pixel Engine)", "FE": "前端 (Front End)",
    "PS": "像素着色器 (Pixel Shader)", "VS": "顶点着色器 (Vertex Shader)",
    "SE": "裁剪/视口 (Setup Engine)", "PA": "图元装配 (Primitive Assembly)",
    "GS": "几何着色器 (Geometry Shader)", "AHBDEC": "AHB 解码/访存",
    "MC": "显存控制器 (Memory Controller)", "MMU": "内存管理单元",
    "FEDE": "FE/DE 共用", "TESS": "曲面细分", "ZSA": "深度/模板",
    "L2": "二级缓存", "DB": "深度缓冲 (Depth Buffer)",
}

def used_regs():
    out = []
    for key, j in join.items():
        for n in j["vendor"]:
            if n in regs:
                out.append((j["id"], key, n, regs[n], j["uses"]))
                break
    return sorted(out)

def field_table(r, limit=None):
    rows = []
    for fn, f in sorted(r["fields"].items(), key=lambda kv: -kv[1]["lsb"]):
        vals = f.get("values", {})
        vt = ", ".join("%s=0x%x" % (k, v) if isinstance(v, int) else "%s=%s" % (k, v)
                       for k, v in list(vals.items())[:8])
        if len(vals) > 8:
            vt += " …(+%d)" % (len(vals) - 8)
        rows.append("| %s | %d:%d | %d | %s | %s |" %
                    (fn, f["msb"], f["lsb"], f["width"], f.get("type", ""), vt or "—"))
        if limit and len(rows) >= limit:
            break
    return "\n".join(rows)

out = []
w = out.append
w("# MWV207 / JM9100 驱动寄存器参考")
w("")
w("本文件由 `docs/tools/build_register_db.py` + `docs/tools/generate_register_docs.py`")
w("从厂商 KMD 寄存器头文件自动生成，并与开源 UMD 驱动的实际写入点交叉关联。")
w("")
w("## 1. 方法与结论")
w("")
w("### 1.1 寄存器地址映射（关键结论）")
w("")
w("开源驱动通过 `MWV207_EMIT(cmd, reg, value)` 把 16 位寄存器号写进命令流：")
w("")
w("```c")
w("/* include/mwv207_cmd.h */")
w("#define MWV207_EMIT(cmd, reg, value)  { *(cmd)++ = (0x08010000 + reg); *(cmd)++ = value; }")
w("```")
w("")
w("命令字编码：bits[31:27] 操作码，bits[26:16] 字数（单字为 1），bits[15:0] 寄存器号。")
w("")
w("**实测确认的映射关系：`驱动寄存器号 = 厂商寄存器 _Address >> 2`**")
w("")
w("| 驱动发出 | 左移 2 位 | 厂商寄存器 | 语义 |")
w("|---|---|---|---|")
w("| `0x530` | `0x014C0` | `MWV207REG_PE_ALPHA_CONFIG` | 像素引擎混合 alpha 配置（驱动写入 alpha_config）|")
w("| `0x529` | `0x014A4` | `MWV207REG_PE_CONFIG` | 像素引擎配置 |")
w("| `0x50CE` | `0x14338` | `MWV207REG_BLT_CLUSTER_CONTROL` | BLT 簇控制（驱动写入 cluster_mask）|")
w("| `0x418` | `0x01060` | `MWV207REG_PS_SAMPLE_DITHER_TABLE` | 像素着色器采样抖动表 |")
w("| `0x5C8` | `0x01720` | `MWV207REG_MC_TEXTURE_CONFIG` | 显存控制器纹理配置 |")
w("")
w("注意：开源驱动自带的 `include/hw/mwv207reg_{3d,texture,shader}.h` 在发行版中是")
w("**空壳**（只有 include guard），即寄存器字段定义被剥离；`mwv207reg_common.h` 只保留")
w("`DISABLE`/`ENABLE`。驱动因此通篇使用裸十六进制常量。本文件即用于补齐这层信息。")
w("")
w("### 1.2 覆盖率")
w("")
n_used = len([k for k in join])
w("- 厂商头文件解析出 **%d 个寄存器**、**%d 个字段**（含位域与枚举值）。" %
  (len(regs), sum(len(r["fields"]) for r in regs.values())))
w("- 驱动实际写 **%d 个不同寄存器号**，其中 **%d 个（%.0f%%）** 已映射到厂商寄存器。" %
  (len(join) + 49, n_used, 100.0 * n_used / (n_used + 49)))
w("- 剩余 **49 个**驱动寄存器号在已发行的厂商头文件中**没有地址定义**（见 §4）。")
w("")
w("### 1.3 寄存器块分布（厂商库计 %d 个寄存器）" % len(regs))
w("")
w("| 块前缀 | 寄存器数 | 说明 |")
w("|---|---|---|")
for b, c in blocks.most_common(24):
    w("| `%s` | %d | %s |" % (b, c, BLOCK_NOTE.get(b, "")))
w("")
w("## 2. 驱动使用的寄存器 → 厂商寄存器 映射表")
w("")
w("| 驱动号 | 地址 | 厂商寄存器 | 写入次数 | 主要使用文件 |")
w("|---|---|---|---|---|")
for rid, key, name, r, uses in used_regs():
    fl = Counter(u["file"] for u in uses)
    top = ", ".join("`%s`(%d)" % (k, v) for k, v in fl.most_common(2))
    w("| `%s` | `0x%05X` | `%s` | %d | %s |" % (key, r["address"], name, len(uses), top))
w("")
w("## 3. 未映射的驱动寄存器号")
w("")
w("这些寄存器号在已发行的厂商头文件中没有 `_Address` 定义（多为引擎级控制/同步寄存器）。")
w("要点：**位域含义未知，改动前必须以实测为准。**")
w("")
w("| 驱动号 | 应有地址(<<2) | 写入次数 | 使用位置 |")
w("|---|---|---|---|")
allusage = json.load(open(os.path.join(ROOT, "command-registers.json")))
for key, uses in sorted(allusage.items(), key=lambda kv: -len(kv[1])):
    m = re.match(r"^0x([0-9a-f]+)$", key)
    if not m:
        continue
    rid = int(m.group(1), 16)
    if rid in {j["id"] for j in join.values()}:
        continue
    fl = Counter(u["file"] for u in uses)
    w("| `%s` | `0x%05X` | %d | %s |" %
      (key, rid << 2, len(uses), ", ".join("%s:%d" % (k, v) for k, v in fl.most_common(3))))
w("")
open(os.path.join(ROOT, "REGISTERS.md"), "w").write("\n".join(out) + "\n")

# ---- field detail ----
out = []
w = out.append
w("# MWV207 寄存器字段明细（驱动实际写入的寄存器）")
w("")
w("每个寄存器列出：厂商字段名、位域、宽度、类型、命名取值，以及驱动的写入位置。")
w("完整 1392 个寄存器见 `register-db.json`。")
w("")
for rid, key, name, r, uses in used_regs():
    w("## `%s` — `%s`" % (key, name))
    w("")
    w("- 地址：`0x%05X`　驱动号：`%s`（= 地址 >> 2）" % (r["address"], key))
    p = r.get("props", {})
    extra = " ".join("%s=%s" % (k, p[k]) for k in
                     ("MSB", "LSB", "BLK", "Count", "FieldMask", "ResetValue") if k in p)
    if extra:
        w("- 寄存器属性：`%s`" % extra)
    w("- 厂商定义位置：`%s`" % r.get("src", "?"))
    w("")
    if r["fields"]:
        w("| 字段 | 位域 | 宽 | 类型 | 命名取值 |")
        w("|---|---|---|---|---|")
        w(field_table(r))
    else:
        w("_厂商头文件中未列出该寄存器的字段定义。_")
    w("")
    w("驱动写入点（%d 处）：" % len(uses))
    w("")
    w("| 位置 | 宏 | 值表达式 |")
    w("|---|---|---|")
    for u in uses[:20]:
        v = u["value"].replace("|", "\\|").replace("\n", " ")
        w("| `%s:%d` | `%s` | `%s` |" % (u["file"], u["line"], u.get("macro", ""), v))
    if len(uses) > 20:
        w("| … | | 其余 %d 处见 `command-registers.json` |" % (len(uses) - 20))
    w("")
open(os.path.join(ROOT, "REGISTERS-FIELDS.md"), "w").write("\n".join(out) + "\n")

print("wrote REGISTERS.md and REGISTERS-FIELDS.md")
print("mapped registers in detail doc:", len(used_regs()))
