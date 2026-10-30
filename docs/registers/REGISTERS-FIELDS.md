# MWV207 寄存器字段明细（驱动实际写入的寄存器）

每个寄存器列出：厂商字段名、位域、宽度、类型、命名取值，以及驱动的写入位置。
完整 1392 个寄存器见 `register-db.json`。

## `0x6b` — `MWV207REG_MMU_CONFIG`

- 地址：`0x001AC`　驱动号：`0x6b`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFC01FFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_teamleader.h:350`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| MASK_SECURITY | 31:31 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| SECURITY | 30:30 | 1 | U01 | NON_SECURE_MODE=0x0, SECURE_MODE=0x1 |
| MASK_OUT_OF_BOUND | 29:29 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| OUT_OF_BOUND | 28:28 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| MASK_VIRTUAL_ADDRESS_40_BITS | 27:27 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| VIRTUAL_ADDRESS_40_BITS | 26:26 | 1 | U01 | ENABLED=0x0, DISABLED=0x1 |
| MASK_PAGE_TABLE_ID | 16:16 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| PAGE_TABLE_ID | 15:0 | 16 | U16 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_cmd.c:43` | `MWV207_EMIT` | `0x0fffffff` |

## `0x19d` — `MWV207REG_FE_PRIMITIVE_RESTART`

- 地址：`0x00674`　驱动号：`0x19d`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_exclusive.h:534`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| INDEX | 31:0 | 32 | U32 | — |

驱动写入点（3 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:166` | `MWV207_EMIT` | `dinfo->restart_index` |
| `mwv207_draw.c:168` | `MWV207_EMIT` | `restartIndex` |
| `mwv207_draw.c:173` | `MWV207_EMIT` | `0` |

## `0x1f0` — `MWV207REG_FE_PATCH_CONTROL`

- 地址：`0x007C0`　驱动号：`0x1f0`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x0000003F ResetValue=0x00000003`
- 厂商定义位置：`jmgpu_exclusive.h:667`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| VERTEX_COUNT | 5:0 | 6 | U06 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:175` | `MWV207_EMIT` | `0x00000003` |
| `mwv207_draw.c:98` | `MWV207_EMIT` | `ctx->info.vertices_per_patch` |

## `0x1f1` — `MWV207REG_FE_ID`

- 地址：`0x007C4`　驱动号：`0x1f1`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x007F7F13 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_exclusive.h:687`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| INSTANCE_ID_LOCATION | 22:16 | 7 | U07 | — |
| VERTEX_ID_LOCATION | 14:8 | 7 | U07 | — |
| PRIMITIVE_ID_RESET | 4:4 | 1 | U01 | PER_INSTANCE=0x0, PER_DRAW=0x1 |
| ID_SELECT | 1:0 | 2 | U02 | DISABLE=0x0, VERTEX_ID=0x1, INSTANCE_ID=0x2, BOTH=0x3 |

驱动写入点（3 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:246` | `MWV207_EMIT` | `(ALIGN_BIT(0x3, 0x0) & (0x3)) \|          (ALIGN_BIT((uint32_t) (0), 0x8) & (0x7F00)) \|          (ALIGN_BIT((uint32_t) (1), 0x10) & (0x7F0000))` |
| `mwv207_shader.c:257` | `MWV207_EMIT` | `(ALIGN_BIT(0x3, 0x0) & (0x3)) \|          (ALIGN_BIT((uint32_t) (vs->infile.num_reg * 4), 0x8) & (0x7F00)) \|          (ALIGN_BIT((uint32_t) (vs->infile.num_reg * 4 + 1), 0x10) & (0x7F0000))` |
| `mwv207_shader.c:266` | `MWV207_EMIT` | `(ALIGN_BIT(0x0, 0x0) & (0x3))` |

## `0x1f2` — `MWV207REG_FE_RESET_ATTRIBUTES`

- 地址：`0x007C8`　驱动号：`0x1f2`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x00000001 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_exclusive.h:737`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| RESET | 0:0 | 1 | U01 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_cmd.c:51` | `MWV207_EMIT` | `true` |
| `mwv207_draw.c:102` | `MWV207_EMIT` | `true` |

## `0x1f6` — `MWV207REG_FE_CONTROL`

- 地址：`0x007D8`　驱动号：`0x1f6`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x00000003 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_exclusive.h:894`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| FETCH_BREAK_MODE | 1:1 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| RETAIN_MODE | 0:0 | 1 | U01 | DEFAULT=0x0, RETAIN=0x1 |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_cmd.c:45` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (ENABLE), 0x1) & (0x2))` |

## `0x201` — `MWV207REG_AHBDEC_CONTROL_EX`

- 地址：`0x00804`　驱动号：`0x201`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00080000`
- 厂商定义位置：`jmgpu_entity.h:203`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| DISABLE_TSC_AUTO_EVICT | 31:31 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| TILE_YF | 30:30 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| READ_MISS_POLICY | 29:29 | 1 | U01 | POLICY0=0x0, POLICY1=0x1 |
| DISABLE_DCACHE | 28:28 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| DISABLE_CACHE_TSC | 27:27 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| TGL_DISABLE_COMPRESSION | 26:26 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| DISABLE_ADDRESS_HASH | 25:25 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| DISABLE_CACHE_TFC | 24:24 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| ENABLE_BURST_SPLIT_EX | 23:22 | 2 | U02 | BYTES64=0x0, BYTES128=0x1, BYTES256=0x2, RESERVED=0x3 |
| COMPRESSION_MODE | 21:20 | 2 | U02 | MODE0=0x0, MODE1=0x1, MODE2=0x2, RESERVED=0x3 |
| WRITE_MISS_POLICY | 19:19 | 1 | U01 | POLICY0=0x0, POLICY1=0x1 |
| FAST_CLEAR_WIDTH_SEL | 18:18 | 1 | U01 | — |
| ENABLE_END_ADDRESS_RANGE_CHECK | 17:17 | 1 | U01 | — |
| ENABLE_BURST_SPLIT | 16:16 | 1 | U01 | EX=23:22 |
| AXQOS | 15:12 | 4 | U04 | — |
| AXUSER | 11:0 | 12 | U12 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:237` | `MWV207_EMIT` | `0x00000001` |
| `mwv207_shader.c:361` | `MWV207_EMIT` | `link.num_varyings_fixed + output_add` |

## `0x202` — `MWV207REG_AHBDEC_CONTROL_EX2`

- 地址：`0x00808`　驱动号：`0x202`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x003FC810`
- 厂商定义位置：`jmgpu_entity.h:341`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| TS_CACHE_MAP_MODE | 31:31 | 1 | U01 | MODE0=0x0, MODE1=0x1 |
| PROBE_STREAM_ID | 30:26 | 5 | U05 | — |
| ENABLE_GLOBAL_CR_STATISTICS | 25:25 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| ENABLE_TSC_HIT_MISS_SEPARATE_REQUEST | 24:24 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| RESERVE0 | 23:22 | 2 | U02 | — |
| WR_OT_CNT | 21:14 | 8 | U08 | RESET=0xff |
| TILE_STATUS_WRITE_ID | 13:7 | 7 | U07 | — |
| TILE_STATUS_READ_ID | 6:0 | 7 | U07 | — |

驱动写入点（3 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:241` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (1), 0x0) & (0x3F)) \|          (ALIGN_BIT((uint32_t) (1), 0x8) & (0x1F00)) \|          (ALIGN_BIT((uint32_t) (1), 0x1F) & (0x80000000))` |
| `mwv207_shader.c:252` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (vs->infile.num_reg), 0x0) & (0x3F)) \|          (ALIGN_BIT((uint32_t) (time_out), 0x8) & (0x1F00)) \|          (ALIGN_BIT((uint32_t) (1), 0x1F) & (0x80000000))` |
| `mwv207_shader.c:262` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (vs->infile.num_reg), 0x0) & (0x3F)) \|          (ALIGN_BIT((uint32_t) (time_out), 0x8) & (0x1F00))` |

## `0x203` — `MWV207REG_AHBDEC_INTR_ENBL`

- 地址：`0x0080C`　驱动号：`0x203`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_entity.h:407`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| INTR_ENBL_VEC | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:305` | `MWV207_EMIT` | `vs->num_temps` |

## `0x20c` — `MWV207REG_AHBDEC_TOTAL_READS_IN`

- 地址：`0x00830`　驱动号：`0x20c`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_entity.h:575`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| COUNT | 31:0 | 32 | U32 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:236` | `MWV207_EMIT` | `0x0f3f0000` |
| `mwv207_shader.c:318` | `MWV207_EMIT` | `vs->vs_load_balancing` |

## `0x219` — `MWV207REG_VERTEX_SHADER_CONST`

- 地址：`0x00864`　驱动号：`0x219`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000003FF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:812`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| BASE_OFFSET | 9:0 | 10 | U10 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:308` | `MWV207_EMIT` | `(0x0 / 4)` |

## `0x21a` — `MWV207REG_SH_CACHE_CONTROL`

- 地址：`0x00868`　驱动号：`0x21a`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000003F1 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:831`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| INVALIDATE_GS | 9:9 | 1 | U01 | — |
| INVALIDATE_TES | 8:8 | 1 | U01 | — |
| INVALIDATE_TCS | 7:7 | 1 | U01 | — |
| INVALIDATE_CE | 6:6 | 1 | U01 | — |
| INVALIDATE_PS | 5:5 | 1 | U01 | — |
| INVALIDATE_VS | 4:4 | 1 | U01 | — |
| MODE | 0:0 | 1 | U01 | STATES=0x0, MEMORY=0x1 |

驱动写入点（6 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:185` | `MWV207_EMIT` | `(ALIGN_BIT(0x1, 0x0) & (0x1))` |
| `mwv207_shader.c:310` | `MWV207_EMIT` | `(ALIGN_BIT(0x1, 0x0) & (0x1))` |
| `mwv207_shader.c:419` | `MWV207_EMIT` | `(ALIGN_BIT(0x1, 0x0) & (0x1))` |
| `mwv207_shader.c:544` | `MWV207_EMIT` | `(ALIGN_BIT(0x1, 0x0) & (0x1))` |
| `mwv207_shader.c:620` | `MWV207_EMIT` | `(ALIGN_BIT(0x1, 0x0) & (0x1))` |
| `mwv207_shader.c:675` | `MWV207_EMIT` | `(ALIGN_BIT(0x1, 0x0) & (0x1))` |

## `0x21c` — `MWV207REG_VS_ATTRIBUTE`

- 地址：`0x00870`　驱动号：`0x21c`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x0007FF3F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:908`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| GROUP_SIZE | 18:8 | 11 | U11 | — |
| VERTEX_SIZE | 5:0 | 6 | U06 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:294` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (vs->outfile.num_reg + vertex_num), 0x0) & (0x3F)) \|        (ALIGN_BIT((uint32_t) (max_thread * (vs->outfile.num_reg + vertex_num)), 0x8) & (0x7FF00))` |

## `0x21d` — `MWV207REG_VS_START_PC`

- 地址：`0x00874`　驱动号：`0x21d`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:932`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| START | 19:0 | 20 | U20 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:312` | `MWV207_EMIT` | `0` |

## `0x21f` — `MWV207REG_PS_START_PC`

- 地址：`0x0087C`　驱动号：`0x21f`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:972`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| START | 19:0 | 20 | U20 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:206` | `MWV207_EMIT` | `0` |
| `mwv207_shader.c:676` | `MWV207_EMIT` | `0` |

## `0x221` — `MWV207REG_SH_SAMPLER_ARBITRATION`

- 地址：`0x00884`　驱动号：`0x221`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x00003F3F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:1011`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| VS_GROUP_LIMIT | 13:8 | 6 | U06 | IN_ORDER=0x0, UNLIMITED=0x3f |
| PS_GROUP_LIMIT | 5:0 | 6 | U06 | IN_ORDER=0x0, UNLIMITED=0x3f |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_cmd.c:49` | `MWV207_EMIT` | `0x0808` |

## `0x223` — `MWV207REG_VS_INSTRUCTION_PREFETCH`

- 地址：`0x0088C`　驱动号：`0x223`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:1068`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PC | 19:0 | 20 | U20 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:741` | `MWV207_EMIT` | `0` |

## `0x228` — `MWV207REG_VS_THROTTLE`

- 地址：`0x008A0`　驱动号：`0x228`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x1FFFF03F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:1236`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| RESULT_WINDOW_SIZE | 28:20 | 9 | U09 | — |
| MAX_THREADS | 19:12 | 8 | U08 | — |
| MAX_PAGES | 5:0 | 6 | U06 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:219` | `MWV207_EMIT` | `0` |
| `mwv207_shader.c:298` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (alloc_pages), 0x0) & (0x3F)) \|       (ALIGN_BIT((uint32_t) (max_thread), 0xC) & (0xFF000)) \|       (ALIGN_BIT((uint32_t) (res_window_size), 0x14) & (0x1FF00000))` |

## `0x22a` — `MWV207REG_VS_SAMPLER_BASE`

- 地址：`0x008A8`　驱动号：`0x22a`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x03FF007F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:1361`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| TXD_OFFSET | 25:16 | 10 | U10 | — |
| OFFSET | 6:0 | 7 | U07 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:307` | `MWV207_EMIT` | `0x10` |

## `0x22c` — `MWV207REG_SH_ICACHE_INVALIDATE`

- 地址：`0x008B0`　驱动号：`0x22c`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x0000001F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:1412`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PS | 4:4 | 1 | U01 | — |
| GS | 3:3 | 1 | U01 | — |
| TES | 2:2 | 1 | U01 | — |
| TCS | 1:1 | 1 | U01 | — |
| VS | 0:0 | 1 | U01 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_cmd.c:153` | `MWV207_EMIT` | `0x1F` |

## `0x22f` — `MWV207REG_VS_RELATIVE_END`

- 地址：`0x008BC`　驱动号：`0x22f`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x001FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:1584`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PC | 20:0 | 21 | U21 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:313` | `MWV207_EMIT` | `(vs->code_size / 4)` |

## `0x230` — `MWV207REG_VS_INPUT`

- 地址：`0x008C0`　驱动号：`0x230`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=3 BLK=3 Count=8 FieldMask=0x3F3F3F3F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:1266`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| ATTRIBUTE3 | 29:24 | 6 | U06 | — |
| ATTRIBUTE2 | 21:16 | 6 | U06 | — |
| ATTRIBUTE1 | 13:8 | 6 | U06 | — |
| ATTRIBUTE0 | 5:0 | 6 | U06 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:269` | `MWV207_EMITS` | `8` |

## `0x238` — `MWV207REG_VS_OUTPUT`

- 地址：`0x008E0`　驱动号：`0x238`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=3 BLK=3 Count=8 FieldMask=0x3F3F3F3F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:1304`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| OUTPUT3 | 29:24 | 6 | U06 | — |
| OUTPUT2 | 21:16 | 6 | U06 | — |
| OUTPUT1 | 13:8 | 6 | U06 | — |
| OUTPUT0 | 5:0 | 6 | U06 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:336` | `MWV207_EMITS` | `8` |

## `0x240` — `MWV207REG_TW_CONFIG`

