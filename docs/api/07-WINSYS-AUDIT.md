# 07 · WINSYS 观察项 O2 / O3 的实际影响面审计

- 被测树：`src/oh-mesa/User_source_code/mesa-24.1.2`（下文 T）
- 对照树：`src/mesa`（厂商 JM9_1.8.0 发布件同源 Mesa **22.3.7**，下文 V）
- KMD：`src/kernel/drivers/gpu/drm/mwv207/`
- 路径缩写：`D/` = T `src/gallium/drivers/mwv207/`；`W/` = T `src/gallium/winsys/mwv207/drm/`；`K/` = KMD 目录
- 方法：逐个 `read` 实现与全部调用点；`sizeof` 用原样结构体 gcc 实测（非 grep 推断）

---

## 1. 结论摘要

**O2 —— 当前不是可触发的 bug，但确为移植引入的语义分歧（未爆炸的哑弹）。**
`MWV207_FLAG_SUBALLOC` 在 T 里是**死代码**：全树 5 处 `SUBALLOC` 命中（枚举
`W/mwv207_drm_winsys.h:62`、判定 `W/mwv207_drm_bo.c:130`、判定
`D/mwv207_resource.c:288-289`、宏定义 `D/include/mwv207_resource.h:42`）中
**没有任何一处把该 flag 写进任何 `pipe_resource.flags`**（见 §2.3）→ 不走 slab 子分配
→ 所有 BO 的 `bo->offset == 0` → 两种返回语义返回同一地址。保护它的**两条不变式叠加**：
(a) 只有 slab entry 的 `offset` 非 0（`W/mwv207_drm_bo.c:509`）；(b) slab entry 创建时
即预置 `logical`（`W/mwv207_drm_bo.c:510`），故 slab BO **永远命中 `:276` 早返回**、
走不到首调用分支。触发条件：任何调用方以 `MWV207_FLAG_SUBALLOC` 建 `size <= 4096`
的 BO（上限 `1<<(min_order+num_orders-1)` = `1<<12`，见 §2.3）—— 届时应先修 O2 再启用。

**O3 —— 真实缺陷，但后果不是"错位"，而是每个描述符槽尾部 0x64 字节未初始化。**
唯一生产者把 `src_dat->size` 设为 `sizeof(struct mwv207_texture_config)` = **0x9C**
（实测 156，`D/mwv207_sampler.c:469`），而 `W/mwv207_drm_cs.c:268` 用硬编码 `0x100`
推进拼接基址 —— **"`src_dat->size == 0x100`"这一假设已被打破**。但拼接基址（`:267`
memcpy 目标、`:262` `cmd_offset` 记账、`:270` 返回值）三者**统一用同一个 0x100 步进的
`dest_dat->size`**，与 `src_dat->size` 无关 → **布局不错位、reloc 不错位、也不越界**
（证明见 §3.4）。实际后果：**每个 256 B 槽的 `[0x9C, 0x100)` 共 100 字节从不被写**，
保留 `MALLOC`/`realloc` 的**未初始化堆内容**（`W/mwv207_drm_cs.c:226`、`:242` 均非零
初始化；`:436-440` 每帧只复位 `size`（`:438`）、不复位 `data`），该缓冲被 KMD 逐字节读走
并写进 dat BO（`K/submit.c:262`、`K/submit.c:485-508`）→ GPU 可见。触发条件：任何一次
带纹理采样的 draw（`D/mwv207_sampler.c:578` 与 `:598`）。**除未初始化数据暴露（尾字段
不确定 / 堆内容泄漏）外没有第二个后果**：`K/` 侧不要求 `dat_size` 是 256 的倍数
（`K/submit.c:228-239` 只校验 `0 < dat_size <= 0x20000`），故不会报错。

---

## 2. O2 详查：`bo_map` 的两种返回语义

### 2.1 实现（`W/mwv207_drm_bo.c:269-294`）

