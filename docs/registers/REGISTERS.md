# MWV207 / JM9100 驱动寄存器参考

本文件由 `docs/tools/build_register_db.py` + `docs/tools/generate_register_docs.py`
从厂商 KMD 寄存器头文件自动生成，并与开源 UMD 驱动的实际写入点交叉关联。

## 1. 方法与结论

### 1.1 寄存器地址映射（关键结论）

开源驱动通过 `MWV207_EMIT(cmd, reg, value)` 把 16 位寄存器号写进命令流：

```c
/* include/mwv207_cmd.h */
#define MWV207_EMIT(cmd, reg, value)  { *(cmd)++ = (0x08010000 + reg); *(cmd)++ = value; }
```

命令字编码：bits[31:27] 操作码，bits[26:16] 字数（单字为 1），bits[15:0] 寄存器号。

**实测确认的映射关系：`驱动寄存器号 = 厂商寄存器 _Address >> 2`**

| 驱动发出 | 左移 2 位 | 厂商寄存器 | 语义 |
|---|---|---|---|
| `0x530` | `0x014C0` | `MWV207REG_PE_ALPHA_CONFIG` | 像素引擎混合 alpha 配置（驱动写入 alpha_config）|
| `0x529` | `0x014A4` | `MWV207REG_PE_CONFIG` | 像素引擎配置 |
| `0x50CE` | `0x14338` | `MWV207REG_BLT_CLUSTER_CONTROL` | BLT 簇控制（驱动写入 cluster_mask）|
| `0x418` | `0x01060` | `MWV207REG_PS_SAMPLE_DITHER_TABLE` | 像素着色器采样抖动表 |
| `0x5C8` | `0x01720` | `MWV207REG_MC_TEXTURE_CONFIG` | 显存控制器纹理配置 |

注意：开源驱动自带的 `include/hw/mwv207reg_{3d,texture,shader}.h` 在发行版中是
**空壳**（只有 include guard），即寄存器字段定义被剥离；`mwv207reg_common.h` 只保留
`DISABLE`/`ENABLE`。驱动因此通篇使用裸十六进制常量。本文件即用于补齐这层信息。

### 1.2 覆盖率

- 厂商头文件解析出 **1392 个寄存器**、**3954 个字段**（含位域与枚举值）。
- 驱动实际写 **219 个不同寄存器号**，其中 **170 个（78%）** 已映射到厂商寄存器。
- 剩余 **49 个**驱动寄存器号在已发行的厂商头文件中**没有地址定义**（见 §4）。

### 1.3 寄存器块分布（厂商库计 1392 个寄存器）

| 块前缀 | 寄存器数 | 说明 |
|---|---|---|
| `DE` | 113 | 2D 显示引擎 (Display Engine) |
| `BLT` | 90 | 2D 位块传输 (Blitter) |
| `DEC400` | 80 |  |
| `AHBDEC` | 79 | AHB 解码/访存 |
| `AHBX` | 60 |  |
| `TX` | 60 | 纹理单元 (Texture) |
| `PC` | 46 |  |
| `PS` | 46 | 像素着色器 (Pixel Shader) |
| `AHB` | 45 |  |
| `PE` | 43 | 像素引擎 (Pixel Engine) |
| `FE` | 42 | 前端 (Front End) |
| `VS` | 36 | 顶点着色器 (Vertex Shader) |
| `CE` | 28 |  |
| `TPC` | 27 |  |
| `TW` | 25 |  |
| `DEC` | 24 |  |
| `GS` | 20 | 几何着色器 (Geometry Shader) |
| `MC` | 18 | 显存控制器 (Memory Controller) |
| `RS` | 18 |  |
| `TCS` | 17 |  |
| `TES` | 17 |  |
| `DC` | 12 |  |
| `SH` | 12 | 着色器 (Shader core) |
| `MMUAHB` | 11 |  |

## 2. 驱动使用的寄存器 → 厂商寄存器 映射表

