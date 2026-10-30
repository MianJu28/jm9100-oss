# MWV207 / JM9100 驱动中文 API 文档 —— 状态与绘制通路

- 对象源码：`/home/admin/jm9100-oss/src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium/drivers/mwv207/`（下称 **P/**）
- 寄存器参考：`/home/admin/jm9100-oss/docs/registers/REGISTERS.md`（下称 **REG**）、`REGISTERS-FIELDS.md`
- winsys 结构体：`src/gallium/winsys/mwv207/drm/mwv207_drm_winsys.h`（下称 **WSH**）
- 缺陷清单：`/home/admin/jm9100-oss/reports/ANALYSIS-REPORT.md`（W8–W11 在 winsys，D 系列在驱动层）

## 0. 阅读约定

1. **寄存器号规则**：驱动写进命令流的 16 位寄存器号 `D`，对应厂商 KMD 头文件的寄存器地址 `D << 2`（`docs/registers/REGISTERS.md:19`）。本文每处寄存器给出「驱动号 / 厂商寄存器名（若已映射）/ 位域 / 文件:行号」。REG：`_Address` 已知但无字段定义者标 **未映射**（`REGISTERS.md:243-299`）；连 `_Address` 都没有、或数值由 `base+i` 动态算出而未进映射表者标 **未收录**。
2. 所有位域写法沿用驱动源码：`ALIGN_BIT(v, off) & mask`（`P/include/mwv207_cmd.h:66`）。本文字段表一律写成「bit 位置 / 掩码 / 取值」。
3. 本文所有结论均取自逐行通读；读不到的定义（空壳头文件、非本范围文件）显式标 **未确认**。

### 0.1 覆盖文件

| 文件 | 行数 | 本文覆盖 |
|---|---|---|
| `P/mwv207_state.c` | 499 | §3、§4 |
| `P/mwv207_blend.c` | 369 | §5 |
| `P/mwv207_sampler.c` | 1083 | §6 |
| `P/mwv207_vertex.c` | 567 | §7 |
| `P/mwv207_draw.c` | 1297 | §8 |
| `P/mwv207_blit.c` | 1239 | §9 |
| `P/mwv207_query.c` | 349 | §10 |
| `P/mwv207_cmd.c` | 372 | §2 |
| `P/mwv207_compute.c` | 319 | §11 |
| `P/include/mwv207_state.h` | 145 | §3、§4 |
| `P/include/mwv207_cmd.h` | 223 | §1 |
| `P/include/mwv207_blit.h` | 98 | §9 |
| `P/include/mwv207_query.h` | 55 | §10 |
| 补充：`P/mwv207_context.c` | 232 | §2.6 |
| 补充：`P/mwv207_surface.c` | 363 | §12 |
| 补充：`P/include/mwv207_resource.h`、`P/include/mwv207_format.h`、`P/include/mwv207_surface.h`、`P/nir/mwv207_common.h`、`WSH` | — | §6、§7、§12 |

---

## 1. 命令流编码（`P/include/mwv207_cmd.h`）

### 1.1 通用命令字格式

| 位域 | 含义 | 依据 |
|---|---|---|
| bits[31:27] | 操作码（5 bit） | `REGISTERS.md:17` |
| bits[26:16] | 字数（单字寄存器写为 1） | `REGISTERS.md:17` |
| bits[15:0] | 目标寄存器号（= 厂商 `_Address >> 2`） | `REGISTERS.md:17,19` |

### 1.2 发射宏完整位域

| 宏 | 定义位置 | 第 1 个字（命令头） | 后续字 | 备注 |
|---|---|---|---|---|
| `MWV207_EMIT(cmd, reg, value)` | `mwv207_cmd.h:92-96` | `0x08010000 + reg` | 1 个 `value` | 单寄存器写；`0x08010000` = opcode 1（bit27）+ 字数 1（bit16） |
| `MWV207_EMITF(cmd, reg, value)` | `mwv207_cmd.h:98-102` | `0x0C010000 + reg` | 1 个 `value` | 唯一差别：基址多置 **bit26**（`0x0C000000` vs `0x08000000`） |
| `MWV207_EMITS(cmd, reg, count, value)` | `mwv207_cmd.h:104-110` | `0x08000000 + reg + (count << 16)` | `memcpy(cmd, value, count*4)`，随即 `cmd += count`；**若 count 为偶数再补 `0xdeadbeef`** | 连续写 `count` 个寄存器；`count<<16` 落在 bits[26:16] |
| `MWV207_EMITFS(cmd, reg, count, value)` | `mwv207_cmd.h:112-118` | `0x0C000000 + reg + (count << 16)` | 同 EMITS（含偶数补 `0xdeadbeef`） | 基址同上多置 bit26 |
| `MWV207_STALL(cmd, value)` | `mwv207_cmd.h:86-90` | `(ALIGN_BIT(0x9, 0x1B) & 0xF8000000)` = `0x48000000`（opcode = 0x9） | 1 个 `value` | 写法特殊：使用 `*(*cmd)++` 双重解引用（要求调用方传 `uint32_t **`） |
| `MWV207_SNAP_TO_PAGE(cmd)` | `mwv207_cmd.h:120-124` | `0x9800001f` | `0x00000000` | 页级同步/对齐；opcode = `0x9800001f >> 27` = 0x13 |

**关于 `0x08010000` / `0x0C010000` 的精确解读：**

- `0x08010000` = `0x08000000`(bit27 = opcode 域最低位 → opcode 1) + `0x00010000`(bit16 → 字数域 = 1)。与 REG 的「单字为 1」一致。
- `0x0C010000` = `0x0C000000` + `0x00010000`；`0x0C000000` = bit27|bit26。若严格按 bits[26:16] 的 11 位域解读，字数 = `0x400 | 0x1` = **1025**；若把 bit26 当作独立标志、字数域缩为 bits[25:16] 的 10 位，则字数 = 1。
- 结论：**「F」后缀的确切语义在本发行版中无定义** —— `P/include/hw/mwv207reg_{3d,texture,shader}.h` 是只有 include guard 的空壳，`mwv207reg_common.h` 只剩 `DISABLE`/`ENABLE`（`REGISTERS.md:29-31`），故无法从符号名反推。可确证的是结构差异：`EMIT/EMITS` 基址 `0x08000000`，`EMITF/EMITFS` 基址 `0x0C000000`（`mwv207_cmd.h:94/100/106/114`）。**标记为未确认。**

**`0xdeadbeef` 填充的规律**（`mwv207_cmd.h:109,117`）：`count` 为偶数时补 1 个字，因此 `EMITS/EMITFS` 的「命令头 + count 字 + 填充」恒为偶数个字 = 8 字节对齐。这是观测定律（推断其目的为保持命令流 8 字节对齐）。

### 1.3 `VALUE / NAME / MASK / MASK_VALUE / MASK_NAME` 宏语义

```c
/* P/include/mwv207_cmd.h:66-84 */
#define ALIGN_BIT(data, offset) (((uint32_t) (data)) << (offset))

