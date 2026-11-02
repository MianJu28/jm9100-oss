# 03 · 寄存器映射（闭源 HAL 实证 + 厂商库交叉）

## 1. 编码约定的双向实证

开源驱动里命令字的寄存器号 = 厂商 `MWV207REG_*_Address >> 2`。此前这是**统计推断**
（170/219 命中）。闭源 HAL 给出了**直接证据**：

```
$ objdump -d libdrm_jmgpu.so.1.0.0 | grep -A3 "<jmo_HARDWARE_LoadState32>:"
0000000000093e40 <jmo_HARDWARE_LoadState32>:
   93e40:  stp x29, x30, [sp, #-32]!
   93e44:  lsr w1, w1, #2          ← 入口第一条：寄存器地址 >> 2
```

`jmo_HARDWARE_LoadState32NEW`(0xa1630) 同样以 `lsr w1, w1, #2` 开头。
而 `jmo_HARDWARE_LoadCtrlStateNEW`(0x94190) 内：

```
   941c8:  ubfx  x1, x21, #2, #16      ← 取 reg（16 位）
   941cc:  mov   w2, #0x8010000        ← 0x0801_0000
   941d0:  orr   w1, w1, w2            ← 命令字 = 0x08010000 | reg
   941d8:  stp   w1, w23, [x0]         ← [命令字, 值] 成对写入
```

与开源 `include/mwv207_cmd.h` 的 `MWV207_EMIT(cmd,reg,value)` →
`*(cmd)++ = (0x08010000 + reg); *(cmd)++ = value;` **完全一致**。

**结论：两套栈使用同一套编码，闭源 HAL 里出现的寄存器常量可直接换算到开源驱动。**

## 2. 提取方法

`reverse/tools/extract_jm_regs.py`（可重跑）走两条路径：

- **A 立即数**：函数内 `mov/movk wN, #imm` 后 30 条内 `bl` 到
  `HARDWARE_LoadState*` / `LoadCtrlState*` / `WriteBuffer` / `LoadProgram` / `Load2DState`。
- **B 字面量池**：aarch64 大常量常走 `ldr`，故另扫 `.rodata`/`.rodata1`/`.data.rel.ro`
  的每个 u32 `v`，若 `v*4` 命中厂商库则采纳。

厂商库解析正则：`#define\s+MWV207REG_(\w+?)_Address\s+0x([0-9A-Fa-f]+)`。
> ⚠ 坑：厂商头里地址是**大写 5 位**（`0x03824`）。早先用小写 `0x3824` grep 得到
> "查不到"，据此把 OCCLUSION_* 误判为不可解——**已纠正**。

## 3. 覆盖结果

| 项 | 数量 |
|---|---|
| 厂商库寄存器地址总数 | 1353 |
| 1.8.0 与 1.6.1 地址集合 | **完全相同**（1353，交集 1353，差集 0） |
| 高置信实证引用到的寄存器 | **379** |
| 分布在 4KB 块 | 21 |

### 3.1 关于统计显著性的诚实说明

"某个立即数 `v` 满足 `v*4` 在厂商库里"这件事**有随机命中的可能**：
厂商库 1353 个地址分布在 `0..0x36000`，除以 4 后候选空间是 55297 个值，
随机命中率 = 1353/55297 ≈ **2.45%**。

因此做了零假设检验，并据此收紧了口径：

| 口径 | 候选数 | 实际命中 | 随机期望假命中 | 信噪比 |
|---|---|---|---|---|
| 全库 `mov` 立即数（宽松） | 15020 | 1165 | 367.5 | 3.2× |
| 字面量池 u32 | 3980 | 243 | 97.4 | 2.5× |
| **仅 `Load/Flush/Set/Write/…` 类函数且 `v>=0x18`** | — | **379 个地址** | — | 高 |

宽松口径下仍有 1/3 左右的命中可能来自巧合，**不足以逐条断言**。
故最终发布的 379 采用最严口径：只接受出现在 `Load*`/`Flush*`/`Set*`/`Write*`
等**确实会写寄存器的函数**里、且值 `>= 0x18`（排除小常量如 `0`、`1`、`4`）的立即数。
即便如此，`0xFFFF`、`0xE00` 这类同时是"常见掩码/边界值"的号仍应视为
**待确认**，已在 §4 表里按其所在函数判断。

> 教训：把"统计上显著"当成"每一条都对"是错的。总量显著 ≠ 单条可信；
> 单条可信必须靠**函数名 + 开源驱动自身用法**双重印证（见 `04-UNMAPPED-REGS.md`）。

## 4. 按硬件块分布

| 地址段 | 寄存器数 | 主要前缀 |
|---|---|---|

