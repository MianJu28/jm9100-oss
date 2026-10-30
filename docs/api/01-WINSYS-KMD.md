# 01 · WINSYS 层与 KMD ABI（JM9100 / MWV207）

> 目标：逐行通读 Mesa mwv207 的 winsys（drm）层 8 个文件 + KMD 侧 ABI 相关文件，产出可核对的 API/ABI 文档。
> 方法：全部结论来自 `read` 工具完整读取的源码，均以 `文件:行号` 标注；无法确认处明确写「未确认」。
> 日期：2026-10-30。

## 0. 路径图例（全文使用短名，短名与全路径一一对应）

| 短名 | 完整路径 |
|------|----------|
| `winsys.c` | `src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium/winsys/mwv207/drm/mwv207_drm_winsys.c`（276 行） |
| `winsys.h` | `src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium/winsys/mwv207/drm/mwv207_drm_winsys.h`（285 行） |
| `bo.c` | `src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium/winsys/mwv207/drm/mwv207_drm_bo.c`（575 行） |
| `bo.h` | `src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium/winsys/mwv207/drm/mwv207_drm_bo.h`（33 行） |
| `cs.c` | `src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium/winsys/mwv207/drm/mwv207_drm_cs.c`（460 行） |
| `cs.h` | `src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium/winsys/mwv207/drm/mwv207_drm_cs.h`（37 行） |
| `drm.h`(UMD) | `src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium/winsys/mwv207/drm/mwv207_drm.h`（215 行） |
| `public.h` | `src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium/winsys/mwv207/drm/mwv207_drm_public.h`（37 行） |
| `K/drm.h` | `src/kernel/drivers/gpu/drm/mwv207/mwv207_drm.h`（160 行） |
| `K/gem.c` | `src/kernel/drivers/gpu/drm/mwv207/mwv207_gem.c`（155 行） |
| `K/ctx.c` | `src/kernel/drivers/gpu/drm/mwv207/mwv207_ctx.c`（249 行） |
| `K/submit.c` | `src/kernel/drivers/gpu/drm/mwv207/mwv207_submit.c`（653 行） |
| `K/drv.c` | `src/kernel/drivers/gpu/drm/mwv207/mwv207_drv.c`（556 行） |
| `K/db.c` | `src/kernel/drivers/gpu/drm/mwv207/mwv207_db.c`（144 行） |
| `K/bo.c` | `src/kernel/drivers/gpu/drm/mwv207/mwv207_bo.c`（307 行） |
| `K/bo.h` | `src/kernel/drivers/gpu/drm/mwv207/mwv207_bo.h`（111 行） |
| `K/mwv207.h` | `src/kernel/drivers/gpu/drm/mwv207/mwv207.h`（127 行） |
| `K/ctx.h` | `src/kernel/drivers/gpu/drm/mwv207/mwv207_ctx.h`（51 行） |
| `K/sched.h` | `src/kernel/drivers/gpu/drm/mwv207/mwv207_sched.h`（153 行） |
| `K/submit.h` | `src/kernel/drivers/gpu/drm/mwv207/mwv207_submit.h`（23 行） |
| `D/screen.c` | `src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium/drivers/mwv207/mwv207_screen.c`（679 行） |
| `D/context.c` | `src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium/drivers/mwv207/mwv207_context.c`（232 行） |
| `D/cmd.c` | `src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium/drivers/mwv207/mwv207_cmd.c`（372 行） |
| `D/include/context.h` | `src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium/drivers/mwv207/include/mwv207_context.h` |
| `D/include/screen.h` | `src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium/drivers/mwv207/include/mwv207_screen.h`（56 行） |

### 0.1 本树状态（重要）

本 checkout 的 winsys 源码**并非**缺陷清单（`/home/admin/jm9100-oss/reports/ANALYSIS-REPORT.md:25-38`）所描述的那份原始树：W1/W2/W3/W4/W5/W8/W9/W10/W11 的修复**已经落在代码里**（详见 §12），W6/W7/W12 的问题**在当前行号上仍然存在**。读本文时请以「本树行号」为准，报告行号仅作对照。
KMD 侧同样已是「K1–K8、K10 修复后」的树（见 §12.2 的逐条核对）。

---

## 1. 模块分层与创建链

### 1.1 分层图（谁创建谁）

```
应用 / EGL / GBM
    │  打开 /dev/dri/renderD128 或 card0 → drm fd (int)
    ▼
Mesa loader: src/gallium/auxiliary/target-helpers/drm_helper.h:126
    screen = mwv207_drm_create_screen(fd)
    ▼
mwv207_drm_create_screen(fd)                         winsys.c:209-275
    ├─ 全局单例表 mwv207_ws_tab (util_hash_table_create_fd_keys)   winsys.c:52, 216
    │      命中 → pipe_reference(NULL,&ws->reference) 后直接返回旧 screen  winsys.c:224-229
    └─ 未命中 → CALLOC_STRUCT(mwv207_winsys)                        winsys.c:231
            ├─ ws->fd = fcntl(fd, F_DUPFD_CLOEXEC, 3)                winsys.c:238
            ├─ mwv207_ws_init(ws)      → CTX create ioctl(0x4, op=0) winsys.c:119-148
            │        ├─ debug / cluster 初始化                       winsys.c:125-127
            │        ├─ bo_export_table / bo_name_table / 锁         winsys.c:129-134
            │        └─ ws->ctx = args.handle                        winsys.c:146
            ├─ mwv207_bo_init_function(ws)   (pb_cache/pb_slabs + 11 个函数指针)  winsys.c:253 → bo.c:541
            ├─ mwv207_cs_init_function(ws)   (5 个 fence + 6 个 cmdbuf 函数指针)  winsys.c:254 → cs.c:444
            ├─ ws->screen = mwv207_create_screen(ws)                 winsys.c:256
            │        └─ D/screen.c:642 创建 struct mwv207_screen
            │               screen->ws = winsys                      D/screen.c:649
            │               screen->base.context_create = mwv207_context_create   D/screen.c:665
            ├─ ws->destroy = mwv207_drm_destroy                       winsys.c:265
            ├─ pipe_reference_init(&ws->reference, 1)                winsys.c:267
            └─ _mesa_hash_table_insert(mwv207_ws_tab, ws->fd, ws)    winsys.c:269
    ▼
pipe_screen  (struct mwv207_screen)                  D/include/screen.h:39-45
    │   context_create → mwv207_context_create        D/screen.c:665 / D/context.c:174
    ▼
pipe_context (struct mwv207_context)                 D/include/context.h:109-168
    │   ctx->ws = screen->ws                          D/context.c:186
    │   ctx->in_fence_fd = -1                         D/context.c:188
    │   mwv207_create_cmd(ctx)                        D/context.c:217 → D/cmd.c:313-323
    │        ctx->cmd = ws->cmdbuf_create(ws); cmd->engine_type = 0x0   D/cmd.c:316-318
    │        ctx->dma = ws->cmdbuf_create(ws); dma->engine_type = 0x4   D/cmd.c:320-322
    ▼
命令提交：D/cmd.c:354 mwv207_submit() → ws->cmdbuf_commit()  D/cmd.c:362
                                       → ioctl(0x5 = DRM_MWV207_SUBMIT)  cs.c:416
```

销毁方向相反：`D/screen.c:629 mwv207_destroy_screen` → `screen->ws->destroy(screen->ws)`（`D/screen.c:633`）→ `winsys.c:151 mwv207_drm_destroy`；引用计数归零时：CTX destroy ioctl（`winsys.c:163`，op=0x1）→ `pb_slabs_deinit`/`pb_cache_deinit`（`winsys.c:165-167`）→ 销毁哈希表与锁（`169-171`）→ 从 `mwv207_ws_tab` 摘除（`173`）→ `close(ws->fd)` + `FREE(ws)`（`175-176`）。

### 1.2 关键分工

| 层 | 结构 | 职责 | 关键行 |
|----|------|------|--------|
| winsys | `struct mwv207_winsys` | 持 fd/内核 ctx、BO 缓存与 slab、跨进程导出表、函数指针表 | `winsys.h:188-271` |
| screen | `struct mwv207_screen` | gallium pipe_screen；转发 fence 调用到 winsys | `D/include/screen.h:39-45`，`D/screen.c:600-626` |
| context | `struct mwv207_context` | 每 context 两条 `mwv207_cmd`（3D engine=0，DMA engine=4）、in fence fd | `D/include/context.h:112-115,155` |
| cmd | `struct mwv207_cmd` | 命令缓冲、BO 表、reloc 表、dat 表 | `winsys.h:163-186` |

---

## 2. `struct mwv207_winsys` 完整字段表

> 任务书写作 `struct mwv207_drm_winsys`，源码中的真实名字是 **`struct mwv207_winsys`**（`winsys.h:188`），无 `struct mwv207_drm_winsys` 这个类型。

| 字段 | 类型 | 含义 | 赋值点（文件:行号） |
|------|------|------|---------------------|
| `screen` | `struct pipe_screen *` | 回指 gallium screen | `winsys.c:256`（`ws->screen = mwv207_create_screen(ws)`） |
| `reference` | `struct pipe_reference` | winsys 共享引用计数 | `winsys.c:267` init=1；`winsys.c:227` 命中单例时 `pipe_reference(NULL,&ws->reference)`；`winsys.c:156` 销毁时 `pipe_reference(&ws->reference,NULL)` |
| `fd` | `int` | **内核用 fd（副本）**，同时是全局单例表的 key | `winsys.c:238`（`fcntl(fd, F_DUPFD_CLOEXEC, 3)`）；`winsys.c:175` close |
| `ctx` | `int` | 内核 context handle（`drm_mwv207_ctx.handle`） | `winsys.c:146`（`ws->ctx = args.handle`，来自 `winsys.c:138` 的 ioctl 0x4/op=0） |
| `device` | `int` | 声明存在，**在本 8 文件范围内从未被赋值**（恒为 0） | 未确认其用途（本层内无读点） |
| `debug` | `uint32_t` | `mwv207_debug` 位掩码 | `winsys.c:125`（`debug_get_option_mwv207_debug()`） |
| `cluster_mask` | `uint32_t` | GPU cluster 位掩码 | `winsys.c:78`（硬编码 `0xf`） |
| `cluster_count` | `uint32_t` | cluster 个数 | `winsys.c:79` 置 0 → `winsys.c:87` 计数 → `winsys.c:100` 下限 1 |
| `clusterMinID` | `uint32_t` | 最低 cluster ID | `winsys.c:90` |
| `clusterMaxID` | `uint32_t` | 最高 cluster ID | `winsys.c:92` |
| `bo_cache` | `struct pb_cache` | pb_cache 对象缓存 | `bo.c:552-555`（`pb_cache_init(..., 500000, 2.0f, 0, 0x200000, offsetof(u.real.cache_entry), NULL, mwv207_bo_destroy, mwv207_bo_can_reclaim)`）；`winsys.c:167` deinit |
| `bo_slabs` | `struct pb_slabs` | slab 子分配器 | `bo.c:565-571`（`pb_slabs_init(..., min_order=9, max_order=?, MWV207_MAX_SLAB_HEAPS, false, ws, ...)`）；`winsys.c:166` deinit（条件 `ws->bo_slabs.groups`） |
| `bo_export_table` | `struct hash_table *` | `handle → mwv207_bo*` 导出/导入表 | `winsys.c:129-130` 创建；销毁 `winsys.c:170` |
| `bo_name_table` | `struct hash_table *` | `flink name → mwv207_bo*` | `winsys.c:131-132` 创建；销毁 `winsys.c:171` |
| `bo_export_table_lock` | `simple_mtx_t` | 保护上述两张表 | `winsys.c:134` init；`winsys.c:169` destroy；加锁点 `bo.c:66,325,346` |
| `destroy` | `bool (*)(struct mwv207_winsys *ws)` | winsys 销毁入口 | `winsys.c:265`（= `mwv207_drm_destroy`）；调用点 `D/screen.c:633` |
| `bo_create` | `struct mwv207_bo *(*)(struct mwv207_winsys*, uint64_t size, enum mwv207_bo_domain, enum mwv207_bo_flag)` | 建 BO | `bo.c:543`（= `mwv207_drm_bo_create`）；调用点 `bo.c:485`、`D/query.c:45,47`、`D/state.c:392`、`D/resource.c:113,303`、`D/nir/mwv207_compiler.c:379` |
| `bo_wait` | `int (*)(struct mwv207_winsys*, struct mwv207_bo*, uint64_t timeout)` | 等 BO 空闲 | `bo.c:544`（= `mwv207_drm_bo_wait`）；调用点 `bo.c:445,459`、`D/query.c:246`、`D/resource.c:405`、`D/nir/…:386` |
| `bo_reference` | `void (*)(struct mwv207_winsys*, struct mwv207_bo **dst, struct mwv207_bo *src)` | 引用转移/释放 | `bo.c:548`；调用点 `cs.c:156,212,434`、`D/query.c:66,67`、`D/state.c:431`、`D/shader.c:1107`、`D/resource.c:244,248` |
| `bo_map` | `void *(*)(struct mwv207_winsys*, struct mwv207_bo*)` | mmap 到用户态 | `bo.c:545`；调用点 `bo.c:416,489`、`D/query.c:51,248`、`D/sampler.c:253-255,310-312`、`D/resource.c:116,439,467,482,510`、`D/vertex.c:461`、`D/nir/…:389,395` |
| `bo_unmap` | `void (*)(... , struct mwv207_bo*)` | 解除映射（本实现为空） | `bo.c:546`；调用点 `bo.c:433`、`D/query.c:53,285`、`D/resource.c:118,533,541`、`D/nir/…:392,398` |
| `bo_get_handle` | `uint32_t (*)(struct mwv207_winsys*, struct mwv207_bo*, struct winsys_handle *whandle)` | 导出 KMS/FD/SHARED 句柄 | `bo.c:549`；调用点 `D/resource.c:343` |
| `bo_from_handle` | `struct mwv207_bo *(*)(struct mwv207_winsys*, struct winsys_handle*, uint32_t size)` | 由外部句柄导入 | `bo.c:550`；调用点 `D/resource.c:301` |
| `bo_dump` | `void (*)(struct mwv207_winsys*, struct mwv207_bo*, uint32_t size)` | 调试打印 BO 内容 | `bo.c:547`；**在 `drivers/mwv207` 下未检索到调用点** |
| `fence_wait` | `int (*)(struct mwv207_winsys*, struct pipe_fence_handle*, uint64_t timeout)` | 等 fence | `cs.c:449`；调用点 `D/screen.c:617`、`D/video-codec/common/mwv207_video_cs.c:127` |
| `fence_get_fd` | `int (*)(struct mwv207_winsys*, struct pipe_fence_handle*)` | 取 fence fd（dup） | `cs.c:450`；调用点 `D/screen.c:625` |
| `fence_accumulate` | `void (*)(struct mwv207_winsys*, int *target_fd, struct pipe_fence_handle*)` | 合并 in-fence | `cs.c:451`；调用点 `D/context.c:171` |
| `fence_reference` | `void (*)(struct pipe_fence_handle **dst, struct pipe_fence_handle *src)` | fence 引用计数 | `cs.c:448`；调用点 `D/screen.c:606` |
| `fence_create` | `struct pipe_fence_handle *(*)(struct mwv207_winsys*, int fd)` | 用 fd 造 fence | `cs.c:447`；调用点 `D/context.c:94,163`、`D/video-codec/…/mwv207_video_cs.c:125` |
| `cmdbuf_create` | `struct mwv207_cmd *(*)(struct mwv207_winsys*)` | 建命令缓冲 | `cs.c:453`；调用点 `D/cmd.c:316,320`、`D/video-codec/…/mwv207_video_cs.c:39` |
| `cmdbuf_destory` | `void (*)(struct mwv207_winsys*, struct mwv207_cmd*)` | 销毁（注意拼写 destory） | `cs.c:454`；调用点 `D/context.c:141,143` |
| `cmdbuf_add_bo` | `int (*)(struct mwv207_cmd*, struct mwv207_bo*, uint32_t flags)` | BO 入表（去重） | `cs.c:455`；调用点 `cs.c:263`、`D/cmd.c:221,269,286`、`D/vertex.c:149,174,192,206,227,296`、video-codec 多处 |
| `cmdbuf_add_dat` | `int (*)(struct mwv207_cmd*, struct cmd_dat*)` | 追加 dat 段 | `cs.c:456`；调用点 `D/cmd.c:245`（唯一） |
| `cmdbuf_add_relloc` | `int (*)(struct mwv207_cmd*, uint32_t idx, uint32_t cmd_offset, uint32_t bo_offset, uint32_t type, uint32_t pad)` | 追加重定位项 | `cs.c:457`；调用点 `D/cmd.c:234,252,270,287`、`D/vertex.c:153,178,194,210,239,300`、video-codec |
| `cmdbuf_commit` | `void (*)(struct mwv207_winsys*, struct mwv207_cmd*, int in_fence_fd, int *out_fence_fd)` | 提交 ioctl（0x5） | `cs.c:458`；调用点 `D/cmd.c:308,362`、`D/video-codec/…/mwv207_video_cs.c:123,133` |

