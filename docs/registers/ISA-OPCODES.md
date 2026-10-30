# 着色器 ISA 指令操作码

> 权威依据：厂商 KMD 头文件中的 `MWV207REG_SH_*_OPCODE_*` 字段与取值宏。
> 交叉验证：驱动 `nir/mwv207_disasm.c` 的扩展操作码表、`nir/mwv207_emit.c` 的使用点。
> 生成：`../tools/generate_isa_opcodes.py`（可重复执行）。

## 0. 关键结论

驱动反汇编表把扩展指令编码在 **`0x7f + n`** 区段。逐条比对显示：
**`n` 就是厂商 `SH_EXTENDED_OPCODE`（U08，bits 0–7）的取值**，例如
`n = 0x4 → LODQ`、`0xF → HALFMIX`、`0xB → BIT_FINDLSB`、`0x13 → MOVBIX`，
与厂商命名**逐项一致**。

因此：**ISA 扩展操作码具备厂商依据**；此前 `04-NIR-COMPILER.md` §12 中
"101 条 opcode 数值无厂商文档依据"的限制，对**扩展操作码部分应予修正**
（基础 ALU 操作码仍未在厂商头文件中找到对应枚举）。

## 1. 厂商操作码字段清单

| 字段 | 位宽 | 位段 | 取值数 |
|---|---:|---|---:|
| `MWV207REG_SH_EXTENDED_OPCODE` | U08 | 0–7 | 16 |
| `MWV207REG_SH_CMPLX_OPCODE` | U04 | 0–3 | 3 |
| `MWV207REG_SH_IMG_ATOM_OPCODE` | U03 | 0–2 | 8 |
| `MWV207REG_SH_VISION_OPCODE` | U06 | 0–5 | 32 |

## 2.1 扩展操作码 `SH_EXTENDED_OPCODE`

定义：`MWV207REG_SH_EXTENDED_OPCODE`，U08，bits 0–7，共 16 个命名取值。

| 值 | 厂商名 | 驱动反汇编（`0x7f + n`）| 驱动是否实现 |
|---:|---|---|---|
| `0x01` | `EMIT` | `0x7f + 0x01` = "EMIT"（`mwv207_disasm.c:1176`）| ✅ |
| `0x02` | `RESTART` | `0x7f + 0x02` = "RESTART"（`mwv207_disasm.c:1177`）| ✅ |
| `0x03` | `FLUSH` | `0x7f + 0x03` = "FLUSH"（`mwv207_disasm.c:1180`）| ✅ |
| `0x04` | `LODQ` | `0x7f + 0x04` = "LODQ"（`mwv207_disasm.c:1184`）| ✅ |
| `0x0B` | `BIT_FINDLSB` | `0x7f + 0x0B` = "BIT_FINDLSB"（`mwv207_disasm.c:1181`）| ✅ |
| `0x0C` | `BIT_FINDMSB` | `0x7f + 0x0C` = "BIT_FINDMSB"（`mwv207_disasm.c:1182`）| ✅ |
| `0x0D` | `TEXLD_FETCH_MS` | `0x7f + 0x0D` = "TEXLD_FETCH_MS_S"（`mwv207_disasm.c:1185`）| ✅ |
| `0x0D` | `TEXLD_FETCH_MS_S` | `0x7f + 0x0D` = "TEXLD_FETCH_MS_S"（`mwv207_disasm.c:1185`）| ✅ |
| `0x0E` | `TEXLD_FETCH_MS_U` | `0x7f + 0x0E` = "TEXLD_FETCH_MS_U"（`mwv207_disasm.c:1186`）| ✅ |
| `0x0F` | `HALFMIX` | `0x7f + 0x0F` = "HALFMIX"（`mwv207_disasm.c:1178`）| ✅ |
| `0x10` | `LDEXP` | `0x7f + 0x10` = "LDEXP"（`mwv207_disasm.c:1179`）| ✅ |
| `0x11` | `WAIT_GROUP_EVENT` | — | ❌ 未实现 |
| `0x12` | `TD_OP` | — | ❌ 未实现 |
| `0x13` | `MOVBIX` | `0x7f + 0x13` = "MOVBIX"（`mwv207_disasm.c:1187`）| ✅ |
| `0x14` | `TEX_INVALIDATE` | — | ❌ 未实现 |
| `0x15` | `PRE_NORM` | — | ❌ 未实现 |

## 2.2 复式操作码 `SH_CMPLX_OPCODE`（三操作数 MAD）