- 地址：`0x00900`　驱动号：`0x240`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xB7FFF773 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:1601`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| TRIGGER_MODE | 31:31 | 1 | U01 | SYNC=0x0, ASYNC=0x1 |
| SELECT_HIGH_COUNT | 29:28 | 2 | U02 | X=0x0, Y=0x1, Z=0x2 |
| VALUE_ORDER | 26:24 | 3 | U03 | GLW=0x0, GWL=0x1, LGW=0x2, WGL=0x3, LWG=0x4, WLG=0x5 |
| SWATH_SIZE_Z | 23:20 | 4 | U04 | — |
| SWATH_SIZE_Y | 19:16 | 4 | U04 | — |
| SWATH_SIZE_X | 15:12 | 4 | U04 | — |
| SWATH_ENABLE_Z | 10:10 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| SWATH_ENABLE_Y | 9:9 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| SWATH_ENABLE_X | 8:8 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| TRAVERSE_ORDER | 6:4 | 3 | U03 | XYZ=0x0, XZY=0x1, YXZ=0x2, ZXY=0x3, YZX=0x4, ZYX=0x5 |
| WORK_DIM | 1:0 | 2 | U02 | DEFAULT=0x0, ONE=0x1, TWO=0x2, THREE=0x3 |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:243` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (3), 0x0) & (0x3)) \|       (ALIGN_BIT(0x1, 0x18) & (0x7000000))` |

## `0x247` — `MWV207REG_TW_SHADER_INFO`

- 地址：`0x0091C`　驱动号：`0x247`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:1845`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| THREAD_ALLOCATION | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:263` | `MWV207_EMIT` | `thread` |

## `0x248` — `MWV207REG_TW_TRIGGER`

- 地址：`0x00920`　驱动号：`0x248`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:1861`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| TRIGGER | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:277` | `MWV207_EMIT` | `0xbadabeeb` |

## `0x249` — `MWV207REG_TW_SHADER_INFO2`

- 地址：`0x00924`　驱动号：`0x249`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x33F1FFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:1878`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| LOCAL_ID_W | 29:28 | 2 | U02 | DISABLED=0x0, LOCAL_STORAGE_BASE=0x1, RUNNING_WORK_GROUP_ID=0x2 |
| WORK_GROUP_IN_CLUSTER | 25:20 | 6 | U06 | — |
| BARRIER | 16:16 | 1 | U01 | NOT_PRESENT=0x1, PRESENT=0x0 |
| LOCAL_COUNT | 15:0 | 16 | U16 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:248` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (0), 0x0) & (0xFFFF)) \|          (ALIGN_BIT((uint32_t) (63), 0x14) & (0x3F00000)) \|          (ALIGN_BIT(0x0, 0x10) & (0x10000)) \|          (ALIGN_BIT(0x0, 0x1C) & (0x30000000))` |
| `mwv207_compute.c:254` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (local_memory_pack_size), 0x0) & (0xFFFF)) \|          (ALIGN_BIT((uint32_t) (63), 0x14) & (0x3F00000)) \|          (ALIGN_BIT(0x0, 0x10) & (0x10000)) \|          (ALIGN_BIT(0x1, 0x1C) & (0x30000000))` |

## `0x24b` — `MWV207REG_TW_INFO_GLOBAL_OFFSET_X`

- 地址：`0x0092C`　驱动号：`0x24b`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:1933`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| GLOBAL_OFFSET | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:265` | `MWV207_EMIT` | `0` |

## `0x24d` — `MWV207REG_TW_INFO_GLOBAL_OFFSET_Y`

- 地址：`0x00934`　驱动号：`0x24d`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:1967`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| GLOBAL_OFFSET | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:266` | `MWV207_EMIT` | `0` |

## `0x24f` — `MWV207REG_TW_INFO_GLOBAL_OFFSET_Z`

- 地址：`0x0093C`　驱动号：`0x24f`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:2001`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| GLOBAL_OFFSET | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:267` | `MWV207_EMIT` | `0` |

## `0x250` — `MWV207REG_TW_WORK_GROUP_COUNT_X`

- 地址：`0x00940`　驱动号：`0x250`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:2018`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| COUNT | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:269` | `MWV207_EMIT` | `ctx->info.grid[0] - 1` |

## `0x251` — `MWV207REG_TW_WORK_GROUP_COUNT_Y`

- 地址：`0x00944`　驱动号：`0x251`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:2034`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| COUNT | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:270` | `MWV207_EMIT` | `ctx->info.grid[1] - 1` |

## `0x252` — `MWV207REG_TW_WORK_GROUP_COUNT_Z`

- 地址：`0x00948`　驱动号：`0x252`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:2050`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| COUNT | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:271` | `MWV207_EMIT` | `ctx->info.grid[2] - 1` |

## `0x253` — `MWV207REG_TW_WORKGROUP_SIZE_X`

- 地址：`0x0094C`　驱动号：`0x253`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000003FF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:2066`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| SIZE | 9:0 | 10 | U10 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:273` | `MWV207_EMIT` | `info->block[0] - 1` |

## `0x254` — `MWV207REG_TW_WORKGROUP_SIZE_Y`

- 地址：`0x00950`　驱动号：`0x254`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000003FF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:2083`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| SIZE | 9:0 | 10 | U10 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:274` | `MWV207_EMIT` | `info->block[1] - 1` |

## `0x255` — `MWV207REG_TW_WORKGROUP_SIZE_Z`

- 地址：`0x00954`　驱动号：`0x255`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000003FF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:2100`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| SIZE | 9:0 | 10 | U10 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:275` | `MWV207_EMIT` | `info->block[2] - 1` |

## `0x280` — `MWV207REG_AHBDEC_WRITE_EX_CONFIG`

- 地址：`0x00A00`　驱动号：`0x280`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=5 BLK=5 Count=32 FieldMask=0x3DFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_entity.h:1384`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| TS_CACHE_REPLACEMENT | 29:29 | 1 | U01 | LRU=0x0, FIFO=0x1 |
| INTEL_P010 | 28:28 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| PIXEL_CACHE_REPLACEMENT | 27:27 | 1 | U01 | LRU=0x0, FIFO=0x1 |
| TS_CACHE_READ_MODE | 26:26 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| STREAM_MODE | 24:20 | 5 | U05 | RESERVED=0x0, MIPI_STREAM0=0x1, MIPI_STREAM1=0x2, MIPI_STREAM2=0x3, MIPI_STREAM3=0x4, OSF_STREAM0_Y=0x5, OSF_STREAM0_U=0x6, OSF_STREAM0_V=0x7 …(+16) |
| TILE_Y | 19:19 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| BIT_DEPTH | 18:16 | 3 | U03 | BIT8=0x0, BIT10=0x1, BIT12=0x2, BIT14=0x3, BIT16=0x4 |
| CBSR_WIDTH | 15:3 | 13 | U13 | — |
| RESERVE0 | 2:1 | 2 | U02 | — |
| TPC | 0:0 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:192` | `MWV207_EMIT` | `fui(ctx->viewport.scale[0])` |

## `0x28e` — `MWV207REG_PA_LINE_ADJUST_SUB`

- 地址：`0x00A38`　驱动号：`0x28e`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_nearest.h:701`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| FL32_VALUE | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:522` | `MWV207_EMIT` | `rast_state->line_scale` |

## `0x28f` — `MWV207REG_PA_LINE_ADJUST_ADD`

- 地址：`0x00A3C`　驱动号：`0x28f`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_nearest.h:719`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| FL32_VALUE | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:523` | `MWV207_EMIT` | `rast_state->line_scale` |

## `0x2a0` — `MWV207REG_AHBDEC_READ_BUFFER_BASE`

- 地址：`0x00A80`　驱动号：`0x2a0`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=5 BLK=5 Count=32 FieldMask=0xFFFFFFFF ResetValue=0xFFFFFFFF`
- 厂商定义位置：`jmgpu_entity.h:1492`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| ADDRESS | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:200` | `MWV207_EMIT` | `fui(clip_w)` |

## `0x2a1` — `MWV207REG_PA_MAX_POINT_SIZE`

- 地址：`0x00A84`　驱动号：`0x2a1`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_nearest.h:757`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| FL32 | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:203` | `MWV207_EMITF` | `0x00800000` |

## `0x2a2` — `MWV207REG_PA_CLIP_CONTROL`

- 地址：`0x00A88`　驱动号：`0x2a2`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x53111111 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_nearest.h:776`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| ZSCALE | 30:30 | 1 | U01 | SCALE=0x0, BYPASS=0x1 |
| DENORM | 28:28 | 1 | U01 | CLAMP_TO_ZERO=0x0, PASS_THROUGH=0x1 |
| ADJUST_FAR_Z | 25:25 | 1 | U01 | ENABLED=0x0, DISABLED=0x1 |
| CLIP_FAR_Z | 24:24 | 1 | U01 | ENABLED=0x0, DISABLED=0x1 |
| CLIP_NEGATIVE_W | 20:20 | 1 | U01 | CLAMP=0x0, DROP=0x1 |
| CLAMP_COORDINATES_DISABLE | 16:16 | 1 | U01 | ENABLED=0x0, DISABLED=0x1 |
| CLIP_TRIANGLE_DISABLE | 12:12 | 1 | U01 | ENABLED=0x0, DISABLED=0x1 |
| CLIP_LINE_DISABLE | 8:8 | 1 | U01 | ENABLED=0x0, DISABLED=0x1 |
| CLIP_WPLANE_DISABLE | 4:4 | 1 | U01 | ENABLED=0x0, DISABLED=0x1 |
| CLIP_DISABLE | 0:0 | 1 | U01 | ENABLED=0x0, DISABLED=0x1 |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:509` | `MWV207_EMIT` | `0x40000000` |

## `0x2a3` — `MWV207REG_PA_SMALL_W`

- 地址：`0x00A8C`　驱动号：`0x2a3`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_nearest.h:875`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| FP32 | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:202` | `MWV207_EMIT` | `0x00000000` |

## `0x2a4` — `MWV207REG_PA_PACK`

- 地址：`0x00A90`　驱动号：`0x2a4`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=2 BLK=2 Count=4 FieldMask=0x77777777 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_nearest.h:894`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| VARYING7 | 30:28 | 3 | U03 | — |
| VARYING6 | 26:24 | 3 | U03 | — |
| VARYING5 | 22:20 | 3 | U03 | — |
| VARYING4 | 18:16 | 3 | U03 | — |
| VARYING3 | 14:12 | 3 | U03 | — |
| VARYING2 | 10:8 | 3 | U03 | — |
| VARYING1 | 6:4 | 3 | U03 | — |
| VARYING0 | 2:0 | 3 | U03 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:188` | `MWV207_EMIT` | `0x2` |
| `mwv207_shader.c:731` | `MWV207_EMITS` | `4` |

## `0x2aa` — `MWV207REG_PA_CONTROL`

- 地址：`0x00AA8`　驱动号：`0x2aa`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x0000033F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_nearest.h:1031`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| BYPASS_AA_LINE_FIX | 9:9 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| SKIP_LAST_ATTRIBUTE | 8:8 | 1 | U01 | — |
| OUTPUT_COUNT | 5:0 | 6 | U06 | — |

驱动写入点（3 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:238` | `MWV207_EMIT` | `0` |
| `mwv207_shader.c:697` | `MWV207_EMIT` | `link.num_varyings_fixed + 2` |
| `mwv207_shader.c:699` | `MWV207_EMIT` | `link.num_varyings_fixed + 1` |

## `0x300` — `MWV207REG_AHBDEC_READ_BUFFER_END_EX`

- 地址：`0x00C00`　驱动号：`0x300`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=5 BLK=5 Count=32 FieldMask=0xFFFFFFFF ResetValue=0xFFFFFFFF`
- 厂商定义位置：`jmgpu_entity.h:1547`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| ADDRESS | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:243` | `MWV207_EMITF` | `left << 16` |

## `0x308` — `MWV207REG_SE_CLIP_COMPARE_RIGHT`

- 地址：`0x00C20`　驱动号：`0x308`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_borders.h:201`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| FL32 | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:246` | `MWV207_EMITF` | `((right << 16) + 0xFFFF)` |

## `0x309` — `MWV207REG_SE_CLIP_COMPARE_BOTTOM`

- 地址：`0x00C24`　驱动号：`0x309`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_borders.h:221`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| FL32 | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:247` | `MWV207_EMITF` | `((bottom << 16) + 0xFFFF)` |

## `0x380` — `MWV207REG_AHBDEC_WRITE_BUFFER_BASE_EX`

- 地址：`0x00E00`　驱动号：`0x380`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=5 BLK=5 Count=32 FieldMask=0xFFFFFFFF ResetValue=0xFFFFFFFF`
- 厂商定义位置：`jmgpu_entity.h:1623`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| ADDRESS | 31:0 | 32 | U32 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:528` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (ENABLE), 0x0) & (0x1))` |
| `mwv207_shader.c:735` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (true), 0x0) & (0x1)) \|       (ALIGN_BIT((uint32_t) (half_attribute), 0x1) & (0x2))` |

## `0x381` — `MWV207REG_MULTI_SAMPLE_JITTER`

- 地址：`0x00E04`　驱动号：`0x381`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_protocol.h:135`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| JITTER_Y3_X3 | 31:30 | 2 | U02 | — |
| JITTER_Y3_X2 | 29:28 | 2 | U02 | — |
| JITTER_Y3_X1 | 27:26 | 2 | U02 | — |
| JITTER_Y3_X0 | 25:24 | 2 | U02 | — |
| JITTER_Y2_X3 | 23:22 | 2 | U02 | — |
| JITTER_Y2_X2 | 21:20 | 2 | U02 | — |
| JITTER_Y2_X1 | 19:18 | 2 | U02 | — |
| JITTER_Y2_X0 | 17:16 | 2 | U02 | — |
| JITTER_Y1_X3 | 15:14 | 2 | U02 | — |
| JITTER_Y1_X2 | 13:12 | 2 | U02 | — |
| JITTER_Y1_X1 | 11:10 | 2 | U02 | — |
| JITTER_Y1_X0 | 9:8 | 2 | U02 | — |
| JITTER_Y0_X3 | 7:6 | 2 | U02 | — |
| JITTER_Y0_X2 | 5:4 | 2 | U02 | — |
| JITTER_Y0_X1 | 3:2 | 2 | U02 | — |
| JITTER_Y0_X0 | 1:0 | 2 | U02 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:599` | `MWV207_EMIT` | `0x0` |

## `0x382` — `MWV207REG_RA_CONTROL`

- 地址：`0x00E08`　驱动号：`0x382`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFF1113F5 ResetValue=0x17000031`
- 厂商定义位置：`jmgpu_protocol.h:306`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| SUB_SAMPLE_MASK_LOCATION | 31:30 | 2 | U02 | SUB_Z=0x0, Z=0x1, W=0x2 |
| ZSELECT | 29:28 | 2 | U02 | READ_WRITE=0x0, READ=0x1, DISABLED=0x2 |
| QUAD_DIRECTION | 27:27 | 1 | U01 | ZIG_ZAG=0x0, CYCLE=0x1 |
| SHADER_W | 26:26 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| SHADER_SUB_SAMPLES | 25:25 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| SHADER_Z | 24:24 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| CENTROID_Z | 20:20 | 1 | U01 | AVERAGE=0x0, CENTROID=0x1 |
| ZMODE | 16:16 | 1 | U01 | AUTOMATIC=0x0, FLOAT=0x1 |
| DISABLE_ZCONTROL | 12:12 | 1 | U01 | ENABLED=0x0, DISABLED=0x1 |
| WALKER | 9:8 | 2 | U02 | AUTOMATIC=0x0, HORIZONTAL=0x1, VERTICAL=0x2, HIERARCHICAL=0x3 |
| DEPTH_RELAXATION | 7:4 | 4 | U04 | DEFAULT=0x3 |
| SMALL_BATCH | 2:2 | 1 | U01 | ENABLED=0x0, DISABLED=0x1 |
| EARLY_EARLY_Z | 0:0 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:1007` | `MWV207_EMIT` | `ra_control \|             (ALIGN_BIT((uint32_t) (DISABLE), 0x2) & (0x4)) \|             (ALIGN_BIT((uint32_t) (true), 0x1A) & (0x4000000)) \|             (ALIGN_BIT((uint32_t) (true), 0x18) & (0x1000000))` |
| `mwv207_draw.c:1018` | `MWV207_EMIT` | `ra_control \|             (ALIGN_BIT(0x1, 0x1E) & (0xC0000000)) \|             (ALIGN_BIT((uint32_t) (DISABLE), 0x1C) & (0x30000000)) \|             (ALIGN_BIT((uint32_t) (use_w), 0x1A) & (0x4000000)) \|             (ALIGN_BIT((uint32_t) (fs->useFragCoord), 0x18) & (0x1000000))` |

## `0x383` — `MWV207REG_RA_MEDIUMP_MASK`

- 地址：`0x00E0C`　驱动号：`0x383`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_protocol.h:445`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| WMASK | 31:16 | 16 | U16 | — |
| ATTRIBUTE_MASK | 15:0 | 16 | U16 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:606` | `MWV207_EMIT` | `0x00000000` |

## `0x388` — `MWV207REG_RA_CONTROL_HZ`

- 地址：`0x00E20`　驱动号：`0x388`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x00117131 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_protocol.h:468`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| MSAA | 20:20 | 1 | U01 | NORMAL=0x0, PRE_PASS=0x1 |
| KILL | 16:16 | 1 | U01 | NONE=0x0, HZ_PRE_PASS=0x1 |
| COMPARE | 14:12 | 3 | U03 | NEVER=0x0, LESS=0x1, EQUAL=0x2, LESS_EQUAL=0x3, GREATER=0x4, NOT_EQUAL=0x5, GREATER_EQUAL=0x6, ALWAYS=0x7 |
| LEVEL1 | 8:8 | 1 | U01 | TILE2X2=0x0, TILE4X4=0x1 |
| MODE1 | 5:4 | 2 | U02 | DISABLED=0x0, READ_ONLY=0x1, ENABLED=0x2 |
| FORMAT | 0:0 | 1 | U01 | HZ16_MIN_MAX=0x0, HZ24_MIN_MAX=0x1 |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:657` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (ctx->zsa->func), 0xC) & (0x7000))` |