### 2.1 配套枚举 / 调试位 / inline 助手

| 名称 | 定义 | 语义 |
|------|------|------|
| `MWV207_DOMAIN_GTT` | `winsys.h:53` = `1<<0` | 映射到 `preferred_domain |= 0x4`（`bo.c:176-177`） |
| `MWV207_DOMAIN_VRAM` | `winsys.h:54` = `1<<1` | → `0x2`（`bo.c:172-173`） |
| `MWV207_DOMAIN_SYSTEM` | `winsys.h:55` = `1<<2` | → `0x1`（`bo.c:174-175`） |
| `MWV207_FLAG_CPU_ACCESS` | `winsys.h:59` = `1<<0` | → 内核 `flags |= 0x1`（`bo.c:179-180`） |
| `MWV207_FLAG_NO_CPU_ACCESS` | `winsys.h:60` = `1<<1` | → 内核 `flags |= 0x2`（`bo.c:181-182`） |
| `MWV207_FLAG_2D_ACCESS` | `winsys.h:61` = `1<<2` | → `alignment = 0x10000`（`bo.c:184-185`） |
| `MWV207_FLAG_SUBALLOC` | `winsys.h:62` = `1<<3` | 走 slab 子分配（`bo.c:130`） |
| `MWV207_FLAG_REUSE` | `winsys.h:63` = `1<<4` | 走 `pb_cache_reclaim_buffer`（`bo.c:157`）＋ `use_reusable_pool=true`（`bo.c:208-209`） |
| `MWV207_HEAP_3D` / `MWV207_HEAP_DMA` | `winsys.h:67-68` | slab/cache heap 下标；`MWV207_MAX_SLAB_HEAPS` = 2（`winsys.h:69`） |
| `mwv207_domain_from_heap()` | `winsys.h:73-85` | 3D→VRAM，DMA→GTT，其他 `assert(0)` 后返回 0 |
| `mwv207_flags_from_heap()` | `winsys.h:87-99` | 3D→`CPU_ACCESS`；DMA→`CPU_ACCESS|REUSE` |
| `mwv207_get_heap_index()` | `winsys.h:101-109` | **只按 domain 判定**：GTT→DMA 堆，其余（含 SYSTEM/VRAM）→3D 堆 |
| `mwv207_bo()` / `mwv207_slab()` | `winsys.h:273-283` | 指针强转（`pb_buffer_lean*`→`mwv207_bo*`；`pb_slab*`→`mwv207_slab*`） |
| `MWV207_DEBUG_*` 位 | `winsys.h:33-40`（bit0..bit7） | 与 `winsys.c:58-71` 的 `debug_options[]` 对应：nir=0x1、nir_all=0x201、tgsi=0x2、bo=0x4、cmd=0x8、size=0x10、tex=0x20、flush=0x40、video=0x80、spill=0x100。**头文件缺 `SPILL(1<<8)` 定义**（未被本文档范围代码引用） |
| 全局 `int mwv207_debug` | `winsys.c:56` | 非 static 全局；`DEBUG_GET_ONCE_FLAGS_OPTION` 于 `winsys.c:73`，读入口 `debug_get_option_mwv207_debug()`（`winsys.c:125`） |

---

## 3. `struct mwv207_bo` 字段表（`winsys.h:111-136`）

| 字段 | 类型 | 含义 | 赋值点 |
|------|------|------|--------|
| `base` | `struct pb_buffer` | pipebuffer 基类（含 `base.base` 为 `pb_buffer_lean`） | `bo.c:197-200`（真实 BO）、`bo.c:501-504`（slab 子 BO） |
| `fd` | `int` | prime 导出的 fd 缓存 | 仅在 `WINSYS_HANDLE_TYPE_FD` 导出时由 `drmPrimeHandleToFD` 写入：`bo.c:310`；**创建路径未初始化**（`bo.c:202-207`、`bo.c:388-391` 都未写 `fd`） |
| `name` | `uint32_t` | flink 全局名（`DRM_IOCTL_GEM_FLINK` 返回） | `bo.c:204`（=0）、`bo.c:322`（= `args.name`）、`bo.c:397`（导入时 = `whandle->handle`） |
| `handle` | `uint32_t` | 本进程 GEM handle | `bo.c:202`（`args.out.handle`）、`bo.c:388`（`drmPrimeFDToHandle`/`GEM_OPEN` 的 handle）；slab 子 BO 共享父 handle：`bo.c:506` |
| `offset` | `uint32_t` | 子 BO 在父 BO（slab 大块）内的字节偏移 | `bo.c:205`（=0）、`bo.c:389`（=0）、`bo.c:509`（= `i*entry_size`） |
| `logical` | `void *` | 映射基址 + `offset`（即「这个 BO 自己的」可写指针） | `bo.c:203`（=0）、`bo.c:291`（= `ptr + offset`）、`bo.c:507`（=0）、`bo.c:510`（= `slab->buffer->logical + offset`）；销毁时 `drmUnmap`：`bo.c:63-64` |
| `flags` | `uint32_t` | **声明存在，本 winsys 内既无写入也无读取**（W8 修复后访问 flags 已移到 per-cmdbuf `cmd->bos[i].flags`，见 `cs.c:208-210`） | 未确认原始用途 |
| `ws` | `struct mwv207_winsys *` | 回指 winsys | `bo.c:207`、`bo.c:392`、`bo.c:511` |
| `u.real.cache_entry` | `struct pb_cache_entry` | pb_cache 链表节点 | `bo.c:195`、`bo.c:383` |
| `u.real.use_reusable_pool` | `bool` | 销毁时是否归还缓存 | `false`/`true`：`bo.c:208-211`；`false`：`bo.c:391` |
| `u.slab.entry` | `struct pb_slab_entry` | slab 条目 | `bo.c:514,518`（链表挂入） |
| `u.slab.real` | `struct mwv207_bo *` | 子 BO 指向的真实（父）BO | `bo.c:206`（=NULL）、`bo.c:390`（=NULL）、`bo.c:516`（= slab->buffer） |

### 3.1 相关内部结构

| 结构 | 位置 | 字段 |
|------|------|------|
| `struct mwv207_slab` | `winsys.h:138-142` | `base: pb_slab`、`buffer: mwv207_bo*`（真实大块）、`entries: mwv207_bo*`（子 BO 数组） |
| `struct cmd_dat` | `winsys.h:144-151` | `data[64]`（uint32，**恰好 256 B**）、`size`、`bos[16]`、`relocs[16]`、`nr_relocs` |
| `struct cmd_dat_arrary` | `winsys.h:153-161` | `data`、`size`、`max_size`、`relocs`、`nr_relocs`、`nr_max_relocs`（拼写 arrary 为源码原样） |
| `struct mwv207_cmd` | `winsys.h:163-186` | `ws`、`buf`、`size`、`offset`、`stamp`、`relocs`、`nr_relocs`、`nr_max_relocs`、`mwv207_bos`（`mwv207_bo**`）、`bos`（`drm_mwv207_bo_acc*`）、`nr_bos`、`nr_max_bos`、`mwv207_dats`、`dats`（`drm_mwv207_submit_cmd_dat`）、`nr_dats`、`poisoned`、`engine_type` |

---

## 4. BO 生命周期

### 4.1 创建（`bo.c:116-219`，`mwv207_drm_bo_create`）

```
heap = mwv207_get_heap_index(domain, flag)                        bo.c:125（winsys.h:101-109）
max_slab_entry_size = 1 << (min_order + num_orders - 1)           bo.c:127-128（min_order=9：bo.c:561）
（A）SUBALLOC 且 size <= max_slab_entry_size：                     bo.c:130
      heap 越界 → goto no_slab                                    bo.c:132-133
      entry = pb_slab_alloc(slabs, size, heap)                     bo.c:136
      失败 → mwv207_clean_up_buffer_managers(ws)（pb_slabs_reclaim+pb_cache_release_all_buffers bo.c:46-52）
             再试一次                                             bo.c:137-142
      仍失败 → return NULL                                        bo.c:143-144
      bo = container_of(entry, struct mwv207_bo, u.slab.entry)     bo.c:146
      pipe_reference_init(count=1)                                bo.c:148
      return bo（字段由 slab 分配时预设，见 4.4）                   bo.c:153
（B）REUSE：pb_cache_reclaim_buffer(&bo_cache, size, 0x1000, 0, heap) 命中即返回   bo.c:157-162
（C）新建：CALLOC_STRUCT(mwv207_bo)（无 NULL 检查，见 §12 W6）      bo.c:164
      args.in.size = size; alignment = 0x1000                     bo.c:169-170
      preferred_domain：VRAM→0x2 / SYSTEM→0x1 / GTT→0x4            bo.c:172-177
      flags：CPU_ACCESS→0x1 / NO_CPU_ACCESS→0x2                    bo.c:179-182
      2D_ACCESS → alignment = 0x10000                             bo.c:184-185
      ioctl 0x1 = DRM_IOCTL_MWV207_GEM_CREATE                     bo.c:187
      失败 → fprintf(stderr,...) + FREE(bo) + NULL                 bo.c:188-193
      pb_cache_init_entry(&bo_cache, &u.real.cache_entry, &base.base, heap)  bo.c:195
      base.base.placement = args.in.alignment（回读用户入参，非内核回填）    bo.c:197
      base.base.usage=0; size=size; base.vtbl = &mwv207_winsys_bo_vtbl      bo.c:198-200
      handle/name/offset/u.slab.real/logical/ws 初始化                      bo.c:202-207
      use_reusable_pool = (flag & REUSE) ? true : false                     bo.c:208-211
      pipe_reference_init(count=1)                                          bo.c:213
```

