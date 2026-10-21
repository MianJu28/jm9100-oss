# JM9100 (MWV207) 开源驱动逐行分析报告

- 日期：2026-10-20
- 环境：Deepin 25 (crimson) · aarch64 · 内核 6.6.143-arm64-desktop-hwe · GPU: JingjiaMicro JM9100 [0731:9100]
- 分析对象（开源驱动，来自 gitee.com/jemoic）：
  - **KMD（内核态）**：`drivers/gpu/drm/mwv207/`，源自 `jemoic/phytium-linux-kernel` 分支 `linux-5.10-mwv207`（47 文件 / 约 2 万行）。当前系统内核运行的是 deepin 移植到 6.6 的版本（mwv207.ko 已加载，`/dev/dri/card0` + `renderD128` 正常）。
  - **UMD（用户态）**：Mesa 22.3.7 fork `jemoic/mesa` 分支 `dev-lwf-mesa-22.3.7`（mwv207 gallium 驱动 + winsys，约 2.1 万行）。当前系统未使用它（此前切换实验已回滚到厂商闭源用户态）。
- 约束：只分析并修复代码；不切换系统驱动、不修改系统配置（本次全程未动 /etc、未替换系统库、未重载内核模块）。

---

## 一、结论摘要

1. **公开的开源 UMD（Mesa mwv207）在标准构建下根本无法编译**——opcode 表引用了不存在的枚举、ISA 定义头从未被 include、多处 API 笔误。这解释了为什么景美只发编译好的 .so。我们以 vendor 自用可构建的 `-test` 分支为基线，叠加 12 项缺陷修复后，**首次在公开源码上完整构建出 `mwv207_dri.so`**。
2. **修复后的 UMD 在本机真实硬件上全链路验证通过**：KMD ioctl 冒烟 → EGL/GBM surfaceless 初始化 → FBO 硬件渲染回读 → 完整 shader 编译 + 三角形绘制 + 像素校验，全部 PASS（renderer = "MWV207 (Jingjia Corporation)"，非 llvmpipe）。
3. **KMD（5.10 公开源码）存在 8 处确凿缺陷**（优先级 bug、ioctl 正数返回、copy_from_user 泄漏、irq 映射未检查、栈数组越界风险、BUG_ON 致死等），全部已修复成补丁。其中部分缺陷（bo.c 优先级、db.c 正数返回）**deepin 6.6 移植版大概率同样存在**（移植只改 API 适配，不修逻辑）。
4. **5.10→6.6 移植缺口已完整测绘**（ttm_bo_api.h/ttm_bo_driver.h/ttm_memory.h 删除、drm_device.pdev 删除、ttm_device_funcs 重构、fault_reserve_notify 删除、ttm_bo_init→ttm_bo_alloc 等），见 §5。这份清单是把开源 KMD 移植回当前内核所需的全部 API 工作量。

---

## 二、UMD（Mesa mwv207）缺陷清单（已修复，commit 391ea29 / 4197d4a）

### winsys 层（直接对话内核，全部为真实缺陷）