## `0x38d` — `MWV207REG_RA_CONTROL_EX`

- 地址：`0x00E34`　驱动号：`0x38d`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000003F3 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_protocol.h:650`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| FORCED_SAMPLE_COUNT | 9:7 | 3 | U03 | DISABLED=0x0, SAMPLE1=0x1, SAMPLE2=0x2, SAMPLE4=0x3, SAMPLE8=0x4, SAMPLE16=0x5 |
| FORCE_HZ_EZ_CLOCK_ON | 6:6 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| EARLY_DEPTH_IN_APP | 5:5 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| RENDER_ARRAY | 4:4 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| POSITION | 1:0 | 2 | U02 | CENTER=0x0, CENTROID=0x1, SAMPLE=0x2 |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:493` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) ((rtMaxLayers != 0)), 0x4) & (0x10))` |

## `0x390` — `MWV207REG_CENTROIDS`

- 地址：`0x00E40`　驱动号：`0x390`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=4 BLK=4 Count=16 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_protocol.h:244`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| CENTROID_Y_3_7_11_15 | 31:28 | 4 | U04 | — |
| CENTROID_X_3_7_11_15 | 27:24 | 4 | U04 | — |
| CENTROID_Y_2_6_10_14 | 23:20 | 4 | U04 | — |
| CENTROID_X_2_6_10_14 | 19:16 | 4 | U04 | — |
| CENTROID_Y_1_5_9_13 | 15:12 | 4 | U04 | — |
| CENTROID_X_1_5_9_13 | 11:8 | 4 | U04 | — |
| CENTROID_Y_0_4_8_12 | 7:4 | 4 | U04 | — |
| CENTROID_X_0_4_8_12 | 3:0 | 4 | U04 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:600` | `MWV207_EMIT` | `0x4a6e2688` |

## `0x409` — `MWV207REG_PIXEL_SHADER_CONST`

- 地址：`0x01024`　驱动号：`0x409`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000003FF ResetValue=0x00000100`
- 厂商定义位置：`jmgpu_announce.h:3129`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| BASE_OFFSET | 9:0 | 10 | U10 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:204` | `MWV207_EMIT` | `0` |
| `mwv207_shader.c:673` | `MWV207_EMIT` | `(0x400 / 4)` |

## `0x40b` — `MWV207REG_PS_OUTPUT_CONTROL`

- 地址：`0x0102C`　驱动号：`0x40b`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:3167`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| SATURATE7 | 31:31 | 1 | U01 | — |
| ALPHA7 | 30:30 | 1 | U01 | USER=0x0, ONE=0x1 |
| OUTPUT7 | 29:24 | 6 | U06 | — |
| SATURATE6 | 23:23 | 1 | U01 | — |
| ALPHA6 | 22:22 | 1 | U01 | USER=0x0, ONE=0x1 |
| OUTPUT6 | 21:16 | 6 | U06 | — |
| SATURATE5 | 15:15 | 1 | U01 | — |
| ALPHA5 | 14:14 | 1 | U01 | USER=0x0, ONE=0x1 |
| OUTPUT5 | 13:8 | 6 | U06 | — |
| SATURATE4 | 7:7 | 1 | U01 | — |
| ALPHA4 | 6:6 | 1 | U01 | USER=0x0, ONE=0x1 |
| OUTPUT4 | 5:0 | 6 | U06 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:662` | `MWV207_EMIT` | `output_reg[1]` |

## `0x40c` — `MWV207REG_PS_OUTPUT_MODE`

- 地址：`0x01030`　驱动号：`0x40c`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:3257`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| CONTROL7 | 31:28 | 4 | U04 | FLOAT=0x0, INTEGER16=0x1, INTEGER32=0x2, UNSIGNED8=0x3, UNSIGNED16=0x4, SIGNED8=0x5, SIGNED16=0x6, FLOAT32=0x7 …(+2) |
| CONTROL6 | 27:24 | 4 | U04 | FLOAT=0x0, INTEGER16=0x1, INTEGER32=0x2, UNSIGNED8=0x3, UNSIGNED16=0x4, SIGNED8=0x5, SIGNED16=0x6, FLOAT32=0x7 …(+2) |
| CONTROL5 | 23:20 | 4 | U04 | FLOAT=0x0, INTEGER16=0x1, INTEGER32=0x2, UNSIGNED8=0x3, UNSIGNED16=0x4, SIGNED8=0x5, SIGNED16=0x6, FLOAT32=0x7 …(+2) |
| CONTROL4 | 19:16 | 4 | U04 | FLOAT=0x0, INTEGER16=0x1, INTEGER32=0x2, UNSIGNED8=0x3, UNSIGNED16=0x4, SIGNED8=0x5, SIGNED16=0x6, FLOAT32=0x7 …(+2) |
| CONTROL3 | 15:12 | 4 | U04 | FLOAT=0x0, INTEGER16=0x1, INTEGER32=0x2, UNSIGNED8=0x3, UNSIGNED16=0x4, SIGNED8=0x5, SIGNED16=0x6, FLOAT32=0x7 …(+2) |
| CONTROL2 | 11:8 | 4 | U04 | FLOAT=0x0, INTEGER16=0x1, INTEGER32=0x2, UNSIGNED8=0x3, UNSIGNED16=0x4, SIGNED8=0x5, SIGNED16=0x6, FLOAT32=0x7 …(+2) |
| CONTROL1 | 7:4 | 4 | U04 | FLOAT=0x0, INTEGER16=0x1, INTEGER32=0x2, UNSIGNED8=0x3, UNSIGNED16=0x4, SIGNED8=0x5, SIGNED16=0x6, FLOAT32=0x7 …(+2) |
| CONTROL0 | 3:0 | 4 | U04 | FLOAT=0x0, INTEGER16=0x1, INTEGER32=0x2, UNSIGNED8=0x3, UNSIGNED16=0x4, SIGNED8=0x5, SIGNED16=0x6, FLOAT32=0x7 …(+2) |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:233` | `MWV207_EMIT` | `0` |
| `mwv207_draw.c:428` | `MWV207_EMIT` | `output_config` |

## `0x40d` — `MWV207REG_PS_OUTPUT_PRECISION`

- 地址：`0x01034`　驱动号：`0x40d`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x7F7F7F7F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:3398`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| HIGHP3 | 30:30 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| T1_HIGHP_OUTPUT3 | 29:24 | 6 | U06 | — |
| HIGHP2 | 22:22 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| T1_HIGHP_OUTPUT2 | 21:16 | 6 | U06 | — |
| HIGHP1 | 14:14 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| T1_HIGHP_OUTPUT1 | 13:8 | 6 | U06 | — |
| HIGHP0 | 6:6 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| T1_HIGHP_OUTPUT0 | 5:0 | 6 | U06 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:234` | `MWV207_EMIT` | `0` |
| `mwv207_draw.c:429` | `MWV207_EMITS` | `2` |

## `0x410` — `MWV207REG_PS_INPUT_TYPE`

- 地址：`0x01040`　驱动号：`0x410`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=1 BLK=1 Count=2 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:3528`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| VARYING7W | 31:31 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING7Z | 30:30 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING7Y | 29:29 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING7X | 28:28 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING6W | 27:27 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING6Z | 26:26 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING6Y | 25:25 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING6X | 24:24 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING5W | 23:23 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING5Z | 22:22 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING5Y | 21:21 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING5X | 20:20 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING4W | 19:19 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING4Z | 18:18 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING4Y | 17:17 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING4X | 16:16 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING3W | 15:15 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING3Z | 14:14 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING3Y | 13:13 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING3X | 12:12 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING2W | 11:11 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING2Z | 10:10 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING2Y | 9:9 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING2X | 8:8 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING1W | 7:7 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING1Z | 6:6 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING1Y | 5:5 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING1X | 4:4 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING0W | 3:3 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING0Z | 2:2 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING0Y | 1:1 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |
| VARYING0X | 0:0 | 1 | U01 | FLOAT16=0x0, INTEGER16=0x1 |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:739` | `MWV207_EMIT` | `0x00000000` |

## `0x412` — `MWV207REG_PS_INSTRUCTION_PREFETCH`

- 地址：`0x01048`　驱动号：`0x412`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:3860`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PC | 19:0 | 20 | U20 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:241` | `MWV207_EMIT` | `0` |
| `mwv207_shader.c:752` | `MWV207_EMIT` | `0` |

## `0x415` — `MWV207REG_PS_SAMPLE_COVERAGE_CTRL`

- 地址：`0x01054`　驱动号：`0x415`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x99F9998F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:3914`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| MASK_GLOBAL_COVERAGE | 31:31 | 1 | U01 | INVERT=19:19, VALUE=27:27, ENABLED=0x0, MASKED=0x1 |
| GLOBAL_COVERAGE | 28:28 | 1 | U01 | INVERT=16:16, VALUE=24:20, ENABLE=0x1, DISABLE=0x0 |
| MASK_GLOBAL_COVERAGE_VALUE | 27:27 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| GLOBAL_COVERAGE_VALUE | 24:20 | 5 | U05 | ENABLE=0x1, DISABLE=0x0 |
| MASK_GLOBAL_COVERAGE_INVERT | 19:19 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| GLOBAL_COVERAGE_INVERT | 16:16 | 1 | U01 | ENABLE=0x1, DISABLE=0x0 |
| MASK_ALPHA_TO_COVERAGE | 15:15 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| ALPHA_TO_COVERAGE | 12:12 | 1 | U01 | ENABLE=0x1, DISABLE=0x0 |
| MASK_MASK_COVERAGE | 11:11 | 1 | U01 | VALUE=7:7, ENABLED=0x0, MASKED=0x1 |
| MASK_COVERAGE | 8:8 | 1 | U01 | VALUE=3:0, ENABLE=0x1, DISABLE=0x0 |
| MASK_MASK_COVERAGE_VALUE | 7:7 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| MASK_COVERAGE_VALUE | 3:0 | 4 | U04 | ENABLE=0x1, DISABLE=0x0 |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:630` | `MWV207_EMIT` | `((ALIGN_BIT((uint32_t) (coverage_invert), 0x10) & (0x10000)) \| (~(0x10000 \| 0x80000))) &       ((ALIGN_BIT((uint32_t) (sample_mask), 0x14) & (0x1F00000)) \| (~(0x1F00000 \| 0x8000000))) &       ((ALIGN_BIT((uint32_t) (coverage_enable), 0x1C) & (0x10000000)) \| (~(0x10000000 \| 0x80000000)))` |
| `mwv207_draw.c:968` | `MWV207_EMIT` | `((ALIGN_BIT((uint32_t) (ctx->blend_state->blend_state.alpha_to_coverage), 0xC) & (0x1000)) \| (~(0x1000 \| 0x8000)))` |

## `0x416` — `MWV207REG_PS_SAMPLER_BASE`

- 地址：`0x01058`　驱动号：`0x416`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x03FF007F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:4140`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| TXD_OFFSET | 25:16 | 10 | U10 | — |
| OFFSET | 6:0 | 7 | U07 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:203` | `MWV207_EMIT` | `0` |
| `mwv207_shader.c:672` | `MWV207_EMIT` | `0x0` |

## `0x420` — `MWV207REG_PS_UNPACK`

- 地址：`0x01080`　驱动号：`0x420`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=2 BLK=2 Count=4 FieldMask=0x77777777 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:4086`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| VARYING7 | 30:28 | 3 | U03 | — |
| VARYING6 | 26:24 | 3 | U03 | — |
| VARYING5 | 22:20 | 3 | U03 | — |
| VARYING4 | 18:16 | 3 | U03 | — |
| VARYING3 | 14:12 | 3 | U03 | — |
| VARYING2 | 10:8 | 3 | U03 | — |
| VARYING1 | 6:4 | 3 | U03 | — |
| VARYING0 | 2:0 | 3 | U03 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:189` | `MWV207_EMIT` | `0x2` |
| `mwv207_shader.c:732` | `MWV207_EMITS` | `4` |

## `0x424` — `MWV207REG_PS_RELATIVE_END`

- 地址：`0x01090`　驱动号：`0x424`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x001FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:4191`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PC | 20:0 | 21 | U21 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:207` | `MWV207_EMIT` | `(cs->code_size / 4)` |
| `mwv207_shader.c:677` | `MWV207_EMIT` | `(fs->code_size / 4)` |

## `0x425` — `MWV207REG_PS_INSTRUCTION_PREFETCH_RELATIVE_END`

- 地址：`0x01094`　驱动号：`0x425`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:4209`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PC | 19:0 | 20 | U20 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:210` | `MWV207_EMIT` | `(cs->code_size / 4 - 1)` |
| `mwv207_shader.c:680` | `MWV207_EMIT` | `(fs->code_size / 4 - 1)` |

## `0x426` — `MWV207REG_PS_MAX`

- 地址：`0x01098`　驱动号：`0x426`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000F07FF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:4228`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| VIEWPORT_ARRAY_INDEX | 19:16 | 4 | U04 | — |
| RENDER_ARRAY_INDEX_BIT8_10 | 10:8 | 3 | U03 | — |
| RENDER_ARRAY_INDEX | 7:0 | 8 | U08 | BIT8_10=10:8 |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:491` | `MWV207_EMIT` | `rtMaxLayers` |

## `0x427` — `MWV207REG_PSCS_THROTTLE`

- 地址：`0x0109C`　驱动号：`0x427`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x0FFFFF7F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:4257`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| MAX_RUNNING_WORK_GROUPS_MINUS_ONE | 27:16 | 12 | U12 | — |
| WORK_GROUP_COUNT_IN_SHADER_GROUP | 15:8 | 8 | U08 | — |
| ATTRIB_MAX_PAGES | 6:0 | 7 | U07 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:192` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (max_local_memory_size >> 10), 0x0) & (0x7F)) \|          (ALIGN_BIT((uint32_t) (0), 0x8) & (0xFF00)) \|          (ALIGN_BIT((uint32_t) (0), 0x10) & (0xFFF0000))` |
| `mwv207_compute.c:197` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (max_local_memory_size >> 10), 0x0) & (0x7F)) \|          (ALIGN_BIT((uint32_t) (work_group_count_per_shader_group), 0x8) & (0xFF00)) \|          (ALIGN_BIT((uint32_t) (align(max_running_work_groups, 2) * 2 - 1), 0x10) & (0xFFF0000))` |

## `0x440` — `MWV207REG_GS_CONTROL`

- 地址：`0x01100`　驱动号：`0x440`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x3F37F3F1 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:2172`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| LAST_ATTRIBUTE | 29:28 | 2 | U02 | FOUR=0x0, ONE=0x1, TWO=0x2, THREE=0x3 |
| HAS_INSTANCE_ID | 27:27 | 1 | U01 | NO=0x0, YES=0x1 |
| HAS_PRIMITIVE_ID | 26:26 | 1 | U01 | NO=0x0, YES=0x1 |
| HAS_STREAM_OUT | 25:25 | 1 | U01 | — |
| HAS_RESTART | 24:24 | 1 | U01 | — |
| INPUT_START | 21:20 | 2 | U02 | R0Z=0x0, R1X=0x1, R1Z=0x2, R2X=0x3 |
| TEMP_COUNT | 18:12 | 7 | U07 | — |
| OUTPUT_COUNT | 9:4 | 6 | U06 | — |
| MODE | 0:0 | 1 | U01 | BYPASS=0x0, ENABLE=0x1 |