### 4.2 映射 / 解除映射

| 步骤 | 实现 | 备注 |
|------|------|------|
| 幂等短路 | `if (bo->logical) return bo->logical;`（`bo.c:276-277`） | 已映射时不重复 mmap，但**返回的是 `bo->logical`（含 offset），与首次调用的返回语义不同** |
| mmap ioctl | `drmCommandWriteRead(fd, 0x2, &args, sizeof(args))`（`bo.c:282`）；失败仅 printf 继续（`bo.c:283-284`，W7） | 0x2 = `DRM_MWV207_GEM_MMAP`（`drm.h:199`） |
| 真正 mmap | `os_mmap(0, bo->base.base.size, PROT_READ|PROT_WRITE, MAP_SHARED, ws->fd, args.out.offset)`（`bo.c:286`） | 长度用**整个父 BO size**，即使这是 slab 子 BO |
| 失败判定 | `ptr == MAP_FAILED → NULL`（`bo.c:288-289`） | |
| 记账 | `bo->logical = (char*)ptr + bo->offset`（`bo.c:291`） | |
| 返回值 | `return ptr;`（`bo.c:293`） | **返回父块基址，不含 `bo->offset`**：对 `offset != 0` 的 slab 子 BO，调用方直接使用返回值会指向错误数据；正确指针在 `bo->logical`（`bo.c:291`） |
| unmap | `mwv207_drm_bo_unmap` 函数体为空（`bo.c:296-300`） | 真正 `drmUnmap` 只在销毁时发生（`bo.c:63-64`） |

### 4.3 引用计数与销毁

| 操作 | 实现 | 行 |
|------|------|----|
| 引用转移 | `mwv207_drm_bo_reference` → `pb_reference((struct pb_buffer**)dst,(struct pb_buffer*)src)` | `bo.c:260-267` |
| 描述符级引用 | 导入命中/新建命中时 `p_atomic_inc(&bo->base.base.reference.count)`（**直接操作原子计数，不走 pb_reference**） | `bo.c:355`、`bo.c:378` |
| 真实 BO 销毁 vtbl | `mwv207_bo_destroy_or_cache`：`use_reusable_pool ? pb_cache_add_buffer : mwv207_bo_destroy` | `bo.c:100-109`，vtbl `bo.c:111-114` |
| slab 子 BO 销毁 vtbl | `mwv207_bo_slab_destroy` → `pb_slab_free(&bo->ws->bo_slabs, &bo->u.slab.entry)` | `bo.c:87-93`，vtbl `bo.c:95-98` |
| 销毁主体 | `mwv207_drm_bo_destory`：debug 打印（`60-61`）→ `drmUnmap(logical, size)`（`63-64`）→ 持锁从 `bo_name_table`（仅当 `bo->name`）与 `bo_export_table` 摘除（`66-70`）→ `DRM_IOCTL_GEM_CLOSE`（`72-74`）→ `FREE(bo)`（`76`） | `bo.c:54-77` |
| 可回收判定 | `mwv207_bo_can_reclaim`：`reference.count != 0 → false`；否则 `bo_wait(ws,bo,OS_TIMEOUT_INFINITE)` | `bo.c:436-446` |
| slab 可回收 | `mwv207_bo_can_reclaim_slab`：同上（用 `container_of` 取回 BO） | `bo.c:448-460` |

### 4.4 slab 子分配（`bo.c:462-539`）

- `mwv207_bo_slab_alloc(priv=ws, heap, entry_size, group_index)`：`CALLOC_STRUCT(mwv207_slab)`（`468`）；由 heap 反推 domain/flags（`469-470`，用 `winsys.h:73-99` 的助手）；`slab_size = max_entry_size * 2`（`477-482`，`max_entry_size = 1<<(min_order+num_orders-1)`）；`assert(slab_size != 0)`（`483`）；`slab->buffer = ws->bo_create(ws, slab_size, domains, flags)`（`485`）；`ws->bo_map(ws, slab->buffer)`（`489`）；`num_entries = slab->buffer->base.base.size / entry_size`（`491`）；`entries = CALLOC(num_entries, sizeof(*entries))`（`493`）。
- 每个子 BO（`499-519`）：`base.base.placement = entry_size`、`size = entry_size`、`vtbl = mwv207_winsys_bo_slab_vtbl`（`501-504`）；`handle = slab->buffer->handle`（**父子共用一个 GEM handle**，`506`）；`offset = i * entry_size`（`509`）；`logical = parent->logical + offset`（`510`）；`ws`（`511`）；`u.slab.entry.slab`（`514`）；`u.slab.real = slab->buffer`（`516`）；`list_addtail(...,&slab->base.free)`（`518`）。
- 注意：子 BO 的 `pipe_reference` **未在此处初始化**，依赖 `pb_slab_alloc` 返回后由 `bo.c:148` 的 `pipe_reference_init` 完成。
- `mwv207_bo_slab_free`：`FREE(slab->entries)` → `mwv207_drm_bo_destory(ws, slab->buffer)` → `FREE(slab)`（`530-539`）。
- 失败路径：`fail_buffer: mwv207_drm_bo_destory(ws, slab->buffer)`，`fail: FREE(slab)`（`523-527`）。

---

## 5. 哈希表键的坑（导出表 / flink 表）

**表的哈希与比较函数是「解引用」型**：

```c
/* winsys.c:107-117 */
static uint32_t hash_uint32_t(const void *key) { return *(uint32_t *)key; }
static bool handle_equals(const void *key1, const void *key2)
{ return (*(uint32_t *)key1 == *(uint32_t *)key2); }
```

两张表都用它创建：`winsys.c:129-132`。因此 key 是 **`uint32_t*`（指向键值的指针）**，而不是键值本身。由此产生 4 个必须遵守的约束（当前代码均满足，但很脆弱）：

| 约束 | 当前实现 | 行 |
|------|----------|----|
| 插入的 key 必须指向**长期存活**的内存，不能是栈变量 | 插入一律用 BO 内字段地址 `&bo->handle` / `&bo->name` | `bo.c:327`、`bo.c:328`、`bo.c:398`、`bo.c:400` |
| 删除必须用**同一地址**（同值不同地址在 `_mesa_hash_table_remove_key` 下按值比较可删除，但更稳的是同地址） | `&bo->handle` / `&bo->name` | `bo.c:68`、`bo.c:69` |
| 删除必须在 BO 还活着时做（key 指向 BO 字段，BO 一旦 free，桶内 key 悬垂） | 在 `mwv207_drm_bo_destory` 内、`FREE(bo)` 之前 | `bo.c:66-70` vs `bo.c:76` |
| 键值在插入后不得变化 | `handle` 只在创建/导入时写一次；`name` 导入后不再改 | `bo.c:202`、`bo.c:322`、`bo.c:388`、`bo.c:397` |

额外坑：

1. **查询用栈变量是合法且必要的**：`from_handle` 用 `&name`（`bo.c:352`）和 `&handle`（`bo.c:375`）做查找，因为查询只做比较，不保存指针。但**绝不能把栈地址用于 insert**——那会让表里留下悬垂 key。
2. **`_mesa_hash_table_insert` 按值比较等于覆盖**：既然 `handle_equals` 按值比较，两个 BO 若拿到相同 `handle` 值插入同一张表，`insert` 会覆盖前一个桶（键指针和 value 都换成后者）。此时先销毁的那一个 `remove_key(&先者->handle)` 仍会按值删掉这个桶 → **后者的表项丢失**（这正是报告 W4 描述的「重复 BO 包装/导出表无限增长」的机理面）。
3. 两张表语义不同：`bo_export_table` 键是**进程内 GEM handle**（`bo.c:328,375,400,69`）；`bo_name_table` 键是 **flink 全局名**（`bo.c:327,352,398,68`）。销毁时 `bo_name_table` 仅在 `bo->name` 非 0 时摘除（`bo.c:67`）。
4. 表中 value 是 `mwv207_bo*`，**insert 不增加引用计数**；命中后由调用方手工 `p_atomic_inc`（`bo.c:355,378`）。因此若命中者不增加引用，BO 可能在他人仍持指针时被释放 —— 当前两处命中都做了 inc。
5. **表与 fd 生命周期**：表属于 winsys（每个 drm fd 一份，`winsys.c:269` 按 `ws->fd` 入全局表），随 winsys 销毁而销毁（`winsys.c:170-171`）。

---

## 6. CS（命令提交）通路

### 6.1 fence 对象

```c
/* cs.c:40-43 */
struct pipe_fence_handle { struct pipe_reference reference; int fd; };
```

| 函数 | 签名 | 行为 / 错误处理 | 行 |
|------|------|-----------------|----|
| 创建 | `static struct pipe_fence_handle *mwv207_drm_fence_create(struct mwv207_winsys *ws, int fd)` | `CALLOC_STRUCT`；失败 `debug_printf("CALLOC_STRUCT failed\n")` 并返回 NULL；否则 `fence->fd=fd`、引用计数=1 | `cs.c:53-67` |
| 销毁 | `static void mwv207_drm_destroy_fence(struct pipe_fence_handle *fence)` | `fd >= 0 → close(fd)`；`FREE` | `cs.c:45-51` |
| 引用 | `static void mwv207_drm_fence_reference(struct pipe_fence_handle **dst, struct pipe_fence_handle *src)` | 双向 NULL 早退；`pipe_reference(old,src)` 返回真则销毁 old；**随后 `*dst = src` 无条件执行**（即使 old==src，先销毁再赋值会产生自赋值悬垂，属边界风险） | `cs.c:69-82` |
| 等待 | `static int mwv207_drm_fence_wait(struct mwv207_winsys *ws, struct pipe_fence_handle *fence, uint64_t timeout)` | `fd < 0 → true`（视为已完成）；`OS_TIMEOUT_INFINITE → -1`（`sync_wait` 永久等待）；否则 `timeout/1e6`，为负则夹到 `INT_MAX`；返回 `!sync_wait(...)` | `cs.c:84-104` |
| 取 fd | `static int mwv207_drm_fence_get_fd(struct mwv207_winsys *ws, struct pipe_fence_handle *fence)` | `return dup(fence->fd);` —— **对 `fd < 0` 也直接 dup**（失败返回 -1 并置 EBADF） | `cs.c:106-111` |
| 合并 | `static void mwv207_drm_fence_accumulate(struct mwv207_winsys *ws, int *target_fd, struct pipe_fence_handle *fence)` | `fd >= 0 → sync_accumulate("mwv207", target_fd, fence->fd)` | `cs.c:113-120` |

`ws` 参数在等待/取 fd/合并三个函数中**未被使用**（仅签名占位）。

### 6.2 `struct mwv207_cmd` 的建立与增长（`cs.c:122-150`）

```c
cmd = CALLOC_STRUCT(mwv207_cmd);                       cs.c:125
cmd->ws   = ws;                                        cs.c:127
cmd->buf  = (uint8_t *)MALLOC(0x8000); assert(cmd->buf);  cs.c:129-130
cmd->size = 0x8000; cmd->offset = 0; cmd->stamp = NULL;  cs.c:132-134
cmd->nr_relocs = 0; cmd->nr_max_relocs = 0x80;
cmd->relocs = CALLOC(0x80, sizeof(struct drm_mwv207_submit_reloc));  cs.c:136-138
cmd->nr_bos = 0; cmd->nr_max_bos = 0x80;
cmd->mwv207_bos = CALLOC(0x80, sizeof(struct mwv207_bo *));          cs.c:140-142
cmd->bos = CALLOC(0x80, sizeof(struct drm_mwv207_bo_acc));           cs.c:143
cmd->mwv207_dats.size = 0; cmd->mwv207_dats.nr_relocs = 0; cmd->nr_dats = 0;  cs.c:145-147
```

**分配全部依赖 `assert`，release 构建下失败即崩溃（报告 W12，当前树仍未修）**。