```c
269 mwv207_drm_bo_map(ws, bo) {
276    if (bo->logical) return bo->logical;              /* 含 bo->offset */
286    ptr = os_mmap(0, bo->base.base.size, ..., ws->fd, args.out.offset);
288    if (ptr == MAP_FAILED) return NULL;               /* 失败：logical 保持 0 */
291    bo->logical = (char *)ptr + bo->offset;
293    return ptr;                                       /* 首调用：不含 offset */
294 }
```

`bo->offset` 的全部写入点（T）：`W/mwv207_drm_bo.c:205` = 0（真实 BO 创建，`no_slab`
路径）、`:389` = 0（`bo_from_handle()` 导入）、**`:509 bo->offset = i * entry_size`
（仅 slab entry，i>0 时非 0）**。slab entry 在 `:507-510` 创建时即把
`logical` 写死为 `(char *)slab->buffer->logical + bo->offset` → `:276` 早返回**总是**先命中。

### 2.2 驱动层调用点全表（`ws->bo_map`，含返回值处置）

| # | 位置 | 用返回值 | 用途 |
|---|---|---|---|
| 1 | `D/mwv207_resource.c:116` | 用 | `memset(dst, 0, tile_size)`（tile BO 清零） |
| 2 | `D/mwv207_resource.c:439` | 用 | PIPE_BUFFER transfer 基址，再 `+= res->offset + stride*x` |
| 3 | `D/mwv207_resource.c:467` | 用 | grading staging 资源基址，再 `+= staging_res->offset + level/stride/y/z` |
| 4 | `D/mwv207_resource.c:482` | 用 | 线性资源基址，同 #3 叠加 |
| 5 | `D/mwv207_resource.c:510` | 用 | `PIPE_MAP_READ` 大尺寸 DMA staging 基址，同 #3 叠加 |
| 6 | `D/mwv207_vertex.c:461` | 用 | 存成 `ctx->cb[shader][index].user_buffer`，后由 uniform 发射路径当基址读 |
| 7 | `D/nir/mwv207_compiler.c:389` | 用 | `dst += shader->bo_offset;` 后 memcpy shader code |
| 8 | `D/nir/mwv207_compiler.c:395` | 用 | `dst += state->bo_offset;` 后 memcpy shader code |
| 9 | `D/mwv207_query.c:51` | 用 | `memset(data, 0, 64*100)` |
| 10 | `D/mwv207_query.c:248` | 用 | 当 `uint64_t *results` 读回 query 结果 |
| 11-13 | `D/mwv207_sampler.c:253,254,255` | 用 | `merge_128bit` 读 src、写 fake_rg / fake_ba |
| 14-16 | `D/mwv207_sampler.c:310,311,312` | 用 | 同族另一函数（另一 tile 布局变体） |
| 17 | `D/video-codec/mwv207_video_buffer.c:586` | 用 | 存成 `buffer->virtual_addr`（CPU 指针，`dec_ppu.c:534` 等直接解引用） |
| 18 | `D/video-codec/mwv207_video_buffer.c:602` | 用 | 同上（延迟分配路径） |
| 19 | `D/video-codec/mwv207_video_decoder.c:424` | 用 | 存成 `dec->bs_ptr`（CPU memcpy 目标） |
| 20 | `D/video-codec/mwv207/mwv207_dec_hevc.c:1102` | 用 | 存成 `dec->misc_buf`（CPU 侧） |
| 21 | `D/video-codec/mwv207/mwv207_dec_h264.c:2150` | 用 | 存成 `dec->cabac_buf`，随后 memset/memcpy |
| 22 | `D/video-codec/mwv207/mwv207_dec_h264.c:2299,2304` | **忽略** | 先 map 保证已映射，再读 `bo->logical` 做 fwrite（调试 dump） |
| — | `W/mwv207_drm_bo.c:416` | 用 | `bo_dump()` 打印内容 |
| — | `W/mwv207_drm_bo.c:489` | **忽略** | slab 缓冲自身 map，仅借副作用置 `slab->buffer->logical` |
| — | `W/mwv207_drm_bo.c:545` | — | 函数表装配，非调用 |