驱动写入点（3 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:226` | `MWV207_EMIT` | `0` |
| `mwv207_shader.c:584` | `MWV207_EMIT` | `(ALIGN_BIT(0x1, 0x0) & (0x1)) \|          (ALIGN_BIT((uint32_t) (0), 0x4) & (0x3F0)) \|          (ALIGN_BIT((uint32_t) (gs->num_temps), 0xC) & (0x7F000)) \|          (ALIGN_BIT((uint32_t) (input_start), 0x14) & (0x300000)) \|          (ALIGN_BIT((uint32_t) (gs->gs_use_restart), 0x18) & (0x1000000)) \|          (ALIGN_BIT((uint32_t) (state->nir->info.gs.active_stream_mask != 1), 0x19) & (0x2000000)) \|          (ALIGN_BIT((uint32_t) (gs->gs_use_primitive_id), 0x1A) & (0x4000000)) \|          (ALIGN_BIT((uint32_t) (gs->gs_use_invocation_id), 0x1B) & (0x8000000)) \|          (ALIGN_BIT((uint32_t) (0x0), 0x1C) & (0x30000000))` |
| `mwv207_shader.c:627` | `MWV207_EMIT` | `0` |

## `0x441` — `MWV207REG_GS_TYPE`

- 地址：`0x01104`　驱动号：`0x441`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x0000000F ResetValue=0x00000001`
- 厂商定义位置：`jmgpu_announce.h:2257`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| OUTPUT_TYPE | 3:0 | 4 | U04 | POINT_LIST=0x1, LINE_LIST=0x2, LINE_STRIP=0x3, TRIANGLE_LIST=0x4, TRIANGLE_STRIP=0x5, TRIANGLE_FAN=0x6, LINE_LOOP=0x7, RECTANGLE=0x8 …(+5) |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:595` | `MWV207_EMIT` | `mwv207_gs_type[state->nir->info.gs.output_primitive]` |

## `0x442` — `MWV207REG_GS_COUNTERS`

- 地址：`0x01108`　驱动号：`0x442`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x01FF007F ResetValue=0x01000001`
- 厂商定义位置：`jmgpu_announce.h:2287`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| VERTEX_COUNT | 24:16 | 9 | U09 | — |
| INSTANCE_COUNT | 6:0 | 7 | U07 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:598` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (state->nir->info.gs.invocations), 0x0) & (0x7F)) \|          (ALIGN_BIT((uint32_t) (state->nir->info.gs.vertices_out), 0x10) & (0x1FF0000))` |

## `0x443` — `MWV207REG_GS_START`

- 地址：`0x0110C`　驱动号：`0x443`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:2347`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PC | 19:0 | 20 | U20 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:621` | `MWV207_EMIT` | `0` |

## `0x444` — `MWV207REG_GS_RELATIVE_END`

- 地址：`0x01110`　驱动号：`0x444`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x001FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:2367`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PC | 20:0 | 21 | U21 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:622` | `MWV207_EMIT` | `(gs->code_size / 4)` |

## `0x446` — `MWV207REG_GS_INSTRUCTION_PREFETCH`

- 地址：`0x01118`　驱动号：`0x446`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:2405`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PC | 19:0 | 20 | U20 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:750` | `MWV207_EMIT` | `0` |

## `0x447` — `MWV207REG_GS_INSTRUCTION_PREFETCH_RELATIVE_END`

- 地址：`0x0111C`　驱动号：`0x447`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:2424`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PC | 19:0 | 20 | U20 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:625` | `MWV207_EMIT` | `(gs->code_size / 4 - 1)` |

## `0x448` — `MWV207REG_GS_OUTPUT`

- 地址：`0x01120`　驱动号：`0x448`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=3 BLK=3 Count=8 FieldMask=0x3F3F3F3F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:2313`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| OUTPUT3 | 29:24 | 6 | U06 | — |
| OUTPUT2 | 21:16 | 6 | U06 | — |
| OUTPUT1 | 13:8 | 6 | U06 | — |
| OUTPUT0 | 5:0 | 6 | U06 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:602` | `MWV207_EMIT` | `0` |

## `0x450` — `MWV207REG_GS_THROTTLE`

- 地址：`0x01140`　驱动号：`0x450`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x07E0FF3F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:2443`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| MIN_PAGES | 26:21 | 6 | U06 | — |
| MAX_THREADS | 15:8 | 8 | U08 | — |
| MAX_PAGES | 5:0 | 6 | U06 | — |

驱动写入点（3 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:227` | `MWV207_EMIT` | `0` |
| `mwv207_shader.c:611` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (max_thread), 0x8) & (0xFF00)) \|          (ALIGN_BIT((uint32_t) (ctx->info.alloc_page[PIPE_SHADER_GEOMETRY]), 0x0) & (0x3F)) \|          (ALIGN_BIT((uint32_t) (ctx->info.alloc_page[PIPE_SHADER_GEOMETRY]), 0x15) & (0x7E00000))` |
| `mwv207_shader.c:628` | `MWV207_EMIT` | `0` |

## `0x451` — `MWV207REG_GS_ATTRIBUTE`

- 地址：`0x01144`　驱动号：`0x451`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFF1FFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:2472`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| META_DATA_OFFSET | 31:20 | 12 | U12 | — |
| THREAD_SIZE | 16:8 | 9 | U09 | — |
| VERTEX_SIZE | 7:0 | 8 | U08 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:604` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (outputs_num << 2), 0x0) & (0xFF)) \|          (ALIGN_BIT((uint32_t) (thread_size), 0x8) & (0x1FF00)) \|          (ALIGN_BIT((uint32_t) (thread_size * max_thread), 0x14) & (0xFFF00000))` |

## `0x452` — `MWV207REG_GS_ATTRIBUTE_EX`

- 地址：`0x01148`　驱动号：`0x452`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x00000FFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:2501`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| GROUP_SIZE | 11:0 | 12 | U12 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:609` | `MWV207_EMIT` | `group_size` |

## `0x453` — `MWV207REG_GS_UNIFORM_BASE`

- 地址：`0x0114C`　驱动号：`0x453`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000003FF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:2520`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| OFFSET | 9:0 | 10 | U10 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_vertex.c:362` | `MWV207_EMIT` | `(offset / 4)` |

## `0x455` — `MWV207REG_GS_SAMPLER_BASE`

- 地址：`0x01154`　驱动号：`0x455`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x03FF007F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:2559`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| TXD_OFFSET | 25:16 | 10 | U10 | — |
| OFFSET | 6:0 | 7 | U07 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:618` | `MWV207_EMIT` | `0x30` |

## `0x457` — `MWV207REG_GS_THROTTLE_EX`

- 地址：`0x0115C`　驱动号：`0x457`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000000FF ResetValue=0x000000FF`
- 厂商定义位置：`jmgpu_announce.h:2610`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| MAX_RMISSES | 7:0 | 8 | U08 | — |

驱动写入点（3 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:228` | `MWV207_EMIT` | `0xff` |
| `mwv207_shader.c:616` | `MWV207_EMIT` | `max_thread` |
| `mwv207_shader.c:629` | `MWV207_EMIT` | `0x000000ff` |

## `0x515` — `MWV207REG_PE_HIERARCHICAL_CONTROL`

- 地址：`0x01454`　驱动号：`0x515`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x00000F0F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_toolkit.h:1283`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| RA_FORMAT | 11:8 | 4 | U04 | OFF=0x0, MIN_D16=0x1, MIN_D24_LOW=0x2, MIN_D24_HIGH=0x3, MIN_D24=0x4, MAX_D16=0x5, MAX_D24_LOW=0x6, MAX_D24_HIGH=0x7 …(+5) |
| FORMAT | 3:0 | 4 | U04 | OFF=0x0, MIN_D16=0x1, MIN_D24_LOW=0x2, MIN_D24_HIGH=0x3, MIN_D24=0x4, MAX_D16=0x5, MAX_D24_LOW=0x6, MAX_D24_HIGH=0x7 …(+5) |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:660` | `MWV207_EMIT` | `0` |

## `0x528` — `MWV207REG_PE_EXTRA_REFERENCE`

- 地址：`0x014A0`　驱动号：`0x528`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFF03FF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_toolkit.h:1458`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| ALPHA_REFERENCE | 31:16 | 16 | U16 | — |
| MASK_ALPHA_REFERENCE | 9:9 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| MASK_BACKSIDE_REFERENCE | 8:8 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| BACKSIDE_REFERENCE | 7:0 | 8 | U08 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:655` | `MWV207_EMIT` | `ctx->zsa->alpha_reference` |
| `mwv207_draw.c:1032` | `MWV207_EMIT` | `ctx->stencil_ref.stencil_back_reference[ctx->rast_state->base.front_ccw]` |

## `0x529` — `MWV207REG_PE_CONFIG`

- 地址：`0x014A4`　驱动号：`0x529`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFBFFFFF ResetValue=0x000E400C`
- 厂商定义位置：`jmgpu_toolkit.h:1666`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| GAMMA_CORRECTION | 31:31 | 1 | U01 | LINEAR=0x0, S_RGB=0x1 |
| MASK_GAMMA_CORRECTION | 30:30 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| MASK_EARLY_ALPHA_KILL | 29:29 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| EARLY_ALPHA_KILL | 28:28 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| MASK_DEPTH_CACHE_CONTROL | 27:27 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| DEPTH_CACHE_CONTROL | 26:26 | 1 | U01 | _16X64=0x0, _8X256=0x1 |
| MASK_COLOR_CACHE_CONTROL | 25:25 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| COLOR_CACHE_CONTROL | 24:24 | 1 | U01 | _16X64=0x0, _8X256=0x1 |
| MASK_STENCIL_FORMAT | 23:23 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| STENCIL_FORMAT | 21:20 | 2 | U02 | D24S8=0x0, S8=0x1, S1=0x2, D32FS8=0x3 |
| SWIZZLE_A | 19:18 | 2 | U02 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3 |
| SWIZZLE_B | 17:16 | 2 | U02 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3 |
| SWIZZLE_G | 15:14 | 2 | U02 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3 |
| SWIZZLE_R | 13:12 | 2 | U02 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3 |
| MASK_SWIZZLE | 11:11 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| MASK_CONVERSION_CONTROL | 10:10 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| COLOR_ADDRESS_MODE | 9:8 | 2 | U02 | TILED=0x0, LINEAR=0x1, SINGLE_BUFFER4X4=0x2, SINGLE_BUFFER8X4=0x3 |
| MASK_COLOR_ADDRESS_MODE | 7:7 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| CONVERSION_CONTROL | 6:5 | 2 | U02 | EXPAND=0x0, ADD_RGB111=0x1, ADD_RGB212=0x2, ADD_RGB737=0x3 |
| MASK_LOGIC_OP | 4:4 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| LOGIC_OP | 3:0 | 4 | U04 | CLEAR=0x0, NOR=0x1, AND_INVERTED=0x2, COPY_INVERTED=0x3, AND_REVERSE=0x4, INVERT=0x5, XOR=0x6, NAND=0x7 …(+8) |

驱动写入点（6 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:269` | `MWV207_EMIT` | `((ALIGN_BIT(0x1, 0x8) & (0x300)) \| (~(0x300 \| 0x80)))` |
| `mwv207_draw.c:272` | `MWV207_EMIT` | `((ALIGN_BIT(0x3, 0x8) & (0x300)) \| (~(0x300 \| 0x80)))` |
| `mwv207_draw.c:339` | `MWV207_EMIT` | `surface->peInfo.pe_config` |
| `mwv207_draw.c:477` | `MWV207_EMIT` | `surface->peInfo.pe_config` |
| `mwv207_draw.c:608` | `MWV207_EMIT` | `pe_config` |
| `mwv207_draw.c:674` | `MWV207_EMIT` | `ctx->blend_state->logic_config` |

## `0x52a` — `MWV207REG_PE_DITHER_LOW`

- 地址：`0x014A8`　驱动号：`0x52a`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0xFFFFFFFF`
- 厂商定义位置：`jmgpu_toolkit.h:1499`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PIXEL_X3_Y1 | 31:28 | 4 | U04 | — |
| PIXEL_X2_Y1 | 27:24 | 4 | U04 | — |
| PIXEL_X1_Y1 | 23:20 | 4 | U04 | — |
| PIXEL_X0_Y1 | 19:16 | 4 | U04 | — |
| PIXEL_X3_Y0 | 15:12 | 4 | U04 | — |
| PIXEL_X2_Y0 | 11:8 | 4 | U04 | — |
| PIXEL_X1_Y0 | 7:4 | 4 | U04 | — |
| PIXEL_X0_Y0 | 3:0 | 4 | U04 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:723` | `MWV207_EMIT` | `0x6e4ca280` |
| `mwv207_draw.c:726` | `MWV207_EMIT` | `0xffffffff` |

## `0x52b` — `MWV207REG_PE_DITHER_HIGH`

- 地址：`0x014AC`　驱动号：`0x52b`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0xFFFFFFFF`
- 厂商定义位置：`jmgpu_toolkit.h:1558`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PIXEL_X3_Y3 | 31:28 | 4 | U04 | — |
| PIXEL_X2_Y3 | 27:24 | 4 | U04 | — |
| PIXEL_X1_Y3 | 23:20 | 4 | U04 | — |
| PIXEL_X0_Y3 | 19:16 | 4 | U04 | — |
| PIXEL_X3_Y2 | 15:12 | 4 | U04 | — |
| PIXEL_X2_Y2 | 11:8 | 4 | U04 | — |
| PIXEL_X1_Y2 | 7:4 | 4 | U04 | — |
| PIXEL_X0_Y2 | 3:0 | 4 | U04 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:724` | `MWV207_EMIT` | `0x5d7f91b3` |
| `mwv207_draw.c:727` | `MWV207_EMIT` | `0xffffffff` |

## `0x52c` — `MWV207REG_PE_BLEND_FACTOR_LOW`

- 地址：`0x014B0`　驱动号：`0x52c`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_toolkit.h:1617`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| GREEN | 31:16 | 16 | U16 | — |
| RED | 15:0 | 16 | U16 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:765` | `MWV207_EMIT` | `colorLow` |

## `0x52d` — `MWV207REG_PE_BLEND_FACTOR_HIGH`

- 地址：`0x014B4`　驱动号：`0x52d`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_toolkit.h:1640`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| ALPHA | 31:16 | 16 | U16 | — |
| BLUE | 15:0 | 16 | U16 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:766` | `MWV207_EMIT` | `colorHigh` |

## `0x52e` — `MWV207REG_PE_BACKFACE_STENCIL`

- 地址：`0x014B8`　驱动号：`0x52e`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x0000FFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_toolkit.h:1903`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| BACKFACE_WRITE_MASK | 15:8 | 8 | U08 | — |
| BACKFACE_READ_MASK | 7:0 | 8 | U08 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:1035` | `MWV207_EMIT` | `ctx->zsa->stencil_back_config[ctx->rast_state->base.front_ccw]` |

## `0x52f` — `MWV207REG_PE_CONFIG_EX`

- 地址：`0x014BC`　驱动号：`0x52f`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x0509FFF9 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_toolkit.h:1929`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| DEPTH_CACHE_CONTROL | 26:26 | 1 | U01 | _128_BCACHELINE=0x0, _256_BCACHELINE=0x1 |
| COLOR_CACHE_CONTROL | 24:24 | 1 | U01 | _128_BCACHELINE=0x0, _256_BCACHELINE=0x1 |
| FULL_TILE_COVERAGE_DISABLE | 19:19 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| DEPTH_VMSAA | 16:16 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| COLOR_VMSAA | 15:15 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| PERF_COUNTER_ENDIAN_CONTROL | 14:13 | 2 | U02 | NO_SWAP=0x0, SWAP_WORD=0x1, SWAP_DWORD=0x2, SWAP_DDWORD=0x3 |
| OQ_ENDIAN_CONTROL | 12:11 | 2 | U02 | NO_SWAP=0x0, SWAP_WORD=0x1, SWAP_DWORD=0x2, SWAP_DDWORD=0x3 |
| FENCE_ENDIAN_CONTROL | 10:9 | 2 | U02 | NO_SWAP=0x0, SWAP_WORD=0x1, SWAP_DWORD=0x2, SWAP_DDWORD=0x3 |
| DEPTH_ENDIAN_CONTROL | 8:7 | 2 | U02 | NO_SWAP=0x0, SWAP_WORD=0x1, SWAP_DWORD=0x2, SWAP_DDWORD=0x3 |
| COLOR_ENDIAN_CONTROL | 6:5 | 2 | U02 | NO_SWAP=0x0, SWAP_WORD=0x1, SWAP_DWORD=0x2, SWAP_DDWORD=0x3 |
| CLOCK_GATING | 4:4 | 1 | U01 | AUTOMATIC=0x0, DISABLE=0x1 |
| BUG1293_FIX_DISABLE | 3:3 | 1 | U01 | AUTOMATIC=0x0, DISABLE=0x1 |
| PIPE_ENABLE | 0:0 | 1 | U01 | BOTH=0x0, SINGLE=0x1 |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:609` | `MWV207_EMIT` | `pe_config_ex` |