| 增长对象 | 机制 | 行 |
|----------|------|----|
| 命令缓冲 `buf`（0x8000=32 KiB） | **winsys 内部不增长**；由驱动层节流 + 强制提交：`D/cmd.c:340-352 check_cmd()` 要求 `size >= offset+4096` 且 `nr_bos<=64`、`nr_relocs<=128`、`dats.nr_relocs<=128`；不满足即 `D/cmd.c:354-369 mwv207_submit()` 提交并复位 | `D/cmd.c:340-372` |
| `relocs[]` | `nr_relocs >= nr_max_relocs` → `nr_max_relocs *= 2`，`realloc` 到**临时变量**再赋值，失败返回 `-PIPE_ERROR_OUT_OF_MEMORY` | `cs.c:281-290` |
| `mwv207_bos[]` / `bos[]` | `i >= nr_max_bos` → `nr_max_bos *= 2`，两次 `realloc` 分别到 `new_bos`/`new_accs`，任一失败返回 `-PIPE_ERROR_OUT_OF_MEMORY` | `cs.c:190-206` |
| `mwv207_dats.data` / `relocs` | 首次调用时惰性建立（`max_size=0x8000`、`nr_max_relocs=0x80`，`cs.c:223-236`）；随后 `while (size+src > max)` 与 `while (nr+nr >= max)` **循环**扩容 | `cs.c:238-257` |

### 6.3 BO 入表与去重（`cs.c:178-216`，`mwv207_drm_cmdbuf_add_bo`）

- 线性扫描已有表：同指针 → `cmd->bos[i].flags |= flags; return i;`（`cs.c:183-188`）。
- 新条目：**把 flags 存在 per-cmdbuf 的 `cmd->bos[i].flags`**（`cs.c:210`，W8 修复点），`mwv207_bos[i]` 置 NULL 后经 `ws->bo_reference` 取引用（`cs.c:211-212`），`nr_bos = i + 1`（`cs.c:213`）。
- 语义：返回下标即 KMD `drm_mwv207_bo_acc` 表中的 BO 下标，也是 reloc 的 `idx`。
- 去重是**按指针**，故满足 KMD 头注释「bo table should not contain duplicates」（`KMD drm.h:72`）。

### 6.4 dat 段与 reloc（`cs.c:218-300`）

`mwv207_drm_cmdbuf_add_dat(cmd, src_dat)`（`cs.c:218-271`）：

1. 首次建立 `mwv207_dats`（`cs.c:223-236`）。
2. 循环扩容 data/relocs（`cs.c:238-257`）。
3. 逐个迁移 reloc（`cs.c:259-264`）：取 `bo = src_dat->bos[src_dat->relocs[i].idx]`；`dest->relocs[nr+i] = src->relocs[i]`；`cmd_offset += dest_dat->size`（把「dat 段内偏移」换算成「拼接后 dat 缓冲内偏移」）；`idx = mwv207_drm_cmdbuf_add_bo(cmd, bo, 0)`（**flags=0**，把 dat 引用的 BO 也纳入 submit 的 bos 表，下标接在已有 BO 之后）。
4. `memcpy(dest->data + dest->size, src->data, src->size)`；**`dest_dat->size += 0x100`**（`cs.c:268`）并 `return size - 0x100`（`cs.c:270`）。
   → 每个 dat 段**硬编码占用 256 B**。`struct cmd_dat` 的 `data[64]` 正好 256 B（`winsys.h:145`），故只要调用方让 `src_dat->size == 256` 两者自洽；若某个 dat 的 `size` 不是 256，拼接布局与 `cmd_offset` 记账就会错位（**潜在缺陷，不在 W 清单**；本次未检索全部 `cmd_dat` 构造点是否恒为 256 —— 见 §13）。

`mwv207_drm_cmdbuf_add_relloc(cmd, idx, cmd_offset, bo_offset, type, pad)`（`cs.c:273-300`）：扩容后逐字段写入并 `nr_relocs++`，返回 `PIPE_OK`（0）。**没有对 `idx` 做合法性检查**（越界 idx 会由 KMD 在 patch 阶段拒绝，见 §9.3）。

reloc 类型（驱动层实际使用）：

| 使用点 | idx | cmd_offset | bo_offset | type | pad |
|--------|-----|-----------|-----------|------|-----|
| `D/cmd.c:221-234`（普通 BO 引用） | `cmdbuf_add_bo` 返回 | 相对 cmd buf 的字节偏移 | 用户 offset | `0x1`（LOW32） | 0 |
| `D/cmd.c:245-252`（dat 段地址回填） | **0** | cmd buf 内该寄存器字的位置 | `cmdbuf_add_dat` 返回值 | `0x1 \| 0x80000000`（指向 cmd_dat[0] 的 BO） | 0 |
| `D/cmd.c:269-270`（DMA 提交，src） | 0 | 0 | 0 | `0x0`（WORD64） | 0 |
| `D/cmd.c:286-288`（DMA 提交，dst） | 1 | `4*sizeof(uint64_t)=32` | 0 | `0x0` | 0 |
| `D/vertex.c:194,239` 等 | | | | `0x1` / 变量 | pad |

### 6.5 提交：`mwv207_drm_cmdbuf_commit`（`cs.c:371-442`）

```c
struct drm_mwv207_submit submit = {0};                       cs.c:378
if (debug & (0x8|0x10)) mwv207_drm_cmdbuf_print(ws, cmd);    cs.c:381-382   /* 0x8 cmd, 0x10 size */
submit.ctx         = ws->ctx;                                cs.c:384
submit.engine_type = cmd->engine_type;                       cs.c:385
submit.engine_id   = 0;                                     cs.c:386   /* 恒 0，永不使用 0xffffffff */
if (in_fence_fd >= 0) { submit.flags |= 0x1; submit.fence_fd = in_fence_fd; }  cs.c:387-390
if (out_fence_fd)     { submit.flags |= 0x2; }               cs.c:391-392
submit.cmd_size  = cmd->offset;                              cs.c:393
submit.nr_relocs = cmd->nr_relocs;                           cs.c:394
submit.nr_bos    = cmd->nr_bos;                              cs.c:395
submit.relocs    = (__u64)cmd->relocs;                       cs.c:396
submit.cmds      = (__u64)cmd->buf;                          cs.c:397
for (i=0;i<nr_bos;i++) cmd->bos[i].handle = cmd->mwv207_bos[i]->handle;  cs.c:399-402
submit.bos       = (__u64)cmd->bos;                          cs.c:403
submit.nr_cmd_dat = cmd->mwv207_dats.size ? cmd->nr_dats : 0; cs.c:405-408
cmd->dats.nr_relocs = cmd->mwv207_dats.nr_relocs;            cs.c:410
cmd->dats.dat_size  = cmd->mwv207_dats.size;                 cs.c:411
cmd->dats.relocs    = (__u64)cmd->mwv207_dats.relocs;        cs.c:412
cmd->dats.dat       = (__u64)cmd->mwv207_dats.data;          cs.c:413
submit.cmd_dats     = (__u64)&cmd->dats;                     cs.c:414
err = drmCommandWriteRead(ws->fd, 0x5, &submit, sizeof(submit));   cs.c:416  /* DRM_MWV207_SUBMIT */
while (err == -EBUSY) { usleep(1000); 重发同一 submit; printf("retry submit\n"); }  cs.c:418-423
if (err) { printf("%s ret : %d\n",...); mwv207_drm_context_print(cmd); }            cs.c:425-428
if (!err && out_fence_fd) *out_fence_fd = submit.fence_fd;   cs.c:430-431  /* 内核回填的 out fence fd */
/* 无论成败，全部释放并按需复位 */                          cs.c:433-440
for (i...) ws->bo_reference(ws, &cmd->mwv207_bos[i], NULL);  cs.c:433-434
cmd->nr_bos = 0; cmd->nr_relocs = 0;                         cs.c:436-437
cmd->mwv207_dats.size = 0; cmd->mwv207_dats.nr_relocs = 0;   cs.c:438-439
cmd->offset = 0;                                             cs.c:440
```

提交语义要点 / 风险：

| # | 事实 | 行 |
|---|------|----|
| C1 | **`engine_id` 恒为 0**：UMD 从不使用 `MWV207_SUBMIT_ENGINE_ID_ANY = 0xffffffff`（`drm.h:103`），故 KMD 的「any 专属 entity」分支（`K/submit.c:62-63`）对本 UMD 不可达 | `cs.c:386` |
| C2 | in-fence 与 out-fence **共用同一个 `fence_fd` 字段**，两者可同时置位（`flags = 0x3`）；KMD 先用入参 `fence_fd` 取入 fence（`K/submit.c:566-570`），再在 commit 阶段把新分配的 out fd 写回同一字段（`K/submit.c:587-601`） | `cs.c:387-392`、`K/submit.c:566-601` |
| C3 | `-EBUSY` 无限重试（1 ms 间隔）且每次 `printf`；若 KMD 在某次尝试中已 `get_unused_fd_flags()+fd_install` 了 out fence（`K/submit.c:591-599`）后才失败返回，**重试会再分配一个 fd，前一个 fd 泄漏**（KMD 侧失败路径未关闭已安装的 fd） | `cs.c:418-423`、`K/submit.c:591-599` |
| C4 | 出错时 `*out_fence_fd` 不被写（保持调用方初值 -1）：`D/cmd.c:355-362` 与 `D/context.c:93-94` 随后用 -1 造 fence，该 fence 的 wait 恒返回真（`cs.c:89-101`）——即**提交失败会被上层静默当作已完成** | `cs.c:430-431`、`D/context.c:93-94` |
| C5 | 提交后无条件清空 `nr_bos/nr_relocs/dats/offset`，命令缓冲 `buf`/`relocs`/`bos` 复用（不 free） | `cs.c:433-440` |
| C6 | 只回填 `cmd->bos[i].handle`，**不动 flags**（flags 已由 `add_bo` 维护） | `cs.c:399-402` |
| C7 | `cmd->dats` 是内嵌在 `cmd` 中的单元素结构（`winsys.h:181`），`nr_cmd_dat` 恒为 0 或 1（`cs.c:405-408`）——KMD ABI 允许数组，UMD 只用 1 项 | `cs.c:405-414` |

### 6.6 调试打印（`cs.c:302-369`）

| 函数 | 行 | 行为 |
|------|----|------|
| `mwv207_drm_cmdbuf_print(ws, cmd)` | `cs.c:302-341` | 一次性读取 `mwv207_count` 选项（top5/10/20/50，默认 100000，`cs.c:311-326`）；计数为 0 直接返回；打印 `size` 后**若 `debug & 0x10` 则 return（只打印大小）**；否则按 4 字节逐行打印 |
| `mwv207_print_dat(dat)` | `cs.c:343-351` | 打印每个 reloc 与全部 dat 字 |
| `mwv207_drm_context_print(cmd)` | `cs.c:353-369` | 打印 BO 表（handle/size）、reloc 表（`type & 0x80000000` 判 dat/reloc）、再转 `print_dat` |

### 6.7 初始化（`cs.c:444-459`）

`mwv207_cs_init_function(ws)` 一次性装配 11 个函数指针：`fence_create/fence_reference/fence_wait/fence_get_fd/fence_accumulate`（`447-451`）、`cmdbuf_create/destory/add_bo/add_dat/add_relloc/commit`（`453-458`）。调用点 `winsys.c:254`。

`cs.h:35` 声明了 `void mwv207_cs_destory(struct mwv207_winsys *ws);`，但**全仓库只有该声明、没有定义**（`grep` 仅命中 `cs.h:35`），`mwv207_cs_init_function` 也未装配对应指针 —— 属死声明。

---

## 7. 对外函数逐条说明

### 7.1 非 static（跨编译单元可见）

| 签名 | 行 | 行为 | 错误处理 | 被谁调用 |
|------|----|------|----------|----------|
| `struct pipe_screen *mwv207_drm_create_screen(int fd)` | `winsys.c:208-275`；声明 `public.h:34-35` | 以 fd 为 key 在全局表 `mwv207_ws_tab` 中查/建 winsys，返回 `ws->screen` | `util_hash_table_create_fd_keys()` 失败→NULL（`216-221`）；`CALLOC` 失败→NULL（`233-236`）；`fcntl` 失败→stderr+`FREE`+NULL（`238-244`）；`mwv207_ws_init` 失败→close+FREE+NULL（`246-251`）；`mwv207_create_screen` 失败→close+FREE+NULL 且**不入表**（`256-263`）；命中旧 ws 时仅 `pipe_reference(NULL,&ws->reference)` 并返回其 screen（`224-229`） | `src/gallium/auxiliary/target-helpers/drm_helper.h:126` |
| `void mwv207_bo_init_function(struct mwv207_winsys *ws)` | `bo.c:541-574`；声明 `bo.h:31` | 装配 8 个 BO 函数指针（`543-550`）、初始化 `bo_cache`（`552-555`）与 `bo_slabs`（`565-571`，min_order=9、max_order=18、每分配器 3 个 order，`557-563`） | `pb_slabs_init` 失败仅 `printf("pb_slabs_init faile\n")` 不返回错误（`572`）；`assert` 与 `CALLOC` 失败由 pb 层承担 | `winsys.c:253` |
| `void mwv207_cs_init_function(struct mwv207_winsys *ws)` | `cs.c:444-459`；声明 `cs.h:33` | 装配 11 个 fence/cmdbuf 函数指针 | 无错误路径 | `winsys.c:254` |
| `void mwv207_cs_destory(struct mwv207_winsys *ws)` | 仅声明 `cs.h:35` | **未定义**（死声明） | — | 无 |
| `int mwv207_debug`（变量） | `winsys.c:56` | 全局调试位（`static` 缺失 → 外部链接） | — | 由 `debug_get_option_mwv207_debug()` 读取（`winsys.c:125`）；`winsys.c` 内无其他读写点 |

