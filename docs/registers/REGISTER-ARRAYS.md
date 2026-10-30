# 多实例寄存器文件与未映射寄存器分析

本表由 `docs/tools/generate_register_arrays.py` 生成。
配合 [REGISTERS.md](REGISTERS.md)（驱动寄存器号映射总表）与
[REGISTERS-FIELDS.md](REGISTERS-FIELDS.md)（字段位域）阅读。

驱动写入寄存器的编号有两种来源：

1. **字面量**（如 `0x530`）—— 可直接映射到厂商寄存器名，见 REGISTERS.md。
2. **基址 + 索引**（如 `0x5240 + (render_num - 1)`）—— 属于"多实例寄存器文件"，
   厂商头文件只定义了第 0 个实例的地址，其余实例靠地址递增访问。本节记录这类结构。

驱动命令流中出现 12 个非字面量键（含 `<STALL>`、`<computed:reg>` 等占位符），
分类如下：

| 键 | 出现次数 | 位置 |
|---|---:|---|
| `<STALL>` | 8 | include/mwv207_cmd.h, mwv207_cmd.c, mwv207_compute.c, mwv207_draw.c:114,167,184,180,281,820 |
| `<computed:(0x5240 + (ctx->info.pixel_record[i] - 1))>` | 1 | mwv207_draw.c:963 |
| `<computed:(0x5240 + (render_num - 1))>` | 1 | mwv207_draw.c:959 |
| `<computed:base>` | 2 | mwv207_vertex.c:442,561 |
| `<computed:reg>` | 6 | include/mwv207_cmd.h, mwv207_cmd.c:231,250,92,98,104,112 |
| `<computed:sampler_regs[config_id].sampler_anisval + index>` | 2 | mwv207_sampler.c:575,595 |
| `<computed:sampler_regs[config_id].sampler_ctrl0 + index>` | 2 | mwv207_sampler.c:567,587 |
| `<computed:sampler_regs[config_id].sampler_ctrl1 + index>` | 2 | mwv207_sampler.c:569,589 |
| `<computed:sampler_regs[config_id].sampler_lod_bias + index>` | 2 | mwv207_sampler.c:573,593 |
| `<computed:sampler_regs[config_id].sampler_lod_config + index>` | 2 | mwv207_sampler.c:571,591 |
| `<computed:sampler_regs[config_id].tex_command>` | 2 | mwv207_sampler.c:616,621 |
| `<computed:sampler_regs[config_id].tex_control + index>` | 2 | mwv207_sampler.c:580,600 |

## 1. PE 渲染目标寄存器文件（8 个 RT 槽）

`.640` 段：驱动按"基址 + RT 序号"写入。厂商头文件把第 0 个实例命名为
`MWV207REG_PE_RENDER_TARGET_*`，第 N 个实例即 `基址 + N`。

证据（`mwv207_draw.c`）：

```c
// mwv207_draw.c:959   render_num 从 1 开始
MWV207_EMIT(cmd_buf, (0x5240 + (render_num - 1)),
            surface->peInfo.pixel_config_ex | ctx->blend_state->blend_configex[i]);
// mwv207_draw.c:652   RT0 单独处理，其余走基址+索引
if (i == 0) MWV207_EMIT(cmd_buf, 0x508, ctx->zsa->alpha_config);
else        MWV207_EMIT(cmd_buf, 0x5250 + (i - 1), ctx->zsa->alpha_config);
```