## `0x530` — `MWV207REG_PE_ALPHA_CONFIG`

- 地址：`0x014C0`　驱动号：`0x530`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x1DDDFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_toolkit.h:2063`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| DEST_BLEND_ALPHA | 28:26 | 3 | U03 | SRC1_COLOR=0x0, INV_SRC1_COLOR=0x1, SRC1_ALPHA=0x2, INV_SRC1_ALPHA=0x3 |
| DEST_BLEND | 24:22 | 3 | U03 | SRC1_COLOR=0x0, INV_SRC1_COLOR=0x1, SRC1_ALPHA=0x2, INV_SRC1_ALPHA=0x3, ALPHA=28:26 |
| SRC_BLEND_ALPHA | 20:18 | 3 | U03 | SRC1_COLOR=0x0, INV_SRC1_COLOR=0x1, SRC1_ALPHA=0x2, INV_SRC1_ALPHA=0x3 |
| SRC_BLEND | 16:14 | 3 | U03 | SRC1_COLOR=0x0, INV_SRC1_COLOR=0x1, SRC1_ALPHA=0x2, INV_SRC1_ALPHA=0x3, ALPHA=20:18 |
| DUAL_SOURCE_BLEND_MODE | 13:13 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| COLOR_ALPHA_ZERO_NO_READ | 12:12 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| ALPHA_ZERO_NO_READ | 11:11 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| COLOR_ALPHA_ONE_KILL | 10:10 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| ALPHA_ONE_KILL | 9:9 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| COLOR_ALPHA_ZERO_KILL | 8:8 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| ALPHA_ZERO_KILL | 7:7 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| COLOR_ALPHA_ONE_NO_READ | 6:6 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| ALPHA_ONE_NO_READ | 5:5 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| PRE_MULTIPLIED | 4:4 | 1 | U01 | NORMAL=0x0, PRE_MULTIPLIED=0x1 |
| ALPHA_MODE | 3:0 | 4 | U04 | LEGACY=0x0, MULTIPLY=0x1, SCREEN=0x2, OVERLAY=0x3, DARKEN=0x4, LIGHTEN=0x5, HARDLIGHT=0x6, DIFFERENCE=0x7 …(+1) |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:690` | `MWV207_EMIT` | `ctx->blend_state->alpha_config` |

## `0x5af` — `MWV207REG_MC_COLOR_CLEAR64`

- 地址：`0x016BC`　驱动号：`0x5af`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_multiuser.h:1865`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| VALUE | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:320` | `MWV207_EMIT` | `tsInfo->clear_values[1]` |

## `0xe06` — `MWV207REG_MULTI_SAMPLE_CONFIG`

- 地址：`0x03818`　驱动号：`0xe06`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x99FBF1FB ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_engine.h:431`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| MASK_SAMPLE_CENTER | 31:31 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| SAMPLE_CENTER | 28:28 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| MASK_SAMPLE_SHADING_ENABLE | 27:27 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| SAMPLE_SHADING_ENABLE | 24:24 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| MASK_SAMPLE_COUNT | 23:23 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| SAMPLE_COUNT | 22:20 | 3 | U03 | — |
| MASK_VAA | 19:19 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| VAA | 17:16 | 2 | U02 | OFF=0x0, TWO_COLOR=0x1, TWO_COLOR8=0x2, FOUR_COLOR=0x3 |
| MASK_DEPTH | 15:15 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| DEPTH | 14:12 | 3 | U03 | DISABLED=0x0, MATRIX0=0x1, MATRIX1=0x2, MATRIX2=0x3, MATRIX3=0x4 |
| MASK_ENABLE | 8:8 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| ENABLE | 7:4 | 4 | U04 | — |
| MASK_MODE | 3:3 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| MODE | 1:0 | 2 | U02 | OFF=0x0, MSAA2=0x1, MSAA4=0x2, MSAA8=0x3 |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:590` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (sample_mode), 0x0) & (0x3)) \|       (ALIGN_BIT((uint32_t) (sample_enable_bits), 0x4) & (0xF0)) \|       (ALIGN_BIT(0x0, 0xC) & (0x7000)) \|       (ALIGN_BIT(0x0, 0x10) & (0x30000)) \|       (ALIGN_BIT((uint32_t) (3), 0x14) & (0x300000)) \|       (ALIGN_BIT((uint32_t) (DISABLE), 0x18) & (0x1000000)) \|       (ALIGN_BIT((uint32_t) (ENABLE), 0x1C) & (0x10000000))` |

## `0xe07` — `MWV207REG_VARYINGS`

- 地址：`0x0381C`　驱动号：`0xe07`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x0000007F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_engine.h:561`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| COMPONENT_COUNT | 6:0 | 7 | U07 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:231` | `MWV207_EMIT` | `0x22` |
| `mwv207_shader.c:729` | `MWV207_EMIT` | `component_count` |

## `0xe0c` — `MWV207REG_OCCLUSION_DATA`

- 地址：`0x03830`　驱动号：`0xe0c`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_engine.h:979`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| COUNTER | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_query.c:195` | `MWV207_EMIT` | `0x01df5e76` |

## `0xe18` — `MWV207REG_OCCLUSION_CONTROL`

- 地址：`0x03860`　驱动号：`0xe18`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x00000007 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_engine.h:1647`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| MODE | 2:0 | 3 | U03 | COUNT=0x0, BOOLEAN=0x1, BOOLEAN_CONDITIONAL=0x2, COUNT_EARLY=0x3, BOOLEAN_EARLY=0x4, BOOLEAN_CONDITIONAL_EARLY=0x5, RENDER=0x6, RENDER_IN_RA=0x7 |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_query.c:93` | `MWV207_EMIT` | `(ALIGN_BIT(0x7, 0x0) & (0x7))` |
| `mwv207_query.c:96` | `MWV207_EMIT` | `(ALIGN_BIT(0x6, 0x0) & (0x7))` |

## `0xe1b` — `MWV207REG_FENCE_DATA`

- 地址：`0x0386C`　驱动号：`0xe1b`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_engine.h:1733`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| DATA | 31:0 | 32 | U32 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_query.c:112` | `MWV207_EMIT` | `time_stamp_low` |
| `mwv207_query.c:205` | `MWV207_EMIT` | `time_stamp_low` |

## `0xe22` — `MWV207REG_SEMANTIC_LOCATION`

- 地址：`0x03888`　驱动号：`0xe22`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x7F7F7F7F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_engine.h:2202`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PRIMITIVE_ID | 30:24 | 7 | U07 | — |
| RENDER_ARRAY | 22:16 | 7 | U07 | — |
| POINT_COORD | 14:8 | 7 | U07 | — |
| POINT_SIZE | 6:0 | 7 | U07 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:239` | `MWV207_EMIT` | `0x007f0000` |
| `mwv207_shader.c:362` | `MWV207_EMIT` | `0x7f000000 \|        (ALIGN_BIT((uint32_t) (render_arrary_ofs), 0x10) & (0x7F0000)) \|        (ALIGN_BIT((uint32_t) (pcoord_varying_comp_ofs), 0x8) & (0x7F00)) \|        (ALIGN_BIT((uint32_t) (point_size_ops), 0x0) & (0x7F))` |

## `0xe23` — `MWV207REG_DEPTH_SLICE`

- 地址：`0x0388C`　驱动号：`0xe23`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x1FFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_engine.h:2257`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| SLICE | 28:0 | 29 | U29 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:474` | `MWV207_EMIT` | `surface->size` |
| `mwv207_draw.c:488` | `MWV207_EMIT` | `0` |

## `0xe26` — `MWV207REG_FENCE_DATA_HIGH`

- 地址：`0x03898`　驱动号：`0xe26`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_engine.h:2355`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| DATA | 31:0 | 32 | U32 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_query.c:111` | `MWV207_EMIT` | `time_stamp_high` |
| `mwv207_query.c:204` | `MWV207_EMIT` | `time_stamp_high` |

## `0xe27` — `MWV207REG_DRAW_ID`

- 地址：`0x0389C`　驱动号：`0xe27`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_engine.h:2374`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| DATA | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:907` | `MWV207_EMIT` | `draw_count++` |

## `0xe28` — `MWV207REG_COLOR_SLICE`

- 地址：`0x038A0`　驱动号：`0xe28`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=3 BLK=3 Count=8 FieldMask=0x3FFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_engine.h:2237`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| SLICE | 29:0 | 30 | U30 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:334` | `MWV207_EMIT` | `surface->size` |

## `0xe30` — `MWV207REG_SEMANTICS`

- 地址：`0x038C0`　驱动号：`0xe30`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=4 BLK=4 Count=16 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_engine.h:2036`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| LOCATION1_W | 31:30 | 2 | U02 | CENTER=0x0, CENTROID=0x1, SAMPLE=0x2 |
| INTERPOLATION1_W | 29:28 | 2 | U02 | SMOOTH=0x0, NO_PERSPECTIVE=0x1, FLAT=0x2, INTEGER=0x3 |
| LOCATION1_Z | 27:26 | 2 | U02 | CENTER=0x0, CENTROID=0x1, SAMPLE=0x2 |
| INTERPOLATION1_Z | 25:24 | 2 | U02 | SMOOTH=0x0, NO_PERSPECTIVE=0x1, FLAT=0x2, INTEGER=0x3 |
| LOCATION1_Y | 23:22 | 2 | U02 | CENTER=0x0, CENTROID=0x1, SAMPLE=0x2 |
| INTERPOLATION1_Y | 21:20 | 2 | U02 | SMOOTH=0x0, NO_PERSPECTIVE=0x1, FLAT=0x2, INTEGER=0x3 |
| LOCATION1_X | 19:18 | 2 | U02 | CENTER=0x0, CENTROID=0x1, SAMPLE=0x2 |
| INTERPOLATION1_X | 17:16 | 2 | U02 | SMOOTH=0x0, NO_PERSPECTIVE=0x1, FLAT=0x2, INTEGER=0x3 |
| LOCATION0_W | 15:14 | 2 | U02 | CENTER=0x0, CENTROID=0x1, SAMPLE=0x2 |
| INTERPOLATION0_W | 13:12 | 2 | U02 | SMOOTH=0x0, NO_PERSPECTIVE=0x1, FLAT=0x2, INTEGER=0x3 |
| LOCATION0_Z | 11:10 | 2 | U02 | CENTER=0x0, CENTROID=0x1, SAMPLE=0x2 |
| INTERPOLATION0_Z | 9:8 | 2 | U02 | SMOOTH=0x0, NO_PERSPECTIVE=0x1, FLAT=0x2, INTEGER=0x3 |
| LOCATION0_Y | 7:6 | 2 | U02 | CENTER=0x0, CENTROID=0x1, SAMPLE=0x2 |
| INTERPOLATION0_Y | 5:4 | 2 | U02 | SMOOTH=0x0, NO_PERSPECTIVE=0x1, FLAT=0x2, INTEGER=0x3 |
| LOCATION0_X | 3:2 | 2 | U02 | CENTER=0x0, CENTROID=0x1, SAMPLE=0x2 |
| INTERPOLATION0_X | 1:0 | 2 | U02 | SMOOTH=0x0, NO_PERSPECTIVE=0x1, FLAT=0x2, INTEGER=0x3 |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:733` | `MWV207_EMITS` | `16` |

## `0xe42` — `MWV207REG_CLUSTER_CHECKER_BOARD`

- 地址：`0x03908`　驱动号：`0xe42`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x00000007 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_engine.h:2451`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| SIZE | 2:0 | 3 | U03 | BLOCK16X16=0x0, BLOCK16X32=0x1, BLOCK32X32=0x2, BLOCK32X64=0x3, BLOCK64X64=0x4, BLOCK64X128=0x5, BLOCK128X128=0x6 |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_cmd.c:47` | `MWV207_EMIT` | `0x2` |

## `0xe80` — `MWV207REG_MULTI_CHIP_CONTROL`

- 地址：`0x03A00`　驱动号：`0xe80`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xF1FFFF77 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_deploy.h:29`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| ENABLE_GPU_ENABLE | 31:28 | 4 | U04 | — |
| SIMPLIFIED_CHECKERBOARD_MODE | 24:24 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| GPU3_MAPPING_SET3 | 23:23 | 1 | U01 | MAPPED=0x1, UNMAPPED=0x0 |
| GPU3_MAPPING_SET2 | 22:22 | 1 | U01 | MAPPED=0x1, UNMAPPED=0x0 |
| GPU3_MAPPING_SET1 | 21:21 | 1 | U01 | MAPPED=0x1, UNMAPPED=0x0 |
| GPU3_MAPPING_SET0 | 20:20 | 1 | U01 | MAPPED=0x1, UNMAPPED=0x0 |
| GPU2_MAPPING_SET3 | 19:19 | 1 | U01 | MAPPED=0x1, UNMAPPED=0x0 |
| GPU2_MAPPING_SET2 | 18:18 | 1 | U01 | MAPPED=0x1, UNMAPPED=0x0 |
| GPU2_MAPPING_SET1 | 17:17 | 1 | U01 | MAPPED=0x1, UNMAPPED=0x0 |
| GPU2_MAPPING_SET0 | 16:16 | 1 | U01 | MAPPED=0x1, UNMAPPED=0x0 |
| GPU1_MAPPING_SET3 | 15:15 | 1 | U01 | MAPPED=0x1, UNMAPPED=0x0 |
| GPU1_MAPPING_SET2 | 14:14 | 1 | U01 | MAPPED=0x1, UNMAPPED=0x0 |
| GPU1_MAPPING_SET1 | 13:13 | 1 | U01 | MAPPED=0x1, UNMAPPED=0x0 |
| GPU1_MAPPING_SET0 | 12:12 | 1 | U01 | MAPPED=0x1, UNMAPPED=0x0 |
| GPU0_MAPPING_SET3 | 11:11 | 1 | U01 | MAPPED=0x1, UNMAPPED=0x0 |
| GPU0_MAPPING_SET2 | 10:10 | 1 | U01 | MAPPED=0x1, UNMAPPED=0x0 |
| GPU0_MAPPING_SET1 | 9:9 | 1 | U01 | MAPPED=0x1, UNMAPPED=0x0 |
| GPU0_MAPPING_SET0 | 8:8 | 1 | U01 | MAPPED=0x1, UNMAPPED=0x0 |
| LOAD_BALANCE_MODE | 6:4 | 3 | U03 | SET1=0x0, SET2=0x1, SET3=0x2, SET4=0x3, SET8=0x7 |
| MODE | 2:0 | 3 | U03 | OFF=0x0, WINDOW=0x1, INTERLEAVED16X16=0x2, INTERLEAVED32X32=0x3, INTERLEAVED64X64=0x4, INTERLEAVED128X64=0x5, INTERLEAVED128X128=0x6, INTERLEAVED=0x7 |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_cmd.c:39` | `MWV207_EMIT` | `0x10000000` |

## `0x5002` — `MWV207REG_BLT_SRC_CONFIG`

- 地址：`0x14008`　驱动号：`0x5002`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:48`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| MULTI_TILING | 31:31 | 1 | U01 | DISABLED=0x0, ENABLE=0x1 |
| SUPER_TILING | 30:30 | 1 | U01 | DISABLED=0x0, ENABLE=0x1 |
| TILING | 29:29 | 1 | U01 | DISABLED=0x0, ENABLE=0x1 |
| MSAA | 28:27 | 2 | U02 | NONE=0x0, MSAA2X1=0x1, MSAA1X2=0x2, MSAA4=0x3 |
| FORMAT | 26:21 | 6 | U06 | X4R4G4B4=0x0, A4R4G4B4=0x1, X1R5G5B5=0x2, A1R5G5B5=0x3, R5G6B5=0x4, X8R8G8B8=0x5, A8R8G8B8=0x6, YUY2=0x7 …(+39) |
| STRIDE | 20:0 | 21 | U21 | — |