| `0x00000`–`0x00FFF` | 138 | AHB, AHBDEC, AXI, CENTROIDS, CONTROL2, COUNTER0, DBI, DC, DISPLAY, END |
| `0x01000`–`0x01FFF` | 88 | AHBDEC, DE, DEPTH, DERGB2, DEYUV, DEYUV2, GS, HIST, MC, MRT, OCL, PE,  |
| `0x02000`–`0x02FFF` | 11 | AHBTT, MCFE, TX |
| `0x03000`–`0x03FFF` | 32 | AHBX, CE, COLOR, DEPTH, FENCE, MULTI, OCCLUSION, PROBE, SEMANTIC, SEMA |
| `0x08000`–`0x08FFF` | 1 | PS |
| `0x0C000`–`0x0CFFF` | 1 | VS |
| `0x10000`–`0x10FFF` | 13 | TX, TX3 |
| `0x11000`–`0x11FFF` | 7 | TX |
| `0x12000`–`0x12FFF` | 1 | DE |
| `0x13000`–`0x13FFF` | 10 | PC, PCHZ, PCTX, PCZ |
| `0x14000`–`0x14FFF` | 73 | BLT, FE, PE, SO, TCS, TES, TPC, TX |
| `0x15000`–`0x15FFF` | 8 | FE, TX, VS |
| `0x16000`–`0x16FFF` | 7 | TX |
| `0x17000`–`0x17FFF` | 7 | FE, PE, TX |
| `0x18000`–`0x18FFF` | 5 | DEC, DEC400 |
| `0x19000`–`0x19FFF` | 1 | DEC400 |
| `0x1C000`–`0x1CFFF` | 6 | TFB |
| `0x20000`–`0x20FFF` | 1 | SH |
| `0x30000`–`0x30FFF` | 1 | SH |
| `0x34000`–`0x34FFF` | 1 | GPIPE |
| `0x36000`–`0x36FFF` | 1 | PIXEL |

## 5. 引用次数最多的核心寄存器（Top 30）

引用次数 = 在 HAL 中作为立即数出现 / 出现在字面量池的不同函数数，反映其重要程度。

| 地址 | 命令号 | 寄存器名 | 引用 |
|---|---|---|---|

| `0x8000` | `0x2000` | `PS_SHADER_CODE` | 15 |
| `0x3C0` | `0xF0` | `AXI_AHB_CONFIG_EX` | 14 |
| `0x10000` | `0x4000` | `TX_CONFIG` | 13 |
| `0x190` | `0x64` | `MMU_EXCEPTION` | 12 |
| `0x14BC` | `0x52F` | `PE_CONFIG_EX` | 11 |
| `0x1000` | `0x400` | `AHBDEC_WRITE_FLUSH_CACHE_EX` | 10 |
| `0x394` | `0xE5` | `MMUAHB_TABLE_ARRAY_SIZE` | 9 |
| `0x860` | `0x218` | `AHBDEC_TOTAL_WRITE_BURSTS_OUT/SHADER_CONFIG` | 9 |
| `0x3B0` | `0xEC` | `AHB_CONTROL0` | 9 |
| `0x20000` | `0x8000` | `SH_INSTRUCTIONS` | 9 |
| `0x158` | `0x56` | `AHB_POWER_CONTROL1` | 7 |
| `0x7E0` | `0x1F8` | `FE_DESCRIPTOR_END_ADDRESS` | 7 |
| `0x908` | `0x242` | `TW_INFO_Y` | 7 |
| `0x14000` | `0x5000` | `BLT_SRC_ADDRESS` | 7 |
| `0x160` | `0x58` | `AHB_POWER_CONTROL3` | 6 |
| `0x67C` | `0x19F` | `FE_INDEX_MAX` | 6 |
| `0x168` | `0x5A` | `AHB_POWER_CONTROL5` | 6 |
| `0x90` | `0x24` | `CONTROL2` | 6 |
| `0x30000` | `0xC000` | `SH_UNIFORMS` | 6 |
| `0x800` | `0x200` | `AHBDEC_CONTROL` | 6 |
| `0x14338` | `0x50CE` | `BLT_CLUSTER_CONTROL` | 6 |
| `0x140F4` | `0x503D` | `BLT_SRC_BUFFER_END_ADDRESS` | 6 |
| `0x14334` | `0x50CD` | `BLT_DST_BUFFER_END_ADDRESS` | 6 |
| `0x14A4` | `0x529` | `PE_CONFIG` | 6 |
| `0xA00` | `0x280` | `AHBDEC_WRITE_EX_CONFIG` | 6 |
| `0x18818` | `0x6206` | `DEC400_INTR_ACKNOWLEDGE` | 6 |
| `0x820` | `0x208` | `AHBDEC_INTR_ACKNOWLEDGE_EX2` | 6 |
| `0x678` | `0x19E` | `FE_INDEX_MIN` | 5 |
| `0x960` | `0x258` | `TW_INFO_GLOBAL_SCALE_Z` | 5 |
| `0x818` | `0x206` | `AHBDEC_INTR_ACKNOWLEDGE` | 5 |