| 驱动基址 | 厂商寄存器（第 0 实例） | 地址 | 驱动用途 | 写入点 |
|---|---|---|---|---|
| `0x5200` | MWV207REG_PE_RENDER_TARGET_ADDRESS_EX | `0x14800` | 颜色缓冲起始地址 | mwv207_draw.c:959/963/715/717/652/776/777 |
| `0x5240` | MWV207REG_PE_RENDER_TARGET_CONFIG_EX | `0x14900` | pixel_config_ex（像素格式/α 等，与 blend_configex 或运算） | mwv207_draw.c:959/963/715/717/652/776/777 |
| `0x5248` | MWV207REG_PE_RENDER_TARGET_EXTRA_CONFIG_EX | `0x14920` | colormask 与 dst_read 位域 | mwv207_draw.c:959/963/715/717/652/776/777 |
| `0x5250` | MWV207REG_PE_RENDER_TARGET_ALPHA_TEST | `0x14940` | α 测试配置（zsa->alpha_config） | mwv207_draw.c:959/963/715/717/652/776/777 |
| `0x5258` | MWV207REG_PE_RENDER_TARGET_ALPHA_CONTROL | `0x14960` | blend_config（RT 级混合方程） | mwv207_draw.c:959/963/715/717/652/776/777 |
| `0x5260` | MWV207REG_PE_RENDER_TARGET_BLEND_COLOR_LOW | `0x14980` | 混合常量低 32 位 | mwv207_draw.c:959/963/715/717/652/776/777 |
| `0x5268` | MWV207REG_PE_RENDER_TARGET_BLEND_COLOR_HIGH | `0x149A0` | 混合常量高 32 位 | mwv207_draw.c:959/963/715/717/652/776/777 |
| `0x5270` | MWV207REG_PE_RENDER_TARGET_END_ADDRES_EX | `0x149C0` | 颜色缓冲结束地址 | mwv207_draw.c:959/963/715/717/652/776/777 |
| `0x5278` | MWV207REG_PE_RENDER_TARGET_FORMAT_EX | `0x149E0` | 像素格式（_EX 变体） | mwv207_draw.c:959/963/715/717/652/776/777 |

各数组的槽位数可从"与下一个具名寄存器的地址间隔"推断，但厂商头文件未给出明确
数组长度，**槽位上限为未确认**；本硬件 `PIPE_MAX_COLOR_BUFS = 4`，驱动实际只用到
槽 0–3。

### 同一地址空间的另两个变体（驱动未使用）

| 族 | 基址 | 第一个寄存器 |
|---|---|---|
| 非 EX（单 RT） | `0x0532` | MWV207REG_PE_RENDER_TARGET_FORMAT |
| `_EX`（本节主用） | `0x5200` | MWV207REG_PE_RENDER_TARGET_ADDRESS_EX |
| `_EX2` | `0x5F00` | MWV207REG_PE_RENDER_TARGET_ADDRESS_EX2 |

结论：同一硬件里存在 3 套渲染目标寄存器编码，驱动统一使用 `_EX` 族；
排布原因未确认（推断与多引擎/多客户端有关）。

## 2. 纹理单元寄存器文件（2 个纹理单元）

`.640` 段：`mwv207_sampler.c:78-90` 的 `static const struct mwv207_sampler_desc sampler_regs[]`
给出两个纹理单元各自的 8 个子寄存器基址；写入时再加上采样器槽号 `index`。

`struct mwv207_sampler_desc`（`include/mwv207_surface.h:76-85`）字段顺序：
`sampler_ctrl0`、`sampler_ctrl1`、`sampler_lod_config`、`sampler_lod_bias`、
`sampler_anisval`、`tex_desc_address`、`tex_control`、`tex_command`。

| 单元 | 字段 | 驱动基址 | 厂商寄存器 | 地址 |
|---|---|---|---|---|
| GPIPE（VS/GS/TCS/TES） | `sampler_ctrl0` | `0x5800` | MWV207REG_TX_GPIPE_SAMPLER_CTRL0 | `0x16000` |
| GPIPE（VS/GS/TCS/TES） | `sampler_ctrl1` | `0x5880` | MWV207REG_TX_GPIPE_SAMPLER_CTRL1 | `0x16200` |
| GPIPE（VS/GS/TCS/TES） | `sampler_lod_config` | `0x5900` | MWV207REG_TX_GPIPE_SAMPLER_LOD_MAX_MIN | `0x16400` |
| GPIPE（VS/GS/TCS/TES） | `sampler_lod_bias` | `0x5980` | MWV207REG_TX_GPIPE_SAMPLER_LOD_BIAS | `0x16600` |
| GPIPE（VS/GS/TCS/TES） | `sampler_anisval` | `0x5A00` | MWV207REG_TX_GPIPE_SAMPLER_ANISO_CTRL | `0x16800` |
| GPIPE（VS/GS/TCS/TES） | `tex_desc_address` | `0x5600` | MWV207REG_TX_GPIPE_TEXTURE_DESCRIPTOR_ADDR | `0x15800` |
| GPIPE（VS/GS/TCS/TES） | `tex_control` | `0x5680` | MWV207REG_TX_GPIPE_TEXTURE_CTRL | `0x15A00` |
| GPIPE（VS/GS/TCS/TES） | `tex_command` | `0x5311` | MWV207REG_TX_GPIPE_COMMAND | `0x14C44` |
| PIXEL（FS） | `sampler_ctrl0` | `0x5B00` | MWV207REG_TX_PIXEL_SAMPLER_CTRL0 | `0x16C00` |
| PIXEL（FS） | `sampler_ctrl1` | `0x5B80` | MWV207REG_TX_PIXEL_SAMPLER_CTRL1 | `0x16E00` |
| PIXEL（FS） | `sampler_lod_config` | `0x5C00` | MWV207REG_TX_PIXEL_SAMPLER_LOD_MAX_MIN | `0x17000` |
| PIXEL（FS） | `sampler_lod_bias` | `0x5C80` | MWV207REG_TX_PIXEL_SAMPLER_LOD_BIAS | `0x17200` |
| PIXEL（FS） | `sampler_anisval` | `0x5D00` | MWV207REG_TX_PIXEL_SAMPLER_ANISO_CTRL | `0x17400` |
| PIXEL（FS） | `tex_desc_address` | `0x5700` | MWV207REG_TX_PIXEL_TEXTURE_DESCRIPTOR_ADDR | `0x15C00` |
| PIXEL（FS） | `tex_control` | `0x5780` | MWV207REG_TX_PIXEL_TEXTURE_CTRL | `0x15E00` |
| PIXEL（FS） | `tex_command` | `0x5312` | MWV207REG_TX_PIXEL_COMMAND | `0x14C48` |