驱动写入点（6 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:248` | `MWV207_EMIT` | `config` |
| `mwv207_blit.c:407` | `MWV207_EMIT` | `levelInfo->config` |
| `mwv207_blit.c:499` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (src_level->stride), 0x0) & (0x1FFFFF)) \|          (ALIGN_BIT((uint32_t) (src_blit_desc.format), 0x15) & (0x7E00000)) \|          (ALIGN_BIT((uint32_t) (src_msaa_mode), 0x1B) & (0x18000000))` |
| `mwv207_blit.c:510` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (src_level->stride), 0x0) & (0x1FFFFF)) \|          (ALIGN_BIT((uint32_t) (src_blit_desc.format), 0x15) & (0x7E00000)) \|          (ALIGN_BIT((uint32_t) ((src->mode == MWV207_SUPERTILED)), 0x1D) & (0x20000000)) \|          (ALIGN_BIT((uint32_t) ((src->mode == MWV207_SUPERTILED)), 0x1E) & (0x40000000))` |
| `mwv207_blit.c:519` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (src_level->stride), 0x0) & (0x1FFFFF)) \|          (ALIGN_BIT((uint32_t) (src_blit_desc.format), 0x15) & (0x7E00000)) \|          (ALIGN_BIT((uint32_t) (src_msaa_mode), 0x1B) & (0x18000000)) \|          (ALIGN_BIT((uint32_t) ((src->mode == MWV207_SUPERTILED)), 0x1D) & (0x20000000)) \|          (ALIGN_BIT((uint32_t) ((src->mode == MWV207_SUPERTILED)), 0x1E) & (0x40000000))` |
| `mwv207_blit.c:651` | `MWV207_EMIT` | `src_level->config` |

## `0x5003` — `MWV207REG_BLT_SRC_CONFIG_EX`

- 地址：`0x1400C`　驱动号：`0x5003`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x03FFFFFF ResetValue=0x0001C800`
- 厂商定义位置：`jmgpu_medial.h:159`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| COLOR128 | 25:25 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| COLOR_YMAJOR | 24:24 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| MSAA_COMPRESSION | 23:23 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| TILE_MODE | 22:21 | 2 | U02 | TILED4X4=0x0, SUPER_TILED_XMAJOR=0x1, SUPER_TILED_YMAJOR=0x2, TILED8X8=0x3 |
| TILE_STATUS4_BIT | 20:20 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| COLOR64 | 19:19 | 1 | U01 | COLOR16OR32=0x0, COLOR64=0x1 |
| UV_SWIZZLE | 18:18 | 1 | U01 | DISABLED=0x0, ENABLE=0x1 |
| CACHE_MODE | 17:17 | 1 | U01 | DISABLED=0x0, ENABLE=0x1 |
| SWIZZLE_ALPHA | 16:15 | 2 | U02 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3 |
| SWIZZLE_BLUE | 14:13 | 2 | U02 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3 |
| SWIZZLE_GREEN | 12:11 | 2 | U02 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3 |
| SWIZZLE_RED | 10:9 | 2 | U02 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3 |
| SRGB | 8:8 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| COLOR_FORMAT | 7:4 | 4 | U04 | ARGB4=0x0, A1RGB5=0x1, R5G6B5=0x2, ARGB8=0x3, RGB8=0x4, Z24S8=0x5, Z24=0x6, VAA16=0x7 …(+8) |
| ADDR_REMAP | 3:3 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| DEPTH_ADDR_REMAP | 2:2 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| COMPRESSION_EN_C | 1:1 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| FAST_CLEAR | 0:0 | 1 | U01 | DISABLED=0x0, ENABLE=0x1 |

驱动写入点（6 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:249` | `MWV207_EMIT` | `src_configex` |
| `mwv207_blit.c:408` | `MWV207_EMIT` | `src_configex \|       (ALIGN_BIT((uint32_t) (fast_clear), 0x0) & (0x1))` |
| `mwv207_blit.c:503` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) ((util_format_get_blocksize(src->base.format) == 8)), 0x13) & (0x80000)) \|          (ALIGN_BIT((uint32_t) (src_blit_desc.srgb), 0x8) & (0x100)) \|          (ALIGN_BIT((uint32_t) ((CACHE_MODE == 0x1)), 0x11) & (0x20000))` |
| `mwv207_blit.c:515` | `MWV207_EMIT` | `src_configex \|          (ALIGN_BIT((uint32_t) (src_blit_desc.srgb), 0x8) & (0x100))` |
| `mwv207_blit.c:525` | `MWV207_EMIT` | `src_configex \|          (ALIGN_BIT((uint32_t) (src_blit_desc.srgb), 0x8) & (0x100))` |
| `mwv207_blit.c:652` | `MWV207_EMIT` | `src_configex` |

## `0x5004` — `MWV207REG_BLT_SRC_TILE_STATUS_ADDRESS`

- 地址：`0x14010`　驱动号：`0x5004`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:338`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| ADDRESS | 31:0 | 32 | U32 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:169` | `MWV207_EMIT` | `0` |
| `mwv207_blit.c:649` | `MWV207_EMIT` | `0` |

## `0x5005` — `MWV207REG_BLT_SRC_ORIGIN`

- 地址：`0x14014`　驱动号：`0x5005`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:357`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| Y | 31:16 | 16 | U16 | — |
| X | 15:0 | 16 | U16 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:127` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (src_startx), 0x0) & (0xFFFF)) \|                (ALIGN_BIT((uint32_t) (src_starty), 0x10) & (0xFFFF0000))` |
| `mwv207_blit.c:284` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (0), 0x0) & (0xFFFF)) \|          (ALIGN_BIT((uint32_t) (0), 0x10) & (0xFFFF0000))` |

## `0x5006` — `MWV207REG_BLT_DEST_ADDRESS`

- 地址：`0x14018`　驱动号：`0x5006`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=1 BLK=1 Count=2 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:383`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| ADDRESS | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:654` | `MWV207_EMIT` | `0` |

## `0x5008` — `MWV207REG_BLT_DEST_TILE_STATUS_ADDRESS`

- 地址：`0x14020`　驱动号：`0x5008`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:402`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| ADDRESS | 31:0 | 32 | U32 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:170` | `MWV207_EMIT` | `0` |
| `mwv207_blit.c:655` | `MWV207_EMIT` | `0` |

## `0x5009` — `MWV207REG_BLT_DEST_CONFIG`

- 地址：`0x14024`　驱动号：`0x5009`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:421`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| MULTI_TILING | 31:31 | 1 | U01 | DISABLED=0x0, ENABLE=0x1 |
| SUPER_TILING | 30:30 | 1 | U01 | DISABLED=0x0, ENABLE=0x1 |
| TILING | 29:29 | 1 | U01 | DISABLED=0x0, ENABLE=0x1 |
| MSAA | 28:27 | 2 | U02 | NONE=0x0, MSAA2X1=0x1, MSAA1X2=0x2, MSAA4=0x3 |
| FORMAT | 26:21 | 6 | U06 | X4R4G4B4=0x0, A4R4G4B4=0x1, X1R5G5B5=0x2, A1R5G5B5=0x3, R5G6B5=0x4, X8R8G8B8=0x5, A8R8G8B8=0x6, YUY2=0x7 …(+39) |
| STRIDE | 20:0 | 21 | U21 | — |

驱动写入点（4 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:253` | `MWV207_EMIT` | `config` |
| `mwv207_blit.c:399` | `MWV207_EMIT` | `levelInfo->config` |
| `mwv207_blit.c:550` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (dst_level->stride), 0x0) & (0x1FFFFF)) \|       (ALIGN_BIT((uint32_t) (dst_blit_desc.format), 0x15) & (0x7E00000)) \|       (ALIGN_BIT((uint32_t) (dst_msaa_mode), 0x1B) & (0x18000000)) \|       (ALIGN_BIT((uint32_t) ((dst->mode == MWV207_SUPERTILED)), 0x1D) & (0x20000000)) \|       (ALIGN_BIT((uint32_t) ((dst->mode == MWV207_SUPERTILED)), 0x1E) & (0x40000000))` |
| `mwv207_blit.c:656` | `MWV207_EMIT` | `src_level->config` |

## `0x500a` — `MWV207REG_BLT_DEST_CONFIG_EX`

- 地址：`0x14028`　驱动号：`0x500a`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x0001C800`
- 厂商定义位置：`jmgpu_medial.h:532`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| DESTINATION_CONTROL | 31:31 | 1 | U01 | NOT_READ=0x0, READ=0x1 |
| VMSAA | 30:30 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| COLOR_YMAJOR | 29:29 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| MSAA_COMPRESSION | 28:28 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| TILE_MODE | 27:26 | 2 | U02 | TILED4X4=0x0, SUPER_TILED_XMAJOR=0x1, SUPER_TILED_YMAJOR=0x2, TILED8X8=0x3 |
| TILE_STATUS4_BIT | 25:25 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| CONVERSION_CONTROL | 24:23 | 2 | U02 | EXPAND=0x0, ADD_RGB111=0x1, ADD_RGB212=0x2, ADD_RGB737=0x3 |
| BLOCK_SIZE_CONFIG | 22:22 | 1 | U01 | DEFAULT=0x0, CUSTOMIZE=0x1 |
| DISABLE_SET_SWIZZLE | 21:21 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| COLOR64 | 20:20 | 1 | U01 | COLOR16OR32=0x0, COLOR64=0x1 |
| FLIP_Y | 19:19 | 1 | U01 | DISABLED=0x0, ENABLE=0x1 |
| UV_SWIZZLE | 18:18 | 1 | U01 | DISABLED=0x0, ENABLE=0x1 |
| CACHE_MODE | 17:17 | 1 | U01 | DISABLED=0x0, ENABLE=0x1 |
| SWIZZLE_ALPHA | 16:15 | 2 | U02 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3 |
| SWIZZLE_BLUE | 14:13 | 2 | U02 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3 |
| SWIZZLE_GREEN | 12:11 | 2 | U02 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3 |
| SWIZZLE_RED | 10:9 | 2 | U02 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3 |
| SRGB | 8:8 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| COLOR_FORMAT | 7:4 | 4 | U04 | ARGB4=0x0, A1RGB5=0x1, R5G6B5=0x2, ARGB8=0x3, RGB8=0x4, Z24S8=0x5, Z24=0x6, VAA16=0x7 …(+8) |
| ADDR_REMAP | 3:3 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| COLOR128 | 2:2 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| COMPRESSION_EN_C | 1:1 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| FAST_CLEAR | 0:0 | 1 | U01 | DISABLED=0x0, ENABLE=0x1 |

驱动写入点（4 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:254` | `MWV207_EMIT` | `dst_configex` |
| `mwv207_blit.c:400` | `MWV207_EMIT` | `dst_configex \|       (ALIGN_BIT((uint32_t) (fast_clear), 0x0) & (0x1)) \|       (ALIGN_BIT((uint32_t) (dst->sample_info.is_msaa ? ENABLE : DISABLE), 0x1E) & (0x40000000))` |
| `mwv207_blit.c:556` | `MWV207_EMIT` | `dst_configex \|       (ALIGN_BIT((uint32_t) (dst_blit_desc.srgb), 0x8) & (0x100)) \|       (ALIGN_BIT(0x1, 0x16) & (0x400000)) \|       (ALIGN_BIT((uint32_t) (info.flip_y), 0x13) & (0x80000))` |
| `mwv207_blit.c:657` | `MWV207_EMIT` | `src_level->dst_configex` |

## `0x500b` — `MWV207REG_BLT_DEST_ORIGIN`

- 地址：`0x1402C`　驱动号：`0x500b`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:753`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| Y | 31:16 | 16 | U16 | — |
| X | 15:0 | 16 | U16 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:131` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (dst_startx), 0x0) & (0xFFFF)) \|                (ALIGN_BIT((uint32_t) (dst_starty), 0x10) & (0xFFFF0000))` |
| `mwv207_blit.c:288` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (0), 0x0) & (0xFFFF)) \|          (ALIGN_BIT((uint32_t) (0), 0x10) & (0xFFFF0000))` |

## `0x500c` — `MWV207REG_BLT_WINDOW_SIZE`

- 地址：`0x14030`　驱动号：`0x500c`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:779`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| HEIGHT | 31:16 | 16 | U16 | — |
| WIDTH | 15:0 | 16 | U16 | — |

驱动写入点（3 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:135` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (task_width), 0x0) & (0xFFFF)) \|                (ALIGN_BIT((uint32_t) (task_height), 0x10) & (0xFFFF0000))` |
| `mwv207_blit.c:292` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (extra_size / sizeof(uint32_t)), 0x0) & (0xFFFF)) \|          (ALIGN_BIT((uint32_t) (1), 0x10) & (0xFFFF0000))` |
| `mwv207_blit.c:662` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (src_level->width), 0x0) & (0xFFFF)) \|       (ALIGN_BIT((uint32_t) (src_level->height), 0x10) & (0xFFFF0000))` |

## `0x500d` — `MWV207REG_BLT_SRC_CLEAR_VALUE`

- 地址：`0x14034`　驱动号：`0x500d`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:805`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| VALUE | 31:0 | 32 | U32 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:434` | `MWV207_EMITS` | `2` |
| `mwv207_blit.c:544` | `MWV207_EMIT` | `src->ts.clear_values[0]` |

## `0x500f` — `MWV207REG_BLT_DST_CLEAR_VALUE`

- 地址：`0x1403C`　驱动号：`0x500f`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:844`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| VALUE | 31:0 | 32 | U32 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:433` | `MWV207_EMITS` | `2` |
| `mwv207_blit.c:797` | `MWV207_EMITS` | `2` |

## `0x5011` — `MWV207REG_BLT_NEW_CLEAR_VALUE`

- 地址：`0x14044`　驱动号：`0x5011`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:883`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| VALUE | 31:0 | 32 | U32 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:258` | `MWV207_EMITS` | `2` |
| `mwv207_blit.c:425` | `MWV207_EMITS` | `2` |

## `0x5013` — `MWV207REG_BLT_CLEAR_MASK`

- 地址：`0x1404C`　驱动号：`0x5013`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:922`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| MASK | 31:0 | 32 | U32 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:259` | `MWV207_EMIT` | `0xFFFFFFFF` |
| `mwv207_blit.c:426` | `MWV207_EMITS` | `2` |

## `0x5015` — `MWV207REG_BLT_COPY_SIZE`

- 地址：`0x14054`　驱动号：`0x5015`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:1080`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| SIZE | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:180` | `MWV207_EMIT` | `(uint32_t)copy_size` |

## `0x5016` — `MWV207REG_BLT_DITHER_LOW`

- 地址：`0x14058`　驱动号：`0x5016`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:962`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PIXEL_X3_Y1 | 31:28 | 4 | U04 | — |
| PIXEL_X2_Y1 | 27:24 | 4 | U04 | — |
| PIXEL_X1_Y1 | 23:20 | 4 | U04 | — |
| PIXEL_X0_Y1 | 19:16 | 4 | U04 | — |
| PIXEL_X3_Y0 | 15:12 | 4 | U04 | — |
| PIXEL_X2_Y0 | 11:8 | 4 | U04 | — |
| PIXEL_X1_Y0 | 7:4 | 4 | U04 | — |
| PIXEL_X0_Y0 | 3:0 | 4 | U04 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:588` | `MWV207_EMIT` | `~0U` |
| `mwv207_blit.c:640` | `MWV207_EMIT` | `~0U` |

## `0x5017` — `MWV207REG_BLT_DITHER_HIGH`

- 地址：`0x1405C`　驱动号：`0x5017`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:1021`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PIXEL_X3_Y3 | 31:28 | 4 | U04 | — |
| PIXEL_X2_Y3 | 27:24 | 4 | U04 | — |
| PIXEL_X1_Y3 | 23:20 | 4 | U04 | — |
| PIXEL_X0_Y3 | 19:16 | 4 | U04 | — |
| PIXEL_X3_Y2 | 15:12 | 4 | U04 | — |
| PIXEL_X2_Y2 | 11:8 | 4 | U04 | — |
| PIXEL_X1_Y2 | 7:4 | 4 | U04 | — |
| PIXEL_X0_Y2 | 3:0 | 4 | U04 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:589` | `MWV207_EMIT` | `~0U` |
| `mwv207_blit.c:641` | `MWV207_EMIT` | `~0U` |

## `0x5018` — `MWV207REG_BLT_EXECUTE`

- 地址：`0x14060`　驱动号：`0x5018`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFF11F197 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:1100`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| SEMAPHORE_NUMBER | 31:24 | 8 | U08 | — |
| SEMAPHORE | 20:20 | 1 | U01 | NUMBER=31:24 |
| INTERRUPT_NUMBER | 16:12 | 5 | U05 | — |
| INTERRUPT | 8:8 | 1 | U01 | NUMBER=16:12 |
| FENCE | 7:7 | 1 | U01 | — |
| DITHER | 4:4 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| COMMAND | 2:0 | 3 | U03 | CLEAR=0x1, BLT=0x2, COPY=0x3, TILE_FILLER=0x4, YUV_TILER=0x5, MIPMAP=0x6 |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `include/mwv207_cmd.h:166` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (op), 0x0) & (0x7)) \|       (ALIGN_BIT((uint32_t) (DISABLE), 0x4) & (0x10))` |

## `0x5019` — `MWV207REG_BLT_GENERAL_CONFIG`

- 地址：`0x14064`　驱动号：`0x5019`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x01FFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:1248`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| DEST_DEPTH_ADDR_REMAP | 24:24 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| DEST1_SIGN | 23:23 | 1 | U01 | UNSIGN=0x0, SIGN=0x1 |
| SRC1_SIGN | 22:22 | 1 | U01 | UNSIGN=0x0, SIGN=0x1 |
| DEST_SIGN | 21:21 | 1 | U01 | UNSIGN=0x0, SIGN=0x1 |
| SRC_SIGN | 20:20 | 1 | U01 | UNSIGN=0x0, SIGN=0x1 |
| FILLER128 | 19:19 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| TILE_STATUS4_BIT | 18:18 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| FILLER_BPP | 17:15 | 3 | U03 | FILLER8_BPP=0x0, FILLER16_BPP=0x1, FILLER32_BPP=0x2, FILLER64_BPP=0x3, FILLER128_BPP=0x4 |
| FENCE_ENDIAN_CONTROL | 14:13 | 2 | U02 | NO_SWAP=0x0, SWAP_WORD=0x1, SWAP_DWORD=0x2, SWAP_DDWORD=0x3 |
| FILTER_TYPE | 12:12 | 1 | U01 | AVERAGE=0x0, SAMPLE=0x1 |
| FILLER_FORMAT | 11:11 | 1 | U01 | FILLER32=0x0, FILLER64=0x1 |
| FILLER_TILE_SIZE | 10:10 | 1 | U01 | TILE64=0x0, TILE256=0x1 |
| CLEAR_DEST_BPP | 9:7 | 3 | U03 | ONE_BYTE=0x0, TWO_BYTE=0x1, THREE_BYTE=0x2, FOUR_BYTE=0x3, SIXTH_BYTE=0x6, EIGHT_BYTE=0x7 |
| INTERLEAVING | 6:5 | 2 | U02 | OFF=0x0, PARALLAX=0x1, HONEYCOMB=0x2 |
| DESTINATION_ENDIAN_CONTROL | 4:3 | 2 | U02 | NO_SWAP=0x0, SWAP_WORD=0x1, SWAP_DWORD=0x2, SWAP_DDWORD=0x3 |
| SOURCE_ENDIAN_CONTROL | 2:1 | 2 | U02 | NO_SWAP=0x0, SWAP_WORD=0x1, SWAP_DWORD=0x2, SWAP_DDWORD=0x3 |
| YUVSTANDARD | 0:0 | 1 | U01 | YUV601=0x0, YUV709=0x1 |