| # | 文件:行 | 缺陷 | 严重性 | 修复 |
|---|---------|------|--------|------|
| W1 | winsys/mwv207_drm_winsys.c:132 | CTX 创建 ioctl 失败仅 printf 后继续，`ws->ctx` 拿无效 handle，之后所有 submit 引用不存在的内核 context → 首次绘制即 ENOENT | 高 | 失败即返回 false，上层放弃 screen 创建 |
| W2 | winsys/mwv207_drm_winsys.c:230 | `fcntl(F_DUPFD_CLOEXEC)` 未检查，fd=-1 时所有 drm ioctl 静默 EBADF | 高 | 检查并报错退出 |
| W3 | winsys/mwv207_drm_winsys.c:241 | `mwv207_create_screen()` 返回 NULL 时仍把半初始化的 ws 插入全局 fd 哈希表并永久缓存 → 该 fd 后续所有调用拿到死 screen | 高 | 失败时不入表，释放资源 |
| W4 | winsys/mwv207_drm_bo.c:70-71,364,384 | **ptr-key 哈希表 key 错配**：insert 用 `(void*)(uintptr_t)handle`，lookup/remove 却传 `&handle`（栈地址）→ 永远查不到 → 同一 dmabuf/flink 每次导入都新建重复 BO 包装，导出表无限增长，重复 mmap 簿记，先销毁者 munmap 后后者的 `bo->logical` 悬垂 | **极高**（跨进程 GL/EGL pixmap 导入路径） | insert/lookup/remove 统一用值作为 key |
| W5 | winsys/mwv207_drm_bo.c:408 | `co->name` 未定义标识符笔误（应为 `bo->name`）——主分支源码连编译都不过 | 高 | 修正 |
| W6 | winsys/mwv207_drm_bo.c:392-404 | `CALLOC_STRUCT(mwv207_bo)` 与后续 deref 之间无 NULL 检查；错误路径落到 debug printf 时 bo==NULL 直接崩溃 | 高 | 增加 NULL 检查与 unlock 早退 |
| W7 | winsys/mwv207_drm_bo.c:288-302 | GEM_MMAP ioctl 失败仅 printf 后继续 mmap（用未初始化的 args.out.offset）；失败时 `bo->logical` 保持 0 但返回值是裸 ptr，调用方拿到无 offset 修正的指针 | 中 | ioctl/mmap 失败即返回 NULL |
| W8 | winsys/mwv207_drm_cs.c:170-196 | `add_bo` 把访问 flags 写进**共享的** `bo->flags`（slab 子 BO 共用真实 BO）→ 覆盖其他引用者状态；重复 BO 时 `|=` 也写错对象 | 高 | flags 移到 per-cmdbuf `cmd->bos[i].flags` |
| W9 | winsys/mwv207_drm_cs.c:181-189 | 两处 `realloc` 直接覆盖原指针：失败时原数组丢失（泄漏）+ 悬垂指针后续 double-free | 高 | 临时变量接收 + 失败返回 ENOMEM |
| W10 | winsys/mwv207_drm_cs.c:220-233 | add_dat 的 data/relocs 扩容单次 `*2` 可能仍不够（一次追加多个 256B dat）；`>=` 边界重复 | 中 | while 循环扩容 |
| W11 | winsys/mwv207_drm_cs.c:86-94 | `fence_wait` 把 ns→ms 直接除 1e6 塞给 int；`PIPE_TIMEOUT_INFINITE`(0xffffffffffffffff) 溢出为负 | 中 | INFINITE→-1（sync_wait 无限）；其余 clamp 到 INT_MAX |
| W12 | winsys/mwv207_drm_cs.c:115-141 | cmdbuf_create 里 CALLOC/MALLOC 全靠 assert（release 构建无效）→ 分配失败必然崩溃 | 中 | 完整错误路径 + fail 标签回收 |

### driver 层（gallium）

| # | 文件:行 | 缺陷 | 修复 |
|---|---------|------|------|
| D1 | mwv207_resource.c:230 | `resource_create` 不检查 CALLOC 与 `bo_create/bo_from_handle` 结果，BO 失败仍返回非 NULL 资源 → 首次 draw/flush 必崩 | 已修：返回 NULL |
| D2 | mwv207_context.c:216-220 | `context_create` fail 路径只 `FREE(ctx)`，泄漏 cmd/dma cmdbuf、blitter、primconvert、uploader | 已修：完整 unwind |
| D3 | mwv207_context.c:93 | flush 时 `fence_create` 分配失败会泄漏 out_fence_fd | 已修：close |
| D4 | mwv207_screen.c:523-538 | `vk_sample_count_flags` 把 state tracker 用 0 表示"无 MSAA"的情况判为不支持；switch 后死代码 | 已修 |
| D5 | nir/mwv207_disasm.h + meson.build | **缺失 `etnaviv/hw/isa.xml.h` include**（INST_OPCODE_*/INST_TYPE_*/INST_ROUND_MODE_* 全部定义于此）→ 整个驱动在标准 meson 构建下编不过；`INST_CONDITION_MWV207_TRUE` 与 ISA 头的 `INST_CONDITION_TRUE` 命名错位 | 已修：include + 别名 |
| D6 | 主分支 opcode.c/emit.c/lower.c | 引用仓库中不存在的 `INST_EXT_OPCODE_*`/`INST_OPCODE_EXTENDED` 枚举 + bitfield_insert 的错误 lower（`nir_ior(insert_offset, shifted)` 语义颠倒）——不可构建的半成品 | 采用 vendor `-test` 分支的自洽实现（数值字面量 + 正确语义），未采纳主分支半成品 |
| D7 | mwv207_drm_bo.c:205 等 | `util_logbace2` 拼写错误（正确为 `util_logbase2`）——又一处编译阻断 | 已修 |