驱动写入时统一体现在 `sampler_regs[config_id].<field> + index`
（`mwv207_sampler.c:567-590`），`index` 即采样器槽号。

注意纹理描述符表通过 **DAT**（而非 EMIT）写入：
`mwv207_add_dat(ctx, &cmd_buf, sampler_regs[config_id].tex_desc_address + index, &tex_desc->dat)`
（`mwv207_sampler.c:578`）。

## 3. 着色器 uniform / 立即数区

`mwv207_vertex.c:305-344` `mwv207_config_uniform()` 按 shader stage 选择基址，
随后每 4 个字用一次 `MWV207_EMITS(cmd_buf, base, 4, unifrom)` 写入：

| stage | 驱动基址 | 厂商寄存器 | 地址 |
|---|---|---|---|
| VERTEX / TESS_CTRL / TESS_EVAL / GEOMETRY | `0xD000` | MWV207REG_GPIPE_UNIFORMS | `0x34000` |
| FRAGMENT | `0xDC00` | —（未定义，未确认） | `0x37000` |
| COMPUTE | `0xD800` | MWV207REG_PIXEL_UNIFORMS | `0x36000` |

另有每 stage 的 uniform 基址/锁寄存器（驱动在 `mwv207_vertex.c:350-360` 写入 TCS/TES 的）：

| 寄存器 | 地址 | reg_id | 用途 |
|---|---|---|---|
| MWV207REG_GS_UNIFORM_BASE | `0x0114C` | `0x0453` | uniform 基址 / 锁 |
| MWV207REG_GS_UNIFORM_LOCK | `0x01150` | `0x0454` | uniform 基址 / 锁 |
| MWV207REG_TCS_UNIFORM_BASE | `0x14A44` | `0x5291` | uniform 基址 / 锁 |
| MWV207REG_TCS_UNIFORM_LOCK | `0x14A48` | `0x5292` | uniform 基址 / 锁 |
| MWV207REG_TES_UNIFORM_BASE | `0x14B24` | `0x52C9` | uniform 基址 / 锁 |
| MWV207REG_TES_UNIFORM_LOCK | `0x14B28` | `0x52CA` | uniform 基址 / 锁 |

## 4. 未映射寄存器分析

驱动写入了 **49** 个在厂商发布头文件中没有 `_Address` 定义的寄存器号
（占 219 个键的 19%）。这不是映射规则的问题，而是发布出来的寄存器定义不完整。