### 7.2 static（经 `ws->` 指针暴露，等价对外 API）

| ws 成员 | 实现签名 | 行 | 行为 | 错误处理 |
|---------|----------|----|------|----------|
| `ws->destroy` | `static bool mwv207_drm_destroy(struct mwv207_winsys *ws)` | `winsys.c:150-182` | 持全局 mutex，`pipe_reference(&ws->reference,NULL)`；归零才真正销毁：CTX destroy ioctl（`159-163`，失败被 `(void)` 忽略）→ slabs/cache deinit（`165-167`）→ 锁/表销毁（`169-171`）→ 从全局表摘除（`173`）→ `close(fd)`+`FREE`（`175-176`） | 返回「是否已销毁」；CTX destroy 失败静默忽略 |
| `ws->bo_create` | `static struct mwv207_bo *mwv207_drm_bo_create(struct mwv207_winsys *ws, uint64_t size, enum mwv207_bo_domain domain, enum mwv207_bo_flag flag)` | `bo.c:116-219` | 见 §4.1 | slab 两次尝试后 NULL；缓存未命中则新建；ioctl 失败 stderr+NULL；**`CALLOC_STRUCT` 无 NULL 检查（W6）** |
| `ws->bo_wait` | `static int mwv207_drm_bo_wait(struct mwv207_winsys *ws, struct mwv207_bo *bo, uint64_t timeout)` | `bo.c:237-258` | `drm_mwv207_gem_wait{handle, op=0x2, timeout=绝对 ns}`（`245-247`）→ `drmCommandWrite(fd, 0x3, ...)`（`249`） | 失败：`-EBUSY` 静默，其余 printf，**返回 `false`**（成功返回 `true`）；返回 int 但取值只有 0/1（与 `ws->fence_wait` 的 1/0 语义一致） |
| `ws->bo_reference` | `static void mwv207_drm_bo_reference(struct mwv207_winsys *ws, struct mwv207_bo **dst, struct mwv207_bo *src)` | `bo.c:260-267` | `pb_reference` | — |
| `ws->bo_map` | `static void *mwv207_drm_bo_map(struct mwv207_winsys *ws, struct mwv207_bo *bo)` | `bo.c:269-294` | 见 §4.2 | mmap ioctl 失败仅 printf 继续（**W7**）；`os_mmap` 失败→NULL |
| `ws->bo_unmap` | `static void mwv207_drm_bo_unmap(struct mwv207_winsys *ws, struct mwv207_bo *bo)` | `bo.c:296-300` | 空实现 | — |
| `ws->bo_get_handle` | `static uint32_t mwv207_drm_bo_get_handle(struct mwv207_winsys *ws, struct mwv207_bo *bo, struct winsys_handle *whandle)` | `bo.c:302-335` | KMS：`whandle->handle = bo->handle`（`308`）；FD：`drmPrimeHandleToFD(fd, handle, DRM_RDWR\|DRM_CLOEXEC, &bo->fd)`（`310`）；SHARED：`DRM_IOCTL_GEM_FLINK`→`whandle->handle`/`bo->name`（`312-322`）；然后登记入表（`325-329`） | `GEM_FLINK` 失败返回其正值/负值（`319-320`，**未取反也未清理**）；其余成功返回 0 |
| `ws->bo_from_handle` | `static struct mwv207_bo *mwv207_drm_bo_from_handle(struct mwv207_winsys *ws, struct winsys_handle *whandle, uint32_t size)` | `bo.c:337-409` | SHARED：先查 `bo_name_table`（`352-356`）→ 未命中 `DRM_IOCTL_GEM_OPEN`（`359-365`）；FD：`drmPrimeFDToHandle`（`366-373`）；再按 handle 查 `bo_export_table`（`375-380`）命中则 `p_atomic_inc` 后返回；否则新建包装（`382-400`） | **`CALLOC_STRUCT` 无 NULL 检查**（`382-383`，W6）；`GEM_OPEN`/`drmPrimeFDToHandle` 失败 printf 后落到 `unlock` 并 `return bo`（此时 `bo` 仍为 NULL，除非前面赋值）；`size` 由调用方给（`D/resource.c:301` 传 `res->size`），**不校验与真实 BO 大小是否一致** |
| `ws->bo_dump` | `static void mwv207_drm_bo_dump(struct mwv207_winsys *ws, struct mwv207_bo *bo, uint32_t size)` | `bo.c:411-434` | 按 4 字节打印 BO 内容（`size==0` 或超界则用整 BO），最后 `bo_unmap` | 未做 NULL 判定 |
| `ws->fence_create` | `static struct pipe_fence_handle *mwv207_drm_fence_create(struct mwv207_winsys *ws, int fd)` | `cs.c:53-67` | 见 §6.1 | 分配失败 debug_printf+NULL |
| `ws->fence_reference` | `static void mwv207_drm_fence_reference(struct pipe_fence_handle **dst, struct pipe_fence_handle *src)` | `cs.c:69-82` | 见 §6.1 | — |
| `ws->fence_wait` | `static int mwv207_drm_fence_wait(struct mwv207_winsys *ws, struct pipe_fence_handle *fence, uint64_t timeout)` | `cs.c:84-104` | 见 §6.1 | fd<0 视为完成 |
| `ws->fence_get_fd` | `static int mwv207_drm_fence_get_fd(struct mwv207_winsys *ws, struct pipe_fence_handle *fence)` | `cs.c:106-111` | `dup` | 未判 `fd<0` |
| `ws->fence_accumulate` | `static void mwv207_drm_fence_accumulate(struct mwv207_winsys *ws, int *target_fd, struct pipe_fence_handle *fence)` | `cs.c:113-120` | `sync_accumulate` | — |
| `ws->cmdbuf_create` | `static struct mwv207_cmd *mwv207_drm_cmdbuf_create(struct mwv207_winsys *ws)` | `cs.c:122-150` | 见 §6.2 | **仅 assert（W12）** |
| `ws->cmdbuf_destory` | `static void mwv207_drm_cmdbuf_destory(struct mwv207_winsys *ws, struct mwv207_cmd *cmd)` | `cs.c:152-176` | 先对 `nr_bos` 个 BO 做 `bo_reference(...,NULL)`（`155-156`），再 FREE `buf`/`relocs`/`mwv207_bos`/`bos`（`158-168`），`nr_dats` 非 0 时 FREE dats 的 data/relocs（`170-173`），最后 FREE cmd（`175`） | 未判 NULL |
| `ws->cmdbuf_add_bo` | `static int mwv207_drm_cmdbuf_add_bo(struct mwv207_cmd *cmd, struct mwv207_bo *bo, uint32_t flags)` | `cs.c:178-216` | 见 §6.3 | 扩容失败返回 `-PIPE_ERROR_OUT_OF_MEMORY` 且**不改变 `nr_max_bos` 语义正确**（先减半？否：先 `*=2` 再 realloc，失败时 max 已翻倍但指针未变，数组仍可容纳旧数量，不越界） |
| `ws->cmdbuf_add_dat` | `static int mwv207_drm_cmdbuf_add_dat(struct mwv207_cmd *cmd, struct cmd_dat *src_dat)` | `cs.c:218-271` | 见 §6.4 | 分配失败返回 `-PIPE_ERROR_OUT_OF_MEMORY`；`0x100` 硬编码见 §6.4 |
| `ws->cmdbuf_add_relloc` | `static int mwv207_drm_cmdbuf_add_relloc(struct mwv207_cmd *cmd, uint32_t idx, uint32_t cmd_offset, uint32_t bo_offset, uint32_t type, uint32_t pad)` | `cs.c:273-300` | 见 §6.4 | 扩容失败返回 `-PIPE_ERROR_OUT_OF_MEMORY`；成功 `PIPE_OK` |
| `ws->cmdbuf_commit` | `static void mwv207_drm_cmdbuf_commit(struct mwv207_winsys *ws, struct mwv207_cmd *cmd, int in_fence_fd, int *out_fence_fd)` | `cs.c:371-442` | 见 §6.5 | 无返回值；失败仅 printf；`-EBUSY` 无限重试 |
| 内部 | `static void mwv207_clean_up_buffer_managers(struct mwv207_winsys *ws)` | `bo.c:46-52` | `pb_slabs_reclaim` + `pb_cache_release_all_buffers` | 调用点 `bo.c:139` |
| 内部 | `static int64_t get_abs_timeout(uint64_t timeout)` | `bo.c:221-235` | `timeout > INT64_MAX → INT64_MAX`；加 `os_time_get_nano()`；溢出 → `INT64_MAX` | 调用点 `bo.c:247` |
| 内部 | `static struct pb_slab *mwv207_bo_slab_alloc(void *priv, unsigned heap, unsigned entry_size, unsigned group_index)` | `bo.c:462-528` | 见 §4.4 | `bo_create`/`CALLOC` 失败走 fail 标签释放 |
| 内部 | `static void mwv207_bo_slab_free(void *priv, struct pb_slab *pslab)` | `bo.c:530-539` | 见 §4.4 | — |
| 内部 | `static bool mwv207_bo_can_reclaim(void *winsys, struct pb_buffer_lean *buf)` | `bo.c:436-446` | 引用计数为 0 且 `bo_wait` 成功才可回收 | — |
| 内部 | `static bool mwv207_bo_can_reclaim_slab(void *priv, struct pb_slab_entry *entry)` | `bo.c:448-460` | 同上 | — |
| 内部 | `static void mwv207_bo_destroy_or_cache(void *winsys, struct pb_buffer *buf)` | `bo.c:100-109` | 见 §4.3 | — |
| 内部 | `static void mwv207_bo_destroy(void *winsys, struct pb_buffer_lean *buf)` | `bo.c:79-85` | 转 `mwv207_drm_bo_destory` | — |
| 内部 | `static void mwv207_bo_slab_destroy(void *winsys, struct pb_buffer *buf)` | `bo.c:87-93` | `pb_slab_free` | — |
| 内部 | `static void mwv207_cluster_init(struct mwv207_winsys *ws)` | `winsys.c:75-105` | 从 `cluster_mask=0xf` 数出 count/min/max（`78-97`），count<1 则置 1（`99-100`），两条 assert（`102-103`） | 调用点 `winsys.c:127` |
| 内部 | `static bool mwv207_ws_init(struct mwv207_winsys *ws)` | `winsys.c:119-148` | debug、cluster、建表/锁、CTX create ioctl 0x4/op=0（`136-138`）、`ws->ctx = args.handle`（`146`） | 失败 stderr + 返回 false（**W1 修复点**） |
| 内部 | `static uint32_t hash_fd(void *key)` | `winsys.c:184-192` | `fstat(fd)` 后 `st_dev ^ st_ino ^ st_rdev` | 供 `util_hash_table_create_fd_keys()` 内部使用（本文件不直接引用） |
| 内部 | `static int compare_fd(void *key1, void *key2)` | `winsys.c:194-206` | 同上比较三元组 | 同上；**未检查 fstat 返回值**（fd 已关闭时读到栈上垃圾） |

---

## 8. ioctl ABI 表

### 8.1 命令号（UMD 侧定义 `drm.h:197-209` ↔ KMD 侧定义 `K/drm.h:150-155`）

| 名称 | 命令码 | 请求结构体 | `sizeof` | `_IOWR` 完整号（实测） | KMD 处理函数 | KMD flags |
|------|--------|-----------|----------|------------------------|--------------|-----------|
| `DRM_MWV207_INFO` | `0x00` | `struct drm_mwv207_info` | 16 | `0xc0106440` | `mwv207_db_ioctl`（`K/db.c:108-144`） | `DRM_AUTH\|DRM_RENDER_ALLOW`（`K/drv.c:111`） |
| `DRM_MWV207_GEM_CREATE` | `0x01` | `union drm_mwv207_gem_create` | 24 | `0xc0186441` | `mwv207_gem_create_ioctl`（`K/gem.c:89-116`） | 同上（`K/drv.c:112`） |
| `DRM_MWV207_GEM_MMAP` | `0x02` | `union drm_mwv207_gem_mmap` | 8 | `0xc0086442` | `mwv207_gem_mmap_ioctl`（`K/gem.c:118-134`） | 同上（`K/drv.c:113`） |
| `DRM_MWV207_GEM_WAIT` | `0x03` | `struct drm_mwv207_gem_wait` | 16 | `0xc0106443` | `mwv207_gem_wait_ioctl`（`K/gem.c:136-155`） | 同上（`K/drv.c:114`） |
| `DRM_MWV207_CTX` | `0x04` | `struct drm_mwv207_ctx` | 16 | `0xc0106444` | `mwv207_ctx_ioctl`（`K/ctx.c:193-210`） | 同上（`K/drv.c:115`） |
| `DRM_MWV207_SUBMIT` | `0x05` | `struct drm_mwv207_submit` | 72 | `0xc0486445` | `mwv207_submit_ioctl`（`K/submit.c:612-653`） | 同上（`K/drv.c:116`） |