驱动写入点（5 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:256` | `MWV207_EMIT` | `blit_config` |
| `mwv207_blit.c:397` | `MWV207_EMIT` | `levelInfo->blit_config` |
| `mwv207_blit.c:496` | `MWV207_EMIT` | `blit_config` |
| `mwv207_blit.c:638` | `MWV207_EMIT` | `blit_config` |
| `mwv207_blit.c:795` | `MWV207_EMIT` | `general_config` |

## `0x501a` — `MWV207REG_BLT_TILE_FILLER_COUNT`

- 地址：`0x14068`　驱动号：`0x501a`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:1425`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| TILE_COUNT | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:817` | `MWV207_EMIT` | `tile_count` |

## `0x5027` — `MWV207REG_BLT_BLOCK_SIZE`

- 地址：`0x1409C`　驱动号：`0x5027`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:1773`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| HEIGHT | 31:16 | 16 | U16 | — |
| WIDTH | 15:0 | 16 | U16 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:537` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (0x40), 0x0) & (0xFFFF)) \|       (ALIGN_BIT((uint32_t) (0x40), 0x10) & (0xFFFF0000))` |
| `mwv207_blit.c:670` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (0x40), 0x0) & (0xFFFF)) \|       (ALIGN_BIT((uint32_t) (0x40), 0x10) & (0xFFFF0000))` |

## `0x5028` — `MWV207REG_BLT_TILE_SIZE`

- 地址：`0x140A0`　驱动号：`0x5028`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:1799`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| HEIGHT | 31:16 | 16 | U16 | — |
| WIDTH | 15:0 | 16 | U16 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:533` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (4), 0x0) & (0xFFFF)) \|       (ALIGN_BIT((uint32_t) (4), 0x10) & (0xFFFF0000))` |
| `mwv207_blit.c:666` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (4), 0x0) & (0xFFFF)) \|       (ALIGN_BIT((uint32_t) (4), 0x10) & (0xFFFF0000))` |

## `0x502b` — `MWV207REG_BLT_CACHE_FLUSH`

- 地址：`0x140AC`　驱动号：`0x502b`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x00000003 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:1870`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| SRC_CACHE | 1:1 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| TILE_CACHE | 0:0 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `include/mwv207_cmd.h:163` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (ENABLE), 0x1) & (0x2)) \|       (ALIGN_BIT((uint32_t) (ENABLE), 0x0) & (0x1))` |
| `include/mwv207_cmd.h:169` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (ENABLE), 0x1) & (0x2)) \|       (ALIGN_BIT((uint32_t) (ENABLE), 0x0) & (0x1))` |

## `0x502c` — `MWV207REG_BLT_LOD_CONFIG`

- 地址：`0x140B0`　驱动号：`0x502c`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x0000007F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:1939`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| MASK_IGNORE_LOD0 | 6:6 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| IGNORE_LOD0 | 5:5 | 1 | U01 | — |
| MASK_LOD_NUMBER | 4:4 | 1 | U01 | ENABLED=0x0, MASKED=0x1 |
| LOD_NUMBER | 3:0 | 4 | U04 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:685` | `MWV207_EMIT` | `((ALIGN_BIT((uint32_t) (info.src_info.level_count), 0x0) & (0xF)) \| (~(0xF \| 0x10))) &       ((ALIGN_BIT((uint32_t) (1), 0x5) & (0x20)) \| (~(0x20 \| 0x40)))` |

## `0x502e` — `MWV207REG_BLT_GENERAL_CONTROL`

- 地址：`0x140B8`　驱动号：`0x502e`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x00000001 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:1997`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| STREAM_CONTROL | 0:0 | 1 | U01 | LOCK=0x1, UNLOCK=0x0 |

驱动写入点（4 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `include/mwv207_cmd.h:127` | `MWV207_EMIT` | `\       (ALIGN_BIT(0x1, 0x0) & (0x1))` |
| `include/mwv207_cmd.h:133` | `MWV207_EMIT` | `\       (ALIGN_BIT(0x0, 0x0) & (0x1))` |
| `include/mwv207_cmd.h:141` | `MWV207_EMIT` | `(ALIGN_BIT(0x1, 0x0) & (0x1))` |
| `include/mwv207_cmd.h:156` | `MWV207_EMIT` | `(ALIGN_BIT(0x0, 0x0) & (0x1))` |

## `0x502f` — `MWV207REG_BLT_SWIZZLE_EX`

- 地址：`0x140BC`　驱动号：`0x502f`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x00FFFFFF ResetValue=0x00688688`
- 厂商定义位置：`jmgpu_medial.h:2018`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| DST_SWIZZLE_ALPHA | 23:21 | 3 | U03 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3, ZERO=0x4, ONE=0x5 |
| DST_SWIZZLE_BLUE | 20:18 | 3 | U03 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3, ZERO=0x4, ONE=0x5 |
| DST_SWIZZLE_GREEN | 17:15 | 3 | U03 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3, ZERO=0x4, ONE=0x5 |
| DST_SWIZZLE_RED | 14:12 | 3 | U03 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3, ZERO=0x4, ONE=0x5 |
| SRC_SWIZZLE_ALPHA | 11:9 | 3 | U03 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3, ZERO=0x4, ONE=0x5 |
| SRC_SWIZZLE_BLUE | 8:6 | 3 | U03 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3, ZERO=0x4, ONE=0x5 |
| SRC_SWIZZLE_GREEN | 5:3 | 3 | U03 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3, ZERO=0x4, ONE=0x5 |
| SRC_SWIZZLE_RED | 2:0 | 3 | U03 | RED=0x0, GREEN=0x1, BLUE=0x2, ALPHA=0x3, ZERO=0x4, ONE=0x5 |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:530` | `MWV207_EMIT` | `src_blit_desc.swizzle \|       (dst_blit_desc.swizzle << 0xC)` |
| `mwv207_blit.c:659` | `MWV207_EMIT` | `src_blit_desc.swizzle \|       (dst_blit_desc.swizzle << 0xC)` |

## `0x50ce` — `MWV207REG_BLT_CLUSTER_CONTROL`

- 地址：`0x14338`　驱动号：`0x50ce`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000000FF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:2223`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| CLUSTER_ENABLE | 7:0 | 8 | U08 | — |

驱动写入点（8 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:124` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) ((1 << set_count)), 0x0) & (0xFF))` |
| `mwv207_blit.c:151` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (used_cluster_mask), 0x0) & (0xFF))` |
| `mwv207_blit.c:174` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) ((1 << set_count)), 0x0) & (0xFF))` |
| `mwv207_blit.c:191` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (ctx->ws->cluster_mask), 0x0) & (0xFF))` |
| `mwv207_blit.c:811` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) ((1 << set_count)), 0x0) & (0xFF))` |
| `mwv207_blit.c:828` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (ctx->ws->cluster_mask), 0x0) & (0xFF))` |
| `include/mwv207_cmd.h:129` | `MWV207_EMIT` | `\       (ALIGN_BIT((uint32_t) (ctx->ws->cluster_mask), 0x0) & (0xFF))` |
| `include/mwv207_cmd.h:145` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (cluster_mask), 0x0) & (0xFF))` |

## `0x50cf` — `MWV207REG_BLT_DEST_FLIP_SIZE`

- 地址：`0x1433C`　驱动号：`0x50cf`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_medial.h:2243`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| BOTTOM | 31:16 | 16 | U16 | — |
| TOP | 15:0 | 16 | U16 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_blit.c:565` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (info.dst_box.y), 0x0) & (0xFFFF)) \|          (ALIGN_BIT((uint32_t) ((info.dst_box.y + info.dst_box.height)), 0x10) & (0xFFFF0000))` |

## `0x5280` — `MWV207REG_TCS_START`

- 地址：`0x14A00`　驱动号：`0x5280`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:29`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PC | 19:0 | 20 | U20 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:420` | `MWV207_EMIT` | `0` |

## `0x5281` — `MWV207REG_TCS_RELATIVE_END`

- 地址：`0x14A04`　驱动号：`0x5281`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x001FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:49`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PC | 20:0 | 21 | U21 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:421` | `MWV207_EMIT` | `(tcs->code_size / 4)` |

## `0x5283` — `MWV207REG_TCS_INSTRUCTION_PREFETCH`

- 地址：`0x14A0C`　驱动号：`0x5283`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:87`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PC | 19:0 | 20 | U20 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:744` | `MWV207_EMIT` | `0` |

## `0x5284` — `MWV207REG_TCS_INSTRUCTION_PREFETCH_RELATIVE_END`

- 地址：`0x14A10`　驱动号：`0x5284`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:105`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PC | 19:0 | 20 | U20 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:424` | `MWV207_EMIT` | `(tcs->code_size / 4 - 1)` |

## `0x5285` — `MWV207REG_TCS_CONTROL`

- 地址：`0x14A14`　驱动号：`0x5285`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x03F7F33F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:124`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| OUTPUT_ATTRIBUTES | 25:20 | 6 | U06 | — |
| TEMP_COUNT | 18:12 | 7 | U07 | — |
| REMAP | 9:8 | 2 | U02 | INPUT=0x0, INPUT_OUTPUT=0x1, PACKED=0x2 |
| OUTPUT_COUNT | 5:0 | 6 | U06 | — |

驱动写入点（3 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:221` | `MWV207_EMIT` | `0` |
| `mwv207_shader.c:391` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (tcs_vertices_out), 0x0) & (0x3F)) \|          (ALIGN_BIT((uint32_t) (remap), 0x8) & (0x300)) \|          (ALIGN_BIT((uint32_t) (tcs->num_temps), 0xC) & (0x7F000)) \|          (ALIGN_BIT((uint32_t) (link.num_varyings_fixed), 0x14) & (0x3F00000))` |
| `mwv207_shader.c:426` | `MWV207_EMIT` | `0` |

## `0x5286` — `MWV207REG_TCS_THROTTLE`

- 地址：`0x14A18`　驱动号：`0x5286`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x03FFF03F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:197`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| MIN_PAGES | 25:20 | 6 | U06 | — |
| MAX_PATCHES | 19:12 | 8 | U08 | — |
| MAX_PAGES | 5:0 | 6 | U06 | — |

驱动写入点（3 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:222` | `MWV207_EMIT` | `0` |
| `mwv207_shader.c:412` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (max_patch), 0xC) & (0xFF000)) \|          (ALIGN_BIT((uint32_t) (ctx->info.alloc_page[PIPE_SHADER_TESS_CTRL]), 0x0) & (0x3F)) \|          (ALIGN_BIT((uint32_t) (ctx->info.alloc_page[PIPE_SHADER_TESS_CTRL]), 0x14) & (0x3F00000))` |
| `mwv207_shader.c:427` | `MWV207_EMIT` | `0` |

## `0x5287` — `MWV207REG_TCS_ATTRIBUTE`

- 地址：`0x14A1C`　驱动号：`0x5287`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xFFF7FF3F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:226`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| LAYOUT | 31:31 | 1 | U01 | INTERLEAVED=0x0, LINEAR=0x1 |
| PATCH_SIZE | 30:20 | 11 | U11 | — |
| PER_PATCH_OFFSET | 18:8 | 11 | U11 | — |
| CONTROL_POINT_SIZE | 5:0 | 6 | U06 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:404` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (control_point_size), 0x0) & (0x3F)) \|          (ALIGN_BIT((uint32_t) (patch_offset), 0x8) & (0x7FF00)) \|          (ALIGN_BIT((uint32_t) (patch_size), 0x14) & (0x7FF00000)) \|          (ALIGN_BIT((uint32_t) (0x1), 0x1F) & (0x80000000))` |

## `0x5288` — `MWV207REG_TCS_OUTPUT`

- 地址：`0x14A20`　驱动号：`0x5288`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=3 BLK=3 Count=8 FieldMask=0x3F3F3F3F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:163`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| OUTPUT3 | 29:24 | 6 | U06 | — |
| OUTPUT2 | 21:16 | 6 | U06 | — |
| OUTPUT1 | 13:8 | 6 | U06 | — |
| OUTPUT0 | 5:0 | 6 | U06 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:402` | `MWV207_EMITS` | `8` |

## `0x5290` — `MWV207REG_TCS_ATTRIBUTE_EX`

- 地址：`0x14A40`　驱动号：`0x5290`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000007FF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:265`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| GROUP_SIZE | 10:0 | 11 | U11 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:410` | `MWV207_EMIT` | `max_patch * patch_size` |

## `0x5291` — `MWV207REG_TCS_UNIFORM_BASE`