**没有一处使用返回值后再补 `bo->offset`**：需要偏移的地方补的都是 `res->offset` /
`level_infos[].offset` / `shader->bo_offset`（BO **内部**偏移），而 `res->offset` 在
`D/mwv207_resource.c:207` 被固定置 0（唯一赋值点；`:210` 只改 `res->size`）。

另有**绕过 `bo_map` 直读 `bo->logical`**（= "第二次调用"语义、含 `bo->offset`）的位置：
`D/mwv207_compute.c:106` `uint32_t va = (uint32_t)(uintptr_t)rsc->bo->logical + offset;`
—— 它把该指针当**硬件虚拟地址**回填给上层。唯一显式使用 `bo->offset` 的 CPU 侧路径是
**DMA 硬件地址**：`D/mwv207_cmd.c:275,278,293,296` 的
`src->bo->offset + src->offset + ...`。即：`bo->offset != 0` 时，"CPU 地址走
`bo_map` 返回值（首/次调用不一致）"与"硬件 VA 走 `bo->logical`/`bo->offset`（恒含偏移）"
必然彼此矛盾 —— 这就是 O2 的爆炸半径。

### 2.3 子分配（slab）触发条件

1. 入口 `W/mwv207_drm_bo.c:130`：`if ((flag & MWV207_FLAG_SUBALLOC) && size <= max_slab_entry_size)`；
   不满足（或 heap 越界，`:132-133`）则 `goto no_slab`（`:155`）走真实 BO 创建。
2. `max_slab_entry_size = 1 << (bo_slabs.min_order + num_orders - 1)`（`:128`）；
   `pb_slabs_init(min_order=9, max_order=12)`（`:557-571`：`min_slab_order=9`、
   `max_slab_order=18`、`num_slab_orders_per_allocator=(18-9)/3=3`），而 `pb_slabs_init`
   里 `num_orders = max_order - min_order + 1 = 4`
   （`src/gallium/auxiliary/pipebuffer/pb_slab.c:256-257`）→ **上限 = 1<<(9+4-1) = 4096 B**
   （entry_size 为 2^9..2^12）。
3. `MWV207_FLAG_SUBALLOC` 的**唯一**可能来源是 `D/mwv207_resource.c:288-289`
   （`res->base.flags & (PIPE_RESOURCE_FLAG_DRV_PRIV<<1)`；宏定义
   `D/include/mwv207_resource.h:42`；`PIPE_RESOURCE_FLAG_DRV_PRIV = 1<<9`，
   `src/gallium/include/pipe/p_defines.h:508`）。
4. **该宏全树无写入点**。唯一的资源 flag 写入是
   `D/video-codec/mwv207_video_buffer.c:192` `templ->flags = MWV207_RESOURCE_FLAG_VIDEO_BUFFER;`
   （即 `<<0`）。因此 `D/mwv207_resource.c:303`、`D/mwv207_resource.c:113`、
   `D/nir/mwv207_compiler.c:379`、`D/mwv207_query.c:45,47`、`D/mwv207_state.c:392`、
   `W/mwv207_drm_bo.c:485`（heap flags；`mwv207_flags_from_heap()`
   `W/mwv207_drm_winsys.h:88-95` 只给 CPU_ACCESS / REUSE）**全部不传 SUBALLOC**。
5. 即使将来有人设了该 flag：`D/mwv207_resource.c:294-295`
   `if (bind & PIPE_BIND_SHARED) flag = MWV207_FLAG_2D_ACCESS;` 是**赋值覆盖**，会丢掉
   SUBALLOC；`D/video-codec/mwv207_video_buffer.c:578-583` 的注释 "request a
   non-sub-allocated buffer" 正依赖这条（video buffer 一律 `PIPE_BIND_SHARED`）。