| 驱动号 | 地址 | 厂商寄存器 | 写入次数 | 主要使用文件 |
|---|---|---|---|---|
| `0x6b` | `0x001AC` | `MWV207REG_MMU_CONFIG` | 1 | `mwv207_cmd.c`(1) |
| `0x19d` | `0x00674` | `MWV207REG_FE_PRIMITIVE_RESTART` | 3 | `mwv207_draw.c`(3) |
| `0x1f0` | `0x007C0` | `MWV207REG_FE_PATCH_CONTROL` | 2 | `mwv207_compute.c`(1), `mwv207_draw.c`(1) |
| `0x1f1` | `0x007C4` | `MWV207REG_FE_ID` | 3 | `mwv207_shader.c`(3) |
| `0x1f2` | `0x007C8` | `MWV207REG_FE_RESET_ATTRIBUTES` | 2 | `mwv207_cmd.c`(1), `mwv207_draw.c`(1) |
| `0x1f6` | `0x007D8` | `MWV207REG_FE_CONTROL` | 1 | `mwv207_cmd.c`(1) |
| `0x201` | `0x00804` | `MWV207REG_AHBDEC_CONTROL_EX` | 2 | `mwv207_compute.c`(1), `mwv207_shader.c`(1) |
| `0x202` | `0x00808` | `MWV207REG_AHBDEC_CONTROL_EX2` | 3 | `mwv207_shader.c`(3) |
| `0x203` | `0x0080C` | `MWV207REG_AHBDEC_INTR_ENBL` | 1 | `mwv207_shader.c`(1) |
| `0x20c` | `0x00830` | `MWV207REG_AHBDEC_TOTAL_READS_IN` | 2 | `mwv207_compute.c`(1), `mwv207_shader.c`(1) |
| `0x219` | `0x00864` | `MWV207REG_VERTEX_SHADER_CONST` | 1 | `mwv207_shader.c`(1) |
| `0x21a` | `0x00868` | `MWV207REG_SH_CACHE_CONTROL` | 6 | `mwv207_shader.c`(5), `mwv207_compute.c`(1) |
| `0x21c` | `0x00870` | `MWV207REG_VS_ATTRIBUTE` | 1 | `mwv207_shader.c`(1) |
| `0x21d` | `0x00874` | `MWV207REG_VS_START_PC` | 1 | `mwv207_shader.c`(1) |
| `0x21f` | `0x0087C` | `MWV207REG_PS_START_PC` | 2 | `mwv207_compute.c`(1), `mwv207_shader.c`(1) |
| `0x221` | `0x00884` | `MWV207REG_SH_SAMPLER_ARBITRATION` | 1 | `mwv207_cmd.c`(1) |
| `0x223` | `0x0088C` | `MWV207REG_VS_INSTRUCTION_PREFETCH` | 1 | `mwv207_shader.c`(1) |
| `0x228` | `0x008A0` | `MWV207REG_VS_THROTTLE` | 2 | `mwv207_compute.c`(1), `mwv207_shader.c`(1) |
| `0x22a` | `0x008A8` | `MWV207REG_VS_SAMPLER_BASE` | 1 | `mwv207_shader.c`(1) |
| `0x22c` | `0x008B0` | `MWV207REG_SH_ICACHE_INVALIDATE` | 1 | `mwv207_cmd.c`(1) |
| `0x22f` | `0x008BC` | `MWV207REG_VS_RELATIVE_END` | 1 | `mwv207_shader.c`(1) |
| `0x230` | `0x008C0` | `MWV207REG_VS_INPUT` | 1 | `mwv207_shader.c`(1) |
| `0x238` | `0x008E0` | `MWV207REG_VS_OUTPUT` | 1 | `mwv207_shader.c`(1) |
| `0x240` | `0x00900` | `MWV207REG_TW_CONFIG` | 1 | `mwv207_compute.c`(1) |
| `0x247` | `0x0091C` | `MWV207REG_TW_SHADER_INFO` | 1 | `mwv207_compute.c`(1) |
| `0x248` | `0x00920` | `MWV207REG_TW_TRIGGER` | 1 | `mwv207_compute.c`(1) |
| `0x249` | `0x00924` | `MWV207REG_TW_SHADER_INFO2` | 2 | `mwv207_compute.c`(2) |
| `0x24b` | `0x0092C` | `MWV207REG_TW_INFO_GLOBAL_OFFSET_X` | 1 | `mwv207_compute.c`(1) |
| `0x24d` | `0x00934` | `MWV207REG_TW_INFO_GLOBAL_OFFSET_Y` | 1 | `mwv207_compute.c`(1) |
| `0x24f` | `0x0093C` | `MWV207REG_TW_INFO_GLOBAL_OFFSET_Z` | 1 | `mwv207_compute.c`(1) |
| `0x250` | `0x00940` | `MWV207REG_TW_WORK_GROUP_COUNT_X` | 1 | `mwv207_compute.c`(1) |
| `0x251` | `0x00944` | `MWV207REG_TW_WORK_GROUP_COUNT_Y` | 1 | `mwv207_compute.c`(1) |
| `0x252` | `0x00948` | `MWV207REG_TW_WORK_GROUP_COUNT_Z` | 1 | `mwv207_compute.c`(1) |
| `0x253` | `0x0094C` | `MWV207REG_TW_WORKGROUP_SIZE_X` | 1 | `mwv207_compute.c`(1) |
| `0x254` | `0x00950` | `MWV207REG_TW_WORKGROUP_SIZE_Y` | 1 | `mwv207_compute.c`(1) |
| `0x255` | `0x00954` | `MWV207REG_TW_WORKGROUP_SIZE_Z` | 1 | `mwv207_compute.c`(1) |
| `0x280` | `0x00A00` | `MWV207REG_AHBDEC_WRITE_EX_CONFIG` | 1 | `mwv207_draw.c`(1) |
| `0x28e` | `0x00A38` | `MWV207REG_PA_LINE_ADJUST_SUB` | 1 | `mwv207_draw.c`(1) |
| `0x28f` | `0x00A3C` | `MWV207REG_PA_LINE_ADJUST_ADD` | 1 | `mwv207_draw.c`(1) |
| `0x2a0` | `0x00A80` | `MWV207REG_AHBDEC_READ_BUFFER_BASE` | 1 | `mwv207_draw.c`(1) |
| `0x2a1` | `0x00A84` | `MWV207REG_PA_MAX_POINT_SIZE` | 1 | `mwv207_draw.c`(1) |
| `0x2a2` | `0x00A88` | `MWV207REG_PA_CLIP_CONTROL` | 1 | `mwv207_draw.c`(1) |
| `0x2a3` | `0x00A8C` | `MWV207REG_PA_SMALL_W` | 1 | `mwv207_draw.c`(1) |
| `0x2a4` | `0x00A90` | `MWV207REG_PA_PACK` | 2 | `mwv207_compute.c`(1), `mwv207_shader.c`(1) |
| `0x2aa` | `0x00AA8` | `MWV207REG_PA_CONTROL` | 3 | `mwv207_shader.c`(2), `mwv207_compute.c`(1) |
| `0x300` | `0x00C00` | `MWV207REG_AHBDEC_READ_BUFFER_END_EX` | 1 | `mwv207_draw.c`(1) |
| `0x308` | `0x00C20` | `MWV207REG_SE_CLIP_COMPARE_RIGHT` | 1 | `mwv207_draw.c`(1) |
| `0x309` | `0x00C24` | `MWV207REG_SE_CLIP_COMPARE_BOTTOM` | 1 | `mwv207_draw.c`(1) |
| `0x380` | `0x00E00` | `MWV207REG_AHBDEC_WRITE_BUFFER_BASE_EX` | 2 | `mwv207_draw.c`(1), `mwv207_shader.c`(1) |
| `0x381` | `0x00E04` | `MWV207REG_MULTI_SAMPLE_JITTER` | 1 | `mwv207_draw.c`(1) |
| `0x382` | `0x00E08` | `MWV207REG_RA_CONTROL` | 2 | `mwv207_draw.c`(2) |
| `0x383` | `0x00E0C` | `MWV207REG_RA_MEDIUMP_MASK` | 1 | `mwv207_draw.c`(1) |
| `0x388` | `0x00E20` | `MWV207REG_RA_CONTROL_HZ` | 1 | `mwv207_draw.c`(1) |
| `0x38d` | `0x00E34` | `MWV207REG_RA_CONTROL_EX` | 1 | `mwv207_draw.c`(1) |
| `0x390` | `0x00E40` | `MWV207REG_CENTROIDS` | 1 | `mwv207_draw.c`(1) |
| `0x409` | `0x01024` | `MWV207REG_PIXEL_SHADER_CONST` | 2 | `mwv207_compute.c`(1), `mwv207_shader.c`(1) |
| `0x40b` | `0x0102C` | `MWV207REG_PS_OUTPUT_CONTROL` | 1 | `mwv207_shader.c`(1) |
| `0x40c` | `0x01030` | `MWV207REG_PS_OUTPUT_MODE` | 2 | `mwv207_compute.c`(1), `mwv207_draw.c`(1) |
| `0x40d` | `0x01034` | `MWV207REG_PS_OUTPUT_PRECISION` | 2 | `mwv207_compute.c`(1), `mwv207_draw.c`(1) |
| `0x410` | `0x01040` | `MWV207REG_PS_INPUT_TYPE` | 1 | `mwv207_shader.c`(1) |
| `0x412` | `0x01048` | `MWV207REG_PS_INSTRUCTION_PREFETCH` | 2 | `mwv207_compute.c`(1), `mwv207_shader.c`(1) |
| `0x415` | `0x01054` | `MWV207REG_PS_SAMPLE_COVERAGE_CTRL` | 2 | `mwv207_draw.c`(2) |
| `0x416` | `0x01058` | `MWV207REG_PS_SAMPLER_BASE` | 2 | `mwv207_compute.c`(1), `mwv207_shader.c`(1) |
| `0x420` | `0x01080` | `MWV207REG_PS_UNPACK` | 2 | `mwv207_compute.c`(1), `mwv207_shader.c`(1) |
| `0x424` | `0x01090` | `MWV207REG_PS_RELATIVE_END` | 2 | `mwv207_compute.c`(1), `mwv207_shader.c`(1) |
| `0x425` | `0x01094` | `MWV207REG_PS_INSTRUCTION_PREFETCH_RELATIVE_END` | 2 | `mwv207_compute.c`(1), `mwv207_shader.c`(1) |
| `0x426` | `0x01098` | `MWV207REG_PS_MAX` | 1 | `mwv207_draw.c`(1) |
| `0x427` | `0x0109C` | `MWV207REG_PSCS_THROTTLE` | 2 | `mwv207_compute.c`(2) |
| `0x440` | `0x01100` | `MWV207REG_GS_CONTROL` | 3 | `mwv207_shader.c`(2), `mwv207_compute.c`(1) |
| `0x441` | `0x01104` | `MWV207REG_GS_TYPE` | 1 | `mwv207_shader.c`(1) |
| `0x442` | `0x01108` | `MWV207REG_GS_COUNTERS` | 1 | `mwv207_shader.c`(1) |
| `0x443` | `0x0110C` | `MWV207REG_GS_START` | 1 | `mwv207_shader.c`(1) |
| `0x444` | `0x01110` | `MWV207REG_GS_RELATIVE_END` | 1 | `mwv207_shader.c`(1) |
| `0x446` | `0x01118` | `MWV207REG_GS_INSTRUCTION_PREFETCH` | 1 | `mwv207_shader.c`(1) |
| `0x447` | `0x0111C` | `MWV207REG_GS_INSTRUCTION_PREFETCH_RELATIVE_END` | 1 | `mwv207_shader.c`(1) |
| `0x448` | `0x01120` | `MWV207REG_GS_OUTPUT` | 1 | `mwv207_shader.c`(1) |
| `0x450` | `0x01140` | `MWV207REG_GS_THROTTLE` | 3 | `mwv207_shader.c`(2), `mwv207_compute.c`(1) |
| `0x451` | `0x01144` | `MWV207REG_GS_ATTRIBUTE` | 1 | `mwv207_shader.c`(1) |
| `0x452` | `0x01148` | `MWV207REG_GS_ATTRIBUTE_EX` | 1 | `mwv207_shader.c`(1) |
| `0x453` | `0x0114C` | `MWV207REG_GS_UNIFORM_BASE` | 1 | `mwv207_vertex.c`(1) |
| `0x455` | `0x01154` | `MWV207REG_GS_SAMPLER_BASE` | 1 | `mwv207_shader.c`(1) |
| `0x457` | `0x0115C` | `MWV207REG_GS_THROTTLE_EX` | 3 | `mwv207_shader.c`(2), `mwv207_compute.c`(1) |
| `0x515` | `0x01454` | `MWV207REG_PE_HIERARCHICAL_CONTROL` | 1 | `mwv207_draw.c`(1) |
| `0x528` | `0x014A0` | `MWV207REG_PE_EXTRA_REFERENCE` | 2 | `mwv207_draw.c`(2) |
| `0x529` | `0x014A4` | `MWV207REG_PE_CONFIG` | 6 | `mwv207_draw.c`(6) |
| `0x52a` | `0x014A8` | `MWV207REG_PE_DITHER_LOW` | 2 | `mwv207_draw.c`(2) |
| `0x52b` | `0x014AC` | `MWV207REG_PE_DITHER_HIGH` | 2 | `mwv207_draw.c`(2) |
| `0x52c` | `0x014B0` | `MWV207REG_PE_BLEND_FACTOR_LOW` | 1 | `mwv207_draw.c`(1) |
| `0x52d` | `0x014B4` | `MWV207REG_PE_BLEND_FACTOR_HIGH` | 1 | `mwv207_draw.c`(1) |
| `0x52e` | `0x014B8` | `MWV207REG_PE_BACKFACE_STENCIL` | 1 | `mwv207_draw.c`(1) |
| `0x52f` | `0x014BC` | `MWV207REG_PE_CONFIG_EX` | 1 | `mwv207_draw.c`(1) |
| `0x530` | `0x014C0` | `MWV207REG_PE_ALPHA_CONFIG` | 1 | `mwv207_draw.c`(1) |
| `0x5af` | `0x016BC` | `MWV207REG_MC_COLOR_CLEAR64` | 1 | `mwv207_draw.c`(1) |
| `0xe06` | `0x03818` | `MWV207REG_MULTI_SAMPLE_CONFIG` | 1 | `mwv207_draw.c`(1) |
| `0xe07` | `0x0381C` | `MWV207REG_VARYINGS` | 2 | `mwv207_compute.c`(1), `mwv207_shader.c`(1) |
| `0xe0c` | `0x03830` | `MWV207REG_OCCLUSION_DATA` | 1 | `mwv207_query.c`(1) |
| `0xe18` | `0x03860` | `MWV207REG_OCCLUSION_CONTROL` | 2 | `mwv207_query.c`(2) |
| `0xe1b` | `0x0386C` | `MWV207REG_FENCE_DATA` | 2 | `mwv207_query.c`(2) |
| `0xe22` | `0x03888` | `MWV207REG_SEMANTIC_LOCATION` | 2 | `mwv207_compute.c`(1), `mwv207_shader.c`(1) |
| `0xe23` | `0x0388C` | `MWV207REG_DEPTH_SLICE` | 2 | `mwv207_draw.c`(2) |
| `0xe26` | `0x03898` | `MWV207REG_FENCE_DATA_HIGH` | 2 | `mwv207_query.c`(2) |
| `0xe27` | `0x0389C` | `MWV207REG_DRAW_ID` | 1 | `mwv207_draw.c`(1) |
| `0xe28` | `0x038A0` | `MWV207REG_COLOR_SLICE` | 1 | `mwv207_draw.c`(1) |
| `0xe30` | `0x038C0` | `MWV207REG_SEMANTICS` | 1 | `mwv207_shader.c`(1) |
| `0xe42` | `0x03908` | `MWV207REG_CLUSTER_CHECKER_BOARD` | 1 | `mwv207_cmd.c`(1) |
| `0xe80` | `0x03A00` | `MWV207REG_MULTI_CHIP_CONTROL` | 1 | `mwv207_cmd.c`(1) |
| `0x5002` | `0x14008` | `MWV207REG_BLT_SRC_CONFIG` | 6 | `mwv207_blit.c`(6) |
| `0x5003` | `0x1400C` | `MWV207REG_BLT_SRC_CONFIG_EX` | 6 | `mwv207_blit.c`(6) |
| `0x5004` | `0x14010` | `MWV207REG_BLT_SRC_TILE_STATUS_ADDRESS` | 2 | `mwv207_blit.c`(2) |
| `0x5005` | `0x14014` | `MWV207REG_BLT_SRC_ORIGIN` | 2 | `mwv207_blit.c`(2) |
| `0x5006` | `0x14018` | `MWV207REG_BLT_DEST_ADDRESS` | 1 | `mwv207_blit.c`(1) |
| `0x5008` | `0x14020` | `MWV207REG_BLT_DEST_TILE_STATUS_ADDRESS` | 2 | `mwv207_blit.c`(2) |
| `0x5009` | `0x14024` | `MWV207REG_BLT_DEST_CONFIG` | 4 | `mwv207_blit.c`(4) |
| `0x500a` | `0x14028` | `MWV207REG_BLT_DEST_CONFIG_EX` | 4 | `mwv207_blit.c`(4) |
| `0x500b` | `0x1402C` | `MWV207REG_BLT_DEST_ORIGIN` | 2 | `mwv207_blit.c`(2) |
| `0x500c` | `0x14030` | `MWV207REG_BLT_WINDOW_SIZE` | 3 | `mwv207_blit.c`(3) |
| `0x500d` | `0x14034` | `MWV207REG_BLT_SRC_CLEAR_VALUE` | 2 | `mwv207_blit.c`(2) |
| `0x500f` | `0x1403C` | `MWV207REG_BLT_DST_CLEAR_VALUE` | 2 | `mwv207_blit.c`(2) |
| `0x5011` | `0x14044` | `MWV207REG_BLT_NEW_CLEAR_VALUE` | 2 | `mwv207_blit.c`(2) |
| `0x5013` | `0x1404C` | `MWV207REG_BLT_CLEAR_MASK` | 2 | `mwv207_blit.c`(2) |
| `0x5015` | `0x14054` | `MWV207REG_BLT_COPY_SIZE` | 1 | `mwv207_blit.c`(1) |
| `0x5016` | `0x14058` | `MWV207REG_BLT_DITHER_LOW` | 2 | `mwv207_blit.c`(2) |
| `0x5017` | `0x1405C` | `MWV207REG_BLT_DITHER_HIGH` | 2 | `mwv207_blit.c`(2) |
| `0x5018` | `0x14060` | `MWV207REG_BLT_EXECUTE` | 1 | `include/mwv207_cmd.h`(1) |
| `0x5019` | `0x14064` | `MWV207REG_BLT_GENERAL_CONFIG` | 5 | `mwv207_blit.c`(5) |
| `0x501a` | `0x14068` | `MWV207REG_BLT_TILE_FILLER_COUNT` | 1 | `mwv207_blit.c`(1) |
| `0x5027` | `0x1409C` | `MWV207REG_BLT_BLOCK_SIZE` | 2 | `mwv207_blit.c`(2) |
| `0x5028` | `0x140A0` | `MWV207REG_BLT_TILE_SIZE` | 2 | `mwv207_blit.c`(2) |
| `0x502b` | `0x140AC` | `MWV207REG_BLT_CACHE_FLUSH` | 2 | `include/mwv207_cmd.h`(2) |
| `0x502c` | `0x140B0` | `MWV207REG_BLT_LOD_CONFIG` | 1 | `mwv207_blit.c`(1) |
| `0x502e` | `0x140B8` | `MWV207REG_BLT_GENERAL_CONTROL` | 4 | `include/mwv207_cmd.h`(4) |
| `0x502f` | `0x140BC` | `MWV207REG_BLT_SWIZZLE_EX` | 2 | `mwv207_blit.c`(2) |
| `0x50ce` | `0x14338` | `MWV207REG_BLT_CLUSTER_CONTROL` | 8 | `mwv207_blit.c`(6), `include/mwv207_cmd.h`(2) |
| `0x50cf` | `0x1433C` | `MWV207REG_BLT_DEST_FLIP_SIZE` | 1 | `mwv207_blit.c`(1) |
| `0x5280` | `0x14A00` | `MWV207REG_TCS_START` | 1 | `mwv207_shader.c`(1) |
| `0x5281` | `0x14A04` | `MWV207REG_TCS_RELATIVE_END` | 1 | `mwv207_shader.c`(1) |
| `0x5283` | `0x14A0C` | `MWV207REG_TCS_INSTRUCTION_PREFETCH` | 1 | `mwv207_shader.c`(1) |
| `0x5284` | `0x14A10` | `MWV207REG_TCS_INSTRUCTION_PREFETCH_RELATIVE_END` | 1 | `mwv207_shader.c`(1) |
| `0x5285` | `0x14A14` | `MWV207REG_TCS_CONTROL` | 3 | `mwv207_shader.c`(2), `mwv207_compute.c`(1) |
| `0x5286` | `0x14A18` | `MWV207REG_TCS_THROTTLE` | 3 | `mwv207_shader.c`(2), `mwv207_compute.c`(1) |
| `0x5287` | `0x14A1C` | `MWV207REG_TCS_ATTRIBUTE` | 1 | `mwv207_shader.c`(1) |
| `0x5288` | `0x14A20` | `MWV207REG_TCS_OUTPUT` | 1 | `mwv207_shader.c`(1) |
| `0x5290` | `0x14A40` | `MWV207REG_TCS_ATTRIBUTE_EX` | 1 | `mwv207_shader.c`(1) |
| `0x5291` | `0x14A44` | `MWV207REG_TCS_UNIFORM_BASE` | 1 | `mwv207_vertex.c`(1) |
| `0x5293` | `0x14A4C` | `MWV207REG_TCS_SAMPLER_BASE` | 1 | `mwv207_shader.c`(1) |
| `0x52c0` | `0x14B00` | `MWV207REG_TPG_CONTROL` | 1 | `mwv207_shader.c`(1) |
| `0x52c1` | `0x14B04` | `MWV207REG_TES_START` | 1 | `mwv207_shader.c`(1) |
| `0x52c2` | `0x14B08` | `MWV207REG_TES_RELATIVE_END` | 1 | `mwv207_shader.c`(1) |
| `0x52c4` | `0x14B10` | `MWV207REG_TES_INSTRUCTION_PREFETCH` | 1 | `mwv207_shader.c`(1) |
| `0x52c5` | `0x14B14` | `MWV207REG_TES_INSTRUCTION_PREFETCH_RELATIVE_END` | 1 | `mwv207_shader.c`(1) |
| `0x52c6` | `0x14B18` | `MWV207REG_TES_CONTROL` | 3 | `mwv207_shader.c`(2), `mwv207_compute.c`(1) |
| `0x52c7` | `0x14B1C` | `MWV207REG_TES_THROTTLE` | 3 | `mwv207_shader.c`(2), `mwv207_compute.c`(1) |
| `0x52c8` | `0x14B20` | `MWV207REG_TES_ATTRIBUTE` | 1 | `mwv207_shader.c`(1) |
| `0x52c9` | `0x14B24` | `MWV207REG_TES_UNIFORM_BASE` | 1 | `mwv207_vertex.c`(1) |
| `0x52cb` | `0x14B2C` | `MWV207REG_TES_SAMPLER_BASE` | 1 | `mwv207_shader.c`(1) |
| `0x52cd` | `0x14B34` | `MWV207REG_TES_THROTTLE_EX` | 3 | `mwv207_shader.c`(2), `mwv207_compute.c`(1) |
| `0x52d0` | `0x14B40` | `MWV207REG_TES_OUTPUT` | 1 | `mwv207_shader.c`(1) |
| `0x5310` | `0x14C40` | `MWV207REG_TX_GLOBAL_CONFIG` | 1 | `mwv207_sampler.c`(1) |
| `0x5311` | `0x14C44` | `MWV207REG_TX_GPIPE_COMMAND` | 1 | `mwv207_cmd.c`(1) |
| `0x5580` | `0x15600` | `MWV207REG_SHADER_MISC_CONFIG` | 3 | `mwv207_shader.c`(2), `mwv207_compute.c`(1) |
| `0x5581` | `0x15604` | `MWV207REG_VS_INSTRUCTION_PREFETCH_RELATIVE_END` | 1 | `mwv207_shader.c`(1) |
| `0x5582` | `0x15608` | `MWV207REG_VS_THROTTLE_EX` | 2 | `mwv207_compute.c`(1), `mwv207_shader.c`(1) |
| `0x6101` | `0x18404` | `MWV207REG_WD_STATIC_CONFIG` | 1 | `mwv207_cmd.c`(1) |
| `0x7000` | `0x1C000` | `MWV207REG_TFB_CONTROL` | 1 | `mwv207_draw.c`(1) |
| `0x7001` | `0x1C004` | `MWV207REG_TFB_COMMAND` | 6 | `mwv207_draw.c`(3), `mwv207_query.c`(3) |
| `0x7003` | `0x1C00C` | `MWV207REG_TFB_INVALIDATE` | 1 | `mwv207_cmd.c`(1) |
| `0x7004` | `0x1C010` | `MWV207REG_TFB_QUERY_COMMAND` | 6 | `mwv207_query.c`(6) |
| `0x7020` | `0x1C080` | `MWV207REG_TFB_BUFFER_SIZE` | 1 | `mwv207_query.c`(1) |
| `0x7030` | `0x1C0C0` | `MWV207REG_TFB_BUFFER_STRIDE` | 1 | `mwv207_query.c`(1) |
| `0x7040` | `0x1C100` | `MWV207REG_TFB_DESCRIPTOR_COUNT` | 1 | `mwv207_draw.c`(1) |