- 地址：`0x14A44`　驱动号：`0x5291`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000003FF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:284`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| OFFSET | 9:0 | 10 | U10 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_vertex.c:351` | `MWV207_EMIT` | `(offset / 4)` |

## `0x5293` — `MWV207REG_TCS_SAMPLER_BASE`

- 地址：`0x14A4C`　驱动号：`0x5293`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x03FF007F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:323`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| TXD_OFFSET | 25:16 | 10 | U10 | — |
| OFFSET | 6:0 | 7 | U07 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:417` | `MWV207_EMIT` | `0x20` |

## `0x52c0` — `MWV207REG_TPG_CONTROL`

- 地址：`0x14B00`　驱动号：`0x52c0`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x17F7F333 ResetValue=0x00040000`
- 厂商定义位置：`jmgpu_domain.h:417`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| FORCE_TRIVIAL_TESSELLATION | 28:28 | 1 | U01 | DISABLED=0x0, ENABLED=0x1 |
| THRESHOLD | 26:20 | 7 | U07 | — |
| MAX_FACTOR | 18:12 | 7 | U07 | — |
| TOPOLOGY | 9:8 | 2 | U02 | POINT=0x0, LINE=0x1, TRIANGLE_CW=0x2, TRIANGLE_CCW=0x3 |
| SPACING | 5:4 | 2 | U02 | EQUAL=0x0, POWER_OF_TWO=0x1, FRACTIONAL_ODD=0x2, FRACTIONAL_EVEN=0x3 |
| MODE | 1:0 | 2 | U02 | TRIANGLE=0x0, QUAD=0x1, ISO_LINE=0x2 |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:503` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (mode), 0x0) & (0x3)) \|          (ALIGN_BIT((uint32_t) (spacing), 0x4) & (0x30)) \|          (ALIGN_BIT((uint32_t) (topolog), 0x8) & (0x300)) \|          (ALIGN_BIT((uint32_t) (64), 0xC) & (0x7F000)) \|          (ALIGN_BIT((uint32_t) (32), 0x14) & (0x7F00000)) \|          (ALIGN_BIT((uint32_t) (false), 0x1C) & (0x10000000))` |

## `0x52c1` — `MWV207REG_TES_START`

- 地址：`0x14B04`　驱动号：`0x52c1`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:489`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PC | 19:0 | 20 | U20 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:545` | `MWV207_EMIT` | `0` |

## `0x52c2` — `MWV207REG_TES_RELATIVE_END`

- 地址：`0x14B08`　驱动号：`0x52c2`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x001FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:509`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PC | 20:0 | 21 | U21 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:546` | `MWV207_EMIT` | `(tes->code_size / 4)` |

## `0x52c4` — `MWV207REG_TES_INSTRUCTION_PREFETCH`

- 地址：`0x14B10`　驱动号：`0x52c4`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:547`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PC | 19:0 | 20 | U20 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:747` | `MWV207_EMIT` | `0` |

## `0x52c5` — `MWV207REG_TES_INSTRUCTION_PREFETCH_RELATIVE_END`

- 地址：`0x14B14`　驱动号：`0x52c5`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:565`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PC | 19:0 | 20 | U20 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:549` | `MWV207_EMIT` | `(tes->code_size / 4 - 1)` |

## `0x52c6` — `MWV207REG_TES_CONTROL`

- 地址：`0x14B18`　驱动号：`0x52c6`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x0007F3F1 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:584`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| TEMP_COUNT | 18:12 | 7 | U07 | — |
| OUTPUT_ATTRIBUTES | 9:4 | 6 | U06 | — |
| EXECUTION | 0:0 | 1 | U01 | SKIPPED=0x0, EXECUTE=0x1 |

驱动写入点（3 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:223` | `MWV207_EMIT` | `0` |
| `mwv207_shader.c:522` | `MWV207_EMIT` | `(ALIGN_BIT(0x1, 0x0) & (0x1)) \|          (ALIGN_BIT((uint32_t) (out_nums), 0x4) & (0x3F0)) \|          (ALIGN_BIT((uint32_t) (tes->num_temps), 0xC) & (0x7F000))` |
| `mwv207_shader.c:551` | `MWV207_EMIT` | `0` |

## `0x52c7` — `MWV207REG_TES_THROTTLE`

- 地址：`0x14B1C`　驱动号：`0x52c7`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x1FFFF03F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:620`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| RESULT_WINDOW_SIZE | 28:20 | 9 | U09 | — |
| MAX_THREADS | 19:12 | 8 | U08 | — |
| MAX_PAGES | 5:0 | 6 | U06 | — |

驱动写入点（3 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:224` | `MWV207_EMIT` | `0` |
| `mwv207_shader.c:533` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (ctx->info.alloc_page[PIPE_SHADER_TESS_EVAL]), 0x0) & (0x3F)) \|          (ALIGN_BIT((uint32_t) (ctx->info.max_thread[PIPE_SHADER_TESS_EVAL]), 0xC) & (0xFF000)) \|          (ALIGN_BIT((uint32_t) (ctx->info.max_thread[PIPE_SHADER_TESS_EVAL] / 4), 0x14) & (0x1FF00000))` |
| `mwv207_shader.c:552` | `MWV207_EMIT` | `0` |

## `0x52c8` — `MWV207REG_TES_ATTRIBUTE`

- 地址：`0x14B20`　驱动号：`0x52c8`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x0007FF3F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:649`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| GROUP_SIZE | 18:8 | 11 | U11 | — |
| VERTEX_SIZE | 5:0 | 6 | U06 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:529` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (out_nums), 0x0) & (0x3F)) \|          (ALIGN_BIT((uint32_t) (out_nums * 64), 0x8) & (0x7FF00))` |

## `0x52c9` — `MWV207REG_TES_UNIFORM_BASE`

- 地址：`0x14B24`　驱动号：`0x52c9`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000003FF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:711`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| OFFSET | 9:0 | 10 | U10 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_vertex.c:357` | `MWV207_EMIT` | `(offset / 4)` |

## `0x52cb` — `MWV207REG_TES_SAMPLER_BASE`

- 地址：`0x14B2C`　驱动号：`0x52cb`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x03FF007F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:750`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| TXD_OFFSET | 25:16 | 10 | U10 | — |
| OFFSET | 6:0 | 7 | U07 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:542` | `MWV207_EMIT` | `0x28` |

## `0x52cd` — `MWV207REG_TES_THROTTLE_EX`

- 地址：`0x14B34`　驱动号：`0x52cd`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x00003FFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:801`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| MIN_PAGES | 13:8 | 6 | U06 | — |
| MAX_PATCHES | 7:0 | 8 | U08 | — |

驱动写入点（3 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:225` | `MWV207_EMIT` | `0` |
| `mwv207_shader.c:538` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (ctx->info.patchs[0x1]), 0x0) & (0xFF)) \|          (ALIGN_BIT((uint32_t) (ctx->info.alloc_page[PIPE_SHADER_TESS_EVAL]), 0x8) & (0x3F00))` |
| `mwv207_shader.c:553` | `MWV207_EMIT` | `0` |

## `0x52d0` — `MWV207REG_TES_OUTPUT`

- 地址：`0x14B40`　驱动号：`0x52d0`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=3 BLK=3 Count=8 FieldMask=0x3F3F3F3F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_domain.h:673`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| OUTPUT3 | 29:24 | 6 | U06 | — |
| OUTPUT2 | 21:16 | 6 | U06 | — |
| OUTPUT1 | 13:8 | 6 | U06 | — |
| OUTPUT0 | 5:0 | 6 | U06 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:527` | `MWV207_EMITS` | `8` |

## `0x5310` — `MWV207REG_TX_GLOBAL_CONFIG`

- 地址：`0x14C40`　驱动号：`0x5310`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x00000001 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_neighbor.h:1884`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| TEXTURE_DESCRIPTOR | 0:0 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_sampler.c:608` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (ENABLE), 0x0) & (0x1))` |

## `0x5311` — `MWV207REG_TX_GPIPE_COMMAND`

- 地址：`0x14C44`　驱动号：`0x5311`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0xF3FF03FF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_neighbor.h:1906`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| COMMAND | 31:28 | 4 | U04 | INVALIDATE_ALL=0x0, INVALIDATE_ONE=0x1, PREFETCH=0x2 |
| NUM | 25:16 | 10 | U10 | — |
| DATA | 9:0 | 10 | U10 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_cmd.c:156` | `MWV207_EMIT` | `(ALIGN_BIT(0x0, 0x1C) & (0xF0000000))` |

## `0x5580` — `MWV207REG_SHADER_MISC_CONFIG`

- 地址：`0x15600`　驱动号：`0x5580`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000007FF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:5505`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PIXEL_OVERLAP_INTERLOCK | 10:10 | 1 | U01 | ENABLE=0x1, DISABLE=0x0 |
| LS_RELATIVE_INDEX_STRIDE | 9:8 | 2 | U02 | COMPATIBLE=0x0, ADAPTIVE=0x1, BYTE=0x2 |
| LS_RELATIVE_OFFSET_SHIFT | 7:4 | 4 | U04 | — |
| CONVERT_NAN | 3:3 | 1 | U01 | ZERO=0x0, INFINITY=0x1 |
| PS_PRECISION | 2:2 | 1 | U01 | HIGH=0x0, MEDIUM=0x1 |
| RTNE_ROUNDING | 1:1 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |
| OUT_COLOR_ROUNDING | 0:0 | 1 | U01 | DISABLE=0x0, ENABLE=0x1 |

驱动写入点（3 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:184` | `MWV207_EMIT` | `0x00000002` |
| `mwv207_shader.c:316` | `MWV207_EMIT` | `0x00000002` |
| `mwv207_shader.c:667` | `MWV207_EMIT` | `0x00000002` |

## `0x5581` — `MWV207REG_VS_INSTRUCTION_PREFETCH_RELATIVE_END`

- 地址：`0x15604`　驱动号：`0x5581`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x000FFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:5576`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| PC | 19:0 | 20 | U20 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_shader.c:317` | `MWV207_EMIT` | `(vs->code_size / 4 - 1)` |

## `0x5582` — `MWV207REG_VS_THROTTLE_EX`

- 地址：`0x15608`　驱动号：`0x5582`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x0000003F ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_announce.h:5595`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| MIN_PAGES | 5:0 | 6 | U06 | — |

驱动写入点（2 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_compute.c:220` | `MWV207_EMIT` | `0` |
| `mwv207_shader.c:303` | `MWV207_EMIT` | `alloc_pages` |

## `0x6101` — `MWV207REG_WD_STATIC_CONFIG`

- 地址：`0x18404`　驱动号：`0x6101`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x00000003 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_overflow.h:69`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| ASSIGNMENT_MODE | 1:0 | 2 | U02 | LOAD_BALANCE=0x0, ROUND_ROBIN=0x1, PSEUDO_RANDOM=0x2, LOWEST_ACTIVE=0x3 |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_cmd.c:41` | `MWV207_EMIT` | `0x1` |

## `0x7000` — `MWV207REG_TFB_CONTROL`

- 地址：`0x1C000`　驱动号：`0x7000`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x00000031 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_dynamic.h:329`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| STREAM_INDEX | 5:4 | 2 | U02 | — |
| IGNORE_ALL_STREAMS | 0:0 | 1 | U01 | PROCESS=0x0, IGNORE=0x1 |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:517` | `MWV207_EMIT` | `(ALIGN_BIT((uint32_t) (rast_state->tfb_control), 0x0) & (0x1)) \|       (ALIGN_BIT((uint32_t) (0), 0x4) & (0x30))` |

## `0x7001` — `MWV207REG_TFB_COMMAND`

- 地址：`0x1C004`　驱动号：`0x7001`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x00000007 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_dynamic.h:358`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| COMMAND | 2:0 | 3 | U03 | IDLE=0x0, ACTIVE=0x1, PAUSE=0x2, RESUME=0x3, RESUME_WITH_READ=0x4 |

驱动写入点（6 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:814` | `MWV207_EMIT` | `(ALIGN_BIT(0x2, 0x0) & (0x7))` |
| `mwv207_draw.c:869` | `MWV207_EMIT` | `(ALIGN_BIT(0x4, 0x0) & (0x7))` |
| `mwv207_draw.c:872` | `MWV207_EMIT` | `(ALIGN_BIT(0x1, 0x0) & (0x7))` |
| `mwv207_query.c:119` | `MWV207_EMIT` | `(ALIGN_BIT(0x4, 0x0) & (0x7))` |
| `mwv207_query.c:137` | `MWV207_EMIT` | `(ALIGN_BIT(0x1, 0x0) & (0x7))` |
| `mwv207_query.c:214` | `MWV207_EMIT` | `(ALIGN_BIT(0x2, 0x0) & (0x7))` |

## `0x7003` — `MWV207REG_TFB_INVALIDATE`

- 地址：`0x1C00C`　驱动号：`0x7003`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x00000001 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_dynamic.h:534`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| INVALIDATE | 0:0 | 1 | U01 | TRUE=0x1 |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_cmd.c:160` | `MWV207_EMIT` | `true` |

## `0x7004` — `MWV207REG_TFB_QUERY_COMMAND`

- 地址：`0x1C010`　驱动号：`0x7004`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=0 BLK=0 Count=1 FieldMask=0x00000FF7 ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_dynamic.h:555`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| QUERY | 11:8 | 4 | U04 | PRIMITIVES_WRITTEN=0x0, PRIMITIVES_GENERATED=0x1 |
| STREAM | 7:4 | 4 | U04 | — |
| COMMAND | 2:0 | 3 | U03 | IDLE=0x0, ACTIVE=0x1, PAUSE=0x2, RESUME=0x3, RESUME_WITH_READ=0x4 |

驱动写入点（6 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_query.c:123` | `MWV207_EMIT` | `(ALIGN_BIT(0x4, 0x0) & (0x7)) \|                         (ALIGN_BIT(0x1, 0x8) & (0xF00))` |
| `mwv207_query.c:128` | `MWV207_EMIT` | `(ALIGN_BIT(0x1, 0x0) & (0x7)) \|                      (ALIGN_BIT(0x1, 0x8) & (0xF00))` |
| `mwv207_query.c:147` | `MWV207_EMIT` | `(ALIGN_BIT(0x4, 0x0) & (0x7)) \|                      (ALIGN_BIT(0x0, 0x8) & (0xF00))` |
| `mwv207_query.c:151` | `MWV207_EMIT` | `(ALIGN_BIT(0x1, 0x0) & (0x7)) \|                      (ALIGN_BIT(0x0, 0x8) & (0xF00))` |
| `mwv207_query.c:209` | `MWV207_EMIT` | `(ALIGN_BIT(0x2, 0x0) & (0x7)) \|                   (ALIGN_BIT(0x1, 0x8) & (0xF00))` |
| `mwv207_query.c:219` | `MWV207_EMIT` | `(ALIGN_BIT(0x2, 0x0) & (0x7)) \|                   (ALIGN_BIT(0x0, 0x8) & (0xF00))` |

## `0x7020` — `MWV207REG_TFB_BUFFER_SIZE`

- 地址：`0x1C080`　驱动号：`0x7020`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=4 BLK=4 Count=16 FieldMask=0xFFFFFFFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_dynamic.h:422`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| SIZE | 31:0 | 32 | U32 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_query.c:136` | `MWV207_EMIT` | `0` |

## `0x7030` — `MWV207REG_TFB_BUFFER_STRIDE`

- 地址：`0x1C0C0`　驱动号：`0x7030`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=4 BLK=4 Count=16 FieldMask=0x00000FFF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_dynamic.h:441`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| STRIDE | 11:0 | 12 | U12 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_query.c:135` | `MWV207_EMIT` | `0` |

## `0x7040` — `MWV207REG_TFB_DESCRIPTOR_COUNT`

- 地址：`0x1C100`　驱动号：`0x7040`（= 地址 >> 2）
- 寄存器属性：`MSB=15 LSB=4 BLK=4 Count=16 FieldMask=0x000000FF ResetValue=0x00000000`
- 厂商定义位置：`jmgpu_dynamic.h:460`

| 字段 | 位域 | 宽 | 类型 | 命名取值 |
|---|---|---|---|---|
| COUNT | 7:0 | 8 | U08 | — |

驱动写入点（1 处）：

| 位置 | 宏 | 值表达式 |
|---|---|---|
| `mwv207_draw.c:856` | `MWV207_EMIT` | `tfb_des_count` |