6. 参考树 V 相同：`src/mesa/.../mwv207_resource.c:256-257` 有同样判定、同样无人写该 flag；
   且 V 的 `bo_map`（`src/mesa/.../mwv207_drm_bo.c:291-314`）语义**单一**：
   `bo->logical = (char *)ptr + bo->offset; return bo->logical;`（`:312-314`）。

### 2.4 判定

- **不可触发（当前）**：T 的 slab 机制整体是死代码（`pb_slab_alloc` 只在
  `W/mwv207_drm_bo.c:136` 被调用，其外层即 `:130` 的 SUBALLOC 判定），
  所有 BO 的 `bo->offset == 0`，`:277` 与 `:293` 返回同一地址。
- **但仍应修**：这是 T 相对 V **移植引入的语义分歧**（V 恒返回 `bo->logical`），
  且驱动层有 21 处直接使用返回值。一旦启用子分配，`:293` 会让**首次**调用比后续调用
  少 `bo->offset` → 首次 CPU 访问落到 slab 起点（读到别的 entry 的数据），后续才正确；
  并且与 §2.2 的硬件 VA / DMA 地址计算不再一致。
- 附带隐患（独立于 O2，仅 mmap 失败时可触发）：slab entry 的 `logical` 由 `:510`
  **无条件**算出；若 `:489` 的 `ws->bo_map(slab->buffer)` 失败（`logical == 0`），i>0 的
  entry 会得到**非 NULL 的小指针**，此后 `:276` 早返回该假指针。当前同样不可触发。

---

## 3. O3 详查：`add_dat` 的硬编码 0x100 步进

### 3.1 通路

```
D/mwv207_sampler.c:420-470  mwv207_reconfig_texture()      ← 唯一生产者
    :462 cmd_buf = tex_desc->dat.data;
    :465-466 每 mip: mwv207_dat_add_reloc(&tex_desc->dat, &cmd_buf, res->bo, address[levelIdx], 0)
    :467-468 memcpy(cmd_buf, &tex_config.address[levelCount],
                    sizeof(struct mwv207_texture_config) - 4*levelCount);
    :469 tex_desc->dat.size = sizeof(struct mwv207_texture_config);   ← size 唯一写入点
D/mwv207_sampler.c:578 / :598   mwv207_add_dat(ctx, &cmd_buf, tex_desc_address + index, &tex_desc->dat)
D/mwv207_cmd.c:240-255  mwv207_add_dat(): :245 ws->cmdbuf_add_dat(ctx->cmd, dat)
    :246-252 MWV207_EMIT(reg,0) + cmdbuf_add_relloc(idx=0, cmd_offset, bo_offset=返回值, type=0x1|0x80000000)
W/mwv207_drm_cs.c:218-271  add_dat(): :267 memcpy(dest->data + dest->size, src->data, src->size);
    :268 dest->size += 0x100;   :270 return dest->size - 0x100;
```

`mwv207_dat_add_reloc()`（`D/mwv207_cmd.c:188-211`）把 reloc 的 `cmd_offset` 记成
`(*ptr - dat->data) * 4`（**相对 dat 段起始**）；`W/mwv207_drm_cs.c:262` 再
`+= dest_dat->size` 换算成拼接缓冲内偏移。

### 3.2 调用点全表 与 `cmd_dat.size` 写入点

| 调用点 | 传入的 `cmd_dat *` | `size` 的值 | 数据用哪个结构填 |
|---|---|---|---|
| `D/mwv207_sampler.c:578`（PIXEL 单元 `tex_desc_address+index`） | `&sampler_view->tex_desc.dat` | `sizeof(struct mwv207_texture_config)` | `struct mwv207_texture_config` |
| `D/mwv207_sampler.c:598`（GPIPE 单元，`tex_128bit` 分支） | 同上 | 同上 | 同上 |

- `mwv207_add_dat()` 无第三处调用点；`ws->cmdbuf_add_dat` 的唯一调用者是
  `D/mwv207_cmd.c:245`。