定义：`MWV207REG_SH_CMPLX_OPCODE`，U04，bits 0–3，共 3 个命名取值。

| 值 | 厂商名 | 说明 |
|---:|---|---|
| `0x00` | `MUL` | 乘法 |
| `0x01` | `MAD` | 乘加（三操作数） |
| `0x02` | `ADD` | 加法 |

## 2.3 图像原子操作码 `SH_IMG_ATOM_OPCODE`

定义：`MWV207REG_SH_IMG_ATOM_OPCODE`，U03，bits 0–2，共 8 个命名取值。

| 值 | 厂商名 |
|---:|---|
| `0x00` | `IMG_OP_CODE_ATOM_ADD` |
| `0x01` | `IMG_OP_CODE_ATOM_XCHG` |
| `0x02` | `IMG_OP_CODE_ATOM_CMP_XCHG` |
| `0x03` | `IMG_OP_CODE_ATOM_MIN` |
| `0x04` | `IMG_OP_CODE_ATOM_MAX` |
| `0x05` | `IMG_OP_CODE_ATOM_OR` |
| `0x06` | `IMG_OP_CODE_ATOM_AND` |
| `0x07` | `IMG_OP_CODE_ATOM_XOR` |

## 2.4 视觉操作码 `SH_VISION_OPCODE`（NN/视觉单元）

定义：`MWV207REG_SH_VISION_OPCODE`，U06，bits 0–5，共 32 个命名取值。

| 值 | 厂商名 |
|---:|---|
| `0x01` | `ABS_DIFF` |
| `0x02` | `IADD` |
| `0x03` | `IACC_SQ` |
| `0x04` | `LERP` |
| `0x05` | `FILTER` |
| `0x06` | `MAG_PHASE` |
| `0x07` | `MUL_SHIFT` |
| `0x08` | `DP16X1` |
| `0x09` | `DP8X2` |
| `0x0A` | `DP4X4` |
| `0x0B` | `DP2X8` |
| `0x0C` | `CLAMP` |
| `0x0D` | `BI_LINEAR` |
| `0x0E` | `SELECT_ADD` |
| `0x0F` | `ATOMIC_ADD` |
| `0x10` | `BIT_EXTRACT` |
| `0x11` | `BIT_REPLACE` |
| `0x12` | `DP32X1` |
| `0x13` | `DP16X2` |
| `0x14` | `DP8X4` |
| `0x15` | `DP4X8` |
| `0x16` | `DP2X16` |
| `0x17` | `INDEX_ADD` |
| `0x18` | `VERT_MIN3` |
| `0x19` | `VERT_MAX3` |
| `0x1A` | `VERT_MED3` |
| `0x1B` | `HORZ_MIN3` |
| `0x1C` | `HORZ_MAX3` |
| `0x1D` | `HORZ_MED3` |
| `0x1E` | `GATHER` |
| `0x1F` | `SCATTER` |
| `0x20` | `ATOMIC_S` |

## 3. 驱动侧使用点（`mwv207_emit.c`）

| 位置 | `ext_opcode` | 对应厂商名 |
|---|---:|---|
| `nir/mwv207_emit.c:472` | `0x4` | `LODQ` |
| `nir/mwv207_emit.c:476` | `0xE` | `TEXLD_FETCH_MS_U` |
| `nir/mwv207_emit.c:1603` | `0x1` | `EMIT` |
| `nir/mwv207_emit.c:1622` | `0x2` | `RESTART` |

## 4. 缺口

厂商定义但驱动反汇编表中**没有**条目的扩展操作码（即驱动未实现或未命名为可解码指令）：

| 值 | 厂商名 | 备注 |
|---:|---|---|
| `0x11` | `WAIT_GROUP_EVENT` | 驱动无对应解码条目 |
| `0x12` | `TD_OP` | 驱动无对应解码条目 |
| `0x14` | `TEX_INVALIDATE` | 驱动无对应解码条目 |
| `0x15` | `PRE_NORM` | 驱动无对应解码条目 |

> 注：`TEXLD_FETCH_MS` 与 `TEXLD_FETCH_MS_S` 同为 `0x0D`，是厂商侧的别名；
> 驱动表里用的是 `TEXLD_FETCH_MS_S`。

## 5. 反向未解释项

驱动反汇编表 `0x7f + n` 区段中存在、而厂商 `SH_EXTENDED_OPCODE` **未命名**的 `n`：

（无）