- 号码由 `DRM_IOWR(DRM_COMMAND_BASE + N, T)` 展开：`DRM_IOCTL_BASE='d'=0x64`、`DRM_COMMAND_BASE=0x40`（`/usr/include/drm/drm.h:1073,1353`）。上表「完整号」与 `sizeof` 是用一段包含该头的 C 程序**实际打印**得出的（非手算），与 UMD `drmCommandWriteRead(..., 0xN, ..., sizeof(...))` 传入的 size 一致（`bo.c:187,249,282`、`winsys.c:138,163`、`cs.c:416`）。
- 方向：两套都用 `DRM_IOWR`（读写双向）。DRM core 在 `_IOC_DIR & _IOC_WRITE` 时 `copy_from_user`、`& _IOC_READ` 时 `copy_to_user`，故 `union` 里的 `in`/`out` 仅是用户态命名习惯，**内核与用户态结构体是同一块内存**。
- `DRM_AUTH|DRM_RENDER_ALLOW`：`DRM_RENDER_ALLOW` 使这些 ioctl 在 render node（`renderD128`）上可用（需要 master 的 ioctl 不在本表）。

### 8.2 UMD 实际调用点（全部使用裸命令码，未用 `DRM_IOCTL_MWV207_*` 宏）

| 调用 | 行 | 命令码 |
|------|----|--------|
| `drmCommandWriteRead(ws->fd, 0x1, &args, sizeof(args))` | `bo.c:187` | GEM_CREATE |
| `drmCommandWriteRead(ws->fd, 0x2, &args, sizeof(args))` | `bo.c:282` | GEM_MMAP |
| `drmCommandWrite(ws->fd, 0x3, &wait, sizeof(wait))` | `bo.c:249` | GEM_WAIT |
| `drmCommandWriteRead(ws->fd, 0x4, &args, sizeof(args))` | `winsys.c:138`、`winsys.c:163` | CTX（op=0 建 / op=1 销） |
| `drmCommandWriteRead(ws->fd, 0x5, &submit, sizeof(submit))` | `cs.c:416`、`cs.c:420` | SUBMIT |
| 另有 `drmIoctl(..., DRM_IOCTL_GEM_CLOSE/FLINK/OPEN, ...)` 与 `drmPrimeHandleToFD/FDToHandle` | `bo.c:74,318,360,310,368` | 标准 GEM/PRIME |

### 8.3 两侧差异（逐条）

| # | 差异 | UMD | KMD |
|---|------|-----|-----|
| A1 | **命令码常量定义** | 有 `DRM_MWV207_INFO=0x00` … `DRM_MWV207_SUBMIT=0x05`（`drm.h:197-202`）与 `DRM_IOCTL_MWV207_*` 宏（`drm.h:204-209`） | **无**任何 `DRM_MWV207_*`/`DRM_IOCTL_MWV207_*` 定义，只把字面量 `0x00..0x05` 直接写进 `DRM_IOWR`（`K/drm.h:150-155`） |
| A2 | **语义常量** | 有 `MWV207_GEM_DOMAIN_*`、`MWV207_GEM_CREATE_*`、`MWV207_GEM_WAIT_READ/WRITE`、`MWV207_CTX_OP_*`、`MWV207_SUBMIT_ENGINE_*`、`MWV207_SUBMIT_FLAG_*`、`MWV207_BO_ACC_FLAG_*`、`MWV207_SUBMIT_RELOC_*`、`MWV207_INFO_OP_*`（`drm.h:42-50,78-79,87-88,103-112,144-155,189-190`） | **一个都没有**；KMD 全部用裸数字（如 `K/gem.c:99-104`、`K/ctx.c:202-206`、`K/submit.c:621`） |
| A3 | 结构体布局 | 与 KMD 逐字段一致（见 §9） | 一致 |
| A4 | 结构体尺寸 | 24/8/16/16/72 | 相同（同一 C 程序实测） |
| A5 | 头注释 | `drm.h:96-101` 保留「cmd buffer size should be limited to 16K」「bo table should not contain duplicates」 | `K/drm.h:69-74` 同样的 4 条注释（含 16K） |
| A6 | 预留字段 | `drm_mwv207_submit_syncobj` 仅声明（`drm.h:138-142`），UMD 未使用；`drm_mwv207_submit` 中 syncobj 字段被注释掉（`drm.h:129-134`） | 同样只有声明与注释（`K/drm.h:99-104,91-96`） |
| A7 | 宏使用率（实测） | `DRM_IOCTL_MWV207*`、`DRM_MWV207_*`、`MWV207_GEM_DOMAIN_*`、`MWV207_GEM_CREATE_CPU*`、`MWV207_GEM_WAIT_READ`、`MWV207_INFO_OP_*`、`MWV207_CTX_OP_*`、`MWV207_SUBMIT_ENGINE_ID_*`、`MWV207_SUBMIT_FLAG_*`、`MWV207_SUBMIT_RELOC_WORD64`、`MWV207_SUBMIT_RELOC_FLAG_CMD_DAT` —— **在整个 mesa 树中除该头文件外零引用**；仅 `MWV207_SUBMIT_ENGINE_TYPE_*`（`D/video-codec/common/mwv207_video_cs.c:40` = `ENGINE_TYPE_DEC`）、`MWV207_BO_ACC_FLAG_WRITE`、`MWV207_SUBMIT_RELOC_LOW32/HIGH32` 被 video-codec 使用 | — |
| A8 | cmd 大小上限 | 实际发送 `cmd->offset`，缓冲上限 `0x8000`=32 KiB（`cs.c:132`），节流阈值「剩余 ≥ 4096」（`D/cmd.c:343`） | 接受上限 `0x20000`=128 KiB（`K/submit.c:167`）—— 两侧与「16K」注释**三方不一致** |
| A9 | engine_id | 恒 0（`cs.c:386`） | 支持 `0xffffffff` = any 专属 entity（`K/submit.c:62-63`），本 UMD 不可达 |
| A10 | ioctl 计数 | 无 | `.num_ioctls = ARRAY_SIZE(...)` = 6（`K/drv.c:137-138`），`nr >= num_ioctls` 的内核默认拒绝 |

---

## 9. `struct drm_mwv207_*` 逐字段表（驱动填什么 / 内核读什么）

### 9.1 `union drm_mwv207_gem_create`（UMD `drm.h:51-64`；KMD `K/drm.h:28-41`）

| 字段 | 类型 | 驱动（UMD）填什么 | 内核（KMD）读什么 |
|------|------|-------------------|-------------------|
| `in.size` | `__u64` | `size`（`bo.c:169`） | 校验 `==0 → -EINVAL`（`K/gem.c:97-98`） |
| `in.alignment` | `__u64` | 默认 `0x1000`（`bo.c:170`）；`MWV207_FLAG_2D_ACCESS` 时 `0x10000`（`bo.c:184-185`） | 校验非 2 的幂 → `-EINVAL`（`K/gem.c:99-100`）；传给 `mwv207_bo_create` 时右移 PAGE_SHIFT（`K/bo.c:139`） |
| `in.preferred_domain` | `__u32` | 按 domain 位或：VRAM `0x2`、SYSTEM `0x1`、GTT `0x4`（`bo.c:172-177`） | 校验 `& ~0x7 → -EINVAL`（`K/gem.c:101-102`）；失败回退 `==0x2` 时或上 `0x1` 重试（`K/gem.c:38-41`） |
| `in.flags` | `__u32` | CPU_ACCESS→`0x1`、NO_CPU_ACCESS→`0x2`（`bo.c:179-182`） | 校验 `& ~0b111 → -EINVAL`（`K/gem.c:103-104`）；`0x1` 失败时清掉重试一次（`K/gem.c:34-37`）；`0x2`（GPU_PHYS32）要求 domain 含 `0x2`（`K/bo.c:117-118`） |
| `out.handle` | `__u32` | 读回 `bo->handle`（`bo.c:202`） | `drm_gem_handle_create(filp, gobj, &args->out.handle)`（`K/gem.c:112`） |
| `out.resv` | `__u32` | 不读 | 不写（`memset` 由用户态 `bo.c:168` 完成；内核未写） |

注：UMD 在 ioctl 之后写 `bo->base.base.placement = args.in.alignment`（`bo.c:197`），读的是**自己填进去的入参**（内核不写回该字段），值只会是 `0x1000`/`0x10000`。

### 9.2 `union drm_mwv207_gem_mmap` 与 `struct drm_mwv207_gem_wait`

| 结构/字段 | UMD 填 | KMD 读/写 |
|-----------|--------|-----------|
| `mmap.in.handle` | `bo->handle`（`bo.c:280`；`memset` 于 `bo.c:279`） | `drm_gem_object_lookup`（`K/gem.c:127-129`，未命中 `-ENOENT`） |
| `mmap.in.pad` | 恒 0（`memset`，`bo.c:279`） | 非 0 → `-EINVAL`（`K/gem.c:124-125`） |
| `mmap.out.offset` | 读回作 `os_mmap` 的 offset（`bo.c:286`） | `mwv207_bo_mmap_offset(obj)` = `drm_vma_node_offset_addr(&jbo->tbo.base.vma_node)`（`K/gem.c:130`，`K/bo.h:69-74`） |
| `wait.handle` | `bo->handle`（`bo.c:245`） | `drm_gem_dma_resv_wait(filp, handle, write, timeout)`（`K/gem.c:150`） |
| `wait.op` | 恒 `0x2`（WRITE）（`bo.c:246`） | 校验 `& ~0x3 → -EINVAL`（`K/gem.c:144-145`）；`write = op & 0x2`（`K/gem.c:147`） |
| `wait.timeout` | `get_abs_timeout(timeout)`：绝对 ns（`bo.c:247`、`bo.c:221-235`） | `drm_timeout_abs_to_jiffies`（`K/gem.c:148`）；`-ETIME → timeout ? -ETIMEDOUT : -EBUSY`（`K/gem.c:151-152`） |

→ UMD 侧错误语义：`-EBUSY`（超时为 0 的非阻塞语义）静默返回 `false`，其它负值打印后返回 `false`（`bo.c:251-257`）。

### 9.3 `struct drm_mwv207_ctx`（UMD `drm.h:89-94`；KMD `K/drm.h:62-67`）

| 字段 | UMD 建/销 | KMD |
|------|-----------|-----|
| `op` | `0x0`（`winsys.c:137`）/ `0x1`（`winsys.c:161`） | `0 → mwv207_ctx_create`、`1 → mwv207_ctx_destroy`、其它 `-EINVAL`（`K/ctx.c:202-209`） |
| `flags` | 恒 0（`memset`，`winsys.c:136`/`160`） | **`if (args->resv || args->flags) return -EINVAL;`**（`K/ctx.c:199-200`）→ 将来 UMD 若用 flags 会立刻被拒 |
| `handle` | 建：读回 `ws->ctx`（`winsys.c:146`）；销：`ws->ctx`（`winsys.c:162`） | 建：`idr_alloc(&mgr->handle_table, ctx, 1, 0, GFP_KERNEL)`（`K/ctx.c:150`，从 1 起）；销：`idr_remove`，找不到 → `-EINVAL`（`K/ctx.c:183-187`） |
| `resv` | 恒 0 | 必须 0（同上） |

### 9.4 `struct drm_mwv207_submit`（UMD `drm.h:114-135`；KMD `K/drm.h:76-97`）