- 两处 `add_dat` 前都紧跟 `mwv207_reconfig_texture()`（`:565` 传 `false`、`:583` 传
  `true`），故 `size` 在被读前**总会**先由 `:469` 写成 0x9C。
- 全树 `->dat.size` 写入点**只有一个**：`D/mwv207_sampler.c:469`；其余
  `dat.size`/`dat->size` 命中都在 winsys 内部（`W/mwv207_drm_cs.c:224,238,262,267,268,270,348`）。

### 3.3 是否有别的更小/更大的 dat 结构

- **没有。** `struct cmd_dat` 实例全树只有一个：`D/include/mwv207_format.h:139`
  `struct cmd_dat dat;`（内嵌于 `struct mwv207_texture_desc`）；其余出现均为函数形参
  （`W/mwv207_drm_winsys.h:144` 定义、`:257` 函数指针、`D/include/mwv207_cmd.h:187,197`、
  `D/mwv207_cmd.c:188,243`、`W/mwv207_drm_cs.c:219,343`）。
- `struct cmd_dat` 本身确是 256 B：`data[64]`（`W/mwv207_drm_winsys.h:145`）。但**填充它的
  结构体不是 256 B**：`struct mwv207_texture_config`（`D/include/mwv207_format.h:98-123`，
  `address[16]` + 23 个 `uint32_t`）实测 `sizeof = 156 = 0x9C`（原样复制后 gcc 编译：
  `156 (0x9c)`）。V 的同名结构（`src/mesa/.../mwv207_format.h:99-123`）字段完全相同，
  V 的 `sampler.c:370` 与 `cs.c:294` 也是 `size = sizeof(...)` + `+= 256`
  → **O3 是厂商原生遗留，不是移植引入**。

### 3.4 判定与不变式

- **假设已被打破**：`src_dat->size = 0x9C != 0x100`，且**每次** add_dat 都如此（非边角）。
- **布局与记账不错位**：三处用的都是同一个 `dest_dat->size` —— memcpy 目标（`:267`）、
  reloc 基准（`:262`）、返回值（`:270`）→ 槽起点永远 0x100 对齐，reloc 永远指向本槽起点，
  注入 cmd_buf 的寄存器字也拿到同一基准。真正缺陷只是**每槽尾部 100 字节是未初始化堆数据**。
- **也不越界**（证明）：`dest_dat->size` 与 `max_size`（初值 `0x8000`，×2 增长，`:225`、`:240`）
  都是 0x100 的倍数；增长判据 `size + 0x9C > max_size`（`:238`）在 `size` 为 0x100 倍数时
  等价于 `size >= max_size` → 递增后恒有 `size <= max_size` ⇒ `:267` 的 memcpy 与 KMD 读出的
  `dat_size` 都在 `MALLOC` 区域之内（无堆越界、无越界读）。
- **数据去向**：`W/mwv207_drm_cs.c:410-416`（`:411 cmd->dats.dat_size =
  cmd->mwv207_dats.size;`，`:416` ioctl 0x5）把该 `size` 作为 `dat_size` 交给 KMD →
  `K/submit.c:256-262` `kvmalloc(dat_size)` + `copy_from_user(mdat->dat, udats[i].dat, dat_size)`
  → `K/submit.c:485-508` `memcpy_toio(ptr, mjob->cmd_dats[i].dat, dat_size)` 写进 dat BO。
  KMD 只校验 `0 < dat_size <= 0x20000`（`K/submit.c:228-239`），**不校验 256 倍数**
  → 不报错，只把未初始化字节搬到 GPU 可见内存。
- **同一处代码会放大更严重的形态**（当前不发生）：若生产者把 `size` 设成**大于** 0x100，
  `:267` 的 memcpy 会覆盖下一槽起始（随后被下一槽的 memcpy 覆盖）→ 静默**数据截断/丢失**；
  若 `size == 0`（将来有人在未调 `mwv207_reconfig_texture()` 时 `add_dat`），整槽都是残留数据。
  二者都源于同一条"size（有效字节数）与槽宽混用"。