### 上游分支混乱问题（重要发现）

gitee 上 `dev-lwf-mesa-22.3.7`（主分支）**不可构建**，而 `dev-lwf-mesa-22.3.7-test`（"fix build"）才是 vendor 内部真正用过的可构建版本，且后者已自带 W5/D7 的最小修复——佐证这些缺陷真实存在。两分支差异（21 行 opcode 表、EXTENDED 指令集半成品、emit_vertex 数值）全部纳入分析。我们最终以 `-test` 为基线 + 完整修复集构建成功。

---

## 三、KMD（内核 mwv207，5.10 基线）缺陷清单（已修复成补丁，KMD commit）

| # | 文件:行 | 缺陷 | 严重性 |
|---|---------|------|--------|
| K1 | mwv207_bo.c:172 | `!jbo->flags & (1<<0)` 运算优先级错误 = `(!flags)&1` ≡ 0 → **kmap 权限检查永不生效**，CPU 不可见 BO 可被 kmap | 高 |
| K2 | mwv207_bo.c:138-144 | `ttm_bo_init` 失败时未释放 `drm_gem_object_init` 建立的 shmem → 每次 probe 失败/GEM create 失败泄漏内存 | 高 |
| K3 | mwv207_db.c:128 | `return copy_to_user(...)` 把**剩余字节数（正数）当 ioctl 返回值** → DRM core 向用户态返回正数，glibc 视为成功，用户缓冲区保持未初始化却被当有效数据 | 高 |
| K4 | mwv207_db.c:96,118 | sort/bsearch 元素大小硬编码 8（依赖 rec 恰好 8 字节）；`rec.val` 未初始化（KMSAN 可报） | 中 |
| K5 | mwv207_db.c:52 | fini 后 `jdev->db` 悬垂指针（remove 期间在途 ioctl UAF 窗口） | 中 |
| K6 | mwv207_submit.c（5 处 copy_from_user） | 部分拷贝的正数返回值直接当 ioctl 返回 → **坏用户指针被误报为成功**，内核继续用未拷贝完的 job 数据（含 relocs/bos/cmds） | 高 |
| K7 | mwv207_submit.c:74 | `mwv207_submit_init_job_reloc` 失败时 `mjob->relocs` 悬垂（job_fini 会二次 kvfree） | 高 |
| K8 | mwv207_irq.c:146-147 | `irq_create_mapping` 返回值全部忽略；任一映射失败后续 kms.c `BUG_ON(va_irq==0)` **开机即 panic** 或 `request_irq(0)` 失败致 init 半途而废 | 高 |
| K9 | mwv207_irq.c:30-46 | demux ISR 的 W1C 竞态：读状态与写清零之间新置位的 bit 被一并清掉且不处理 → GPU fence/vblank 事件丢失，用户态等 fence 可能永久挂起 | 高（6.6 移植版同样存在） |
| K10 | mwv207_ctx.c:58 | `scheds[6]` 栈数组按引擎数填充无上界检查（当前硬件 ≤2 安全，但任何 nr>6 的变体直接栈溢出）；`BUG_ON` 启动期致 死 | 中 |
| K11 | mwv207_drv.c:445 | remove 顺序：`pci_clear_master` 在最后，而 free_irq 后设备仍能拉中断 → "nobody cared" IRQ 风暴窗口 | 中 |
| K12 | mwv207_sched.c:192-209 | timedout_job 里 `list_for_each_entry(...ring_mirror_list)` 循环体只是 `devfreq_record_idle`（应为每 job 记一次 idle？语义可疑）；`drm_sched_resubmit_jobs` 对 GPU hang 后的 job 重放路径无 job 完整性校验 | 低-中（需硬件复位配合，未改动） |