| # | 字段 | 类型 | UMD 填什么（行） | KMD 读什么（行） |
|---|------|------|------------------|------------------|
| 1 | `ctx` | `__u32` | `ws->ctx`（`cs.c:384`） | `mwv207_ctx_lookup(dev, filp, args->ctx)`，失败 `-ENOENT`（`K/submit.c:288-290`） |
| 2 | `engine_type` | `__u32` | `cmd->engine_type`（`cs.c:385`）：3D=0（`D/cmd.c:318`）、DMA=4（`D/cmd.c:322`）、DEC=1（`D/video-codec/common/mwv207_video_cs.c:40`） | `switch`（`K/submit.c:33-58`）：0→3D、1→DEC、2→ENC、3→2D、4→DMA（并置 `is_dma`），其余 `id_max=0 → -ENODEV`（`K/submit.c:59-60`） |
| 3 | `engine_id` | `__u32` | **恒 0**（`cs.c:386`） | `0xffffffff`→any 实体；`< id_max`→对应实体；否则 `-ENODEV`（`K/submit.c:62-67`） |
| 4 | `flags` | `__u32` | 入 fence 置 `0x1`（`cs.c:388`），出 fence 置 `0x2`（`cs.c:392`），两者可同时 | `flags & ~0x3 → -EINVAL`（`K/submit.c:621`）；`& 0x1`→取入 fence（`566-570`）；`& 0x2`→分配并安装 out fence fd（`587-601`） |
| 5 | `fence_fd` | `__u32` | 入方向：填 in fence fd（`cs.c:389`）；出方向：**不填**，从该字段读回 out fd（`cs.c:431`） | 入：`sync_file_get_fence(args->fence_fd)`，失败 `-EINVAL`（`K/submit.c:567-569`）；出：`args->fence_fd = fd`（`K/submit.c:600`） |
| 6 | `cmd_size` | `__u32` | `cmd->offset`（字节，`cs.c:393`） | `<=0 → -EINVAL`（`K/submit.c:164-165`）；`> 0x20000 → -E2BIG`（`K/submit.c:167-168`） |
| 7 | `nr_relocs` | `__u32` | `cmd->nr_relocs`（`cs.c:394`；含 dat 内部 reloc 迁移进主表的项） | `(int) < 0 → -EINVAL`（`K/submit.c:74-75`）；`0` 合法；`relocs==0` 时 `-EINVAL`（`K/submit.c:79-80`）；逐项 patch（`K/submit.c:478-483`） |
| 8 | `nr_bos` | `__u32` | `cmd->nr_bos`（`cs.c:395`） | `(int) < 0 → -EINVAL`（`K/submit.c:103-104`）；`bos==0` 时 `-EINVAL`（`K/submit.c:113-114`）；逐项 `idr_find(&filp->object_idr, ubos[i].handle)`，未命中 `-ENOENT`（`K/submit.c:138-143`） |
| 9 | `nr_cmd_dat` | `__u32` | 有 dat 时 `cmd->nr_dats`（恒 1），否则 0（`cs.c:405-408`） | `(int) < 0 → -EINVAL`（`K/submit.c:195-196`）；每项校验 `dat_size>0 && dat!=0`、`nr_relocs!=0 → relocs!=0`、`dat_size<=0x20000`（`K/submit.c:224-239`）；为每项 `mwv207_bo_create` 一个 BO（`267-271`） |
| 10 | `padding` | `__u32` | 恒 0（`{0}` 初始化，`cs.c:378`） | 非 0 → `-EINVAL`（`K/submit.c:621`） |
| 11 | `cmds` | `__u64` | `(u64)cmd->buf`（`cs.c:397`） | `==0 → -EINVAL`（`K/submit.c:170-171`）；`copy_from_user(cmds, cmd_size)`（`181-182`）；`is_dma` 用 `kmalloc` 否则 `kvmalloc`（`174-177`） |
| 12 | `relocs` | `__u64` | `(u64)cmd->relocs`（`cs.c:396`） | `kvmalloc_array(nr_relocs)` + `copy_from_user`（`K/submit.c:82-90`） |
| 13 | `bos` | `__u64` | `(u64)cmd->bos`（`cs.c:403`，元素 `drm_mwv207_bo_acc`） | 栈上 32 项、超出用 `kvmalloc_array`，再 `copy_from_user`（`K/submit.c:97-122`） |
| 14 | `cmd_dats` | `__u64` | `(u64)&cmd->dats`（`cs.c:414`，单元素） | 栈上 8 项、超出用 `kvmalloc_array`，再 `copy_from_user`（`K/submit.c:189-217`） |

### 9.5 `struct drm_mwv207_bo_acc`（UMD `drm.h:146-149`；KMD `K/drm.h:106-109`）

| 字段 | UMD 填 | KMD 读 |
|------|--------|--------|
| `handle` | `cmd->bos[i].handle = cmd->mwv207_bos[i]->handle`（`cs.c:400`，提交时统一回填） | `idr_find`（`K/submit.c:138`） |
| `flags` | per-cmdbuf 维护（`cs.c:186` 去重时 `|=`，`cs.c:210` 新建时 `=`）；取值 `0`（普通）、`0x1`（`MWV207_BO_ACC_FLAG_WRITE`，见 `D/vertex.c:149` 等传 `0x1`/`0x2`） | `tvb->num_shared = (flags & 0x1) ? 0 : 1;`（`K/submit.c:137`）—— **只认 bit0**，其它位（UMD 会传 `0x2`）被忽略 |

> 注意：UMD 侧 `add_bo` 的 flags 参数在驱动层用的是 `0x1`/`0x2`（如 `D/vertex.c:174` 传 `0x2`），但 KMD 只解释 bit0，bit1 语义未定义（`K/drm.h:106-109` 无 `MWV207_BO_ACC_FLAG_*` 宏）。

### 9.6 `struct drm_mwv207_submit_reloc`（UMD `drm.h:156-162`；KMD `K/drm.h:111-117`）

| 字段 | UMD 填 | KMD 读/校验 |
|------|--------|-------------|
| `idx` | 主表：`cmdbuf_add_bo` 返回值（`D/cmd.c:221`→`234`）；dat 内：`src_dat->relocs[i].idx` 经 `add_bo` 重映射（`cs.c:263`） | 主表 reloc：`idx >= nr_bos → -EINVAL`（`K/submit.c:383-385`）；dat reloc：`idx + nr_bos`（`K/submit.c:379-381`） |
| `cmd_offset` | 相对 `cmd->buf`/dat 缓冲的**字节**偏移（`D/cmd.c:227,230,246`；dat 内 `cs.c:262` 累加 256 基准） | `dat_size` 边界校验：WORD64 +8、LOW32/HIGH32 +4、32_16 +2（`K/submit.c:347,352,357,362`） |
| `bo_offset` | 用户提供的 BO 内偏移（`D/cmd.c:234`） | `>= mwv207_bo_size(jbo) → -ERANGE`（`K/submit.c:340-341`）；最终地址 `= mwv207_bo_gpu_phys(jbo) + bo_offset`（`K/submit.c:342`） |
| `type` | 低 16 位类型：`0`=WORD64、`1`=LOW32、`2`=HIGH32、`3`=32_16；高位 `0x80000000` 表示「idx 指向 cmd_dat」（`D/cmd.c:247`） | `pad != 0 → -EINVAL`（`K/submit.c:336-337`）；`switch (type & 0xffff)` 四种，其余 `-EINVAL`（`K/submit.c:345-368`）；`type & 0x80000000` 判 cmd_dat（`K/submit.c:378`）；**DMA 路径额外要求** `type` 高位为 0、低 16 位为 0、`bo_offset==0`（`K/submit.c:445-450`） |
| `pad` | 恒 0（驱动层全部传 `pad` 变量 = 0，见 `D/cmd.c:224`） | 必须 0 |

### 9.7 `struct drm_mwv207_submit_cmd_dat`（UMD `drm.h:165-170`；KMD `K/drm.h:120-125`）

| 字段 | UMD 填 | KMD 读 |
|------|--------|--------|
| `nr_relocs` | `cmd->mwv207_dats.nr_relocs`（`cs.c:410`） | `(int)<0 → -EINVAL`（`K/submit.c:224-227`）；`!=0 && relocs==0 → -EINVAL`（`232-235`）；据此 `kvmalloc_array`+`copy_from_user`（`243-255`） |
| `dat_size` | `cmd->mwv207_dats.size`（`cs.c:411`，按 256 B/dat 递增） | `<=0 → -EINVAL`；`> 0x20000 → -E2BIG`（`K/submit.c:228-239`）；用于 `kvmalloc` + `copy_from_user`（`257-266`） |
| `relocs` | `(u64)cmd->mwv207_dats.relocs`（`cs.c:412`） | 用户指针 → 拷贝到内核（同上） |
| `dat` | `(u64)cmd->mwv207_dats.data`（`cs.c:413`） | 拷贝到内核缓冲后，`memcpy_toio` 写进新建的 dat BO（`K/submit.c:503-507`，先 `ttm_bo_wait`（`499`）再 `mwv207_bo_kmap_reserved`（`503`）） |

KMD 侧对应的内核内部结构是 `struct mwv207_submit_cmd_dat`（`K/sched.h:24-30`：`nr_relocs`/`dat_size`/`relocs`/`dat`/`jbo`），与用户态 ABI 结构**不同名但字段对应**。

### 9.8 `struct drm_mwv207_info` / `drm_mwv207_info_rec`（UMD `drm.h:184-195`；KMD `K/drm.h:139-148`）

| 字段 | UMD | KMD |
|------|-----|-----|
| `info.op` | 本 UMD winsys **未调用 INFO ioctl**（无调用点） | `op==1`：按 `nr_recs` 当 key 二分查找，命中把 `val` 写回 `args->recs`，未命中 `args->recs=0` 并 `-ENOENT`（`K/db.c:117-127`）；`op==0`：`args->recs` 非 0 则 `copy_to_user` 全部 rec，否则回填 `nr_recs`（`K/db.c:128-138`）；其它 `-EINVAL` |
| `info.nr_recs` | — | 见上（op=1 时被当作 key 使用，语义复用） |
| `info.recs` | — | op=1：出参（val）；op=0：入参（缓冲区地址） |
| `info_rec.key/val` | key 枚举 `DRM_MWV207_ACTIVE_{3D,DEC,ENC,2D,DMA}_NR`（`drm.h:176-182`） | 数据库由 `mwv207_db_add` 填充（`K/db.c:56-74`），排序/查重（`K/db.c:88-106`） |

---

## 10. KMD 侧语义补充（供 UMD 调用契约核对）

| 主题 | 事实 | 行 |
|------|------|----|
| 设备/驱动 | PCI id `0x0731:0x9100`；`DRIVER_MAJOR/MINOR = 0/1`，`.name="mwv207"` | `K/drv.c:41-45,519-522,140-144` |
| ioctl 注册 | 6 个，全部 `DRM_AUTH\|DRM_RENDER_ALLOW` | `K/drv.c:110-117,137-138` |
| 打开/关闭 | `mwv207_driver_open` 建 `struct mwv207_fpriv`（内含 ctx_mgr 的 mutex+idr）；`postclose` 释放 | `K/drv.c:59-81`、`K/mwv207.h:93-100` |
| ctx 实体 | 每 ctx 11 个 `drm_sched_entity`：3D/dec/enc/2d/dma 各占 `nr_engine` + 1（`&ctx->entity_3d[2+1]` 等指针算术）；`WARN_ON` 校验末位 | `K/ctx.c:103-109`、`K/ctx.h:25` |
| ctx 创建 | `kzalloc` → `kref_init` → `mwv207_ctx_entity_init`（5 类引擎各一次）→ `idr_alloc(1,0)` | `K/ctx.c:132-163` |
| submit 流程 | `submit_init`（ctx_lookup→select_engine→cmd_dat→bo→reloc→cmd，`K/submit.c:280-308`）→ `ttm_eu_reserve_buffers`（629）→ `validate`（632）→ `patch`（635）→ `attach_dependency`（638）→ `commit`（641）→ 成功 `ttm_eu_fence_buffer_objects`，失败 `ttm_eu_backoff_reservation`（645-648） | `K/submit.c:612-653` |
| reloc 回填 | `*(u64/32/16*)(dat + cmd_offset) = addr`（WORD64/LOW32/HIGH32/32_16）；`addr = mwv207_bo_gpu_phys(jbo) + bo_offset`（`K/bo.h:76-79`：`tbo.mem.start << PAGE_SHIFT`） | `K/submit.c:330-371` |
| DMA 提交专有校验 | `nr_bos==2 && nr_relocs==2 && nr_cmd_dat==0`；`cmd_size == sizeof(struct mwv207_dma_cmd)`（`K/sched.h:93-98`：两个 `mwv207_dma_loc`+width+height = 80 B）；reloc 必须落在 `src.base`/`dst.base` 偏移；src≠dst | `K/submit.c:430-467` |
| validate | 非 DMA：`jbo->flags |= (1<<2)`（GPU_PHYS32），dat BO 额外 `|= (1<<0)`，domain 强制 `0x2`（VRAM）；DMA：SYSTEM 保留，其余改 `0x4`（GTT） | `K/submit.c:513-547` |
| out fence | `get_unused_fd_flags(O_CLOEXEC)` + `sync_file_create(&s_fence->finished)` + `fd_install`；失败路径 `put_unused_fd` 后 `-ENOMEM` | `K/submit.c:587-601` |
| GEM 生命周期 | `mwv207_gem_free_object` → `mwv207_bo_unref` → `ttm_bo_put`；TTM destroy 里 `drm_gem_object_release` + `kfree` | `K/gem.c:78-87`、`K/bo.c:21-30,210-216` |
| BO kmap 权限 | `if (!(jbo->flags & (1<<0))) return -EPERM;`（K1 修复后） | `K/bo.c:172-197` |
| 本树版本印记 | `K/bo.c:138` 仍是 `ttm_bo_init(...)`, `K/bo.c:133` `ttm_bo_dma_acc_size`, `K/mwv207.h:43` `struct ttm_bo_device`, `K/drv.c:217` `pci_bus_address(jdev->base.pdev,...)` → 5.10 期 API；而 `K/gem.c:150` 已用 `drm_gem_dma_resv_wait`、`K/bo.c:146` 已补 `drm_gem_object_release` → **一棵「部分修复 + 部分向新内核靠拢」的混合树**。与 `ANALYSIS-REPORT.md §4`（5.10→6.6 缺口）一致：它**不能**直接编译进 6.6。 | `K/bo.c:133,138,146`、`K/mwv207.h:43`、`K/drv.c:217`、`K/gem.c:150` |

---

## 11. 与寄存器文档的衔接（引用既有结论）