## 3. 未映射的驱动寄存器号

这些寄存器号在已发行的厂商头文件中没有 `_Address` 定义（多为引擎级控制/同步寄存器）。
要点：**位域含义未知，改动前必须以实测为准。**

| 驱动号 | 应有地址(<<2) | 写入次数 | 使用位置 |
|---|---|---|---|
| `0xe03` | `0x0380C` | 14 | mwv207_draw.c:7, mwv207_cmd.c:3, mwv207_sampler.c:3 |
| `0xe02` | `0x03808` | 8 | mwv207_cmd.c:3, mwv207_compute.c:2, mwv207_draw.c:2 |
| `0x404` | `0x01010` | 3 | mwv207_draw.c:2, mwv207_compute.c:1 |
| `0x500` | `0x01400` | 3 | mwv207_draw.c:3 |
| `0x50b` | `0x0142C` | 3 | mwv207_draw.c:3 |
| `0x192` | `0x00648` | 2 | mwv207_draw.c:2 |
| `0x28c` | `0x00A30` | 2 | mwv207_compute.c:1, mwv207_shader.c:1 |
| `0x28d` | `0x00A34` | 2 | mwv207_draw.c:1, mwv207_shader.c:1 |
| `0x402` | `0x01008` | 2 | mwv207_compute.c:1, mwv207_shader.c:1 |
| `0x403` | `0x0100C` | 2 | mwv207_compute.c:1, mwv207_shader.c:1 |
| `0x503` | `0x0140C` | 2 | mwv207_draw.c:2 |
| `0x594` | `0x01650` | 2 | mwv207_cmd.c:2 |
| `0x595` | `0x01654` | 2 | mwv207_draw.c:2 |
| `0xffff` | `0x3FFFC` | 2 | mwv207_draw.c:2 |
| `0x191` | `0x00644` | 1 | mwv207_draw.c:1 |
| `0x281` | `0x00A04` | 1 | mwv207_draw.c:1 |
| `0x282` | `0x00A08` | 1 | mwv207_draw.c:1 |
| `0x283` | `0x00A0C` | 1 | mwv207_draw.c:1 |
| `0x284` | `0x00A10` | 1 | mwv207_draw.c:1 |
| `0x285` | `0x00A14` | 1 | mwv207_draw.c:1 |
| `0x286` | `0x00A18` | 1 | mwv207_draw.c:1 |
| `0x287` | `0x00A1C` | 1 | mwv207_draw.c:1 |
| `0x28a` | `0x00A28` | 1 | mwv207_draw.c:1 |
| `0x28b` | `0x00A2C` | 1 | mwv207_draw.c:1 |
| `0x301` | `0x00C04` | 1 | mwv207_draw.c:1 |
| `0x302` | `0x00C08` | 1 | mwv207_draw.c:1 |
| `0x303` | `0x00C0C` | 1 | mwv207_draw.c:1 |
| `0x304` | `0x00C10` | 1 | mwv207_draw.c:1 |
| `0x305` | `0x00C14` | 1 | mwv207_draw.c:1 |
| `0x306` | `0x00C18` | 1 | mwv207_draw.c:1 |
| `0x401` | `0x01004` | 1 | mwv207_shader.c:1 |
| `0x501` | `0x01404` | 1 | mwv207_draw.c:1 |
| `0x502` | `0x01408` | 1 | mwv207_draw.c:1 |
| `0x505` | `0x01414` | 1 | mwv207_draw.c:1 |
| `0x506` | `0x01418` | 1 | mwv207_draw.c:1 |
| `0x507` | `0x0141C` | 1 | mwv207_draw.c:1 |
| `0x508` | `0x01420` | 1 | mwv207_draw.c:1 |
| `0x509` | `0x01424` | 1 | mwv207_draw.c:1 |
| `0x50a` | `0x01428` | 1 | mwv207_draw.c:1 |
| `0x50d` | `0x01434` | 1 | mwv207_draw.c:1 |
| `0x596` | `0x01658` | 1 | mwv207_draw.c:1 |
| `0x598` | `0x01660` | 1 | mwv207_draw.c:1 |
| `0x599` | `0x01664` | 1 | mwv207_draw.c:1 |
| `0x59b` | `0x0166C` | 1 | mwv207_draw.c:1 |
| `0x6d0` | `0x01B40` | 1 | mwv207_draw.c:1 |
| `0xe00` | `0x03800` | 1 | mwv207_cmd.c:1 |
| `0xe05` | `0x03814` | 1 | mwv207_cmd.c:1 |
| `0xe13` | `0x0384C` | 1 | mwv207_draw.c:1 |
| `0xf00` | `0x03C00` | 1 | mwv207_draw.c:1 |