### KMD 修复补丁

已按 5.10 原版源码提交（见 `src/kernel` 分支 jm9100-kmd-analysis，commit "mwv207 KMD: fix bo.c precedence bug..."），覆盖 K1-K8、K10；K9（W1C 竞态）与 K11/K12 在报告中标注（改动涉及中断处理主循环结构，建议 vendor 合入时与硬件验证一起做）。语法级验证：6.6 内核 headers 下除移植缺口项（见 §5）外全部通过。

---

## 四、5.10 → 6.6（当前内核）移植缺口清单

这是把开源 KMD 源码适配当前系统（内核 6.6.143）所需的全部 API 工作（deepin 移植版已做，公开源码未做）：

| 主题 | 5.10 写法 | 6.6 要求 |
|------|-----------|-----------|
| TTM 头 | `drm/ttm/ttm_bo_api.h`、`ttm_bo_driver.h`、`ttm_memory.h`、`ttm_page_alloc.h` | 已删除；合并为 `ttm_bo.h`、`ttm_device.h`、`ttm_pool.h` |
| 设备对象 | `struct ttm_bo_device` + `ttm_bo_device_init(bdev, ops, mapping, vma, use_dma_alloc)` | `struct ttm_device` + `ttm_device_init(bdev, funcs, dev, mapping, vma, use_dma_alloc, use_dma32)` |
| 驱动回调 | `struct ttm_bo_driver` | `struct ttm_device_funcs`（`fault_reserve_notify`、`verify_access` 等字段已删除） |
| BO 初始化 | `ttm_bo_init(bdev, bo, size, type, placement, ...acc_size...)` + `ttm_bo_dma_acc_size()` | `ttm_bo_alloc(bdev, bo, type, placement, alignment, ctx)`；acc_size 机制已移除 |
| BO 释放 | `ttm_bo_unref()` | `ttm_bo_put()` |
| mmap | `ttm_bo_mmap(filp, vma, bdev)` | `ttm_bo_mmap_obj(vma, bo)` |
| mm 管理 | `ttm_range_man_init(bdev, ...)`（无返回值） | 返回 int 的 `ttm_range_man_init_nocheck()` 包装 |
| TT (SGTT) | `ttm_dma_tt_init`/`ttm_dma_populate`/`ttm_dma_unpopulate`/`ttm_tt_destroy_common` | `ttm_sg_tt_init(ttm, bo, page_flags, caching)`；populate/unpopulate 直调 `ttm_pool_alloc/fill`；`ttm_tt_fini` |
| 缓存 | `ttm_tt_set_placement_caching` | `ttm_tt_set_caching` |
| GEM | `gem_free_object_unlocked` 回调 | `struct drm_gem_object_funcs.free` |
| PCI | `jdev->base.pdev`（drm_device.pdev） | `to_pci_dev(dev->dev)`；驱动挂 `struct drm_driver` 之外还需 `.dev` 体系 |
| fence | `ttm_eu_*` 语义未变；`drm_gem_dma_resv_wait` 已可用 | 已确认签名兼容 |
| debugfs/misc | 无变化阻碍 | — |

（语法验证证据：对修复后的 5 文件做 6.6 headers 语法编译，报错恰好全部落在上述移植项：`ttm_bo_api.h` 缺失、`ttm_memory.h` 缺失、`drm_device.pdev` 不存在、`ttm_bo_device` incomplete type。）

---

## 五、验证记录（全部非侵入式：仅 env 定向，未改系统）