#define VALUE(reg, field, value) \
   (ALIGN_BIT((uint32_t) (value), reg##_##field##_Offset) & (reg##_##field##_Mask))

#define NAME(reg, field, value) \
   (ALIGN_BIT(reg##_##field##_##value, reg##_##field##_Offset) & (reg##_##field##_Mask))

#define MASK(reg, field)  (~(reg##_##field##_Mask | reg##_MASK_##field##_Mask))

#define MASK_VALUE(reg, field, value)  (VALUE(reg, field, value) | MASK(reg, field))
#define MASK_NAME(reg, field, value)   (NAME(reg, field, value)  | MASK(reg, field))
```

| 宏 | 语义 | 依赖的字段宏 |
|---|---|---|
| `VALUE(reg,field,value)` | 把整型 `value` 左移该字段偏移并按字段掩码截取 | `reg##_##field##_Offset`、`reg##_##field##_Mask` |
| `NAME(reg,field,value)` | 同上，但 `value` 是枚举符号 `reg_field_VALUE`，取其数值 | 额外的 `reg##_##field##_##value` |
| `MASK(reg,field)` | 生成「除该字段外全 1」的掩码，用于与已有寄存器值做 `&`（保留其他位） | `reg##_##field##_Mask`、**`reg##_MASK_##field##_Mask`** |
| `MASK_VALUE` | `VALUE \| MASK`，即「字段置值 + 其他位保留」 | 同上 |
| `MASK_NAME` | `NAME \| MASK` | 同上 |

**关键事实（编译层）**：这五个宏依赖 `reg##_##field##_Offset/_Mask`（以及 `MASK` 额外依赖 `reg##_MASK_##field##_Mask`），而这些宏定义所在的
`P/include/hw/mwv207reg_3d.h`、`mwv207reg_texture.h`、`mwv207reg_shader.h` 在本发行版中**已被剥离为空头文件**（`REGISTERS.md:29-31`），`mwv207reg_common.h` 只剩 `DISABLE`/`ENABLE`。
因此：

- 任何 `VALUE/NAME/MASK/MASK_VALUE/MASK_NAME` 展开都会因未定义标识符而编译失败；**本驱动通篇没有一处使用它们**。
- 实际写法一律退化为裸常量 `(ALIGN_BIT((uint32_t)(v), off) & (mask))`，或配合 `~(mask | next_bit_mask)` 手工构造「保留其他位」的语义（典型如 `mwv207_draw.c:432-434,481-486`、`mwv207_blend.c:191-194`）。
- `NAME()` 所需的 `reg##_##field##_##value` 枚举也随空壳头文件一并消失，故文档中所有字段取值都只能以数值给出。

### 1.4 缓存掩码 `enum cache_mask`（`mwv207_cmd.h:40-64`）

| 值 | 名称 | 值 | 名称 |
|---|---|---|---|
| `1<<0` | `CACHE_TEXTURE_L2` | `1<<7` | `CACHE_DEPTH` |
| `1<<1` | `CACHE_TEXUTRE_L1`（原文拼写错误） | `1<<8` | `CACHE_COLOR` |
| `1<<2` | `CACHE_TEXTURE_DESC` | `1<<9` | `CACHE_TS` |
| `1<<3` | `CACHE_VST_L2` | `1<<10` | `CACHE_INSTRUCTION` |
| `1<<4` | `CACHE_VST_L1` | `1<<11` | `CACHE_TFB` |
| `1<<5` | `CACHE_VST_DESC` | `1<<12` | `CACHE_L2` |
| `1<<6` | `CACHE_SH_L1` | `1<<13` | `CACHE_VERTEX_DATA` |

组合：`CACHE_TEXTURE_DATA = TEXTURE_L2|TEXUTRE_L1`；`CACHE_TEXTURE_ALL = …|TEXTURE_DESC`；`CACHE_VST_DATA`；`CACHE_VST_ALL`；`CACHE_TX_DESC = TEXTURE_DESC|VST_DESC`；`CACHE_WRITABLE = SH_L1|DEPTH|COLOR|TS|TFB`；`CACHE_ALL = ~0`；`CACHE_NONE = 0`（`mwv207_cmd.h:56-63`）。

### 1.5 BLIT 辅助内联（`mwv207_cmd.h:126-172`）

| 宏/函数 | 位置 | 寄存器（驱动号 / 厂商名 / 位域） |
|---|---|---|
| `MWV207_BLIT_LOCK(ctx,cmd)` | `cmd.h:126-130` | `0x502E` = `MWV207REG_BLT_GENERAL_CONTROL`(0x140B8)，bit0=1；`0x50CE` = `MWV207REG_BLT_CLUSTER_CONTROL`(0x14338)，bits7:0 = `ctx->ws->cluster_mask` |
| `MWV207_BLIT_UNLOCK(ctx,cmd)` | `cmd.h:132-135` | `0x502E` bit0=0；再调 `mwv207_wait_blit()` |
| `mwv207_wait_blit(cmd_buf, cluster_mask)` | `cmd.h:137-158` | `0x502E` bit0=1；`cluster_mask` 非 0 时 `0x50CE` bits7:0；`0xE02`（**未映射** 0x03808）bit4:0=`0x1`、bits12:8=`0x10`；同名 `MWV207_STALL`；`0x502E` bit0=0 |
| `mwv207_excute_blit(cmd_buf, op)` | `cmd.h:160-172` | `0x502B` = `MWV207REG_BLT_CACHE_FLUSH`(0x140AC)，bit1=ENABLE、bit0=ENABLE；`0x5018` = `MWV207REG_BLT_EXECUTE`(0x14060)，bits2:0=`op`、bit4=DISABLE；再写一次 `0x502B` |

`op` 取值由调用点归纳（**非头文件枚举，属推断**）：`0x1` 填充/清屏（`blit.c:274,296,437`）、`0x2` 图像拷贝（`blit.c:591`）、`0x3` 缓冲区拷贝/更新（`blit.c:320,343`）、`0x4` tile 填充（`blit.c:831`）、`0x6` mipmap 生成（`blit.c:688`）。

---

## 2. 一帧的提交序列（`P/mwv207_cmd.c`）

### 2.1 dat / reloc / cmdbuf 数据结构（`WSH:111-186`）

```c
/* WSH:144-151 */
struct cmd_dat {
   uint32_t data[64];        /* 256 字节内联数据（纹理描述符即存于此） */
   uint32_t size;
   struct mwv207_bo *bos[16];
   struct drm_mwv207_submit_reloc relocs[16];
   int nr_relocs;
};

/* WSH:153-161 */
struct cmd_dat_arrary {      /* 注意原文拼写 arrary */
   uint8_t *data; uint32_t size; int max_size;
   struct drm_mwv207_submit_reloc *relocs; int nr_relocs, nr_max_relocs;
};

/* WSH:163-186 */
struct mwv207_cmd {
   struct mwv207_winsys *ws;
   uint8_t *buf; uint32_t size; uint32_t offset;
   uint32_t *stamp;                       /* begin/end_record 的写入游标快照 */
   struct drm_mwv207_submit_reloc *relocs; int nr_relocs, nr_max_relocs;
   struct mwv207_bo **mwv207_bos;
   struct drm_mwv207_bo_acc *bos; int nr_bos, nr_max_bos;
   struct cmd_dat_arrary mwv207_dats;
   struct drm_mwv207_submit_cmd_dat dats;
   int nr_dats;
   int poisoned;
   uint32_t engine_type;                  /* 3D = 0x0, DMA = 0x4 */
};
```

- `struct cmd_dat` 是**固定容量**结构（`data[64]`=256 B、`bos[16]`、`relocs[16]`），因此单条 dat 携带的纹理描述符不能超过 256 B；纹理路径正是把 `struct mwv207_texture_config` 整块塞进 `data` 并设 `size = sizeof(struct mwv207_texture_config)`（`mwv207_sampler.c:467-469`）。
- 动态数组（`cmd->relocs / bos / mwv207_dats`）的扩容实现在 winsys；相关缺陷见 `ANALYSIS-REPORT.md` 的 W9（`realloc` 覆盖原指针）、W10（`add_dat` 单次 `*2` 扩容可能不足）。
- 两个 cmdbuf 由 `mwv207_create_cmd()` 建立：`ctx->cmd`（engine_type `0x0`，3D）与 `ctx->dma`（engine_type `0x4`，DMA）（`mwv207_cmd.c:313-323`）。

**硬上限（`mwv207_cmd.c:340-352`，`check_cmd` 返回 false 即需要提交）**：剩余空间 `< offset + 4096`、`nr_bos > 64`、`nr_relocs > 128`、`dats.nr_relocs > 128`。

### 2.2 命令缓冲初始化 `mwv207_start_cmd`（`mwv207_cmd.c:31-58`）

| 顺序 | 驱动号 | 厂商寄存器 | 位域/值 |
|---|---|---|---|
| 1 | `0xE00` | **未映射**（0x03800） | `0` |
| 2 | `0xE80` | `MWV207REG_MULTI_CHIP_CONTROL`（0x03A00） | `0x10000000` |
| 3 | `0x6101` | `MWV207REG_WD_STATIC_CONFIG`（0x18404） | `0x1` |
| 4 | `0x6B` | `MWV207REG_MMU_CONFIG`（0x001AC） | `0x0fffffff` |
| 5 | `0x1F6` | `MWV207REG_FE_CONTROL`（0x007D8） | 位: `(ENABLE<<1)&0x2` |
| 6 | `0xE42` | `MWV207REG_CLUSTER_CHECKER_BOARD`（0x03908） | `0x2` |
| 7 | `0x221` | `MWV207REG_SH_SAMPLER_ARBITRATION`（0x00884） | `0x0808` |
| 8 | `0x1F2` | `MWV207REG_FE_RESET_ATTRIBUTES`（0x007C8） | `true` |
| 9 | `0xE05` | **未映射**（0x03814） | `0x1` |

之后 `mwv207_end_record` 并 `ctx->base.set_active_query_state(true)`（`mwv207_cmd.c:56-57`）。

`mwv207_end_cmd`（`mwv207_cmd.c:60-72`）：`set_active_query_state(false)` → `mwv207_flush_cache(&cmd_buf, CACHE_ALL)`。

### 2.3 记录游标：`begin_record` / `end_record`

| 函数 | 位置 | 行为 |
|---|---|---|
| `mwv207_begin_record(cmd,&start)` | `mwv207_cmd.c:325-331` | `assert(cmd->stamp == NULL)`；`cmd->stamp = (uint32_t*)(cmd->buf + cmd->offset)`；`*start = cmd->stamp` |
| `mwv207_end_record(cmd,&end)` | `mwv207_cmd.c:333-338` | `cmd->offset += (*end - cmd->stamp) * sizeof(uint32_t)`；`cmd->stamp = NULL` |

驱动**从不直接推进 `cmd->offset`**，所有命令都必须包在 `begin_record`/`end_record` 之间；`cmd->stamp != NULL` 时再次 `begin_record` 会触发断言（可测出重入错误）。

### 2.4 dat / reloc 追加

| 函数 | 位置 | 关键行为 |
|---|---|---|
| `mwv207_dat_add_reloc(dat, ptr, bo, offset, flags)` | `mwv207_cmd.c:187-211` | `idx = dat->nr_relocs`；`dat->bos[idx] = bo`；`reloc->idx/ cmd_offset = (*ptr - dat->data)*4 / bo_offset = offset / type = 0x1 / pad = 0`；`*(*ptr)++ = offset`；`nr_relocs++` |
| `mwv207_add_relocs(ctx, ptr, reg, bo, offset, flags)` | `mwv207_cmd.c:213-237` | `idx = ws->cmdbuf_add_bo(cmd, bo, flags)`；**`reg == 0` 时**把 `offset` 直接写成一个数据字（`cmd_offset = (*ptr - cmd->buf)*4`），**否则**先 `MWV207_EMIT(*ptr, reg, offset)` 且 `cmd_offset = (*ptr - cmd->buf + 1)*4`；随后 `cmdbuf_add_relloc(..., type = 0x1, pad = 0)` |
| `mwv207_add_dat(ctx, ptr, reg, dat)` | `mwv207_cmd.c:239-255` | `bo_offset = ws->cmdbuf_add_dat(cmd, dat)`；`cmd_offset = (*ptr - cmd->buf + 1)*4`；`type = 0x1 \| 0x80000000`（高位标记「dat 而非普通 BO」）；`MWV207_EMIT(*ptr, reg, 0)`；`cmdbuf_add_relloc(cmd, 0, cmd_offset, bo_offset, type, 0)` |

> `reg == 0` 的特例是间接绘制路径用的（`mwv207_draw.c:921-922`）。dat 的 `type` 高位 `0x80000000` 与普通 reloc 的 `0x1` 区分（`mwv207_cmd.c:247`）。

### 2.5 提交与 fence：`mwv207_submit`

```c
/* P/mwv207_cmd.c:354-372 */
void mwv207_submit(struct mwv207_context *ctx, int *out_fence_fd, bool force)
{
   if (!force && check_cmd(ctx->cmd))     /* 未达阈值且非强制 → 不提交 */
      return;
   mwv207_end_cmd(ctx);
   ctx->ws->cmdbuf_commit(ctx->ws, ctx->cmd, ctx->in_fence_fd, out_fence_fd);
   if (ctx->in_fence_fd >= 0) { close(ctx->in_fence_fd); ctx->in_fence_fd = -1; }
   mwv207_start_cmd(ctx);
   ctx->dirty = MWV207_DIRTY_ALL;
}
```

| 事实 | 依据 |
|---|---|
| `force=false` 时仅当 `check_cmd` 判定「缓冲将满」才真正提交 | `mwv207_cmd.c:358-359` |
| fence 进：`ctx->in_fence_fd` 由 `fence_server_sync` 累积（`ws->fence_accumulate`），提交后立即 `close` 并置 `-1` | `mwv207_context.c:166-172`；`mwv207_cmd.c:362-364` |
| fence 出：`*pfence = ws->fence_create(ws, out_fence_fd)`，只有 `ctx->flush(pctx, &fence, ...)` 才产出 | `mwv207_context.c:67-95` |
| 提交后 `dirty = MWV207_DIRTY_ALL`，强制下一帧重发全部状态 | `mwv207_cmd.c:370` |

### 2.6 与提交相关的上下文（`P/mwv207_context.c`）

| 函数 | 位置 | 行为 |
|---|---|---|
| `mwv207_flush(pctx, pfence, flags)` | `context.c:67-95` | 对每个 `cbuf` 调 `flush_resource`；`mwv207_submit(ctx, pfence ? &out_fence_fd : NULL, true)`；对 `pixel_128bit` 的 surface 调 `mwv207_merge_128bit`；`*pfence = ws->fence_create(ws, out_fence_fd)` |
| `mwv207_memory_barrier` | `context.c:97-103` | `PIPE_BARRIER_UPDATE` 直接返回；否则 `mwv207_flush(pctx, NULL, 0)` |
| `mwv207_create_fence_fd` | `context.c:153-164` | `assert(type == PIPE_FD_TYPE_NATIVE_SYNC)`；`ws->fence_create(ws, dup(fd))` |
| `mwv207_fence_server_sync` | `context.c:166-172` | `ws->fence_accumulate(ws, &ctx->in_fence_fd, fence)` |
| `mwv207_context_create` | `context.c:174-232` | 依次：`query_init` → `resource_init` → `surface_init` → `state_init` → `shader_init` → `sampler_init` → `blit_init`，绑定 `draw_vbo/flush/create_fence_fd/fence_server_sync/memory_barrier`，再 `compute_init` → `video_codec_init` → `mwv207_create_cmd` → `helper_init` → `slab_create_child` |

### 2.7 屏障、缓存刷新、等待

**`mwv207_pipeline_barrier`（`mwv207_cmd.c:74-95`）**

| 驱动号 | 厂商寄存器 | 位域 |
|---|---|---|
| `0xE03` | **未映射**（0x0380C） | bit1、bit0、bit5（0x20）、bit11（0x800）、bit10（0x400） |
| `0x594` | **未映射**（0x01650） | bit0 |
| `0x50CE`（经 `mwv207_wait_blit`） | `MWV207REG_BLT_CLUSTER_CONTROL` | bits7:0 = `cluster_mask` |

**`mwv207_flush_cache(pbuf, mask)`（`mwv207_cmd.c:97-175`）**：`assert(mask)`；`stall = mask & (CACHE_TEXTURE_ALL | CACHE_VST_ALL | CACHE_INSTRUCTION)`。

| mask 位 | 发射 | 位域 |
|---|---|---|
| （前置 stall） | `0xE02` + `MWV207_STALL` 同值 | bits4:0 = `0x1`，bits12:8 = `0x7`（`(0x1&0x1F) \| (0x7<<8 & 0x1F00)`） |
| `CACHE_DEPTH` | `0xE03` | bit0 |
| `CACHE_COLOR` | `0xE03` | bit1 |
| `CACHE_SH_L1` | `0xE03` | bit5（0x20）、bit10（0x400）、bit11（0x800） |
| `CACHE_L2` | `0xE03` | bit6（0x40） |
| `CACHE_VERTEX_DATA` | `0xE03` | bit14（0x4000） |
| `CACHE_TEXUTRE_L1` | `0xE03` | bit12（0x1000）、bit13（0x2000） |
| `CACHE_VST_DATA` | `0xE03` | bit4（0x10） |
| `CACHE_TS` | `0x594` bit0（**未映射**），并置 `stall = true` | — |
| `CACHE_INSTRUCTION` | `0x22C` | `0x1F`（`MWV207REG_SH_ICACHE_INVALIDATE` 0x008B0） |
| `CACHE_TX_DESC` | `0x5311` | bits31:28 清 0（`(0<<0x1C) & 0xF0000000`）（`MWV207REG_TX_GPIPE_COMMAND` 0x14C44） |
| `CACHE_TFB` | `0x7003` | `true`（`MWV207REG_TFB_INVALIDATE` 0x1C00C） |
| （尾部 stall） | `0xE02` + `MWV207_STALL` 成对 | 同前置 |

**`mwv207_wait_idle(pbuf, target)`（`mwv207_cmd.c:177-185`）**：`value = (0x1 & 0x1F) | (target << 8 & 0x1F00)`；先 `0xE02 = value`，再 `MWV207_STALL(value)`。`target` 常见取值 `0x7`（如 `mwv207_draw.c:101,507,809`、`mwv207_sampler.c:606`）。

### 2.8 DMA 引擎（`mwv207_cmd.c:257-311`）

用独立 cmdbuf `ctx->dma`（engine_type `0x4`）。命令体写成 `uint64_t` 序列：

| 步骤 | 内容 |
|---|---|
| 1 | `cmdbuf_add_bo(dma, src->bo, 0x0)` + `add_relloc(dma, idx, cmd_offset=0, bo_offset=0, type=0x0, pad=0)`，随后两个 0 字占位 |
| 2 | src 地址 = `src->bo->offset + src->offset + level->offset + stride(box->x) + nblocksy(box->y)*level->stride + box->z*layer_size`；`PIPE_BUFFER` 时 = `bo->offset + offset + stride(box->x)`；紧随其后写 stride |
| 3 | dst 同理，但 reloc 的 `cmd_offset = 4*sizeof(uint64_t) = 32` |
| 4 | 再写 width 的 stride、height 的 `nblocksy` |
| 5 | `dma->offset = 10*sizeof(uint64_t) = 80`；`ws->cmdbuf_commit(ws, dma, -1, NULL)` |

---

## 3. 状态原子（atom）清单

### 3.1 关键澄清：atom 不在 `mwv207_state.c`，而在 `mwv207_draw.c`

`mwv207_context_state_init()`（`P/mwv207_state.c:463-499`）注册的全部内容为：

| 类别 | 注册项 | 位置 |
|---|---|---|
| rasterizer | `create_/bind_/delete_rasterizer_state` | `state.c:465-467` |
| depth-stencil-alpha | `create_/bind_/delete_depth_stencil_alpha_state` | `state.c:469-471` |
| 杂项 | `set_clip_state`、`set_polygon_stipple`（均为空实现） | `state.c:473-474` |
| 视口/裁剪 | `set_viewport_states`、`set_scissor_states` | `state.c:475-476` |
| 采样 | `set_sample_mask` | `state.c:477` |
| 变换反馈 | `create_stream_output_target`、`stream_output_target_destroy`、`set_stream_output_targets` | `state.c:479-481` |
| 模板引用 | `set_stencil_ref` | `state.c:483` |
| 子初始化 | `mwv207_context_vertex_init(pctx)`、`mwv207_context_blend_init(pctx)` | `state.c:485-486` |
| 细分 | `set_tess_state`、`set_patch_vertices` | `state.c:488-489` |

`mwv207_state.c` **不注册任何 Mesa 状态原子（`set_*_state` / `atom` 机制）**，也**不发射任何寄存器**：它只做「pipe 状态 → 驱动私有 `uint32_t` 打包」+ 置 `ctx->dirty` 位。

`P/include/mwv207_state.h:143` 声明了 `mwv207_context_zsa_init`，但**全驱动未见定义、也未见调用**（`state.h:124-144` 逐条核对）——该声明为悬空空壳。

**真正的 atom 是 `P/mwv207_draw.c` 中以 `ctx->dirty` 为闸门的 `mwv207_config_*` 函数**，在每次 `mwv207_draw_vbo` 里按位调用（`mwv207_draw.c:1174-1258`）。

### 3.2 dirty 位 ↔ 状态发射函数 ↔ 寄存器

`enum mwv207_dirty`（`P/include/mwv207_context.h:53-76`）与发射函数对照：

| dirty 位（`context.h` 行） | 置位者 | 发射函数（`mwv207_draw.c`） | 主要寄存器（驱动号/厂商名） |
|---|---|---|---|
| `DIRTY_BLEND` `1<<0` (:54) | `mwv207_bind_blend_state`（`blend.c:348`） | `mwv207_config_blend` (:666, 由 :1229 触发) | `0x530` PE_ALPHA_CONFIG、`0x50A`（未映射）、`0x50B`（未映射）、`0x529` PE_CONFIG、`0x5258+n`（未收录）、`0x5248+n`（未收录）、`0x52A/0x52B` PE_DITHER_LOW/HIGH |
| `DIRTY_SAMPLERS` `1<<1` (:55) | `mwv207_bind_sampler_states`（`sampler.c:934`） | `mwv207_config_sampler`（`sampler.c:493`） | `0x5800/0x5880/0x5900/0x5980/0x5A00/0x5600/0x5680/0x5311`（TC）与 `0x5B00…/0x5312`（TC），见 §6 |
| `DIRTY_RASTERIZER` `1<<2` (:56) | `mwv207_bind_rasterizer_state`（`state.c:163`） | `mwv207_config_rasterizer` (:500) | `0x2A2` PA_CLIP_CONTROL、`0x28A`（未映射）、`0xE13`（未映射）、`0x28D`（未映射）、`0x7000` TFB_CONTROL、`0x286/0x28E/0x28F`、`0x287`、`0x380`、`0x306`、`0x305`、`0x304` |
| `DIRTY_ZSA` `1<<3` (:57) | `create_/bind_…zsa`（`state.c:172-299` 打包，绑定在 `state.c:469-471`） | `mwv207_config_zsa` (:639) | `0x508`（未映射）、`0x5250+n`（未收录）、`0x528` PE_EXTRA_REFERENCE、`0x388` RA_CONTROL_HZ、`0x515` PE_HIERARCHICAL_CONTROL |
| `DIRTY_VERTEX_ELEMENTS` `1<<4` (:58) | `mwv207_bind_elements`（`vertex.c:74`） | `mwv207_config_vertex` (:75) | `0x5E00+i / 0x5E80+i / 0x5EA0+i / 0x51A0+i`（**未收录**） |
| `DIRTY_BLEND_COLOR` `1<<6` (:60) | `mwv207_set_blend_color`（`blend.c:359`） | `mwv207_config_blendColor` (:734) | `0x509`（未映射）、`0x52C` PE_BLEND_FACTOR_LOW、`0x52D` PE_BLEND_FACTOR_HIGH、`0x5260+n / 0x5268+n`（未收录） |
| `DIRTY_STENCIL_REF` `1<<7` (:61) | `mwv207_set_stencil_ref`（`state.c:332`） | `mwv207_config_other` (:1026-1037) | `0x506/0x507`（未映射）、`0x528` PE_EXTRA_REFERENCE、`0x52E` PE_BACKFACE_STENCIL |
| `DIRTY_SAMPLE_MASK` `1<<8` (:62) | `mwv207_set_sample_mask`（`state.c:460`） | `mwv207_config_sample_mask` (:615) | `0x415` PS_SAMPLE_COVERAGE_CTRL |
| `DIRTY_VIEWPORT` `1<<9` (:63) | `mwv207_set_viewport_states`（`state.c:343`） | `mwv207_config_viewport` (:184) | `0x280` AHBDEC_WRITE_EX_CONFIG、`0x281–0x285`（未映射）、`0x501/0x502`（未映射）、`0x2A0` AHBDEC_READ_BUFFER_BASE、`0x28B`（未映射）、`0x2A3` PA_SMALL_W、`0x2A1` PA_MAX_POINT_SIZE |
| `DIRTY_FRAMEBUFFER` `1<<10` (:64) | `mwv207_set_framebuffer_state`（`surface.c:354`） | `mwv207_config_framebuffer` (:253)、`mwv207_config_multisamples` (:544) | `0x529`、`0xE03`、`0xffff`、`0x596/0x597/0x598/0x5AF/0x595/0x518+n/0x50D/0x50B/0xE28/0x690+n/0x5F0+n/0x5F8+n/0x680+n/0x688+n/0x5E8+n/0x5200+…/0x404/0x40C/0x40D/0x426/0x38D/0x500/0x503/0xE23/0x505/0x599/0x59A/0x59B/0x6D0`、`0xE06`、`0x418+n`、`0x381`、`0x390+n`、`0x383`、`0x52F` |
| `DIRTY_SCISSOR` `1<<11` (:65) | `mwv207_set_scissor_states`（`state.c:353`） | `mwv207_config_scissor` (:209) | `0x301/0x300/0x302/0x303/0x308/0x309`（未映射） |
| `DIRTY_SAMPLER_VIEWS` `1<<12` (:66) | `mwv207_set_sampler_views`（`sampler.c:1046`） | `mwv207_config_sampler` | 同 §6 |
| `DIRTY_CONSTBUF` `1<<13` (:67) | `mwv207_set_constant_buffer`（`vertex.c:464`）、`config_render_piepline`（`draw.c:1184`） | `mwv207_config_uniform`（`vertex.c:306`） | `0x5291` TCS_UNIFORM_BASE、`0x52C9` TES_UNIFORM_BASE、`0x453` GS_UNIFORM_BASE、`0x…(base+i)` 立即数区（§7） |
| `DIRTY_VERTEX_BUFFERS` `1<<14` (:68) | `mwv207_set_vertex_buffers`（`vertex.c:94`） | `mwv207_config_vertex` | `0x5180+i`（未收录）、`0x5190+i`（未收录） |
| `DIRTY_INDEX_BUFFER` `1<<15` (:69) | （未见单独置位点） | `mwv207_config_vertex` 每帧写索引寄存器 | `0x191/0x192/0x19D` |
| `DIRTY_SHADER` `1<<16` (:70) | `mwv207_config_render_piepline`（`draw.c:1185`）、`update_shader` | `config_vs/tcs/tes/gs/fs/link`（`draw.c:1235-1245`） | 见 `mwv207_shader.c`（本文不展开） |
| `DIRTY_TS` `1<<17` | 资源层 | — | — |
| `DIRTY_TEXTURE_CACHES` `1<<18`、`DIRTY_DERIVE_TS` `1<<19` | 资源层 | — | — |
| `DIRTY_STREAM_OUT` `1<<20` (:74) | `mwv207_set_stream_output_targets`（`state.c:399`） | `mwv207_config_stream_out` (:785) | `0xE03`、`0x7001` TFB_COMMAND、`0x7002`（未收录）、`0x7200+n`（未收录）、`0x7040` TFB_DESCRIPTOR_COUNT、`0x7010+n`（未收录）、`0x7030+i` TFB_BUFFER_STRIDE、`0x7020+i` TFB_BUFFER_SIZE |
| `DIRTY_ALL` `0xFFFFFFFF` (:75) | `mwv207_submit`（`cmd.c:370`） | 全部 | — |

> `DIRTY_INDEX_BUFFER` 与 `DIRTY_STREAM_OUT` 之外的 `1<<5`、`1<<17..19` 在 `context.h:53-76` 定义为位，但本条通路未看到发射点，属资源/着色器子系统的责任范围。

---

## 4. 状态打包（`P/mwv207_state.c`）

### 4.1 查表

| 表 | 位置 | 内容 |
|---|---|---|
| `compare_func[]` | `state.c:38-47` | `{0,1,2,3,4,5,6,7}`——与 `PIPE_FUNC_NEVER..ALWAYS` **恒等**（无实际翻译） |
| `stencil_operation[]` | `state.c:49-58` | `{0,1,2,3,4,6,7,5}`——把 `PIPE_STENCIL_OP_*` 顺序重排为硬件序（KEEP/ZERO/REPLACE/INCR/…) |

### 4.2 光栅化 `mwv207_create_rasterizer_state`（`state.c:92-156`）

| 驱动字段 | 计算 | 位置 |
|---|---|---|
| `line_scale` | `line_scale(state->line_width, 0.125, 1.0, 8.0)`：先按 0.125 量化、`CLAMP(1.0, 8.0)`，再 `fui(width / 2.0f)` | `state.c:60-72, 103` |
| `assert` | `depth_clip_far == depth_clip_near`（否则断言） | `state.c:105` |
| `depth_clamp` | `(depth_clip_near == 0)` | `state.c:106` |
| `fill_mode` | `PIPE_POLYGON_MODE_FILL → 0x2`；`LINE → 0x1`；`POINT → 0x0`；`FILL_RECTANGLE → 0x2` | `state.c:74-90, 109` |
| `cull_mode` | `FACE_NONE → 0x0`；`FACE_FRONT → front_ccw ? 0x2 : 0x1`（并改 `fill_mode = fill_mode(fill_back)`）；`FACE_BACK → front_ccw ? 0x1 : 0x2`；`FACE_FRONT_AND_BACK → 不改 cull_mode` | `state.c:111-134` |
| `tfb_control` | `(rasterizer_discard \|\| cull_face == PIPE_FACE_FRONT_AND_BACK) ? 0x1 : 0x0` | `state.c:135-138` |
| `flat_mode` | 恒 `0x0` | `state.c:139-141` |
| `pa_config` | 见下表 | `state.c:145-153` |

`pa_config` 位域（`state.c:145-153`）：

| bit | 掩码 | 值来源 |
|---|---|---|
| 2 | `0x4` | `base.point_size_per_vertex` |
| 6 | `0x40` | 常量 `false` |
| 8-9 | `0x300` | `cull_mode` |
| 12-13 | `0x3000` | `fill_mode` |
| 16-17 | `0x30000` | `flat_mode` |
| 22 | `0x400000` | 常量 `true` |
| 24-27 | `0xF000000` | 常量 `0` |
| 29 | `0x20000000` | 常量 `false` |

`mwv207_bind_rasterizer_state`（`state.c:158-164`）：`ctx->rast_state = state; dirty \|= DIRTY_RASTERIZER`。

### 4.3 深度/模板/alpha `mwv207_create_depth_stencil_alpha_state`（`state.c:172-299`）

| 驱动字段 | 计算 | 位置 |
|---|---|---|
| `stencil_test` | `stencil[0].enabled \| stencil[1].enabled` | `state.c:183` |
| `front_not_keep` | `zpass_op != KEEP \|\| zfail_op != KEEP` | `state.c:185-187` |
| `back_not_keep` | 同上（back 面） | `state.c:189-191` |
| `depth_bounds_test` | 直接取 `state->depth_bounds_test` | `state.c:193-197` |
| `early_z` | `(depth_func == PIPE_FUNC_NOTEQUAL) \|\| (stencil_test && (front_not_keep \|\| back_not_keep))` | `state.c:199-201` |
| `mode` | `(stencil_test \|\| depth_enabled) ? 0x1 : 0x0` | `state.c:203-206` |
| `func` | `depth_enabled ? compare_func[depth_func] : 0x7` | `state.c:208-211` |
| `depth_config` | 见下表 | `state.c:213-219` |
| `stencil_mode` | 两面都 enabled → `0x2`，否则 `0x1` | `state.c:228-231` |
| 无模板时 | front/back 全部 `func = ALWAYS`、三个 op 全 `KEEP` | `state.c:232-242` |
| alpha（开） | `alpha_config = enabled & 0x1 \| compare_func[alpha_func]<<4 & 0x70 \| (alpha_ref_value*0xFF)<<8 & 0xFF00`；`alpha_reference = (alpha_ref_value*0xFFFF)<<0x10 & 0xFFFF0000 \| ~(0xFFFF0000\|0x200)` | `state.c:282-296` |
| alpha（关） | `alpha_config = 0 \| 0x7<<4 \| 0`；`alpha_reference = 0 \| ~(0xFFFF0000\|0x200)` | `state.c:294-296` |

`depth_config` 位域（`state.c:213-219`）：

| bit | 掩码 | 值来源 |
|---|---|---|
| 24 | `0x1000000` | 常量 `0x1` |
| 18 | `0x40000` | 常量 `0x1` |
| 0-1 | `0x3` | `mode` |
| 16 | `0x10000` | `early_z` |
| 8-10 | `0x700` | `func` |
| 12 | `0x1000` | `depth_writemask` |

模板打包（`mwv207_state.h:75-93` 的 `stencil_op[2] / stencil_config[2] / stencil_back_config[2]`）：

| 数组 | 位置 | 位域 |
|---|---|---|
| `stencil_op[0]` | `state.c:244-252` | bits2:0=`compare_func[front.func]`；bit4-6=`stencil_operation[front.zpass_op]`；bit8-10=`front.fail_op`；bit12-14=`front.zfail_op`；bit16-18=`compare_func[back.func]`；bit20-22=`back.zpass_op`；bit24-26=`back.fail_op`；bit28-30=`back.zfail_op` |
| `stencil_config[0]` | `state.c:254-257` | bits1:0=`stencil_mode`；bits23:16=`front.valuemask`；bits31:24=`front.writemask` |
| `stencil_back_config[0]` | `state.c:259-261` | bits15:8=`back.writemask`；bits7:0=`back.valuemask` |
| `stencil_op[1]` | `state.c:263-271` | **前后互换**：bits2:0=`back.func`；bit4=`back.zpass`；bit8=`back.fail`；bit12=`back.zfail`；bits16..=`front.func/zpass/fail/zfail` |
| `stencil_config[1]` | `state.c:273-276` | bits1:0=`stencil_mode`；bits23:16=`back.valuemask`；bits31:24=`back.writemask` |
| `stencil_back_config[1]` | `state.c:278-280` | bits15:8=`front.writemask`；bits7:0=`front.valuemask` |

> `stencil_op[1]` / `stencil_config[1]` / `stencil_back_config[1]` 的存在说明硬件对 flip（`front_ccw`）情形需要第二套前后互换的模板状态；驱动每次发射时按 `ctx->rast_state->base.front_ccw` 选下标（`mwv207_draw.c:1027-1036`）。

### 4.4 模板引用 `mwv207_set_stencil_ref`（`state.c:315-333`）

| 字段 | 计算 |
|---|---|
| `stencil_reference[0]` | `ref_value[0] << 8 & 0xFF00` |
| `stencil_reference[1]` | `ref_value[1] << 8 & 0xFF00` |
| `stencil_back_reference[0]` | `ref_value[1] & 0xFF \| ~(0xFF \| 0x100)` |
| `stencil_back_reference[1]` | `ref_value[0] & 0xFF \| ~(0xFF \| 0x100)` |

注意 **`back_reference[0]` 取的是 `ref_value[1]`、`back_reference[1]` 取 `ref_value[0]`**（交叉），与正面 `[0]→[0]` 的对应关系不同（`state.c:326-331`）。

### 4.5 其余状态

| 函数 | 位置 | 行为 |
|---|---|---|
| `mwv207_set_viewport_states` | `state.c:335-344` | 只取 `*state`（**忽略 `start_slot`/`num_viewports`，硬编码单视口**）；`dirty \|= DIRTY_VIEWPORT` |
| `mwv207_set_scissor_states` | `state.c:346-354` | 只取 `*states`（同样单裁剪矩形）；`dirty \|= DIRTY_SCISSOR` |
| `mwv207_set_polygon_stipple` / `mwv207_set_clip_state` | `state.c:356-361` / `363-368` | **空实现** |
| `mwv207_set_stream_output_targets` | `state.c:370-400` | 引用各 target 到 `ctx->stream_out.targets[i]`，拷贝 `offsets[i]`；当 `num_targets > 0 && targets[0]->bo == NULL` 时 `bo_create(4096, MWV207_DOMAIN_VRAM, MWV207_FLAG_CPU_ACCESS)` 并置 `is_actived = false`；`dirty \|= DIRTY_STREAM_OUT` |
| `mwv207_create_stream_output_target` | `state.c:402-421` | `CALLOC_STRUCT` + `pipe_reference_init(1)` + `pipe_resource_reference` |
| `stream_output_target_destroy` | `state.c:423-435` | 释放 BO 与 buffer |
| `mwv207_set_tess_state` | `state.c:437-445` | `memcpy` 4 个 outer、2 个 inner float 到 `ctx->tess_outer_level/tess_inner_level` |
| `MWV207_set_patch_vertices` | `state.c:447-452` | `ctx->info.vertices_per_patch = patch_vertices` |
| `mwv207_set_sample_mask` | `state.c:454-461` | `ctx->sample_mask = sample_mask`；`dirty \|= DIRTY_SAMPLE_MASK` |

---

## 5. 混合（`P/mwv207_blend.c`）

### 5.1 查表

| 表 | 位置 | 内容（按枚举下标） |
|---|---|---|
| `alpha_func[]` | `blend.c:31-59` | 27 项：`{0x0,0x1,0x2,0x4,0x6,0x8,0xA,0xD,0xB,~0U×8,0x0(17),0x3,0x5,0x7,0x9,~0U,0xE(23),0xC(24),~0U,~0U}`，下标为 `enum pipe_blendfactor` |
| `alpha_mode[]` | `blend.c:61-67` | `{0x0,0x1,0x2,0x3,0x4}`，下标为 `PIPE_BLEND_ADD..MAX` |
| `alpha_mode_ext[]` | `blend.c:69-90` | 20 项：`{0,0,0,0,0,1,2,3,4,5,~0U,~0U,6,~0U,7,8,~0U,~0U,~0U,~0U}` |
| `alpha_opt[]` | `blend.c:92-114` | 21 项布尔：`{0,T,T,T,F,F,T,T,T,0,0,T,T,T,F,F,0,T,T,0,0}` |
| `logic_opt[]` | `blend.c:116-133` | 16 项恒等 `0x0…0xF` |
| `adv_ext[]`（指定初始化） | `blend.c:148-189` | `BLEND_MULTIPLY=0x1`、`SCREEN=0x2`、`OVERLAY=0x3`、`DARKEN=0x4`、`LIGHTEN=0x5`、`HARDLIGHT=0x6`、`DIFFERENCE=0x7`、`EXCLUSION=0x8`；`COLORDODGE/COLORBURN/SOFTLIGHT/HSL_*`（0x9–0xF）**故意不映射** |

`alpha_opt[]` 的语义 : 判断某 blend factor 是否「无需读取目标色」（`blend.c:231-279` 用它算 `no_read_dst`）。

### 5.2 advanced blend 通路（重大行为变更点）

`blend.c:148-189` 的注释记录了 **Mesa 23 起高级混合方程经 `pipe_blend_state::advanced_blend_func` 传入**，而 `rt[i].blend_enable` / `rgb_func` 保持默认值，因此历史上按 `alpha_mode_ext[rt->rgb_func]` 查表的方式**再也走不到 advanced 表项**。本驱动的做法：

1. 取 `state->advanced_blend_func`，若 `adv <= BLEND_HSL_LUMINOSITY` 且 `adv_ext[adv] != 0`：
   - `ctx->blend_state->advanced_ext = adv_ext[adv]`；
   - 强制 `rt[0].blend_enable = 1`；
   - rgb/alpha 的 src/dst factor 全设为 `PIPE_BLENDFACTOR_ONE`；
   - `colormask = 0xf`；`independent_blend_enable = 1`。
2. 注释明确说明 `0x9..0xF`（COLORDODGE/COLORBURN/SOFTLIGHT/HSL_*）实测会把 framebuffer 清零，属**硬件缺口**，故意不映射（`blend.c:155-159`）。

### 5.3 `alpha_config` / `alpha_blend` / `blend_config` / `blend_configex` 打包

逐 RT 循环 `i = 0..PIPE_MAX_COLOR_BUFS-1`（`blend.c:200-316`），`rt = independent_blend_enable ? &rt[i] : &rt[0]`。

| 中间量 | 计算 | 位置 |
|---|---|---|
| `dst_read` | 初值 `logicop_enable && logicop_func != COPY`；`rt->blend_enable` 也置真；`color_mask[i] != 0xf` 也置真 | `blend.c:196-198, 211-217, 312-313` |
| `logic_config` | `logicop_enable ? logic_opt[logicop_func] & 0xF : logic_opt[PIPE_LOGICOP_COPY]`，再 `\| ~(0xF\|0x10)` | `blend.c:191-194` |
| `blend_enable` | `rt->blend_enable && !(src/dst factor 为 ONE/ZERO 且 func 为 ADD 的全 no-op)` | `blend.c:211-217` |
| `separate_alpha` | `blend_enable && !(rgb 与 alpha 的 src/dst factor 及 func 全相同)` | `blend.c:218-221` |
| `modeAdd` / `modeSub` | `rgb_func==ADD && alpha_func==ADD` / 两者均 `SUBTRACT` | `blend.c:226-229` |

**无目标读优化（`modeAdd \|\| modeSub` 且 `colormask == 0xf`）**（`blend.c:231-279`）：

| 条件 | `alphaConfig` 置位 | `alphaControl` 置位 |
|---|---|---|
| `src_alpha_one_noread`（dst factor 为 `INV_SRC_ALPHA` 两次） | bit5（`0x20`） | bit17（`0x20000`） |
| `src_alpha_zero_noread`（dst factor 为 `SRC_ALPHA` 两次） | bit11（`0x800`） | bit2（`0x4`） |
| `src_all_one_noread`（dst factor 为 `INV_SRC_ALPHA`/`INV_SRC_COLOR`） | bit6（`0x40`） | bit18（`0x40000`） |
| `src_all_zero_noread`（dst factor 为 `SRC_ALPHA`/`SRC_COLOR`） | bit12（`0x1000`） | bit3（`0x8`） |

**RT0（`i == 0`）的两张主表**（`blend.c:282-297`）：

| 字段 | 位域 |
|---|---|
| `alpha_config` | bits3:0 = `advanced_ext ? advanced_ext : alpha_mode_ext[rt->rgb_func]`；bit4 = `0`（`blend.c:283-284`） |
| `alpha_blend` | bit0 = `blend_enable`；bit16 = `separate_alpha`；bits7:4 = `alpha_func[rgb_src]`；bits23:20 = `alpha_func[alpha_src]`；bits11:8 = `alpha_func[rgb_dst]`；bits27:24 = `alpha_func[alpha_dst]`；bits14:12 = `alpha_mode[rgb_func]`；bits30:28 = `alpha_mode[alpha_func]` |

**RTn（`i > 0`，independent blend）**（`blend.c:299-315`）：

| 字段 | 位域 |
|---|---|
| `blend_config[i]` | `= alphaControl \|` 上面 `alpha_blend` 的同一组 7 段（bit0 `blend_enable`、bits7:4、bits11:8、bits14:12、bits23:20、bits27:24、bits30:28）；**不含** `separate_alpha` |
| `blend_configex[i]` | bit29（`0x20000000`）= `separate_alpha` |
| `color_mask[i]` | `rt->colormask`（直存） |
| `dest_read[i]` | `dst_read` |

### 5.4 effect 顺序与 dual-source

| 函数 | 位置 | 行为 |
|---|---|---|
| `mwv207_bind_blend_state` | `blend.c:327-349` | 存 `ctx->blend_state`；`ctx->info.dsbi.is_dual = false`；非空时 `is_dual = util_blend_state_is_dual(&blend_state, 0)`、`dsbi.blend_state = rt[0]`；若 `is_dual`：`alpha_blend = (!is_dual)<<0`（恒 0），且每个 `blend_config[i] = 0 \| (!is_dual)<<0`（**即全部清零**） |
| `mwv207_set_blend_color` | `blend.c:351-360` | `ctx->blend_color = *color`；`dirty \|= DIRTY_BLEND_COLOR` |
| `mwv207_context_blend_init` | `blend.c:362-369` | 注册 `create/bind/delete_blend_state` + `set_blend_color` |

> dual-source 的实现不在 blend.c，而在 `mwv207_draw.c:1114-1171`（`mwv207_config_dual_blend`）：把 cbuf0 拷到临时纹理、作为第 N 号采样器视图/采样器绑定给 FS，见 §8.9。

---

## 6. 采样器 / 纹理（`P/mwv207_sampler.c`）

### 6.1 采样器寄存器基址（`sampler_regs[]`，`sampler.c:78-90`）

`config_id = (stage == PIPE_SHADER_FRAGMENT || stage == PIPE_SHADER_COMPUTE) ? 0x1 : 0x0`（`sampler.c:507-508`）。

| 结构成员 | config_id=0（VS/GS/TCS/TES） | config_id=1（FS/CS） | 用途 |
|---|---|---|---|
| `sampler_ctrl0` | `0x5800` | `0x5B00` | 地址模式/过滤/比较 |
| `sampler_ctrl1` | `0x5880` | `0x5B80` | 采样控制 |
| `sampler_lod_config` | `0x5900` | `0x5C00` | min/max lod |
| `sampler_lod_bias` | `0x5980` | `0x5C80` | lod bias |
| `sampler_anisval` | `0x5A00` | `0x5D00` | 各向异性 |
| `tex_desc_address` | `0x5600` | `0x5700` | **dat 重定位**目标（描述符地址） |
| `tex_control` | `0x5680` | `0x5780` | 写 0 |
| `tex_command` | `0x5311` | `0x5312` | 触发采样器更新 |

> 除 `0x5311`（`MWV207REG_TX_GPIPE_COMMAND`，0x14C44）外，其余基址均为 `base+index` 动态号，**REG 未收录** → 厂商名未确认。

`index = i + stage_offset`，`stage_offset` 由 `mwv207_get_stage_offset` 给出（`sampler.c:472-491`）：VS `0x10`、FS `0x0`、GS `0x30`、TCS `0x20`、TES `0x28`、CS `0x0`，默认 `0x50`。

### 6.2 `sampler_ctrl0` 位域（`sampler.c:536-558`）

| bit | 掩码 | 值 |
|---|---|---|
| 0-2 | `0x7` | `mwv207_address_info[wrap_s]` |
| 3-5 | `0x38` | `mwv207_address_info[wrap_t]` |
| 6-8 | `0x1C0` | `mwv207_address_info[wrap_r]` |
| 9-10 | `0x600` | `mwv207_fliter_info[min_filter]` |
| 11-12 | `0x1800` | `mwv207_fliter_info[mip_filter]` |
| 13-14 | `0x6000` | `mwv207_fliter_info[mag_filter]` |
| 15 | `0x8000` | `!unnormalized_coords` |
| 16 | `0x10000` | `tex_desc->stencil` |
| 17 | `0x20000` | `compare_mode` |
| 18-20 | `0x1C0000` | `compare_func`（`mwv207_compare_func[compare_func]`，`sampler.c:877-880`） |
| 21-22 | `0x600000` | 常量 `0x1`（`(ALIGN_BIT(0x1,0x15) & 0x600000)`） |
| 23 | `0x800000` | `fast_filter && sampler_anisval == 0` 时置 1（`sampler.c:557-558`） |

**查表**：

| 表 | 位置 | 内容 |
|---|---|---|
| `mwv207_address_info[]` | `sampler.c:37-47` | `{0x0, 0x2, 0x2, 0x3, 0x1, 0x1, 0x1, 0x1}`，下标 `enum pipe_tex_wrap`（REPEAT/MIRROR_REPEAT/CLAMP_TO_EDGE/CLAMP_TO_BORDER/MIRROR_CLAMP…） |
| `mwv207_fliter_info[]` | `sampler.c:49-53` | `{0x1, 0x2, 0x0}`，下标 `PIPE_TEX_FILTER_NEAREST/LINEAR` + MIPFILTER |
| `mwv207_compare_func[]` | `sampler.c:55-64` | `{0x7,0x2,0x4,0x0,0x3,0x5,0x1,0x6}` |
| `mwv207_texture_type[]` | `sampler.c:66-76` | `{0x2,0x2,0x2,0x3,0x5,0x2,0x3,0x3,0x5}`，下标 `enum pipe_texture_target` |

**深度/模板纹理强制最近邻**（`sampler.c:543-550`）：纹理格式为 depth/stencil 时，三个 filter 若为 `LINEAR` 一律改 `NEAREST`。

### 6.3 `sampler_ctrl1 / lod_config / lod_bias / anisval`（`sampler.c:560-576, 891-908`）

| 字段 | 位域 / 计算 |
|---|---|
| `sampler_ctrl1` | bits1:0 = `0x2`（`sampler.c:891-892`）；bit6 = `unnormalizedCoordinates`（**常量 0**，`sampler.c:875`）；bit2 = `tex_desc->srgb`（`sampler.c:560-561`） |
| `sampler_lod_config` | bits12:0 = `maxlod`（`float_to_fixed(max_lod,5,8)`）；bits28:16 = `minlod`（`float_to_fixed(MAX2(0.0f,min_lod),5,8)`）（`sampler.c:897-899`） |
| `sampler_lod_bias` | bits15:0 = `lodbias`（`float_to_fixed(lod_bias,8,8)`）；bit16 = `(lodbias != 0)`（`sampler.c:901-903`） |
| `sampler_anisval` | `max_anisotropy > 1` 时 `mwv207_log2(max_anisotropy)`（断言 `<= 16`）；立方体纹理强制写 0（`sampler.c:905-908, 575-576`） |

`mwv207_float_to_fixed(x, trunc_bits, frac_bits)`（`sampler.c:129-159`）：把一个 float 转成定点，做饱和到 `fixed_max`/`fixed_min`，支持负数取补码。
`mwv207_log2(val)`（`sampler.c:94-127`）：返回 **8.8 定点**形式的 log2（`scale = 1<<8`），低 8 位是小数。

### 6.4 纹理描述符 `struct mwv207_texture_config`（`mwv207_format.h:98-123`）

| 偏移（word） | 字段 | 说明 |
|---|---|---|
| 0-15 | `address[16]` | 每级 mip 的物理地址；`mwv207_reconfig_texture` 逐级做 dat reloc（`sampler.c:464-465`） |
| 16 | `tex_config` | 主配置 |
| 17 | `tex_size` | 宽/高 |
| 18 | `tex_stride` | 行 stride |
| 19 | `tex_config_ext` | 扩展配置（格式高位/swizzle/对齐/array/msaa） |
| 20 | `tex_yuv_control` | YUV（本驱动未写） |
| 21 | `tex_yuv_stride` | YUV（未写） |
| 22-25 | `tex_astc0..3` | ASTC 参数 |
| 26 | `tex_base_lod` | base/mip 数 |
| 27 | `tex_config2` | int format 等 |
| 28 | `tex_config3` | msaa 位 |
| 29 | `tex_size_ext` | log2 宽/高 |
| 30 | `tex_volume_ext` | log2 深度 |
| 31 | `tex_slice` | layer size |
| 32 | `tex_border_color` | 打包的 8 位 border |
| 33 | `tex_3d` | 深度 |
| 34 | `tex_log_size` | log size / ASTC 标志 |
| 35-38 | `tex_border_blue32/green32/red32/alpha32` | 32 位 border 分量（**注意顺序：blue 在前**） |

### 6.5 描述符位域构建 `mwv207_get_texture_info`（`sampler.c:672-854`）

| 字段 | 位域 |
|---|---|
| `tex_config` | bits2:0 = `mwv207_texture_type[viewType]`；bits17:13 = `tex_desc->format >> 0`（`& 0x3E000`）；bits21:20 = `addressing_mode`（`& 0x300000`）（`sampler.c:777-780`） |
| `tex_size` | bits14:0 = `level_info->width`；bits30:16 = `level_info->height`（`sampler.c:782-784`） |
| `tex_3d` | bits13:0 = `depth`（`sampler.c:786`） |
| `tex_stride` | `level_info->stride`（`sampler.c:788`） |
| `tex_config_ext` | bits5:0 = `tex_desc->format >> 0x8`（`& 0x3F`）；bits10:8 = `swizzle_r`；bits14:12 = `swizzle_g`；bits18:16 = `swizzle_b`；bits22:20 = `swizzle_a`；bit15 = `false`（`0x8000`）；bit24 = `is_array`；bits28:26 = `horizonal_align`；bit25 = `ENABLE`（`0x2000000`）；bit29 = `yuv_standard`；bit31 = `0x0`；bit23 = `tex_desc->msaa`；bit7 = `false`（`sampler.c:790-803`） |
| `tex_base_lod` | bits3:0 = `0`；bits11:8 = `levelCount - 1`（`sampler.c:805-807`） |
| `tex_config2` | 初值 `0x30000`；`int_format == 0` 时 bit16 = `DISABLE`（清）；bits19:18 = `int_format`（`sampler.c:688, 809-813`） |
| `tex_config3` | bit3 = `tex_desc->msaa`（`sampler.c:815-817`） |
| `tex_size_ext` | bits15:0 = `log_width`；bits31:16 = `log_height`（`sampler.c:819-821`） |
| `tex_volume_ext` | bits15:0 = `log_depth`（`sampler.c:823-824`） |
| `tex_slice` | `level_info->layer_size`（`sampler.c:826`） |
| `tex_log_size` | bits25:24 = `0`（默认），ASTC 路径改 `2`（`sampler.c:828, 837`） |
| `tex_astc0..3` | 见下 |

**ASTC 特例**（`sampler.c:830-851`）：当 `(tex_desc->format >> 0x8) == 0x14` 时，把 `util_format_linear(format) - PIPE_FORMAT_ASTC_4x4` 作为 `astc_size`（断言 `<= 0xD`），四个 `tex_astcN` 各写 8 个 4 位字段（size/srgb 交替，bit0/4/8/12/16/20/24/28），并置 `tex_log_size` bits25:24 = 2。

**地址模式 / 水平对齐**（`sampler.c:708-726`）：

| 条件 | `addressing_mode` | `horizonal_align` |
|---|---|---|
| 压缩格式 | `0x0` | `0x1` |
| `res->mode == MWV207_SUPERTILED` | `0x0` | `0x2` |
| `last_level != 0` | `0x1` | — |
| 其他 | `0x3` | 视 blocksize：`>= 8B → 0x2`；`TILED → 0x0`；否则 `0x1` |

**swizzle 编码**（`sampler.c:646-670, 728-743`）：

| `pipe_swizzle` | 输出 |
|---|---|
| `PIPE_SWIZZLE_NONE` | `current_set`（表面/纹理自身 swizzle） |
| `PIPE_SWIZZLE_0` | `0x4`（= `SWIZZLE_ZERO`，`mwv207_format.h:63`） |
| `PIPE_SWIZZLE_1` | `0x5`（= `SWIZZLE_ONE`） |
| `X/Y/Z/W` | `texture_swizzles[0..3]` |
| 其他 | 断言 `"WRONG SWIZZLE"`，返回 `0x4` |

`tex_desc->srgb/stencil/msaa/cube/fast_filter` 的判定（`sampler.c:745-757`）：`fast_filter` 需同时满足 unorm、非压缩、非 srgb、非 depth/stencil、非 R10G10B10A2/B10G10R10A2，且 `mwv207_texture_type[viewType] == 0x2`。

### 6.6 Border color 与 `tex_fetch`

| 函数 | 位置 | 行为 |
|---|---|---|
| `mwv207_clamp_color(format, sampler)` | `sampler.c:366-417` | `util_clamp_color`；压缩格式按 unorm 钳 `[0,1]` 否则 `[-1,1]`；depth/stencil 按具体格式做 24/16/8 位重缩放；**`Z32_FLOAT*` 与 `X32_S8X24_UINT` 分支为空**；未知格式 `printf("unsupport format …")` |
| `mwv207_transfer_border(swizzle, current)` | `sampler.c:349-364` | `0x0→0`、`0x1→1`、`0x2→2`、`0x3→3`，其他返回 `current` |
| `mwv207_reconfig_texture(view, sampler, use_ba)` | `sampler.c:419-470` | 算 border 四分量（`tex_border_red32/green32/blue32/alpha32`）；`tex_border_color` 打包为 `(a<<24)\|(r<<16)\|(g<<8)\|b`（`sampler.c:456-458`）；随后对每个 mip 做 `mwv207_dat_add_reloc(&tex_desc->dat, &cmd_buf, res->bo, address[levelIdx], 0)`，再把 `address[levelCount]` 之后的整块 memcpy 进 dat，最后 `dat.size = sizeof(struct mwv207_texture_config)`（`sampler.c:460-469`） |
| `tex_fetch[0..3]` | `sampler.c:759-762` | `{width, height, depth, stride}`，供 `get_texture_size()` 立即数用（§7） |

### 6.7 发射顺序 `mwv207_config_sampler`（`sampler.c:493-644`）

| 步骤 | 内容 |
|---|---|
| 1 | `0xE03` 置 bit5（`0x20`）、bit11（`0x800`）、bit10（`0x400`），再 `0xE03` 置 bit12（`0x1000`）、bit13（`0x2000`）（`sampler.c:512-519`） |
| 2 | 对每个已绑定 view：写 `sampler_ctrl0/1`、`lod_config`、`lod_bias`、`anisval`（`+index`），`tex_desc_address + index` 走 `mwv207_add_dat`，`tex_control + index = 0`（`sampler.c:567-580`） |
| 3 | `tex_128bit` 时对 RG 半边重复一遍，索引换成 `ctx->info.tex_record[stage][i] + stage_offset`（`sampler.c:582-601`） |
| 4 | `0x5C8 + 7` = `0x5CF` 写 `0xcccccccd`（`sampler.c:604`；`0x5C8` = `MWV207REG_MC_TEXTURE_CONFIG` 0x01720，`+7` 为未收录） |
| 5 | `mwv207_wait_idle(&cmd_buf, 0x7)`（`sampler.c:606`） |
| 6 | `0x5310` = `MWV207REG_TX_GLOBAL_CONFIG`，bit0 = ENABLE（`sampler.c:608-609`） |
| 7 | FS/CS 时对每个 view 用 `tex_command` 触发：bits31:28 = `0x2`，bits6:0 = `i + stage_offset`；有 `tex_record` 时再发一次（`sampler.c:611-626`） |
| 8 | `0xE03` 置 bit4（`0x10`）（`sampler.c:628`） |
| 9 | `ctx->info.texture_dirty = false`（`sampler.c:632`） |
| 10 | FS 且 `dsbi.is_dual` 时把最后一路 sampler view/sampler state 解绑（为 §8.9 的临时纹理腾位）（`sampler.c:634-643`） |

### 6.8 采样器/视图生命周期

| 函数 | 位置 | 要点 |
|---|---|---|
| `mwv207_create_sampler_state` | `sampler.c:856-911` | `sampler->state = *state`；`unnormalizedCoordinates` 恒 0；`compare_mode` 关闭时 `compare_func = 0x0` |
| `mwv207_bind_sampler_states` | `sampler.c:913-935` | 存指针，并从尾部回缩 `num_samplers`（**支持稀疏绑定**）；`dirty \|= DIRTY_SAMPLERS` |
| `mwv207_create_sampler_view` | `sampler.c:944-988` | `PIPE_BUFFER` 时把 `level_infos[0]` 伪造成 1D 纹理（`width = size/blocksize`、`height=1`、`depth=width`、`stride=size`）（`sampler.c:956-974`）；最后调 `mwv207_get_texture_info` |
| `mwv207_set_sampler_views` | `sampler.c:1001-1047` | 先算 `tex_record[stage][slot]`（`tex_128bit` 的 view 记 `num_views + tex_128_offset`，否则 0）；`take_ownership` 分支；尾部回缩；`dirty \|= DIRTY_SAMPLER_VIEWS` |
| `mwv207_get_sampler_state(ctx)` | `sampler.c:1049-1059` | 造一个 `wrap_s/t/r = CLAMP_TO_EDGE` 的默认采样器（供 §8.9 用） |
| `default_sampler` | `sampler.c:92, 1072-1083` | `wrap_* = PIPE_TEX_WRAP_CLAMP`、三个 filter `NEAREST/NONE/NEAREST`、`anisval=0`、`sampler_ctrl0 = 0x00382292`、`sampler_ctrl1 = 0x2`、`lod_bias = 0`、`lod_config = 0x00000fff` |

### 6.9 128-bit 纹理的拆分/合并

| 函数 | 位置 | 行为 |
|---|---|---|
| `mwv207_is_128bit_sampler` | `sampler.c:192-202` | `res->res_128_bit`，或 `PIPE_BUFFER` 且 blocksize == 16 |
| `mwv207_transfer_128bit(format)` | `sampler.c:204-215` | `R32G32B32A32_{FLOAT,UINT,SINT}` → `R32G32_{FLOAT,UINT,SINT}`，默认 `R32G32_FLOAT` |
| `mwv207_split_128bit(pctx, pres, format)` | `sampler.c:217-282` | 按需建 `rg`/`ba` 两张 R32G32 纹理；逐 level/layer/行做 CPU 拆包（`dst_rg[2i]=src[4i]`、`dst_ba[2i]=src[4i+2]`）；`src_dirty = false` |
| `mwv207_merge_128bit(pctx, pres)` | `sampler.c:284-347` | 反向合并；`last_level == 0` 时用 `tile_map[8] = {0,0,2,2,2,2,-4,-4}` 做 tile 角修正；末置 `rgba_dirty = false; src_dirty = true` |
| `mwv207_print_samper` | `sampler.c:161-190` | 调试打印（`ws->debug & 0x20`），并打印了描述符寄存器字面地址：`TXConfig 0x08014000`、`TXExtConfig 0x080140e0`、`TXConfig2 0x080141e0`、`TXConfig3 0x080144A0`、ASTC0..3 `0x08014140/160/180/1A0`（这些是命令字，不是驱动号） |

---

## 7. 顶点 / 索引 / 立即数（`P/mwv207_vertex.c`）

### 7.1 顶点元素状态

| 函数 | 位置 | 行为 |
|---|---|---|
| `mwv207_create_elements` | `vertex.c:39-65` | 逐元素拷贝 `base`，`fetch_break = true`，调 `mwv207_get_element_desc(&state->elements[i])` 填 `attribue_config/fetch_size`；随后若相邻元素**同一个 `vertex_buffer_index`**，则前一个 `fetch_break = false`（`vertex.c:56-60`） |
| `mwv207_bind_elements` | `vertex.c:67-75` | `ctx->element_state = state`；`dirty \|= DIRTY_VERTEX_ELEMENTS` |
| `mwv207_set_vertex_buffers` | `vertex.c:83-95` | `util_set_vertex_buffers_count`；`dirty \|= DIRTY_VERTEX_BUFFERS` |

**未确认**：`attribue_config` 与 `fetch_size` 的位域定义在 `mwv207_get_element_desc()`（实现于 `P/mwv207_format.c`，不在本文范围），本驱动层只原样透传。

### 7.2 顶点/索引寄存器发射 `mwv207_config_vertex`（`draw.c:75-181`）

| 顺序 | 驱动号 | 厂商寄存器 | 位域/值 | 位置 |
|---|---|---|---|---|
| 1 | `0x1F0` | `MWV207REG_FE_PATCH_CONTROL`（0x007C0） | `ctx->info.vertices_per_patch` | `draw.c:98` |
| 2 | `0x1F2` | `MWV207REG_FE_RESET_ATTRIBUTES`（0x007C8） | `true`（仅当 `num_elements == 0`，先 `wait_idle`） | `draw.c:100-103` |
| 3 | `0x5E00+i` | **未收录** | `element.attribue_config` | `draw.c:108` |
| 4 | `0x5E80+i` | **未收录** | `element.weight` | `draw.c:110` |
| 5 | `0x5EA0+i` | **未收录** | bit11 = `fetch_break`；bits8:0 = `fetch_size` | `draw.c:112-114` |
| 6 | `0x51A0+i` | **未收录** | `base.instance_divisor` | `draw.c:116` |
| 7 | `0x5180+i` | **未收录** | 顶点缓冲地址（`mwv207_add_relocs`，`reg != 0` 路径）；`res == NULL` 时写 0 | `draw.c:121-135` |
| 8 | `0x5190+i` | **未收录** | `stride[i] = element.base.src_stride` | `draw.c:136` |
| 9 | `0x191` | **未映射**（0x00644） | 索引缓冲地址（reloc，`reg != 0`） | `draw.c:158-159` |
| 10 | `0x192` | **未映射**（0x00648） | bits1:0 = `indexType`；bit8 = `primitive_restart` | `draw.c:161-163` |
| 11 | `0x19D` | `MWV207REG_FE_PRIMITIVE_RESTART`（0x00674） | `restart_index`（按需）或默认 `0xFF / 0xFFFF / 0xFFFFFFFF` | `draw.c:165-169` |

**索引类型**（`draw.c:143-152`）：`index_size == 1 → indexType 0x0`（restart `0xFF`）；`== 2 → 0x1`（`0xFFFF`）；否则 `0x2`（`0xFFFFFFFF`）。
无索引时三条索引寄存器全部写 0（`draw.c:170-174`）。

### 7.3 instance 处理

| 机制 | 位置 | 说明 |
|---|---|---|
| 逐属性 divisor | `draw.c:116` | `element.base.instance_divisor` → `0x51A0+i` |
| `start_instance` 预偏移 | `draw.c:128-129` | `offset += dinfo->start_instance * stride[i]`，即**软件把起始实例烘进缓冲基址**（顶点缓冲地址本身不变） |
| `instance_count` 编码 | `draw.c:924-931` | 直接绘制命令字 bits15:0 = `instance_count & 0xFFFF`，bits31:24 = `instance_count >> 16` |
| 间接绘制的实例标志 | `draw.c:919` | bit14 = `(instance_count != 1)` |

### 7.4 立即数机制（`MWV207_IMMEDIATE_*`）

枚举定义在 `P/nir/mwv207_common.h:134-155`（`enum mwv207_immediate_contents`）：`UNUSED=0`、`CONSTANT`、`UNIFORM`、`UNIFORM_TEXTURE_WIDTH/HEIGHT/DEPTH`、`UNIFORM_PATCH_VERTEX_IN`、`IMMEDIATE_TEXRECT_SCALE_X/Y`、`WORK_GROUP_COUNT`、`WORK_GROUP_SIZE`、`UBO`、`SSBO`、`SSIMAGE`、`SSIMAGE_STRIDE/SIZE/INFO/LAYER_SIZE`、`SSBO_SIZE`、`ATOMIC_BO`、`LOCAL_MEMORY_OFFSET`、`GLOBAL_MEMEORY_OFFSET`（原文拼写错误）。

着色器里这些立即数只占 1 个字类型标记 + 20 位索引（`nir/mwv207_common.h` 的 `struct mwv207_inst_src { uint32_t imm_val:20; uint32_t imm_type:2; ... }`），由驱动在**绘制时**展开成真实值写进 uniform 区。

**`mwv207_config_uniform(ctx, stage)`（`vertex.c:305-448`）**

| 阶段 | uniform 基址 |
|---|---|
| VS / TCS / TES / GS | `0xD000 + 0x0` |
| FS | `0xD800 + 0x400` |
| CS | `0xD800 + 0x0` |

TCS/TES/GS 会先写各自的 uniform base 偏移：`0x5291` = `MWV207REG_TCS_UNIFORM_BASE`（0x14A44）、`0x52C9` = `MWV207REG_TES_UNIFORM_BASE`（0x14B24）、`0x453` = `MWV207REG_GS_UNIFORM_BASE`（0x0114C），偏移量累加前级的 `imm_count + 4`（`vertex.c:346-367`）。

逐立即数的取值规则（`vertex.c:374-445`；用 `unifrom[i % 4]` 缓冲，每满 4 个用 `MWV207_EMITS(cmd_buf, base, 4, unifrom)` 发一次并 `base += 4`）：

| `imm_contents` | 取值 | 位置 |
|---|---|---|
| `CONSTANT` | `val` 本身 | `vertex.c:378-380` |
| `UNIFORM` | `data[val + buffer_offset]`（从 `cb[stage][0].user_buffer`） | `vertex.c:382-384` |
| `UNIFORM_TEXTURE_WIDTH/HEIGHT/DEPTH` | `sampler_view->tex_desc.tex_fetch[contents - WIDTH]` | `vertex.c:386-390`、`get_texture_size` `vertex.c:97-110` |
| `IMMEDIATE_TEXRECT_SCALE_X/Y` | `fui(1.0f / texture->width0 或 height0)` | `vertex.c:392-395`、`get_texrect_scale` `vertex.c:112-127` |
| `UNIFORM_PATCH_VERTEX_IN` | `ctx->info.vertices_per_patch` | `vertex.c:397-399` |
| `WORK_GROUP_COUNT` | `ctx->info.grid[val]` | `vertex.c:401-403` |
| `UBO` | `get_ubo_offset()`：必要时 `u_upload_data` 上传 `user_buffer`，再 `cmdbuf_add_bo(flags 0x1)` + `cmdbuf_add_relloc`，**reloc 的 `cmd_offset` 直接指向当前正在拼的 uniform 字**（`cmd_buf + (i % 4) + 1`） | `vertex.c:405-407`、`vertex.c:129-156` |
| `SSBO` | `get_ssbo_offset()`，`idx == 0x4` 特判为 `shader->spill_buffer`；`flags 0x2` | `vertex.c:409-411`、`vertex.c:158-181` |
| `LOCAL_MEMORY_OFFSET` | `get_local_offset()` | `vertex.c:412-414`、`vertex.c:198-213` |
| `SSIMAGE / SSIMAGE_STRIDE / SSIMAGE_SIZE / SSIMAGE_INFO / SSIMAGE_LAYER_SIZE` | `get_ssimage_info()`；`SSIMAGE_INFO` 按 `res->base.format` 返回硬编码常量（`RGBA8888_UNORM 0x32101bd2`、`SINT 0x32101912`、`UINT 0x321019d2`、`SNORM 0x32101b12`、`R16G16B16A16_FLOAT 0x32101853`、`SINT 0x321018d3`、`UINT 0x32101993`、`R32_UINT 0x54405962`、`R32_SINT 0x544058A2`，默认 `0x30121bd2`） | `vertex.c:416-422`、`vertex.c:215-284` |
| `SSBO_SIZE` | `ctx->ssbo[stage][val].buffer_size` | `vertex.c:424-426` |
| `ATOMIC_BO` | `get_atomic_offset()` | `vertex.c:428-430`、`vertex.c:286-303` |
| `GLOBAL_MEMEORY_OFFSET` | `get_global_offset()`（用 `ctx->global_bindings.buf[val]`） | `vertex.c:431-433`、`vertex.c:183-196` |
| `WORK_GROUP_SIZE` / `UNUSED` | `0` | `vertex.c:435-438` |

**`mwv207_emit_kernel_params(ctx, info)`（`vertex.c:515-566`，CS 专用）**：基址 `0xD800`；只处理 `CONSTANT / UBO / WORK_GROUP_COUNT / WORK_GROUP_SIZE(=info->block[val]) / GLOBAL_MEMEORY_OFFSET / UNUSED`；其中 `GLOBAL_MEMEORY_OFFSET` 若 `shader->ubo_mask` 有常量，则先经 `bit_scann()` 重映射索引（`vertex.c:548-551`、`bit_scann` `vertex.c:501-513`）。

**常量缓冲**（`vertex.c:450-465`）：`mwv207_set_constant_buffer` 用 `util_copy_constant_buffer`；若只有 `buffer` 无 `user_buffer`，则用 `ws->bo_map` 取得 CPU 视图写进 `user_buffer`（**依赖 BO 常驻映射**）；`dirty \|= DIRTY_CONSTBUF`。

---

## 8. 绘制（`P/mwv207_draw.c`）

### 8.1 图元映射 `prim_mode[]`（`draw.c:42-73`）

| 下标（`MESA_PRIM_*`，按枚举顺序） | 硬件值 | 下标 | 硬件值 |
|---|---|---|---|
| 0（POINTS） | `0x1` | 8 | `0x4` |
| 1（LINES） | `0x2` | 9 | `0x4` |
| 2（LINE_LOOP） | `0x7` | 10（TRIANGLES） | `0x9` |
| 3（LINE_STRIP） | `0x3` | 11（TRIANGLE_STRIP） | `0xA` |
| 4（TRIANGLES） | `0x4` | 12（TRIANGLE_FAN） | `0xB` |
| 5/6/7 | `0x5/0x6/0x4` | 13/14 | `0xC/0xD` |

> 该表 15 项，覆盖 `MESA_PRIM_POINTS..MESA_PRIM_TRIANGLE_FAN`；`QUADS/QUAD_STRIP/POLYGON` 走 primconvert 回退（见 §8.11）。

### 8.2 `mwv207_config_viewport`（`draw.c:184-206`）

`clip_w = MAX2(scale[0], scale[1]) / (2.0f * 8388607.0f - 8192.0f)`（`draw.c:189-190`）。

| 驱动号 | 厂商寄存器 | 值 | 位置 |
|---|---|---|---|
| `0x280` | `MWV207REG_AHBDEC_WRITE_EX_CONFIG`（0x00A00） | `fui(scale[0])` | `draw.c:192` |
| `0x281`–`0x285` | **未映射** | `fui(scale[1])`、`fui(scale[2])`、`fui(translate[0..2])` | `draw.c:193-197` |
| `0x501`/`0x502` | **未映射** | `fui(0.0f)` / `fui(1.0f)`（depth range 钳位） | `draw.c:198-199` |
| `0x2A0` | `MWV207REG_AHBDEC_READ_BUFFER_BASE`（0x00A80） | `fui(clip_w)` | `draw.c:200` |
| `0x28B` | **未映射** | `fui(1.0f / 8388607.0f)` | `draw.c:201` |
| `0x2A3` | `MWV207REG_PA_SMALL_W` | `0x00000000` | `draw.c:202` |
| `0x2A1` | `MWV207REG_PA_MAX_POINT_SIZE` | `0x00800000`（**用 `MWV207_EMITF`**） | `draw.c:203` |

### 8.3 `mwv207_config_scissor`（`draw.c:209-250`）

- 若 `rast_state->base.scissor` 关闭，则由视口 translate/scale 反算出 scissor 矩形（`draw.c:219-226`）。
- 钳制：`left = MAX2(minx,0)`、`top = MAX2(miny,0)`、`right = MIN2(maxx, fb.width)`、`bottom = MIN2(maxy, fb.height)`（`draw.c:228-231`）；空矩形时全部置 1（`draw.c:233-234`）。
- 特殊修正：`right == 8192 → fix_right = -0x119`；`bottom == 8192 → fix_bottom = -0x111`（`draw.c:236-240`）。

| 驱动号 | 厂商寄存器 | 值 | 位置 |
|---|---|---|---|
| `0x301` | **未映射**（0x00C04） | `top << 16` | `draw.c:242` |
| `0x300` | `MWV207REG_AHBDEC_READ_BUFFER_END_EX`（0x00C00） | `left << 16` | `draw.c:243` |
| `0x302`/`0x303` | **未映射** | `(right<<16)+fix_right` / `(bottom<<16)+fix_bottom` | `draw.c:244-245` |
| `0x308`/`0x309` | `MWV207REG_SE_CLIP_COMPARE_RIGHT/BOTTOM` | `(right<<16)+0xFFFF` / `(bottom<<16)+0xFFFF` | `draw.c:246-247` |

### 8.4 `mwv207_config_framebuffer`（`draw.c:253-497`）

| 驱动号 | 厂商寄存器 | 位域/值 | 位置 |
|---|---|---|---|
| `0x529` | `MWV207REG_PE_CONFIG`（0x014A4） | bits9:8 = `linear_outmode ? 0x1 : 0x3`，并清 bit7/bit9-8（`\| ~(0x300\|0x80)`） | `draw.c:268-274` |
| `0xE03` | **未映射** | bit1 = ENABLE（每 RT 一次） | `draw.c:305, 388` |
| `0xffff` | **未映射** | `0x0`（RT0 与 ZS 各一次） | `draw.c:308, 449` |
| `0x596`/`0x597`/`0x598` | **未映射** | tile status 地址（reloc）/ RT 地址（reloc）/ `tsInfo->clear_values[0]` | `draw.c:311-319` |
| `0x5AF` | `MWV207REG_MC_COLOR_CLEAR64` | `tsInfo->clear_values[1]` | `draw.c:320` |
| `0x595` | **未映射** | `memory_config`（仅当 `tsInfo->enable`） | `draw.c:322-325` |
| `0x518`/`0x519` | **未载入** | RT 地址 reloc ×2 | `draw.c:327-330` |
| `0x50D` | **未映射** | `surface->stride` | `draw.c:332` |
| `0x50B` | **未映射** | `surface->peInfo.pixel_config` | `draw.c:333` |
| `0xE28` | `MWV207REG_COLOR_SLICE` | `surface->size` | `draw.c:334` |
| `0x529` | `MWV207REG_PE_CONFIG` | `surface->peInfo.pe_config` | `draw.c:339` |
| `0x690+n` | **未收录** | `tileCount`（`ts.compressed == false` 时） | `draw.c:343` |
| `0x5F0+n`/`0x5F8+n` | **未收录** | RTn 的 tile status 地址/地址 | `draw.c:346-352` |
| `0x680+n`/`0x688+n` | **未收录** | RTn 的 `clear_values[0/1]` | `draw.c:354-355` |
| `0x5E8+n` | **未收录** | `memory_config_ex` 或 0 | `draw.c:358-360` |
| `0x5200+(n-1)*8`、`+1` | **未收录** | RTn 地址 reloc ×2 | `draw.c:363-366` |
| `0xE28+n` | **未收录** | RTn 的 `size` | `draw.c:368` |
| `0x404` | **未映射** | `color_count == 0 && !query_enabled` 时 bit0 = ENABLE；否则 bits11:8 = `color_count - 1` | `draw.c:420-426` |
| `0x40C` | `MWV207REG_PS_OUTPUT_MODE`（0x01030） | `output_config`（由 `mwv207_config_arrays(&output_config, 4, render_num, output_mode)` 逐 RT 拼装） | `draw.c:371, 428` |
| `0x40D` | `MWV207REG_PS_OUTPUT_PRECISION` | 2 字（`mwv207_config_arrays(output_precision, 8, i, 0x3f)`），用 `MWV207_EMITS(…,2,…)` | `draw.c:372, 415, 429` |
| `0x50B` | **未映射** | 无 cbuf 时清 bits11:8/bit7/bit6、置 bit16 | `draw.c:431-434` |
| `0x599`/`0x59A`/`0x59B`/`0x6D0` | **未映射** | ZS 的 tile status 地址 / ZS 地址 / `clear_values[0]` / `clear_values[1]` | `draw.c:451-461` |
| `0x520`/`0x521` | **未映射** | ZS 地址 reloc ×2 | `draw.c:468-471` |
| `0x505` | **未映射** | `surface->stride` | `draw.c:473` |
| `0xE23` | `MWV207REG_DEPTH_SLICE` | `surface->size` | `draw.c:474` |
| `0x503` | **未映射** | `fui(depth_normalize)` | `draw.c:476` |
| `0x500` | **未映射** | 无 ZS 时的深度配置常量组合 | `draw.c:480-486` |
| `0x426` | `MWV207REG_PS_MAX` | `rtMaxLayers`（= 最大 layer_count - 1） | `draw.c:491` |
| `0x38D` | `MWV207REG_RA_CONTROL_EX` | bit4（`0x10`）= `(rtMaxLayers != 0)` | `draw.c:493-494` |

> `pixel_128bit` 的 surface 会把 `res->rgba_dirty = true` 并改用 `res->rg`（`draw.c:287-290`）；`res->linear` 时置 `need_decompress`（`draw.c:292-293`）；`frag_rgba_swap` 时调用 `mwv207_update_renderInfo(surface)` 重算 pe_config 的 swizzle 段（`draw.c:336-337`、`surface.c:36-49`）。

### 8.5 `mwv207_config_rasterizer`（`draw.c:500-541`）

| 驱动号 | 厂商寄存器 | 值/位域 | 位置 |
|---|---|---|---|
| （先） | — | `mwv207_wait_idle(&cmd_buf, 0x7)` | `draw.c:507` |
| `0x2A2` | `MWV207REG_PA_CLIP_CONTROL` | `0x40000000` | `draw.c:509` |
| `0x28A` | **未映射** | `0x1 \| 0x10` | `draw.c:511` |
| `0xE13` | **未映射**（0x0384C） | `0x0` | `draw.c:513` |
| `0x28D` | **未映射** | `rast_state->pa_config`（§4.2 的位域表） | `draw.c:515` |
| `0x7000` | `MWV207REG_TFB_CONTROL` | bit0 = `tfb_control`；bits5:4 = `0` | `draw.c:517-519` |
| `0x286`/`0x28E`/`0x28F` | `0x286` **未映射**；`0x28E/0x28F` = `MWV207REG_PA_LINE_ADJUST_SUB/ADD` | `line_scale` ×3 | `draw.c:521-523` |
| `0x287` | **未映射** | `fui(point_size / 2.0f)`（仅当 `!point_size_per_vertex`） | `draw.c:525-526` |
| `0x380` | `MWV207REG_AHBDEC_WRITE_BUFFER_BASE_EX` | bit0 = ENABLE | `draw.c:528` |
| `0x306` | **未映射** | `0x00000000` | `draw.c:530` |
| `0x305` | **未映射** | `fui(offset_units * 2.0f / depth_normalize)` | `draw.c:534-535` |
| `0x304` | **未映射** | `fui(offset_scale)` | `draw.c:538` |

### 8.6 `mwv207_config_multisamples`（`draw.c:544-612`）

| 驱动号 | 厂商寄存器 | 位域/值 | 位置 |
|---|---|---|---|
| `0x418+n`（n=0..7） | `MWV207REG_PS_SAMPLE_DITHER_TABLE`（0x01060） | 8 个硬编码常量：`0x6e80e680, 0x2ac42a4c, 0x15fb5d3b, 0x9d7391f7, 0x08e691f7, 0x4ca25d3b, 0xbf512a4c, 0x37d9e680` | `draw.c:581-588` |
| `0xE06` | `MWV207REG_MULTI_SAMPLE_CONFIG`（0x03818） | bits1:0 = `sample_mode`（MSAA 时 `0x2`，否则 `0x0`）；bits7:4 = `sample_enable_bits`（MSAA 时 `0xff`）；bits14:12 = 0；bits17:16 = 0；bits21:20 = `3`；bit24 = DISABLE；bit28 = ENABLE | `draw.c:590-597` |
| `0x381` | `MWV207REG_MULTI_SAMPLE_JITTER`（0x00E04） | `0x0`（仅 MSAA） | `draw.c:599` |
| `0x390+n`（n=0..3） | `MWV207REG_CENTROIDS` | 4 个硬编码常量：`0x4a6e2688, 0x888864a2, 0x88ac88ea, 0x888888c6`（仅 MSAA） | `draw.c:600-603` |
| `0x383` | `MWV207REG_RA_MEDIUMP_MASK`（0x00E0C） | `0x00000000` | `draw.c:606` |
| `0x529` | `MWV207REG_PE_CONFIG` | `pe_config`：MSAA 时置 bit24/bit26；非 MSAA 时清 bits6:5、bit24、bit26 | `draw.c:553-569, 608` |
| `0x52F` | `MWV207REG_PE_CONFIG_EX` | `pe_config_ex`：MSAA + 有 cbuf 时 bit15 = ENABLE、bit16 = DISABLE；`CACHE_MODE == 0x1` 时置 bit24/bit26，否则清 | `draw.c:558-579, 609` |

`CACHE_MODE` 定义在 `P/include/mwv207_resource.h:38`，值为 **`0x1`**（决定 cache line 256B vs 128B，见 `draw.c:304,387,446`、`blit.c:506,785,799`）。

### 8.7 `mwv207_config_sample_mask`（`draw.c:615-636`）

| 量 | 计算 |
|---|---|
| `coverage_enable` | `ctx->sample_mask != 0xffff` |
| `coverage_invert` | `0xfffffff0 & ctx->sample_mask` |
| `sample_mask` | `0xf & ctx->sample_mask`；`coverage_invert` 时取反 `& 0xf`；非 0 时再 `\| 0x10` |

| 驱动号 | 厂商寄存器 | 位域 |
|---|---|---|
| `0x415` | `MWV207REG_PS_SAMPLE_COVERAGE_CTRL` | bit16 = `coverage_invert`；bits24:20 = `sample_mask`；bit28 = `coverage_enable`（`draw.c:630-633`） |

### 8.8 `mwv207_config_zsa`（`draw.c:639-663`）

| 驱动号 | 厂商寄存器 | 值 | 位置 |
|---|---|---|---|
| `0x508`（i==0）/ `0x5250+(i-1)`（i>0） | `0x508` **未映射**；`0x5250+n` **未收录** | `ctx->zsa->alpha_config` | `draw.c:647-649` |
| `0x5250+(pixel_record-1)` | **未收录** | 同上（128bit 的 BA 半边） | `draw.c:651-652` |
| `0x528` | `MWV207REG_PE_EXTRA_REFERENCE` | `ctx->zsa->alpha_reference` | `draw.c:655` |
| `0x388` | `MWV207REG_RA_CONTROL_HZ` | bits14:12 = `ctx->zsa->func` | `draw.c:657-658` |
| `0x515` | `MWV207REG_PE_HIERARCHICAL_CONTROL` | `0` | `draw.c:660` |

### 8.9 `mwv207_config_blend` / `mwv207_config_blendColor`（`draw.c:666-782`）

| 驱动号 | 厂商寄存器 | 值/位域 | 位置 |
|---|---|---|---|
| `0x529` | `MWV207REG_PE_CONFIG` | `blend_state->logic_config` | `draw.c:674` |
| `0xE03` | **未映射** | bit1 = ENABLE（当该 RT `dest_read[i]` 为真时） | `draw.c:685` |
| `0x530` | `MWV207REG_PE_ALPHA_CONFIG` | `alpha_config`（RT0） | `draw.c:690` |
| `0x50A` | **未映射** | `alpha_blend`（RT0） | `draw.c:692` |
| `0x50B` | **未映射** | bits11:8 = `colormask`；bit16 = `dst_read`（RT0） | `draw.c:694-696` |
| `0x5258+(n-1)` | **未收录** | `blend_config[i]`（RTn） | `draw.c:698-699` |
| `0x5248+(n-1)` | **未收录** | bits7:4 = `colormask`；bit8 = `dst_read`（RTn） | `draw.c:701-703` |
| `0x5258+(pixel_record-1)` / `0x5248+(pixel_record-1)` | **未收录** | 128bit BA 半边的 blend_config / colormask（colormask 做 **B/A 交换**：`newrtcm` 把 B→R、A→G 映射） | `draw.c:706-718` |
| `0x52A`/`0x52B` | `MWV207REG_PE_DITHER_LOW/HIGH` | `dither` 开：`0x6e4ca280`/`0x5d7f91b3`；关：`0xffffffff`/`0xffffffff` | `draw.c:722-727` |
| `0x509` | **未映射** | `color_8`（8 位打包，**注意源码里四个分量都 `<< 8`，疑似重复左移缺陷**，`draw.c:751-754`） | `draw.c:763` |
| `0x52C`/`0x52D` | `MWV207REG_PE_BLEND_FACTOR_LOW/HIGH` | `colorLow`/`colorHigh`（`_mesa_float_to_half` 打包） | `draw.c:756-759, 765-766` |
| `0x5260+(i-1)` / `0x5268+(i-1)` | **未收录** | RTn 的 colorLow/colorHigh | `draw.c:772-773` |

> `blend_color` 的分量顺序按**第一个有效 cbuf 的 `peDesc.swizzles[0..3]`** 索引（`draw.c:741-754`），即 mix 前先做 swizzle 对齐。

### 8.10 `mwv207_config_stream_out`（`draw.c:785-878`）

| 驱动号 | 厂商寄存器 | 值/位域 | 位置 |
|---|---|---|---|
| `0xE03` | **未映射** | bit14（`0x4000`） | `draw.c:811` |
| `0x7001` | `MWV207REG_TFB_COMMAND`（0x1C004） | bits2:0 = `0x2`（suspend）/ `0x1`（首次激活）/ `0x4`（已激活） | `draw.c:814, 869, 872` |
| `0xE02` + `MWV207_STALL` | **未映射** | bits29:28 = `0x3`；bits4:0 = `0x1`；bits12:8 = `0x7`（suspend 时） | `draw.c:816-823` |
| `0x7002` | **未收录** | target[0] 的 BO reloc（`MWV207REG_TFB_COMMAND` 的地址寄存器，REG 无此项） | `draw.c:827-828` |
| `0x7200+n` | **未收录** | 输出描述符：bits1:0 = `output_buffer`；bit4 = `true`（空洞填充）；bits21:20 = `offset_components`；或 bits12:8 = `register_index`、bits17:16 = `start_component`、bits21:20 = `num_components` | `draw.c:839-852` |
| `0x7040` | `MWV207REG_TFB_DESCRIPTOR_COUNT`（0x1C100） | `tfb_des_count` | `draw.c:856` |
| `0x7010+i` | **未收录** | 缓冲区地址 reloc | `draw.c:862-863` |
| `0x7030+i` | `MWV207REG_TFB_BUFFER_STRIDE`（0x1C0C0） | `stream_output.stride[i] * 4` | `draw.c:864` |
| `0x7020+i` | `MWV207REG_TFB_BUFFER_SIZE`（0x1C080） | `target->buffer_size` | `draw.c:865` |

### 8.11 绘制命令 `mwv207_config_draw`（`draw.c:880-938`）

| 步骤 | 内容 | 位置 |
|---|---|---|
| 1 | `texture_dirty` 时 `0xE03` 置 bit4（`0x10`） | `draw.c:904-905` |
| 2 | `0xE27` = `MWV207REG_DRAW_ID`（0x0389C），值为**静态递增计数器 `draw_count++`**（函数内 `static`，跨上下文共享，疑似缺陷） | `draw.c:888, 907` |
| 3 | `0xE02` = bits4:0 `0x5`、bits12:8 `0x7` | `draw.c:909-910` |
| 4 | `0xF00` = 同 `0xE02` 的值（**未映射** 0x03C00） | `draw.c:912-913` |
| 5 | 绘制命令体（手工写 `*cmd_buf++`，不用 EMIT 宏） | `draw.c:915-932` |
| 6 | `0xE03` bit0 = ENABLE；随后 `MWV207_SNAP_TO_PAGE(cmd_buf)` | `draw.c:934-935` |

绘制命令体（`draw.c:915-932`）：

| 场景 | 字数 | 编码 |
|---|---|---|
| 间接（`dindirect`） | 命令头 + 1 地址字 | 头部 opcode `0x10`（`ALIGN_BIT(0x10,0x1B)&0xF8000000`）；bit8 = `draw_mode`；bit14 = `(instance_count != 1)`；bits3:0 = `prim_mode[dinfo->mode]`；随后 `mwv207_add_relocs(ctx,&cmd_buf,0x0, buffer->bo, buffer->offset + dindirect->offset, 0x0)`（**reg==0 路径**） |
| 直接 | 4 个字 | ① opcode `0xC`（bit27）；bits22:20 = `draw_mode`；bits15:0 = `instance_count & 0xFFFF`；bits19:16 = `prim_mode`；② bits23:0 = `count`；bits31:24 = `instance_count >> 16`；③ `offset`；④ `0` |

- `draw_mode = (index_size > 0) ? 0x1 : 0x0`（`draw.c:894-897`）；索引绘制时 `offset = draws->index_bias`（否则 `draws->start`）。
- `count_from_stream_output` 时 `count = buffer_size / element_state->elements[0].base.src_stride`（`draw.c:899-900`）。

### 8.12 `mwv207_config_other`（`draw.c:940-1040`）

按 dirty 组合发射三组寄存器：

| 条件 | 寄存器 | 值 |
|---|---|---|
| `FRAMEBUFFER \| BLEND` | `0x5240+(n-1)`（**未收录**）= `peInfo.pixel_config_ex \| blend_state->blend_configex[i]` | `draw.c:958-964` |
| 同上 + `rast_samples > 1` | `0x415` bit12 = `blend_state.alpha_to_coverage` | `draw.c:967-969` |
| `FRAMEBUFFER \| ZSA \| SHADER` 且有 ZS | `0x500`（**未映射**）= `surface->peInfo.depth_config & ctx->zsa->depth_config &`（PS 写深度时的位域组合） | `draw.c:996-1005` |
| 同上 | `0x382` = `MWV207REG_RA_CONTROL`：bits31:30 = `subsamplez_dirty ? 0x0 : 0x1`；bit0/bit4-7；bits29:28 = `ps_modify_z`；bit2 = DISABLE；bit26/bit24 = true | `draw.c:1007-1010, 1018-1022` |
| `ZSA \| STENCIL_REF \| RASTERIZER` | `0x506`（**未映射**）= `zsa->stencil_op[front_ccw]`；`0x507`（**未映射**）= `zsa->stencil_config[front_ccw] \| stencil_ref.stencil_reference[front_ccw]`；`0x528`（PE_EXTRA_REFERENCE）= `stencil_back_reference[front_ccw]`；`0x52E`（PE_BACKFACE_STENCIL）= `zsa->stencil_back_config[front_ccw]` | `draw.c:1027-1036` |

无 ZS 分支（`draw.c:1011-1023`）用 FS 的 `useFragCoord` 与 GS 的 `gs_use_layer` 决定 `0x382` 的 bit26/bit24。

### 8.13 着色器切换与 dual-source

| 函数 | 位置 | 行为 |
|---|---|---|
| `mwv207_update_shader` | `draw.c:1042-1112` | 复位 `texture_dirty/z32_shadow/tg4_lower`；遍历所有已绑定 sampler view：128bit 未拆则 `mwv207_split_128bit`；`res->ts.enable` 时 `mwv207_disable_tile`；`res->src_dirty` → `texture_dirty = true`；`compare_mode != NONE && 格式 == Z32_FLOAT` → `z32_shadow = true`；`desc->channel[0].size == 32` → `tg4_lower = true`。`linear_outmode` 且 cbuf 为 SUPERTILED 时 `disable_tile`。最后按 GS/TCS/FS 顺序 `mwv207_set_current_shader`，`subsamplez_dirty` 也返回 true |
| `mwv207_config_dual_blend` | `draw.c:1114-1171` | 仅 `dsbi.is_dual`：新建临时 2D 纹理，把 `cbufs[0]` 用 `pctx->blit` 拷进去；`util_blitter_default_src_texture` + `create_sampler_view` 绑定到 FS 的**下一个空闲槽**（`sampler_index = num_samplers[FS]`），并用 `mwv207_get_sampler_state` 绑默认采样器；`y_invert = 0` |

### 8.14 一帧的状态发射顺序 `mwv207_config_render_piepline`（`draw.c:1174-1258`）

| 顺序 | 动作 | 触发条件 |
|---|---|---|
| 1 | `mwv207_config_dual_blend` | 恒执行（内部自检 `is_dual`） |
| 2 | `mwv207_update_shader` → 置 `CONSTBUF \| SHADER` | 恒执行 |
| 3 | `mwv207_config_uniform` × 5（VS/TCS/TES/GS/FS） | `SHADER \| CONSTBUF` |
| 4 | `mwv207_config_vertex` | 恒执行 |
| 5 | `mwv207_config_viewport` | `VIEWPORT` |
| 6 | `mwv207_config_scissor` | `VIEWPORT \| FRAMEBUFFER \| SCISSOR \| RASTERIZER` |
| 7 | `mwv207_config_sampler`（i = 0..`PIPE_SHADER_COMPUTE-1`，即**不含 CS**） | `SAMPLER_VIEWS \| SAMPLERS` 且 `num_sampler_views[i] != 0` |
| 8 | `mwv207_config_framebuffer` | `FRAMEBUFFER` |
| 9 | `mwv207_config_rasterizer` | `RASTERIZER` |
| 10 | `mwv207_config_multisamples` | `FRAMEBUFFER` |
| 11 | `mwv207_config_sample_mask` | `SAMPLE_MASK` |
| 12 | `mwv207_config_zsa` | `ZSA` |
| 13 | `mwv207_config_other` | 恒执行（内部按 dirty 分组） |
| 14 | `mwv207_config_blend` | `BLEND` |
| 15 | `mwv207_config_blendColor` | `BLEND_COLOR` |
| 16 | `mwv207_config_vs/tcs/tes/gs` | `SHADER` |
| 17 | `mwv207_config_fs` + `mwv207_config_link` | `SHADER \| FRAMEBUFFER` |
| 18 | `mwv207_config_stream_out(ctx, false)` | `STREAM_OUT` 或 `num_targets != 0` |
| 19 | `mwv207_config_draw` | 恒执行 |
| 20 | `mwv207_config_stream_out(ctx, true)`（suspend） | `num_targets != 0` |
| 21 | `ctx->dirty = 0` | 恒执行 |

### 8.15 入口 `mwv207_draw_vbo`（`draw.c:1260-1297`）

1. `num_draws > 1` → `util_draw_multi()` 拆分后返回（`draw.c:1271-1274`）。
2. 非间接且 `count == 0` 或 `instance_count == 0` → 直接返回（`draw.c:1276-1277`）。
3. `QUADS / QUAD_STRIP / POLYGON` → `u_trim_pipe_prim` 后走 `util_primconvert`（`draw.c:1279-1289`）；primconvert 支持的图元集合由 `mwv207_context_helper_init` 给出：`POINTS|LINES|LINE_STRIP|TRIANGLES|TRIANGLE_STRIP|TRIANGLE_FAN`（`context.c:44-51`）。
4. 其余走 `mwv207_config_render_piepline` → `mwv207_submit(ctx, NULL, false)`（`draw.c:1291-1293`）。
5. `ws->debug & 0x40` 时强制 `pctx->flush`（调试）。（`draw.c:1295-1296`）

---

## 9. 位块传输（`P/mwv207_blit.c`）

### 9.1 硬件直通判定 `mwv207_can_blit`（`blit.c:36-87`）

| 拒绝条件 | 位置 | 说明 |
|---|---|---|
| `info->mask != util_format_get_mask(dst->base.format)` | `blit.c:43-44` | mask 必须与目标格式的全 mask 完全一致 |
| `scissor_enable \|\| alpha_blend` | `blit.c:46-47` | 不支持 |
| `src.box.x < 0 \|\| src.box.width < 0`（dst 同理） | `blit.c:49-53` | 不支持负偏移 |
| `\|src.width\| != \|dst.width\|` 或 `\|height\|` 不等 | `blit.c:55-59` | **不支持缩放** |
| `src.box.depth != dst.box.depth` | `blit.c:61-62` | — |
| fake format 且（格式不同或 MSAA→非 MSAA 的颜色 resolve） | `blit.c:66-74` | resolve 未实现，打印 `color resolve unimplemented…` |
| src 或 dst 的 `util_format_get_blocksize > 8` | `blit.c:76-80` | **只支持 ≤ 64 bit/像素** |
| blocksize 与 `info->src/dst.format` 不一致 | `blit.c:82-84` | — |

> 结论支持矩阵：**仅同尺寸、同 mask、无裁剪、无 alpha 混合、单样本位宽 ≤ 8 字节、非 fake-format 跨格式**的拷贝走硬件 BLT；其余回退 `util_try_blit_via_copy_region` → 128/96bit copy_region → `util_blitter`（`blit.c:931-975`）。

### 9.2 单次任务 `mwv207_set_image_task`（`blit.c:89-154`）

- 按 `src_sampleInfo.xscale/yscale` 展开宽高（`blit.c:102-109`）。
- 宽度 > 64 时按 `cluster_count` 均分，最后一片吸收余量（`blit.c:111-114`）。
- 逐集群写 `0x50CE`（`MWV207REG_BLT_CLUSTER_CONTROL`，bits7:0 = `1<<set_count`）、`0x5005`（`SRC_ORIGIN`：bits15:0 = src_x、bits31:16 = src_y）、`0x500B`（`DEST_ORIGIN`：dst_x/dst_y）、`0x500C`（`WINDOW_SIZE`：task_width/task_height）（`blit.c:124-137`）。
- 结束时把用到的集群写回 `0x50CE`（`blit.c:151-152`）。

### 9.3 缓冲区任务与操作（`blit.c:156-350`）

| 函数 | op | 关键寄存器 |
|---|---|---|
| `mwv207_set_buffer_task`（:156-193） | — | `0x5004`/`0x5008` 清 0；`0x50CE`；`0x5000`/`0x5006` 地址 reloc（`SRC_ADDRESS`/`DEST_ADDRESS`）；`0x5015`（`COPY_SIZE`） |
| `mwv207_fill_buffer`（:195-304） | `0x1` | `CACHE_WRITABLE` 刷新；`BLIT_LOCK`；`0x5002`/`0x5003`（`SRC_CONFIG`/`SRC_CONFIG_EX`）、`0x5006`/`0x5009`/`0x500A`（`DEST_ADDRESS`/`DEST_CONFIG`/`DEST_CONFIG_EX`）、`0x5019`（`GENERAL_CONFIG`，bits9:7 = `0x3`）、`0x5011`（`NEW_CLEAR_VALUE`，2 字）、`0x5013`（`CLEAR_MASK` = `0xFFFFFFFF`）；贴图任务；余数再发一次 |
| `mwv207_copy_buffer`（:306-327） | `0x3` | 与 `update_buffer` 完全相同 |
| `mwv207_update_buffer`（:329-350） | `0x3` | 同上 |

> `config` 常量（`blit.c:221-226`）：bits28:27 = `0x0`、bits20:0 = `stride`(4096)、bit29/30/31 = DISABLE、bits22:17 = `0x6`。
> `dst_configex`/`src_configex`（`blit.c:228-236`）：bit20 = `0x0`、bit0 = DISABLE、bit17 = DISABLE、bit1 = DISABLE。
> `blit_config`（`blit.c:238`）：bits9:7 = `0x3`。

### 9.4 图像清屏 `mwv207_clear_image`（`blit.c:352-444`）

| 步骤 | 内容 |
|---|---|
| 快清判定 | `dst->ts.bo && array_size == 1 && depth0 == 1` 且 `clear_masks[0/1]` 全为 `0xFFFFFFFF` → 写回 `ts->clear_values`、`fast_clear = true`、`ts.enable = true`、`linear_output = false`，并把 `src_compress/dst_compress` OR 进 configex（`blit.c:374-389`）；否则若原本 `ts.enable` 则 `mwv207_disable_tile` |
| clear 值 | `mwv207_get_clearval(dst->base.format, info, clear_values, clear_masks)`（实现于 `mwv207_format.c`） |
| 发射 | `0x5019`（`GENERAL_CONFIG`）= `levelInfo->blit_config`；`0x5009`/`0x500A`（`DEST_CONFIG`/`_EX`，含 bit0 `fast_clear`、bit30 MSAA ENABLE/DISABLE）；`0x5006`（`DEST_ADDRESS`）；`0x5002`/`0x5003`；`0x5000`；任务；`0x5011`/`0x5013` 各 2 字；fast_clear 时 `0x5004`/`0x5008`（tile status）+ `0x500F`/`0x500D`（`DST_CLEAR_VALUE`/`SRC_CLEAR_VALUE`）；最后 `0x1` 执行 |
| 位置 | `blit.c:391-441` |

### 9.5 层拷贝 `mwv207_copy_layer`（`blit.c:446-603`）

关键点：

| 项 | 位置 |
|---|---|
| `src_offset`/`dst_offset` = `res->offset + level->offset + first_layer * layer_size` | `blit.c:465-467` |
| `dst->ts.enable && src != dst` → `disable_tile(dst)` | `blit.c:472-473` |
| 源 tile 快清条件：`src->ts.bo && !first_level && src->ts.enable` → `src_tile_offset = layer_offset * first_layer`、`fast_clear = (src_tile_offset == 0)`（`blit.c:475-479`）（注意 `fast_clear` 在此被重新赋值） |
| 压缩格式时把 blit desc 的 format/swizzle 互抄（`blit.c:483-486`） | — |
| `0x5019` = bits12 = `src_blit_desc.sample_mode`（`blit.c:488, 496`）；`0x5002` = stride + format(bits22:17) + msaa_mode(bits28:27) + bit29/30 = `(src->mode == SUPERTILED)`（`blit.c:498-528`）；`0x502F` = `src_swizzle \| (dst_swizzle << 0xC)`（`blit.c:530-531`）；`0x5028`（`TILE_SIZE`）= `4\|4<<16`；`0x5027`（`BLOCK_SIZE`）= `0x40\|0x40<<16`（`blit.c:533-539`） | — |
| 目标：`0x5009` + `0x500A`（含 bit22 = `0x1`、bit19 = `flip_y`）；`0x5006` reloc；`flip_y` 时 `0x50CF`（`DEST_FLIP_SIZE`）= dst_box.y / y+height（`blit.c:550-567`） | — |
| 压缩格式分支计算完 task_info 后**被紧随其后的无条件赋值覆盖**（`blit.c:568-584`）——即压缩格式路径未生效，疑似缺陷 | — |
| `0x5016`/`0x5017`（`DITHER_LOW/HIGH`）= `~0U`；执行 op `0x2`；末置 `dst->src_dirty = true`、`dst->linear → need_decompress` | `blit.c:588-600` |

### 9.6 mipmap `mwv207_mipmap`（`blit.c:605-695`）

- 逐级用 `0x5030 + i - 1`（**未收录**）写各级地址 reloc、`0x50C0 + i - 1`（**未收录**）写各级 stride（`blit.c:674-683`）。
- `0x502C`（`LOD_CONFIG`）：bits3:0 = `level_count`、bit5 = `1`，其余保留（`blit.c:685-687`）。
- 执行 op `0x6`（`blit.c:688`）。

### 9.7 入口 API

| 函数 | 位置 | 行为 |
|---|---|---|
| `mwv207_copy_image` | `blit.c:697-707` | 逐 layer 调 `copy_layer` |
| `mwv207_blit_self_image` | `blit.c:709-748` | 对自身逐 layer `copy_layer`（用于解压缩 tile） |
| `mwv207_get_filler_bpp` | `blit.c:750-768` | blocksizebits `8→0x0、16→0x1、32→0x2、64→0x3、128→0x4`，默认 `0x2` |
| `mwv207_till_fill` | `blit.c:770-836` | tile 填充：`0x5019` = bit10(`CACHE_MODE==1`)/bit11/filler_bpp<<15；`0x500F` 2 字；逐集群 `0x50CE`、`0x5006`、`0x5008`、`0x501A`（`TILE_FILLER_COUNT`）；执行 op `0x4` |
| `mwv207_disable_tile` | `blit.c:838-851` | `LINEAR` 直接返回；`ts.compressed` → `blit_self_image`，否则 `till_fill`；末置 `ts.enable = false`、`src_dirty = true` |
| `mwv207_blit_image` | `blit.c:862-911` | 组装 `mwv207_region_info`；负高度时置 `flip_y` 并 `mwv207_fix_box`（y += height, height = -height）；逐 layer `copy_layer`；`mwv207_submit(ctx, NULL, true)` |
| `mwv207_try_blit128_or_blit96_copy_region` | `blit.c:913-929` | src/dst blocksize 都 > 8 且 `util_can_blit_via_copy_region(blit, true, false)` → 走 `resource_copy_region` |
| `mwv207_blit`（公开入口） | `blit.c:931-975` | 顺序：`can_blit` → `blit_image`；否则 `util_try_blit_via_copy_region` → `try_blit128_or_blit96` → `util_blitter`（先保存约 18 种状态再 `util_blitter_blit`）；都不支持则 `printf("blit unsupported …")` |
| `mwv207_clear_render_target` | `blit.c:977-1012` | blocksize ≥ 8 时走 `util_clear_render_target`（并视情况 `split_128bit`）；否则逐 layer `clear_image` |
| `mwv207_clear_depth_stencil` | `blit.c:1014-1044` | 逐 layer `clear_image`（`aspectMask = clear_flags`） |
| `mwv207_clear` | `blit.c:1046-1093` | 按 `buffers` 位逐 cbuf 调 `clear_render_target`（有 scissor 时用 scissor 边界），ZS 同理；`mwv207_submit(ctx, NULL, false)`；`dirty \|= DIRTY_FRAMEBUFFER` |
| `mwv207_gen_mipmap` | `blit.c:1095-1140` | fake format 直接返回 false；3D 纹理逐层循环；`mwv207_submit(ctx, NULL, true)` |
| `mwv207_blit_resource_box` | `blit.c:1142-1161` | 构造 `pipe_blit_info`（`mask = util_format_get_mask(dst->format)`、`filter = NEAREST`、src/dst format 取 `util_format_linear`、src/dst box 相同、level 相同）→ 直接调 `mwv207_blit_image`（**绕过 `mwv207_blit` 的能力检查**） |
| `mwv207_resource_copy_region` | `blit.c:1163-1222` | ① buffer→buffer 走 `copy_buffer`；② 同 target 且 src blocksize < 8 走 `copy_image`（先 `disable_tile(src)`）；③ 否则 `util_resource_copy_region` + 视情况 `split_128bit`；末 `mwv207_submit(ctx, NULL, true)` |
| `mwv207_blit_init` | `blit.c:1224-1239` | 注册 `blit/clear/clear_texture/clear_buffer/clear_render_target/clear_depth_stencil/generate_mipmap/transfer_flush_region/buffer_subdata/texture_subdata/resource_copy_region` |

### 9.8 `mwv207_dma`（在 `mwv207_cmd.c:257-311`，见 §2.8）

用于 buffer↔buffer 的线性搬运，走独立 DMA 引擎 cmdbuf（engine_type `0x4`），不占用 3D 命令流。

---

## 10. 查询（`P/mwv207_query.c`）

### 10.1 对象与生命周期

| 项 | 位置 | 内容 |
|---|---|---|
| `struct mwv207_query_info` | `mwv207_query.h:33-37` | `enabled`、`suspend`、`active_queries` 链表 |
| `struct mwv207_query` | `mwv207_query.h:39-47` | `type`、`bo`、`tfb`、`query_id`、`active_list` |
| `mwv207_create_query` | `query.c:36-58` | 两个 BO 各 `64*100` 字节、`MWV207_DOMAIN_VRAM \| MWV207_FLAG_CPU_ACCESS`；`bo` 清零 |
| `mwv207_context_query_init` | `query.c:324-340` | 注册 `create/destroy/begin/end/get_query_result/set_active_query_state` |
| `mwv207_context_query_destory` | `query.c:342-349` | 仅 `FREE(ctx->query_info)`（函数名拼写 `destory` 为原文） |

### 10.2 `mwv207_begin_query` 的分类型发射（`query.c:71-174`）

`only_depth = (nr_cbufs == 0 && zsbuf != 0)`（`query.c:78`）。

| query 类型 | 发射 | 位置 |
|---|---|---|
| `OCCLUSION_COUNTER / OCCLUSION_PREDICATE / OCCLUSION_PREDICATE_CONSERVATIVE / SO_OVERFLOW_PREDICATE / SO_OVERFLOW_ANY_PREDICATE` | `0xE18`（`MWV207REG_OCCLUSION_CONTROL`，0x03834）= bits2:0 `0x7`（only_depth）或 `0x6`；`0xE09`（**未收录**）→ `query->bo + query_id*64` reloc | `query.c:92-101` |
| `TIMESTAMP / TIMESTAMP_DISJOINT / TIME_ELAPSED` | `0xE1A`（**未收录**）→ `query->bo` reloc；`os_time_get_nano()` 拆高低位写 `0xE26`（`MWV207REG_FENCE_DATA_HIGH`）与 `0xE1B`（`MWV207REG_FENCE_DATA`，0x03834 区） | `query.c:103-113` |
| `PRIMITIVES_GENERATED` | `0x7005`（**未收录**）→ `query->bo`；`suspend` 时 `0x7001` = `0x4`，且无 SO target 时 `0x7004` = `0x4 \| (0x1<<8)`；否则 `0x7004` = `0x1 \| (0x1<<8)`，无 SO target 时再写 `0x7002` reloc、`0x7030`=0、`0x7020`=0、`0x7001`=`0x1` | `query.c:114-141` |
| `PRIMITIVES_EMITTED` | `0x7005` reloc；`0x7004` = (`suspend?0x4:0x1`) \| (0x0<<8) | `query.c:142-155` |
| `SO_STATISTICS` / `GPU_FINISHED` / `PIPELINE_STATISTICS` / `PIPELINE_STATISTICS_SINGLE` | **空分支（不发射任何寄存器）** | `query.c:156-163` |
| 其他 | `unreachable("mwv207: unknown query type")` | `query.c:164-165` |

非 `suspend` 时把 query 挂到 `active_queries`（`query.c:170-171`）。

### 10.3 `mwv207_end_query`（`query.c:176-233`）

| 类型 | 发射 | 位置 |
|---|---|---|
| 遮挡/SO overflow 系 | `0xE0C` = `MWV207REG_OCCLUSION_DATA`（0x03830）= `0x01df5e76`（魔术常量）；`query->query_id++` | `query.c:195-196` |
| `TIMESTAMP` | 同 begin：`0xE1A` reloc + `0xE26`/`0xE1B` 写 CPU 时间 | `query.c:198-206` |
| `PRIMITIVES_GENERATED` | `0x7004` = `0x2 \| (0x1<<8)`；无 SO target 时 `0x7001` = `0x2` | `query.c:208-217` |
| `PRIMITIVES_EMITTED` | `0x7004` = `0x2 \| (0x0<<8)` | `query.c:218-222` |
| 其他 | 空 | `query.c:223-224` |

非 `suspend` 且非 `TIMESTAMP` 时从活动链表摘除（`query.c:229-230`）。

### 10.4 `mwv207_get_query_result`（`query.c:235-289`）

1. `pctx->flush(pctx, NULL, 0)`，然后 `ws->bo_wait(bo, OS_TIMEOUT_INFINITE)` —— **同步等待 GPU**（`query.c:245-246`）。
2. `count = Σ results[0..query_id]`（`query.c:250-251`）。
3. `util_query_clear_result` 后按类型填：predicate 类 `result->b = (count != 0)`；`GPU_FINISHED` 恒 `true`；occlusion counter `u64 = count`；timestamp/disjoint `u64 = results[0]`；`TIME_ELAPSED` `u64 = os_time_get_nano() - results[0]`；primitives `u64 = results[0]`；其余 `printf("unhangled query type…")` + `unreachable`（`query.c:253-282`）。
4. `memset(results, 0, 64 * query_id)`、`unmap`、`query_id = 0`（`query.c:284-286`）。

**重要语义偏差**：`TIMESTAMP`/`TIME_ELAPSED` 用的是 **CPU 侧 `os_time_get_nano()`** 写入 `0xE26`/`0xE1B`，而非 GPU 时间戳寄存器（`query.c:108-112, 201-205, 273`）→ 不反映 GPU 时序。

### 10.5 suspend / resume

`mwv207_set_active_query_state(pctx, enable)`（`query.c:309-322`）：置 `enabled`、把 `suspend = true`，`enable` 时 `resume_queries`，否则 `suspend_queries`，最后复位 `suspend = false`。`mwv207_suspend_queries` / `mwv207_resume_queries` 遍历 `active_queries` 逐个 `end_query` / `begin_query`（`query.c:291-307`）。`begin/end_cmd` 在每次命令流首尾各调一次（`cmd.c:56, 62`）。

---

## 11. 计算（`P/mwv207_compute.c`）

### 11.1 资源绑定

| 函数 | 位置 | 行为 |
|---|---|---|
| `mwv207_set_hw_atomic_buffers` | `compute.c:36-45` | `util_set_shader_buffers_mask(ctx->atomic_bo, &ctx->atomic_mask, …)` |
| `mwv207_set_shader_buffers` | `compute.c:47-58` | `util_set_shader_buffers_mask(ctx->ssbo[shader], &ctx->ssbo_mask[shader], …)` |
| `mwv207_set_shader_images` | `compute.c:60-75` | 逐项 `util_copy_image_view`；`num_images[shader] = start_slot + count` |
| `mwv207_set_compute_resources` | `compute.c:77-83` | **空实现** |
| `mwv207_set_global_binding` | `compute.c:85-122` | 引用 buffer；把 `rsc->bo->logical + offset` 的**绝对地址写回 `handles[i]`**，并置 `enable_mask` 位 |

### 11.2 `mwv207_launch_grid`（`compute.c:124-290`）

前置（`compute.c:132-165`）：读 `shared_size` → `local_memory_pack_size = align(size,16)/16`；查屏幕能力 `MAX_LOCAL_SIZE`/`MAX_THREADS_PER_BLOCK`；算 `work_group_size = block[0]*block[1]*block[2]`、`work_group_count_per_shader_group = floor(max_threads/work_group_size)`、`local_memory_count = floor(max_local_mem/local_mem_size)`、二者取小；`max_running_work_groups = MAX2(work_group_size, max_thread_count)`。间接 launch 时 map 间接缓冲前 3 个 `uint32` 得到 grid；任一维为 0 直接返回。

着色器参数：`stage == MESA_SHADER_KERNEL` → `mwv207_emit_kernel_params`，否则 `mwv207_config_uniform(CS)`；随后 `mwv207_config_sampler(CS)`（`compute.c:167-171`）。

发射序列（`compute.c:173-287`）：

| 驱动号 | 厂商寄存器 | 值/位域 | 位置 |
|---|---|---|---|
| `0x1F0` | `MWV207REG_FE_PATCH_CONTROL` | `0x00000003` | `compute.c:175` |
| `0xE02` + `MWV207_STALL` | **未映射** | bits4:0 `0x1`、bits12:8 `0x7` | `compute.c:177-182` |
| `0x5580` | `MWV207REG_SHADER_MISC_CONFIG`（0x15600） | `0x00000002` | `compute.c:184` |
| `0x21A` | `MWV207REG_SH_CACHE_CONTROL` | bit0 `0x1` | `compute.c:185` |
| `0x403` | **未映射** | `cs->num_temps` | `compute.c:186` |
| `0x2A4` | `MWV207REG_PA_PACK`（0x00A90） | `0x2` | `compute.c:188` |
| `0x420` | `MWV207REG_PS_UNPACK` | `0x2` | `compute.c:189` |
| `0x427` | `MWV207REG_PSCS_THROTTLE` | 无共享内存：bits6:0 = `max_local_memory_size >> 10`、bit8-15 = `work_group_count_per_shader_group`、bits19:16 = `align(max_running_work_groups,2)*2 - 1`；有共享内存：后两段为 0 | `compute.c:191-201` |
| `0x416` | `MWV207REG_PS_SAMPLER_BASE` | `0` | `compute.c:203` |
| `0x409` | `MWV207REG_PIXEL_SHADER_CONST` | `0` | `compute.c:204` |
| `0x21F` | `MWV207REG_PS_START_PC` | `0` | `compute.c:206` |
| `0x424` | `MWV207REG_PS_RELATIVE_END` | `cs->code_size / 4` | `compute.c:207` |
| `0x40A` | **未收录** | 着色器 BO reloc（`shader->bo` + `bo_offset`） | `compute.c:208-209` |
| `0x425` | `MWV207REG_PS_INSTRUCTION_PREFETCH_RELATIVE_END` | `cs->code_size / 4 - 1` | `compute.c:210` |
| `0x402` | **未映射** | bits5:0 `3`、bits12:8 `31`、bit24 `false` | `compute.c:212-215` |
| `0x404` | **未映射** | `0x00000f00` | `compute.c:217` |
| `0x228`/`0x5582` | `MWV207REG_VS_THROTTLE` / `MWV207REG_VS_THROTTLE_EX` | `0` | `compute.c:219-220` |
| `0x5285`/`0x5286`/`0x52C6`/`0x52C7`/`0x52CD` | TCS_CONTROL/TCS_THROTTLE/TES_CONTROL/TES_THROTTLE/TES_THROTTLE_EX | `0` | `compute.c:221-225` |
| `0x440`/`0x450` | `MWV207REG_GS_CONTROL` / `MWV207REG_GS_THROTTLE` | `0` | `compute.c:226-227` |
| `0x457` | `MWV207REG_GS_THROTTLE_EX` | `0xff` | `compute.c:228` |
| `0x28C` | **未映射** | `0x00000300` | `compute.c:230` |
| `0xE07` | `MWV207REG_VARYINGS` | `0x22` | `compute.c:231` |
| `0x40C`/`0x40D` | `MWV207REG_PS_OUTPUT_MODE` / `_PRECISION` | `0` | `compute.c:233-234` |
| `0x20C` | `MWV207REG_AHBDEC_TOTAL_READS_IN` | `0x0f3f0000` | `compute.c:236` |
| `0x201` | `MWV207REG_AHBDEC_CONTROL_EX` | `0x00000001` | `compute.c:237` |
| `0x2AA` | `MWV207REG_PA_CONTROL` | `0` | `compute.c:238` |
| `0xE22` | `MWV207REG_SEMANTIC_LOCATION` | `0x007f0000` | `compute.c:239` |
| `0x412` | `MWV207REG_PS_INSTRUCTION_PREFETCH` | `0` | `compute.c:241` |
| `0x240` | `MWV207REG_TW_CONFIG` | bits1:0 `3`、bit24 `0x1`（`& 0x7000000`） | `compute.c:243-245` |
| `0x249` | `MWV207REG_TW_SHADER_INFO2` | 无共享内存：bits15:0 `local_memory_pack_size`、bits25:20 `63`、bit28-29 `0x1`；有共享内存：bits15:0 `0`、bit28-29 `0x0` | `compute.c:247-259` |
| `0x247` | `MWV207REG_TW_SHADER_INFO` | `align(block[0]*block[1]*block[2], 64) / 64` | `compute.c:261-263` |
| `0x24B`/`0x24D`/`0x24F` | `MWV207REG_TW_INFO_GLOBAL_OFFSET_X/Y/Z` | `0` | `compute.c:265-267` |
| `0x250`/`0x251`/`0x252` | `MWV207REG_TW_WORK_GROUP_COUNT_X/Y/Z` | `grid[i] - 1` | `compute.c:269-271` |
| `0x253`/`0x254`/`0x255` | `MWV207REG_TW_WORKGROUP_SIZE_X/Y/Z` | `block[i] - 1` | `compute.c:273-275` |
| `0x248` | `MWV207REG_TW_TRIGGER` | `0xbadabeeb`（**触发常量**） | `compute.c:277` |
| `0xE02` + `MWV207_STALL` | **未映射** | 同前 | `compute.c:278-283` |
| `0xE03` | **未映射** | `0xc23` | `compute.c:285` |

最后 `mwv207_submit(ctx, NULL, false)`（`compute.c:289`）。

### 11.3 本地内存与初始化

| 函数 | 位置 | 行为 |
|---|---|---|
| `mwv207_create_local_memory_buffer` | `compute.c:292-307` | 查 `MAX_LOCAL_SIZE`，建 `PIPE_BUFFER`（`PIPE_BIND_LINEAR`，`width0 = max_local_memory_size`） |
| `mwv207_context_compute_init` | `compute.c:309-319` | 建 local memory，注册 `set_hw_atomic_buffers / set_shader_buffers / set_shader_images / set_compute_resources / set_global_binding / launch_grid` |

---

## 12. 补充：表面与帧缓冲位域（`P/mwv207_surface.c`）

`mwv207_get_renderInfo`（`surface.c:52-124`）与 `mwv207_get_depthInfo`（`surface.c:126-185`）产出 `peInfo`，被 §8.4 直接使用。

| 字段 | 位域 | 位置 |
|---|---|---|
| `pixel_config` | bits29:24 = `peDesc.format`（并清 bit31）；bit6 清；bit20 = `super_tile`；bits14:13 = `tile_config` | `surface.c:77-81` |
| `pe_config` | bits13:12 = `SWIZZLE_RED`、bits15:14 = `GREEN`、bits17:16 = `BLUE`、bits19:18 = `ALPHA`；bit31 = `srgb`（并清 bit30）；bits6:5 = `0x3` | `surface.c:71-85` |
| `pixel_config_ex` | bit30 = `srgb`；bits23:20 = `peDesc.format`；bit28 = `super_tile`；bits27:26 = `tile_config`；bits17:0 = `surface->stride` | `surface.c:87-92` |
| `depth_config` | 初值 `0xFFFFFFFF`，清 bit12/bit22/bit14、置 bits5:4 `0x1`、置 bit26 `true`；`S8_UINT` 再清 bit12 并把 `pe_config` bits21:20 设 `0x1`；`Z16_UNORM` 把 `depth_normalize = 0xFFFF` 并清 bit4；`S8_UINT_Z24_UNORM` 置 `has_stencil` | `surface.c:139-167` |
| `depth_normalize` | 默认 `0xFFFFFF`，Z16 时 `0xFFFF` | `surface.c:131, 154` |
| `memory_config` | 有 TS：bit26 = msaa_compressed、bit27 = DISABLE、bit1 = ENABLE、bit7 = `ts.compressed`、bits11:8 = `ts.compressed_format`、bit30 = `(blocksize == 8)`；无 TS：bit1/bit5/bit7 = DISABLE | `surface.c:98-117` |
| `memory_config_ex` | 有 TS：bit9 = msaa_compressed、bit10 = DISABLE、bit0 = ENABLE、bit2 = `ts.compressed`、bits6:3 = `ts.compressed_format`、bit7 = `(blocksize == 8)`；无 TS：bit0/1/2 = DISABLE | `surface.c:106-122` |
| 深度版 `memory_config` | 有 TS：bit26、bit0 ENABLE、bit6 `ts.compressed`、bit14 `has_stencil`、bit3 `isZ16`；无 TS：bit0/bit4/bit6 = DISABLE | `surface.c:173-184` |

`mwv207_update_renderInfo`（`surface.c:36-49`）：把 `pe_config` 的 `0xFFF007FF` 位段换成 `SWIZZLE_RED/GREEN/BLUE/ALPHA`（位 12/14/16/18），用于 `frag_rgba_swap` 场景。

`mwv207_set_framebuffer_state`（`surface.c:277-355`）关键：
- `cbufs_map[i]` 记录「非空 cbuf 的紧凑序号」（`surface.c:293-299`）；
- 任一非 SUPERTILED 的 cbuf → `info.linear_outmode = true`（`surface.c:303-304`）；
- `rast_samples = max(nr_samples)`，ZS 有 `nr_samples` 且 >1 时 `subsamplez_dirty = true`（`surface.c:306, 344-352`）；
- `pixel_128bit` 的 cbuf 记 `pixel_record[i] = count + pixle_128_offset` 并在 `cbufs_map[nr_cbufs + offset]` 追加一个逻辑 RT（`surface.c:321-326`）；
- 相邻 cbuf 的 swizzle 不同 → `frag_rgba_swap = true`（`surface.c:337-342`）。

---

## 13. 未能确认 / 存疑清单

### 13.1 因空壳头文件而无法确认

| 项 | 原因 |
|---|---|
| `MWV207_EMITF/EMITFS` 中 `0x0C000000` 基址（bit26）的「F」语义 | `P/include/hw/mwv207reg_{3d,texture,shader}.h` 为空壳（`REGISTERS.md:29-31`），无寄存器字段名可反推 |
| `VALUE/NAME/MASK/MASK_VALUE/MASK_NAME` 的实际可用性 | 依赖被剥离的 `reg##_##field##_Offset/_Mask`，本发行版无任何调用点（§1.3） |
| 所有 REG §3 寄存器的位域语义 | `0xE02/0xE03/0x404/0x500/0x50B/0x192/0x28C/0x28D/0x503/0x594/0x595/0xFFFF/0x191/0x281-0x28F/0x301-0x306/0x401/0x501/0x502/0x505-0x50A/0x50D/0x596/0x598/0x599/0x59B/0x6D0/0xE00/0xE05/0xE13/0xF00` 在厂商头文件中**无 `_Address` 定义**（`REGISTERS.md:243-299`），位域含义必须实测 |

### 13.2 因超出本次阅读范围而无法确认

| 项 | 说明 |
|---|---|
| `attribue_config` / `fetch_size` 位域 | 由 `mwv207_get_element_desc()`（`P/mwv207_format.c`）填充，本文未通读该文件 |
| `peDesc.format/peDesc.swizzles/output_mode` 的具体编码 | 由 `mwv207_get_pixel_desc()`（`P/mwv207_format.c`）产生 |
| `config/src_configex/dst_configex/blit_config` 的位域 | 由 `P/mwv207_resource.c` 的 `mwv207_get_image_info()` 计算（在 REG 映射之外） |
| `mwv207_config_vs/tcs/tes/gs/fs/link` 与 `mwv207_config_arrays` 的实现 | 在 `P/mwv207_shader.c`，非本文范围 |
| `mwv207_get_clearval`、`mwv207_get_blit_desc`、`mwv207_get_blit_msaa_mode`、`mwv207_update_colormask` | 在 `P/mwv207_format.c`，非本文范围 |
| `drm_mwv207_submit_reloc` 的字段与 `cmdbuf_*` 的扩容/提交实现 | 在 winsys（`src/gallium/winsys/mwv207/drm/`），本文只读到类型声明（`WSH:144-186`） |

### 13.3 通读中记录的可疑点（供缺陷分析参考，未做修复）

| # | 位置 | 现象 |
|---|---|---|
| A1 | `draw.c:751-754` | `color_8` 四个分量全部 `<< 8` 后按位或，第 2/3 个分量会互相覆盖（有效位仅剩 8 位），疑似复制粘贴缺陷 |
| A2 | `draw.c:888, 907` | `draw_count` 为函数内 `static`，跨 context/跨进程共享，多上下文时 `DRAW_ID` 不单调 |
| A3 | `blit.c:568-584` | 压缩格式分支算出的 `task_info` 被随后无条件赋值覆盖，压缩路径的 stride/nblocksy 适配未生效 |
| A4 | `draw.c:1012` | 无 ZS 时直接解引用 `ctx->shader_states[PIPE_SHADER_FRAGMENT]->current`，未做 NULL 检查 |
| A5 | `mwv207_state.c:326-331` | `stencil_back_reference` 的 0/1 索引与正面交叉（`back[0]=ref[1]`），与 `stencil_reference` 的对应方式不一致；是否合硬件语义未确认 |
| A6 | `query.c:108-112` | TIMESTAMP/TIME_ELAPSED 用 CPU `os_time_get_nano()`，`0xE26/0xE1B` 并非 GPU 时间戳 |
| A7 | `query.c:156-163, 279-282` | `PIPELINE_STATISTICS*` / `SO_STATISTICS` begin/end 不发射寄存器，`get_query_result` 对未支持类型走到 `unreachable` → **管线统计与 SO 统计实际不可用** |
| A8 | `mwv207_state.c:335-354` | 视口/裁剪只保留单份（忽略多视口/多裁剪 slot），Mesa 侧多视口请求会被静默丢弃 |
| A9 | `mwv207_state.h:143` | `mwv207_context_zsa_init` 有声明但无定义、无调用 |
| A10 | `mwv207_query.c:342`、`mwv207_query.h:53` | 函数名拼写 `destory`（原文） |

---

*本文由逐行通读源码生成；所有寄存器名取自 `docs/registers/REGISTERS.md` 的自动映射，映射表未收录的 `base+index` 形式寄存器已在文中逐一标注。*