- **不建议**把 `:268` 改成 `+= src_dat->size`：槽宽按 0x100 约定（厂商 V 同款代码），
  改成 0x9C 步进后每槽只隔 156 B，HW 若读 256 B 就会踩下一个槽。**0x100 应保留为槽宽**，
  `src_dat->size` 应视为"本槽有效字节数 ≤ 0x100"。
- 既有文档需更正：`06-KNOWN-ISSUES.md:45` 的"`struct cmd_dat` 确为 256 字节 …
  故当前不会出错"**不准确**（填充结构是 0x9C，尾部未初始化）；`01-WINSYS-KMD.md:347-348`
  的"未检索全部 cmd_dat 构造点"本轮已检索完毕，结论即上。

---

## 4. 建议的最小修复（仅建议，未改代码）

1. **O3（1 行，winsys 侧，最稳）**：`W/mwv207_drm_cs.c:267` 之前先把整槽清零再拷贝：
   `memset(dest_dat->data + dest_dat->size, 0, 0x100);`
   —— 不动槽宽与记账，彻底消除未初始化字节，对任何 `size < 0x100` 的生产者都成立。
   备选（2 行，生产者侧）：`D/mwv207_sampler.c:469` 处把 `tex_desc->dat.data` 的
   `[sizeof(struct mwv207_texture_config), 256)` 清零，并把 `dat.size` 设为
   `sizeof(tex_desc->dat.data) /* 0x100 */`，使 `size` 与槽宽自洽；建议同时在
   `W/mwv207_drm_cs.c:267` 加 `assert(src_dat->size <= 0x100);` 固化该假设。
2. **O2（1 行）**：`W/mwv207_drm_bo.c:293` `return ptr;` → `return bo->logical;`
   （与厂商 V `mwv207_drm_bo.c:312-314` 一致），使返回值恒含 `bo->offset`；
   在此修复落地前，不得有任何调用方启用 `MWV207_FLAG_SUBALLOC`。

---

## 5. 未能确认项

1. **硬件纹理描述符的真实宽度**：寄存器侧只有
   `MWV207REG_TX_{GPIPE,PIXEL}_TEXTURE_DESCRIPTOR_ADDR`（`docs/registers/REGISTER-ARRAYS.md:90,98`），
   无资料给出槽宽；"HW 是否读取每槽尾部 0x64 字节、那 100 字节是否参与翻译"**未确认**。
   能确定的只有：这些字节**不是驱动写的**（未初始化堆内容）。
2. **厂商 22.3.7 二进制发布件（deb 内 UMD）是否与 `src/mesa` 源码同构**：本轮只比对源码
   （相同结构、相同 0x9C 与 0x100），**未反汇编核对**，故"厂商运行态同样存在该 0x64 尾隙"
   属源码级推断。`JM9_1.8.0` 释放目录内**只有 deb，无源码**。
3. **`struct mwv207_texture_config` 与硬件寄存器字段的逐字段映射**：厂商寄存器数据库中
   没有描述符内存布局定义，故"尾部未写字节对应哪些 HW 字段"未确认。
4. **`bo->logical` 直读点未穷举**：本轮以 `ws->bo_map` 调用点与 `bo->offset` 使用点为主线；
   grep 已知的直读点为 `D/video-codec/mwv207/mwv207_dec_h264.c:2155,2298,2301,2303,2306`、
   `D/mwv207_compute.c:106`、`W/mwv207_drm_bo.c:63-64,510`。
5. **未做运行态验证**：以上均为静态审计；未在 JM9100 上跑带纹理/视频的负载核对槽尾字节与
   GPU 行为。`W/mwv207_drm_cs.c:343-348` 的 `mwv207_print_dat()` 可打印 `dat->size` 范围内
   的字，可作为运行时验证入口。