| reg_id | 地址 | 使用次数 | 相邻的已定义寄存器（地址） | 归属推断 |
|---|---|---:|---|---|
| `0x0191` | `0x00644` | 1 | 前 MWV207REG_DEBUG_COUNTER_VALUE (0x00610) / 后 MWV207REG_FEDE_TIMEOUT (0x00670) | DEBUG/FEDE（未确认） |
| `0x0192` | `0x00648` | 2 | 前 MWV207REG_DEBUG_COUNTER_VALUE (0x00610) / 后 MWV207REG_FEDE_TIMEOUT (0x00670) | DEBUG/FEDE（未确认） |
| `0x0281` | `0x00A04` | 1 | 前 MWV207REG_AHBDEC_WRITE_EX_CONFIG (0x00A00) / 后 MWV207REG_PA_LINE_ADJUST_SUB (0x00A38) | AHBDEC/PA（未确认） |
| `0x0282` | `0x00A08` | 1 | 前 MWV207REG_AHBDEC_WRITE_EX_CONFIG (0x00A00) / 后 MWV207REG_PA_LINE_ADJUST_SUB (0x00A38) | AHBDEC/PA（未确认） |
| `0x0283` | `0x00A0C` | 1 | 前 MWV207REG_AHBDEC_WRITE_EX_CONFIG (0x00A00) / 后 MWV207REG_PA_LINE_ADJUST_SUB (0x00A38) | AHBDEC/PA（未确认） |
| `0x0284` | `0x00A10` | 1 | 前 MWV207REG_AHBDEC_WRITE_EX_CONFIG (0x00A00) / 后 MWV207REG_PA_LINE_ADJUST_SUB (0x00A38) | AHBDEC/PA（未确认） |
| `0x0285` | `0x00A14` | 1 | 前 MWV207REG_AHBDEC_WRITE_EX_CONFIG (0x00A00) / 后 MWV207REG_PA_LINE_ADJUST_SUB (0x00A38) | AHBDEC/PA（未确认） |
| `0x0286` | `0x00A18` | 1 | 前 MWV207REG_AHBDEC_WRITE_EX_CONFIG (0x00A00) / 后 MWV207REG_PA_LINE_ADJUST_SUB (0x00A38) | AHBDEC/PA（未确认） |
| `0x0287` | `0x00A1C` | 1 | 前 MWV207REG_AHBDEC_WRITE_EX_CONFIG (0x00A00) / 后 MWV207REG_PA_LINE_ADJUST_SUB (0x00A38) | AHBDEC/PA（未确认） |
| `0x028A` | `0x00A28` | 1 | 前 MWV207REG_AHBDEC_WRITE_EX_CONFIG (0x00A00) / 后 MWV207REG_PA_LINE_ADJUST_SUB (0x00A38) | AHBDEC/PA（未确认） |
| `0x028B` | `0x00A2C` | 1 | 前 MWV207REG_AHBDEC_WRITE_EX_CONFIG (0x00A00) / 后 MWV207REG_PA_LINE_ADJUST_SUB (0x00A38) | AHBDEC/PA（未确认） |
| `0x028C` | `0x00A30` | 2 | 前 MWV207REG_AHBDEC_WRITE_EX_CONFIG (0x00A00) / 后 MWV207REG_PA_LINE_ADJUST_SUB (0x00A38) | AHBDEC/PA（未确认） |
| `0x028D` | `0x00A34` | 2 | 前 MWV207REG_AHBDEC_WRITE_EX_CONFIG (0x00A00) / 后 MWV207REG_PA_LINE_ADJUST_SUB (0x00A38) | AHBDEC/PA（未确认） |
| `0x0301` | `0x00C04` | 1 | 前 MWV207REG_AHBDEC_READ_BUFFER_END_EX (0x00C00) / 后 MWV207REG_SE_AREA_THRESHOLD (0x00C1C) | AHBDEC/SE（未确认） |
| `0x0302` | `0x00C08` | 1 | 前 MWV207REG_AHBDEC_READ_BUFFER_END_EX (0x00C00) / 后 MWV207REG_SE_AREA_THRESHOLD (0x00C1C) | AHBDEC/SE（未确认） |
| `0x0303` | `0x00C0C` | 1 | 前 MWV207REG_AHBDEC_READ_BUFFER_END_EX (0x00C00) / 后 MWV207REG_SE_AREA_THRESHOLD (0x00C1C) | AHBDEC/SE（未确认） |
| `0x0304` | `0x00C10` | 1 | 前 MWV207REG_AHBDEC_READ_BUFFER_END_EX (0x00C00) / 后 MWV207REG_SE_AREA_THRESHOLD (0x00C1C) | AHBDEC/SE（未确认） |
| `0x0305` | `0x00C14` | 1 | 前 MWV207REG_AHBDEC_READ_BUFFER_END_EX (0x00C00) / 后 MWV207REG_SE_AREA_THRESHOLD (0x00C1C) | AHBDEC/SE（未确认） |
| `0x0306` | `0x00C18` | 1 | 前 MWV207REG_AHBDEC_READ_BUFFER_END_EX (0x00C00) / 后 MWV207REG_SE_AREA_THRESHOLD (0x00C1C) | AHBDEC/SE（未确认） |
| `0x0401` | `0x01004` | 1 | 前 MWV207REG_AHBDEC_WRITE_FLUSH_CACHE_EX (0x01000) / 后 MWV207REG_PS_PROGRAM (0x0101C) | AHBDEC/PS（未确认） |
| `0x0402` | `0x01008` | 2 | 前 MWV207REG_AHBDEC_WRITE_FLUSH_CACHE_EX (0x01000) / 后 MWV207REG_PS_PROGRAM (0x0101C) | AHBDEC/PS（未确认） |
| `0x0403` | `0x0100C` | 2 | 前 MWV207REG_AHBDEC_WRITE_FLUSH_CACHE_EX (0x01000) / 后 MWV207REG_PS_PROGRAM (0x0101C) | AHBDEC/PS（未确认） |
| `0x0404` | `0x01010` | 3 | 前 MWV207REG_AHBDEC_WRITE_FLUSH_CACHE_EX (0x01000) / 后 MWV207REG_PS_PROGRAM (0x0101C) | AHBDEC/PS（未确认） |
| `0x0500` | `0x01400` | 3 | 前 MWV207REG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_B (0x013FC) / 后 MWV207REG_PE_HIERARCHICAL_CONTROL (0x01454) | PE 区（0x1400–0x14FF 有明显未定义空洞）（未确认） |
| `0x0501` | `0x01404` | 1 | 前 MWV207REG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_B (0x013FC) / 后 MWV207REG_PE_HIERARCHICAL_CONTROL (0x01454) | PE 区（0x1400–0x14FF 有明显未定义空洞）（未确认） |
| `0x0502` | `0x01408` | 1 | 前 MWV207REG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_B (0x013FC) / 后 MWV207REG_PE_HIERARCHICAL_CONTROL (0x01454) | PE 区（0x1400–0x14FF 有明显未定义空洞）（未确认） |
| `0x0503` | `0x0140C` | 2 | 前 MWV207REG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_B (0x013FC) / 后 MWV207REG_PE_HIERARCHICAL_CONTROL (0x01454) | PE 区（0x1400–0x14FF 有明显未定义空洞）（未确认） |
| `0x0505` | `0x01414` | 1 | 前 MWV207REG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_B (0x013FC) / 后 MWV207REG_PE_HIERARCHICAL_CONTROL (0x01454) | PE 区（0x1400–0x14FF 有明显未定义空洞）（未确认） |
| `0x0506` | `0x01418` | 1 | 前 MWV207REG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_B (0x013FC) / 后 MWV207REG_PE_HIERARCHICAL_CONTROL (0x01454) | PE 区（0x1400–0x14FF 有明显未定义空洞）（未确认） |
| `0x0507` | `0x0141C` | 1 | 前 MWV207REG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_B (0x013FC) / 后 MWV207REG_PE_HIERARCHICAL_CONTROL (0x01454) | PE 区（0x1400–0x14FF 有明显未定义空洞）（未确认） |
| `0x0508` | `0x01420` | 1 | 前 MWV207REG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_B (0x013FC) / 后 MWV207REG_PE_HIERARCHICAL_CONTROL (0x01454) | PE 区（0x1400–0x14FF 有明显未定义空洞）（未确认） |
| `0x0509` | `0x01424` | 1 | 前 MWV207REG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_B (0x013FC) / 后 MWV207REG_PE_HIERARCHICAL_CONTROL (0x01454) | PE 区（0x1400–0x14FF 有明显未定义空洞）（未确认） |
| `0x050A` | `0x01428` | 1 | 前 MWV207REG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_B (0x013FC) / 后 MWV207REG_PE_HIERARCHICAL_CONTROL (0x01454) | PE 区（0x1400–0x14FF 有明显未定义空洞）（未确认） |
| `0x050B` | `0x0142C` | 3 | 前 MWV207REG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_B (0x013FC) / 后 MWV207REG_PE_HIERARCHICAL_CONTROL (0x01454) | PE 区（0x1400–0x14FF 有明显未定义空洞）（未确认） |
| `0x050D` | `0x01434` | 1 | 前 MWV207REG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_B (0x013FC) / 后 MWV207REG_PE_HIERARCHICAL_CONTROL (0x01454) | PE 区（0x1400–0x14FF 有明显未定义空洞）（未确认） |
| `0x0594` | `0x01650` | 2 | 前 MWV207REG_HIST_BASE_ADDRESS2 (0x015F4) / 后 MWV207REG_RS_CONFIG (0x016A0) | HIST/RS（未确认） |
| `0x0595` | `0x01654` | 2 | 前 MWV207REG_HIST_BASE_ADDRESS2 (0x015F4) / 后 MWV207REG_RS_CONFIG (0x016A0) | HIST/RS（未确认） |
| `0x0596` | `0x01658` | 1 | 前 MWV207REG_HIST_BASE_ADDRESS2 (0x015F4) / 后 MWV207REG_RS_CONFIG (0x016A0) | HIST/RS（未确认） |
| `0x0598` | `0x01660` | 1 | 前 MWV207REG_HIST_BASE_ADDRESS2 (0x015F4) / 后 MWV207REG_RS_CONFIG (0x016A0) | HIST/RS（未确认） |
| `0x0599` | `0x01664` | 1 | 前 MWV207REG_HIST_BASE_ADDRESS2 (0x015F4) / 后 MWV207REG_RS_CONFIG (0x016A0) | HIST/RS（未确认） |
| `0x059B` | `0x0166C` | 1 | 前 MWV207REG_HIST_BASE_ADDRESS2 (0x015F4) / 后 MWV207REG_RS_CONFIG (0x016A0) | HIST/RS（未确认） |
| `0x06D0` | `0x01B40` | 1 | 前 MWV207REG_MRT_COLOR_CLEAR_VALUE128 (0x01B20) / 后 MWV207REG_MC_SHADER_CLEAR96 (0x01B50) | MRT/MC（未确认） |
| `0x0E00` | `0x03800` | 1 | 前 MWV207REG_AHBXDMA4_DBG2 (0x03340) / 后 MWV207REG_MULTI_SAMPLE_CONFIG (0x03818) | CMD/SYNC 区（0x3800 段）（未确认） |
| `0x0E02` | `0x03808` | 8 | 前 MWV207REG_AHBXDMA4_DBG2 (0x03340) / 后 MWV207REG_MULTI_SAMPLE_CONFIG (0x03818) | CMD/SYNC 区（0x3800 段）（未确认） |
| `0x0E03` | `0x0380C` | 14 | 前 MWV207REG_AHBXDMA4_DBG2 (0x03340) / 后 MWV207REG_MULTI_SAMPLE_CONFIG (0x03818) | CMD/SYNC 区（0x3800 段）（未确认） |
| `0x0E05` | `0x03814` | 1 | 前 MWV207REG_AHBXDMA4_DBG2 (0x03340) / 后 MWV207REG_MULTI_SAMPLE_CONFIG (0x03818) | CMD/SYNC 区（0x3800 段）（未确认） |
| `0x0E13` | `0x0384C` | 1 | 前 MWV207REG_STORE_COUNTER (0x03848) / 后 MWV207REG_CONTEXT (0x03850) | CMD/SYNC 区（0x3800 段）（未确认） |
| `0x0F00` | `0x03C00` | 1 | 前 MWV207REG_MULTI_CHIP_SET_MAPPING_EX (0x03A10) / 后 MWV207REG_PS_SHADER_CODE (0x08000) | MULTI/PS（未确认） |
| `0xFFFF` | `0x3FFFC` | 2 | 前 MWV207REG_PIXEL_UNIFORMS (0x36000) | uniform/立即数区（按 stage 分段的连续地址）（未确认） |

### 说明

- `0xFFFF`（地址 `0x3FFFC`）不是寄存器：它是命令流里的终止/哨兵值。
- 未映射寄存器集中在 `0x01400–0x014FF`（PE 区）、`0x00E00–0x00EFF`、
  `0x03800–0x038FF`；这些地址区间在厂商发布头文件里恰好是空的，
  说明发布版**有意或无意地省略**了这批定义。
- 因此这 49 个寄存器**无法**从现有资料确定名称与字段，任何补全都属推断。