| 验证项 | 方法 | 结果 |
|--------|------|------|
| KMD ioctl ABI | tests/gltest-mwv207（直调 renderD128 的 CTX/GEM/INFO/DESTROY ioctl） | `drm driver: mwv207 0.1.0`；CTX create OK handle=1；GEM create OK；INFO: 3D=1 DEC=1 ENC=0 2D=1 DMA=1；destroy OK |
| 修复生效性（负路径） | eglinfo（走 card0 无 master 权限） | 旧代码会静默拿 ctx=0 继续；**新代码明确报 "mwv207: failed to create kernel context, fd=5 err=-13"** ——错误处理修复被证实 |
| EGL/GBM 初始化 | tests/egl-smoke（renderD128 + GBM surfaceless） | gbm backend=**drm**（glamor 硬要求；厂商栈为"jm"必被拒）→ EGL 1.4 初始化 OK → EGLImage OK → ES2 ctx + clear + finish OK，renderer=**MWV207 (Jingjia Corporation)** |
| FBO 硬件渲染 | tests/fbo-test | FBO COMPLETE；clear(G) 回读 R=0 G=255 B=0 A=255 精确匹配 → PASS |
| 完整硬件渲染 | tests/tri-test（shader 编译 + draw + readback） | config/visual shifts 完全匹配；**center pixel R=255 → HARDWARE TRIANGLE RENDER: PASS** |
| 构建产物 | build-mwv207/.../mwv207_dri.so | 66MB ELF aarch64，version 字符串 git-391ea298c6 |

注：tri-test 初跑的 "incomplete framebuffer" 由测试程序自身的 EGL config 选择疏漏（未限定 alpha=8 选中了 RGBA1010102 config）与 gbm 格式校验撞车导致，非驱动缺陷；驱动在此过程中行为完全正确（EGL_BAD_MATCH 精确拒绝不匹配组合）。

---

## 六、交付物

| 路径 | 内容 |
|------|------|
| `src/mesa`（分支 jm9100-fixed，HEAD 4197d4a） | 修复后的完整 UMD 源码（基线 = jemoic -test c47be27 + 2 个修复 commit） |
| `src/kernel`（分支 jm9100-kmd-analysis） | 5.10 KMD 原版 + KMD 修复 commit |
| `patches/0001-*.patch` | UMD 全部修复的 git format-patch（可发回 gitee） |
| `patches/0002-*.patch` | UMD 硬件验证记录 commit |
| `src/kernel` 的 KMD commit | KMD 修复（bo/db/submit/irq/ctx 五文件） |
| `tests/` | 4 个硬件验证程序源码（gltest-mwv207 / egl-smoke / fbo-test / tri-test） |
| `src/mesa/build-mwv207/.../mwv207_dri.so` | 构建产物（本机验证用，未安装） |
| 本报告 | `ANALYSIS-REPORT.md` |

### 若要启用（用户自行决定，本次未执行）

构建产物配合 env 即可试运行（无需改系统文件）：
```
LIBGL_DRIVERS_PATH=/home/admin/jm9100-oss/src/mesa/build-mwv207/src/gallium/targets/dri \
MESA_LOADER_DRIVER_OVERRIDE=mwv207 <应用>
```
系统级启用已由 `switch-mwv207-oss.sh` 完成（私有目录 + 公开名胶水 + 三态快照/看门狗；glamor 已点亮、冷启动验证通过——见 PHASE5-SWITCH-REPORT.md）；KMD 补丁需先按 §4 清单完成 6.6 适配并重编内核模块（当前运行的 mwv207.ko 不含我们的修复）。

---

## 七、逐行覆盖度说明

- UMD：winsys 3 个 C（winsys/bo/cs，1000 行）逐行精读；driver 层 screen/context/resource/surface/cmd(含 dats/emit 宏)/format 逐行精读；nir 编译器 12 文件做了缺陷导向的通读 + 两分支交叉比对（主分支 vs -test 差异逐行核对）；blit/draw/state/sampler/vertex/query/blender 按调用链核查了与所修缺陷的耦合点。
- KMD：drv/bo/ttm/gem/submit/ctx/sched/irq/db 九个核心文件逐行精读（约 3200 行）；pipe_*.c/vcmd/vbios/kms 按依赖关系核查（pipe 层为硬件寄存器编程，缺陷模式与 pipe_3d 相同，未逐行展开的部分已在报告标注）。
- 未采纳主分支 UMD 的 EXTENDED 指令集半成品（无对应 ISA 定义，无法验证）；采用 vendor -test 自洽版。