`docs/registers/REGISTERS-FIELDS.md` 的结论（驱动命令流中的 16 位寄存器号 = 厂商寄存器 `_Address >> 2`）与本层的关系：winsys 只把驱动层已经排好的 `uint32_t` 命令字原样搬运（`cs.c:393` 的 `cmd_size`、`cs.c:397` 的 `cmds`），**不对命令流做任何解码或重写**；唯一对命令流的改写动作是 reloc 回填，且回填发生在 **KMD**（`K/submit.c:349,354,359,364`）。因此寄存器号语义完全由 `src/gallium/drivers/mwv207` 的发射宏（`MWV207_EMIT`，`D/cmd.c:231,250`）决定，winsys 层不参与。

---

## 12. 已知缺陷对照

### 12.1 winsys（W1–W12，引自 `reports/ANALYSIS-REPORT.md:25-38`）

| # | 报告描述（摘要） | 报告行号 | 本树行号 | 本树现状 |
|---|------------------|----------|----------|----------|
| W1 | CTX create 失败仅 printf 后继续，`ws->ctx` 无效 → 首次绘制 ENOENT | `winsys.c:132` | `winsys.c:140-144` | **已修复**（stderr + `return false`；上层 `winsys.c:246-251` 放弃 screen） |
| W2 | `fcntl(F_DUPFD_CLOEXEC)` 未检查，fd=-1 后所有 ioctl EBADF | `winsys.c:230` | `winsys.c:239-244` | **已修复** |
| W3 | `mwv207_create_screen()` 返回 NULL 仍把半初始化 ws 插入全局表永久缓存 | `winsys.c:241` | `winsys.c:257-263` | **已修复**（含注释「never cache a half-initialised winsys」） |
| W4 | 导出/flink 表 key 错配（insert 用值指针 vs lookup 传栈地址）→ 重复 BO 包装、表无限增长、`bo->logical` 悬垂 | `bo.c:70-71,364,384` | `bo.c:68-69,327-328,352,375,398,400` | **已修复**（hash/equals 为解引用型 `winsys.c:107-117`，insert/lookup/remove 统一用 `&bo->字段`；但键的脆弱性见 §5） |
| W5 | `co->name` 未定义标识符笔误 | `bo.c:408` | `bo.c:397` | **已修复**（`bo->name`） |
| W6 | `CALLOC_STRUCT(mwv207_bo)` 后无 NULL 检查，错误路径解引用崩溃 | `bo.c:392-404` | **`bo.c:382-383`（`CALLOC` 后立即 `pb_cache_init_entry(&...,&bo->u.real...)`）与 `bo.c:405-406`（`bo->handle` 打印）** | **仍存在**（本次精读确认无任何 NULL 判定） |
| W7 | GEM_MMAP ioctl 失败仅 printf 后继续 mmap（用未初始化的 `args.out.offset`） | `bo.c:288-302` | **`bo.c:283-284`**（失败 printf 后直接 `os_mmap(..., args.out.offset)`，`bo.c:286`） | **仍存在** |
| W8 | `add_bo` 把 flags 写进共享 `bo->flags` → 覆盖其他引用者 | `cs.c:170-196` | `cs.c:208-213`（写入 `cmd->bos[i].flags`） | **已修复** |
| W9 | 两处 `realloc` 直接覆盖原指针 → 失败即泄漏+悬垂 | `cs.c:181-189` | `cs.c:190-206`、`cs.c:281-290` | **已修复**（`new_bos`/`new_accs`/`new_relocs` 临时变量 + 返回 ENOMEM） |
| W10 | `add_dat` 扩容单次 `*2` 不够；`>=` 边界重复 | `cs.c:220-233` | `cs.c:238-257` | **已修复**（两处改为 `while` 循环） |
| W11 | `fence_wait` 的 ns→ms 溢出（INFINITE 变负） | `cs.c:86-94` | `cs.c:92-98` | **已修复**（INFINITE→-1；负值夹到 `INT_MAX`，含注释） |
| W12 | `cmdbuf_create` 全靠 assert，release 下分配失败崩溃 | `cs.c:115-141` | **`cs.c:125-130`（`CALLOC_STRUCT`/`MALLOC`+`assert`）、`cs.c:138,142,143`（`CALLOC` 无检查）** | **仍存在** |

### 12.2 KMD（K1–K12 中与本文范围相关者，引自 `reports/ANALYSIS-REPORT.md:60-73`）

| # | 报告描述 | 本树行号 | 本树现状 |
|---|----------|----------|----------|
| K1 | `!jbo->flags & (1<<0)` 优先级错误 → kmap 权限检查永不生效 | **`K/bo.c:180`（已改为 `!(jbo->flags & (1<<0))`）+ 说明注释 `K/bo.c:177-179`** | **已修复** |
| K2 | `ttm_bo_init` 失败未 `drm_gem_object_release` → shmem 泄漏 | **`K/bo.c:143-149`**（含注释） | **已修复** |
| K3 | `return copy_to_user(...)` 正数当 ioctl 返回 | **`K/db.c:130-134`**（`left` 局部变量 + `-EFAULT`） | **已修复** |
| K4 | 元素大小硬编码 8；`rec.val` 未初始化 | `K/db.c:97,120`（现用 `sizeof(*db->db)`）、`K/db.c:119`（`rec.val = 0`） | **已修复** |
| K5 | `db_fini` 后 `jdev->db` 悬垂 | **`K/db.c:53`**（`jdev->db = NULL;` + 注释） | **已修复** |
| K6 | 5 处 `copy_from_user` 正数当返回值 → 坏指针误报成功 | `K/submit.c:85-90,121-126,216-221,249-254,262-266`（均显式 `-EFAULT`） | **已修复** |
| K7 | `init_job_reloc` 失败时 `mjob->relocs` 悬垂（二次 kvfree） | **`K/submit.c:87-88`**（`kvfree` 后置 NULL） | **已修复** |
| K8 | `irq_create_mapping` 返回值全部忽略 → `BUG_ON(va_irq==0)` | 本文未逐行读 `K/irq.c`（报告称已修） | **据报告已修**（未在本次范围内复核） |
| K9 | demux ISR 的 W1C 竞态 | 本文未读 `K/irq.c` | **据报告未改** |
| K10 | `scheds[6]` 栈数组无上界检查 | **`K/ctx.c:64-67`**（`nr > MWV207_MAX_SCHED_PER_ENGINE(6) → -EINVAL`） | **已修复** |
| K11 | remove 顺序：`pci_clear_master` 最后 | **`K/drv.c:445`**（仍是最后一步） | **仍存在**（据报告未改） |
| K12 | `timedout_job` 循环语义可疑 / 无 job 完整性校验 | 本文未读 `K/sched.c` | **据报告未改** |

---

## 13. 附加观察（不属于 W/K 清单，本文首次记录）

| ID | 观察 | 依据 |
|----|------|------|
| O1 | `bo->fd` 在创建/导入路径**从未初始化**，只有 `WINSYS_HANDLE_TYPE_FD` 导出时才被 `drmPrimeHandleToFD` 写入；销毁路径也不 `close(bo->fd)` | `winsys.h:114`、`bo.c:202-207,388-391,310`、`bo.c:54-77` |
| O2 | `bo_map` 首调用返回 `ptr`（父块基址，不含 `offset`），已映射后再调用返回 `bo->logical`（含 `offset`）——**同一函数两种返回语义**；slab 子 BO 的 `offset != 0`，直接用返回值会访问到错误偏移 | `bo.c:276-277` vs `bo.c:291-293`；`bo.c:509-510` |
| O3 | `add_dat` 以**硬编码 `0x100`** 推进拼接缓冲的 `size`（`return size-0x100`），仅当每个 `cmd_dat.size == 256` 才自洽；未检索全部构造点 | `cs.c:268-270`；`winsys.h:145`（`data[64]` = 256 B） |
| O4 | `cmdbuf_commit` 的 `-EBUSY` 重试是**无上限 busy-loop** 且每次 printf；重试时若内核已安装 out fence fd，会产生 fd 泄漏（KMD 失败路径不回收已 `fd_install` 的 fd） | `cs.c:418-423`；`K/submit.c:591-599` |
| O5 | 提交失败时 `*out_fence_fd` 保持调用方初值 -1，上层 `fence_create(ws,-1)` 造出的 fence 其 `fence_wait` 恒返回 true（提交失败被静默视为完成） | `cs.c:430-431`、`cs.c:89-101`、`D/context.c:93-94` |
| O6 | `ws->bo_get_handle` 的 SHARED 分支在 `drmIoctl(GEM_FLINK)` 失败时直接 `return ret`，**跳过表登记**且未把 `bo->name`/`whandle->handle` 复位（`bo->name` 仅在成功分支赋值，故此处 0） | `bo.c:312-323` |
| O7 | `mwv207_drm_fence_reference` 在 `old_dst == src` 时先销毁再赋值，理论上产生自引用已释放对象（`pipe_reference` 计数不变，故不会销毁，但代码顺序不直观） | `cs.c:73-82` |
| O8 | `hash_fd`/`compare_fd` 未检查 `fstat` 返回值；`ws->fd` 关闭后若仍被比较会读到未初始化栈数据 | `winsys.c:184-206` |
| O9 | `winsys.h:33-40` 的 `MWV207_DEBUG_*` 缺 `SPILL(1<<8)`，与 `winsys.c:69` 的 `spill=0x100` 不同步 | `winsys.h:33-40`、`winsys.c:69` |
| O10 | `cs.h:35` 的 `mwv207_cs_destory` 无定义，属死声明 | `cs.h:35`（全仓库唯一命中） |
| O11 | `KMD bo_acc.flags` 只解释 bit0（`num_shared`），而 UMD 驱动层会传 `0x2`（如 `D/vertex.c:174`），该位在 KMD 侧被静默忽略 | `K/submit.c:137`、`K/drm.h:106-109`、`D/vertex.c:174` |
| O12 | KMD 头注释「cmd buffer size should be limited to 16K」与 UMD 实际 32 KiB 缓冲（`cs.c:132`）及 KMD 实际 128 KiB 上限（`K/submit.c:167`）三方不一致 | `drm.h:100`、`K/drm.h:73`、`cs.c:132`、`K/submit.c:167` |

---

## 14. 未能确认之处

1. **libdrm 包装函数的返回约定**：`drmCommandWriteRead`/`drmCommandWrite` 的实现（返回 `0` 或 `-errno`）来自调用点的使用方式（`bo.c:188` 用 `<0`、`cs.c:418` 用 `==-EBUSY`、`cs.c:425` 用 `!=0`）推断；本仓库内**没有 libdrm 源码**，`/usr/include/xf86drm.h` 只有声明 —— 未逐行确认。
2. **`ws->device`（`winsys.h:195`）**：在本 8 文件内无赋值点也无读点，用途未确认（可能由本范围外的旧代码或调试路径使用）。
3. **`struct mwv207_bo.flags`（`winsys.h:121`）**：本 winsys 内既无写也无读（W8 修复后），原始用途未确认。
4. **O2/O3 的实际影响面**：`bo_map` 返回值被 `drivers/mwv207` 多处直接使用（如 `D/sampler.c:253-255`、`D/resource.c:439`），这些资源是否带 `MWV207_FLAG_SUBALLOC`（`D/resource.c:288-289` 由 `PIPE_RESOURCE_FLAG_DRV_PRIV<<1` 决定）**未逐一追查**；`add_dat` 的 `cmd_dat.size` 是否恒为 256 也**未逐一追查**（构造点分布在 `D/state.c`/`D/draw.c`/`D/vertex.c` 等多处，超出本次 winsys 范围）。若要坐实，需要单独一轮对 driver 层 `cmd_dat` 构造点与 slab 资源使用点的通读。
5. **KMD 中断/调度相关文件**（`K/irq.c`、`K/sched.c`、`K/ttm.c`、`K/pipe_*.c`）**未在本次逐行读取**，故 K8/K9/K11/K12 的现状引自 `ANALYSIS-REPORT.md` 而未复核（K11 的 `K/drv.c:445` 已确认）。
6. **运行中的内核模块版本**：本报告的 KMD 结论来自 `src/kernel` 下的 5.10 期源码树；当前系统实际加载的 `mwv207.ko`（6.6 移植版）源码不在本仓库，两者的 ABI 兼容性仅按头文件结构体逐字段比对（一致）与报告 §5 的移植清单推断，**未做二进制或运行态 ABI 校验**。
7. **`DRM_AUTH|DRM_RENDER_ALLOW` 的精确门控行为**：按 DRM 通用语义（render node 放行、需 `DRM_AUTH` 的调用需已认证）理解，未在本次内核源码中逐行核对。
8. **`winsys.c:52` 全局单例表的并发模型**：`mwv207_ws_mutex`（`winsys.c:54`）保护创建/销毁与全局表；但 `mwv207_ws_tab` 的成员访问（`winsys.c:224,269,173`）均在同一 mutex 内，跨 winsys 的 `bo_*` 操作则由 `bo_export_table_lock` 保护 —— 两把锁的层级关系未见注释，**未确认是否存在锁序问题**。

---

（完）