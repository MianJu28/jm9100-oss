# 08 — MWV207 内核态驱动（KMD）实现文档

> 对象：景美 JM9100 / MWV207 GPU 的 Linux 内核态驱动，源码位于
> `src/kernel/drivers/gpu/drm/mwv207/`（10 260 行，含 `dc/` 与 `selftest/`）。
> 本文只记录**互操作性所必需的事实**：文件、函数名、结构体字段、寄存器名/地址/位域。
> 一律以 `文件:行号` 给出可追溯出处；无法从源码确认的写「未确认」。

## 0 本树状态与阅读方法

### 0.1 源码来源与版本

| 项 | 内容 | 出处 |
|---|---|---|
| 来源 | gitee `jemoic/phytium-linux-kernel`，分支 `linux-5.10-mwv207` | `src/kernel/drivers/gpu/drm/mwv207/mwv207_drv.c:41-45`（DRIVER_NAME/DESC/DATE 20220407） |
| 本树 git | `bd02d5d` 导入原版 → `0ec4816` 修复 commit（bo/db/submit/irq/ctx）→ `d94c106` devfreq softdep | `git -C src/kernel log --oneline` 顶部三条 |
| 内核 API 基线 | **仍是 5.10 的 TTM / GEM API**，未做 6.6 移植 | `mwv207.h:21` 含 `<drm/ttm/ttm_bo_driver.h>`；`mwv207_bo.h:20-24`；`mwv207_ttm.c:21-25` |
| 编译单元 | 模块名 `mwv207`，28 个 .o | `Makefile` |
| Kconfig | `config DRM_MWV207`（tristate），select `DRM_KMS_HELPER / SND_PCM / DRM_VRAM_HELPER / DRM_SCHED / DRM_TTM / DRM_TTM_HELPER` | `Kconfig` |
| PCI ID | vendor `0x0731`，device `0x9100` | `mwv207_drv.c:519-522` |
| 许可 | `MODULE_LICENSE("GPL")` | `mwv207_drv.c:533-547` |

> ⚠️ 任务书所述「已移植到内核 6.6」在本仓库源码中**不成立**：本树是 5.10 基线 + 5 处修复 + softdep。系统上真实加载的 6.6 版 `.ko` 的源码不在本仓库（同 `docs/api/06-KNOWN-ISSUES.md` 的说明）。因此第 4.7 节写的是「本树实际写法」+「6.6 需替换项」。

### 0.2 覆盖度

| 覆盖级别 | 文件 |
|---|---|
| 逐行通读（本文主体） | `mwv207.h`、`mwv207_drm.h`、`mwv207_drv.c`、`mwv207_ttm.c`、`mwv207_bo.c/.h`、`mwv207_gem.c/.h`、`mwv207_ctx.c/.h`、`mwv207_submit.c`、`mwv207_sched.c/.h`、`mwv207_irq.c/.h`、`mwv207_db.c/.h`、`mwv207_devfreq.c`、`mwv207_vbios.c/.h`、`mwv207_vcmd.c`、`mwv207_pipe_3d.c`（提交/中断/创建）、`mwv207_pipe_2d.c`（中断/创建）、`mwv207_pipe_dec.c`、`mwv207_pipe_enc.c`（轮询线程/提交/创建）、`mwv207_pipe_dma.c`（提交/LLI/创建）、`selftest/selftest.c`、`Kconfig`、`Makefile`、`dc/mwv207_kms.c`、`dc/mwv207_va.c`、`dc/mwv207_hdmi.c`、`dc/mwv207_vga.c`、`dc/mwv207_dvo.c`、`dc/mwv207_edp.c`、`dc/mwv207_vi.c/.h`、`dc/mwv207_fb.c`、`dc/mwv207_i2c.c` |
| 依赖核查 | `mwv207_pipe_codec_common.c`（软后处理寄存器回读）、`mwv207_pipe_3d.c`/`2d.c` 前段（环形缓冲指针运算与命令打包） |
| UMD 契约 | `src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium/winsys/mwv207/drm/mwv207_drm.h`（215 行） |

### 0.3 与 `01-WINSYS-KMD.md` 的分工（互补，不重复）

| 主题 | 01-WINSYS-KMD.md（UMD 视角） | 本文（KMD 视角，互补面） |
|---|---|---|
| ABI 字段 | §9 逐字段表、`drm.h` 命令号对照、UMD/KMD 差异 | 只列 ioctl 号与**内核侧校验/回写**，不重复字段表 |
| 对象模型 | `mwv207_winsys` / UMD `mwv207_bo` 字段 | `mwv207_device` / `mwv207_ctx` / `mwv207_bo`（内核）字段 |
| 提交 | CS 命令流如何构造（用户态） | job 构建、reloc 打补丁、fence 产出、`drm_sched` |
| 缺陷 | W1–W12 + K1–K12 引用 | **K8/K9/K12 补齐复核**，并给本树新发现 N1–N10 |
| 未覆盖 | 设备生命周期、TTM、中断、KMS、调试接口 | 本文 §2/§4/§6/§7/§8 |

### 0.4 图例

- 寄存器分三层，勿混用：
  1. **NT 空间字节地址**（`jdev_read/jdev_write`，`mwv207.h:105-123`）：如 `0x9b0140`；窗口从 `0x100000` 起（`mwv207.h:107` 的 `BUG_ON(reg <= 0x100000)`）。
  2. **管线窗口字节偏移**（`pipe_read/pipe_write`，`mwv207_sched.h:128-136`，基址 `jdev->mmio + regbase`）：如 3D 的 `0x10`。
  3. **显示窗口字节偏移**（`dc/mwv207_va.c` 的 `va_read/write`、`dc/mwv207_hdmi.c` 的 `readb/writeb`）：VA 基址 `0x990000 + 0x100*idx (+0x600/+0xC00)`（`va.c:29-31`）；HDMI 寄存器线性空间 `0x1200000 + 0x40000*idx`（`hdmi.c:22`）。
  4. 驱动命令流里的 **16 位寄存器号 = 厂商 `_Address >> 2`**（见 `docs/registers/REGISTERS.md`）；`mwv207_pipe_dec.c:167` 的 `reg = (cmd & 0xffff) * 4` 就是这一换算的内核侧体现。
- 「✅ 已修」= 本树源码中已修；「⚠️ 仍在」= 本树可复现；「❓ 未确认」= 无法从本树源码判定。

---

## 1 模块总览

### 1.1 编译单元与职责

| 文件 | 行数 | 职责（内核侧） | 关键入口 |
|---|---|---|---|
| `mwv207_drv.c` | 556 | PCI 驱动、probe/remove、BAR/IATU 映射、滑动窗口、VRAM 探测、PM、drm_driver 定义 | `mwv207_pci_probe:321`、`mwv207_pci_remove:432` |
| `mwv207_irq.c` | 199 | irq_domain（64 线）、demux ISR、mask/unmask、挂起恢复 | `mwv207_irq_init:127`、`mwv207_isr:23` |
| `mwv207_db.c` | 144 | **能力键值数据库**（非寄存器接口），INFO ioctl | `mwv207_db_ioctl:108` |
| `mwv207_ctx.c` | 249 | 上下文与 `drm_sched_entity` 池、内核 DMA 实体 | `mwv207_ctx_ioctl:193`、`mwv207_kctx_init:231` |
| `mwv207_submit.c` | 653 | SUBMIT ioctl：job 构建、校验、reloc 打补丁、fence | `mwv207_submit_ioctl:612` |
| `mwv207_sched.c` | 378 | `drm_gpu_scheduler` 创建、引擎划分、超时/挂起 | `mwv207_sched_init:306` |
| `mwv207_gem.c` | 155 | 3 个 GEM ioctl、dumb buffer | `mwv207_gem_create_ioctl:89` |
| `mwv207_bo.c/.h` | 307/111 | `mwv207_bo` 生命周期、pin/kmap/reserve | `mwv207_bo_create:107` |
| `mwv207_ttm.c/.h` | 613/25 | `ttm_bo_driver` 回调、四个搬运路径、TTM 初始化 | `mwv207_ttm_init:570` |
| `mwv207_devfreq.c` | 279 | 每管线 devfreq（PERFORMANCE 调速器）+ sysfs | `mwv207_devfreq_register:159` |
| `mwv207_vbios.c/.h` | 189/51 | VBIOS flash 索引器校验、解析 HDMI PHY 配置、`vdat` 表 | `mwv207_vbios_init:160` |
| `mwv207_vcmd.c` | 178 | PLL 硬件命令通道（set/get PLL） | `mwv207_vbios_set_pll:125` |
| `mwv207_pipe_3d.c` | 613 | 3D 管线：环形缓冲提交、事件位图、fence、ISR | `mwv207_pipe_3d_create:527`、`_isr:424` |
| `mwv207_pipe_2d.c` | 567 | 2D BLT 管线：环形缓冲、16 位序列号 fence、ISR | `mwv207_pipe_2d_create`、`_isr:400` |
| `mwv207_pipe_dec.c` | 264 | 视频解码：命令解码、**轮询内核线程**推进 fence | `mwv207_pipe_dec_create:232`、`pipe_dec_polling:81` |
| `mwv207_pipe_enc.c` | 266 | 视频编码：同上 | `mwv207_pipe_enc_create:234`、`pipe_enc_polling:90` |
| `mwv207_pipe_dma.c` | 418 | 显存搬运引擎：LLI 链表、忙等完成、stub fence | `mwv207_pipe_dma_create:377`、`_submit:329` |
| `mwv207_pipe_codec_common.c` | 87 | codec 软后处理（sw_pp）寄存器回读 | `pipe_codec_sw_pp_submit:63` |
| `selftest/selftest.c` | 107 | 启动自检（BO 可读写） | `mwv207_test:98` |
| `dc/mwv207_kms.c` | 204 | mode_config、atomic commit tail、vblank irq、fbdev 挂钩 | `mwv207_kms_init:157` |
| `dc/mwv207_va.c` | 719 | CRTC/plane/光标/gamma LUT/vblank | `mwv207_va_init:707` |
| `dc/mwv207_hdmi.c` | 1086 | HDMI：PHY 配置表、AVI/VSIF/HDR infoframe、SCDC、HPD 中断 | `mwv207_hdmi_init:1072` |
| `dc/mwv207_vga.c` | 289 | VGA DAC + 负载检测 | `mwv207_vga_detect_ctx` |
| `dc/mwv207_dvo.c` | 180 | DVO/DVI 输出 | `mwv207_dvo_*` |
| `dc/mwv207_edp.c` | 22 | **空实现**（只打印 `mwv207_edp_init TBD`） | `mwv207_edp_init:18-22` |
| `dc/mwv207_vi.c/.h` | 89/101 | 输出公共层：connector helper、EDID、I2C 生命周期 | `mwv207_vi_init:69` |
| `dc/mwv207_fb.c` | 181 | fbdev（CFB 软件加速） | `mwv207_fb_create:43` |
| `dc/mwv207_i2c.c` | 312 | DDC 位磕 I2C（GPIO 复用切换） | `mwv207_i2c_create:240` |

### 1.2 模块加载参数

| 参数 | 类型/权限 | 默认 | 作用 | 出处 |
|---|---|---|---|---|
| `selftest` | int 0644 | 0 | probe 末尾跑 `mwv207_test()` | `mwv207_drv.c:55-57`、`:404` |
| `dump_hang_job` | int 0644 | 1 | 超时时打印管线状态 + job 命令流 | `mwv207_sched.c:24-26`、`:196-199` |
| `mwv207_vga_load_detect` | uint 0444 | 1 | 仅 lite 芯片启用 VGA 负载检测 | `dc/mwv207_vga.c:26-28` |
| `softdep` | — | — | `pre: governor_performance`（否则 devfreq 注册失败、GPU 停在低频） | `mwv207_drv.c:548-556` |

### 1.3 `struct mwv207_device`（`mwv207.h:41-91`）

| 字段 | 类型 | 含义 / 使用者 |
|---|---|---|
| `base` | `struct drm_device` | DRM 主体；`dev_private` 回指本结构（`mwv207.h:39` 宏 `ddev_to_jdev`） |
| `bdev` | `struct ttm_bo_device` | **5.10 类型**；`bdev_to_jdev` 宏（`:40`）；`mwv207_ttm.c` 全篇 |
| `mmio` | `void __iomem *` | BAR1 映射，NT 寄存器空间基址（`drv.c:347`） |
| `iatu` | `void __iomem *` | BAR4 映射，IATU 寄存器（`drv.c:348`） |
| `win` | `void *` | BAR2 尾部 64 KiB 滑动窗口（`drv.c:349`、`WIN_SIZE=0x10000` `:47`） |
| `win_lock` | `spinlock_t` | 保护滑动窗口（`drv.c:243-260`） |
| `visible_vram_size` | `u64` | BAR 可见显存 = `pci_resource_len(2) - WIN_SIZE`（`drv.c:372`） |
| `vram_size` | `u64` | 探测总显存（`drv.c:283-319`） |
| `vram_bar_base` | `u64` | BAR2 起始物理地址（`drv.c:371`） |
| `vram_last_win` / `pci_win_base` | `u64` | 滑动窗口当前/基准（`drv.c:218`、`:227-241`） |
| `dev` | `struct device *` | devm 分配源（`:345`） |
| `fb_helper` | `struct drm_fb_helper *` | fbdev（`dc/mwv207_fb.c:127-156`） |
| `sched[6]` | `struct drm_gpu_scheduler *[6]` | **仅 6 个槽**（`:61`），见 §5.6 的槽位碰撞问题 |
| `sched_3d/dec/enc/2d/dma` | `struct drm_gpu_scheduler **` | 各类引擎基址（`:63-67`，赋值见 `sched.c:310-314`） |
| `nr_3d/2d/dec/enc/dma` | `int` | 引擎数量，写入 INFO 数据库（`sched.c:351-355`、`:357-361`） |
| `dma_entity` | `struct drm_sched_entity *` | TTM 搬运用内核实体（`ctx.c:231-244`） |
| `irq_lock` | `spinlock_t` | 保护 mask/unmask 与影子寄存器（`irq.c:138`） |
| `irq_domain` | `struct irq_domain *` | 64 线线性域（`irq.c:140-143`） |
| `irq_enable_reg[2]` | `u32[2]` | 中断使能影子（`irq.c:64`、`:78`、`:31`） |
| `va_irq` | `int` | CRTC vblank 的 hwirq 42（`kms.c:171`） |
| `db` | `struct mwv207_db *` | 能力库（`db.c:47`） |
| `vbios` | `struct mwv207_vbios` | 见下 |
| `gpio_lock` | `struct mutex` | DDC I2C 串行化（`vi.c:71`、`i2c.c:269`） |
| `lite` | `bool` | 精简版芯片：2 CRTC、1 个 3D/DEC 引擎（`drv.c:365`） |

`struct mwv207_vbios`（`mwv207.h:27-37`）：`vcmd_lock`、`cfg_lock`（互斥）、`cfg_table`（`struct idr`，键 `0xfff0+i` 存 HDMI PHY 数据）、`indexer_valid`、`sector_count`。

### 1.4 上下文结构（`mwv207_ctx.h:22-36`、`mwv207.h:93-100`）

| 结构 | 字段 | 含义 |
|---|---|---|
| `struct mwv207_ctx` | `refcnt`（`kref`） | 由 ioctl 句柄与 job 各持一份 |
| | `entities[11]` | 全部 `drm_sched_entity`，布局见 §5.7 |
| | `entity_3d/dec/enc/2d/dma` | 各引擎实体数组基址（`ctx.c:97-130`） |
| | `jdev` | 回指设备 |
| | `guilty`（`atomic_t`） | 传给 `drm_sched_entity_init` 的 guilty 计数（`ctx.c:74`） |
| `struct mwv207_ctx_mgr` | `lock`、`handle_table`（`idr`） | 句柄分配（id 从 1 开始，`ctx.c:148`） |
| `struct mwv207_fpriv` | `ctx_mgr` | 挂在 `drm_file->driver_priv`（`drv.c:59-72`） |

### 1.5 显存/提交对象（摘要，字段表细节见 §4/§5）

| 结构 | 定义 | 要点 |
|---|---|---|
| `struct mwv207_bo` | `bo.h:31-44` | `tbo`、`placement`、`kmap`、`placements[3]`、`domain`、`flags`、`pin_count`、`map_count`、`kptr` |
| `struct mwv207_ttm_tt` | `bo.h:46-49` | 内嵌 **5.10 的 `struct ttm_dma_tt`** + `jdev` |
| `struct mwv207_job` | `sched.h:40-68` | `base`、`tvblist`、`mtvb`、`nr_bos`、`relocs/nr_relocs`、`cmd_dats/nr_cmd_dat`、`cmds/cmd_size`、`fence_in`、`ctx`、`engine_entity`、`refcount`、`is_dma`、`comm[TASK_COMM_LEN]` |
| `struct mwv207_tvb` | `sched.h:32-37` | `base`（`ttm_validate_buffer`）、`excl`、`nr_shared`、`shared` |
| `struct mwv207_submit_cmd_dat` | `sched.h:24-30` | 内核侧 cmd_dat：`nr_relocs`、`dat_size`、`relocs`、`dat`、`jbo` |
| `struct mwv207_dma_loc` / `mwv207_dma_cmd` | `sched.h:81-98` | 搬运描述：`base`、`pg_nr_type`（bit63 标记 VRAM/RAM）、`offset`、`stride` + `width/height` |
| `struct mwv207_pipe` | `sched.h:102-121` | 函数指针 `submit/reset/destroy/dump_state`、`regbase`、`iosize`、devfreq 状态、`busy_count`、`pll_id` |
| `struct mwv207_sched` | `sched.h:123-126` | 内嵌 `struct drm_gpu_scheduler base` + `pipe` |
| `struct mwv207_db` | `db.c:23-29` | `nr`、`max_nr`（2048）、`sorted`、`db[]` |

---

## 2 设备生命周期

### 2.1 驱动入口

| 项目 | 内容 | 出处 |
|---|---|---|
| 模块初始化 | `pci_register_driver(&mwv207_pci_driver)` / `pci_unregister_driver` | `mwv207_drv.c:533-547` |
| `file_operations` | `.open=drm_open`、`.mmap=mwv207_mmap`、`.unlocked_ioctl=drm_ioctl`、`.compat_ioctl`、`.poll`、`.read`、`.release` | `mwv207_drv.c:96-108` |
| `drm_driver` | `DRIVER_MODESET\|DRIVER_ATOMIC\|DRIVER_GEM\|DRIVER_RENDER`；`dumb_create/map_offset/destroy`；**`.gem_free_object_unlocked`（5.10 回调）** `:129`；prime 系列用 DRM 通用实现 `:131-135` | `mwv207_drv.c:119-145` |
| 每文件私有 | `open` → `kzalloc(mwv207_fpriv)` + `mwv207_ctx_mgr_init`；`postclose` → `ctx_mgr_fini` + `kfree` | `mwv207_drv.c:59-81` |
| `mmap` 实现 | 取 `jdev` 后调 **`ttm_bo_mmap(filp, vma, &jdev->bdev)`**（5.10 API） | `mwv207_drv.c:83-94` |

### 2.2 probe 顺序（`mwv207_drv.c:321-430`）

| # | 步骤 | 行 | 失败处理 |
|---|---|---|---|
| 1 | `mwv207_kick_out_firmware_fb`（alloc_apertures + `drm_fb_helper_remove_conflicting_framebuffers("mwv207drmfb")`） | `:327`、`:262-281` | 忽略返回值 |
| 2 | `pci_set_master` | `:331` | 忽略 |
| 3 | `pcim_enable_device` | `:332` | 返回错误 |
| 4 | `devm_drm_dev_alloc(&pdev->dev, &mwv207_drm_driver, struct mwv207_device, base)` | `:336` | 返回错误 |
| 5 | 设 `jdev->dev`、`jdev->base.dev_private`、**`jdev->base.pdev = pdev`（5.10 字段）** | `:342-343` | — |
| 6 | `devm_ioremap` BAR1→`mmio`、BAR4→`iatu`、BAR2 尾部 `WIN_SIZE`→`win` | `:345-357` | ERR_PTR 检查 |
| 7 | `mwv207_iatu_init` | `:360` | — |
| 8 | `jdev->lite = (jdev_read(jdev, 0x12c0000) == 0x21) ? false : true` | `:365` | — |
| 9 | `pci_set_drvdata` | `:367` | — |
| 10 | `mwv207_db_init`（2048 项能力库） | `:369` | **直接 return，不清理**（见 §2.7） |
| 11 | `mwv207_vbios_init` | `:373` | 同上 |
| 12 | `mwv207_irq_init`（irq_domain + 64 映射 + MSI/legacy 向量 + `request_irq`） | `:375` | `goto err_vbios` |
| 13 | 计算 `visible_vram_size`（`pci_resource_len(2) - WIN_SIZE`）、`vram_size`（`mwv207_vram_size()`）、`vram_bar_base` | `:379-381` | — |
| 14 | `mwv207_ttm_init` | `:383` | `goto err_irq` |
| 15 | `mwv207_sched_init`（5 类 7 个管线） | `:387` | `goto err_ttm` |
| 16 | `mwv207_kctx_init`（内核 DMA 实体） | `:391` | `goto err_sched` |
| 17 | `mwv207_kms_init`（va → vi → vblank irq → vblank init） | `:395` | `goto err_kctx` |
| 18 | `selftest` 参数为真时 `mwv207_test` | `:399-403` | `goto err_kms` |
| 19 | `mwv207_db_sort` | `:405` | — |
| 20 | `drm_dev_register` | `:406` | `goto err_kms` |
| 21 | `mwv207_fbdev_init` | `:409` | `goto err_drm` |

错误标签（`:414-429`）：`err_drm: drm_dev_unregister` / `err_kms: mwv207_kms_fini` / `err_kctx: mwv207_kctx_fini` / `err_sched: mwv207_sched_fini` / `err_ttm: mwv207_ttm_fini` / `err_irq: mwv207_irq_fini` / `err_vbios: mwv207_vbios_fini + mwv207_db_fini`。

> 两个次序错位注意：① `mwv207_kick_out_firmware_fb`（`:327`）排在 `pcim_enable_device`（`:332`）之前；② **`err_vbios` 路径会调用 `mwv207_db_fini`，而 `err_kms/err_kctx/err_sched/err_ttm/err_irq` 路径都不会**，即从第 14 步往后失败时 `db` 内存交由 `devm` 回收（`db.c:31-48` 用 `devm_kmalloc`），不构成泄漏，但 `db_fini` 的悬垂指针防护（K5）在那些路径上未执行。

### 2.3 BAR / IATU / 滑动窗口（`mwv207_drv.c:147-260`）

| 机制 | 关键寄存器 | 说明 | 行 |
|---|---|---|---|
| IATU 寄存器访问 | 基址 `jdev->iatu + 0x10000 + index*0x200 + offset` | `iatu_write/iatu_read` | `:147-157` |
| 滑动窗口 | `0x108`（LOW）、`0x10c`（HIGH）、`0x114`（胜利者）、`0x118`、`0x110`（LIMIT）、`0x100`、`0x104`（`1<<31 \| mode<<30 \| 6<<8`） | `mwv207_win_slide`；`mb()` 后回读 `0x118` 校验，失败 `pr_warn("mwv207: sliding window failed")` | `:159-179` |
| BAR 映射 | region `bar + 9`，`0x104 = 1<<31 \| 1<<30 \| bar<<8` | `mwv207_iatu_map_bar`，回读 `0x114` 校验失败返回 `-ENODEV` | `:181-197` |
| 初始化 | bar0→`0x10000000`、bar1→`0x0`、bar2→`0x10000000` | `spin_lock_init(&win_lock)`；`pci_win_base = pci_bus_address(pdev,2) + pci_resource_len(pdev,2) - WIN_SIZE` | `:199-225` |
| 窗口重映射 | 以 `WIN_SIZE` 对齐并截断 | `mwv207_remap_region` | `:227-241` |
| VRAM 写 | `spin_lock(win_lock)` + `memcpy_toio` | `jdev_write_vram` | `:243-260` |

### 2.4 VRAM 探测（`mwv207_drv.c:283-319`）

| 探测项 | 寄存器/位 | 取值 |
|---|---|---|
| 单颗粒容量 | `0x9b0190` 低 3 位 | 0→2048 MB、1→1024 MB、2→4096 MB、3→8192 MB、default→256 MB（`pr_warn "ddr size default to 256MB"`） |
| 颗粒数（lite） | `0xd101bc & 1` + `0xd11cbc & 1` | 两路相加 |
| 颗粒数（full） | `0x9702f4 & 0x02000000` + `0x9802f4 & 0x02000000` | 非零计 1 |
| 兜底 | — | `nr == 0` → 1（`pr_warn "ddr cnt default to 1"`） |
| 结果 | — | `size(MB) * 1024 * 1024 * nr` |

### 2.5 lite / full 判定（`mwv207_drv.c:365`）

读 `0x12c0000`，等于 `0x21` → `lite = false`（full），否则 `lite = true`。该地址等于 HDMA 3 号单元的基址：`MWV207_HDMI_BASE(3) = 0x1200000 + 0x40000*3 = 0x12c0000`（`dc/mwv207_hdmi.c:22`、`:1078`），即**用「第 4 个 HDMI 是否存在」来判断精简版**。

|lite|影响|
|---|---|
|`true`|CRTC/plane = 2（`va.c:709`）；`pipe_3d_create(unit=1)` 返回 NULL（`pipe_3d.c:535`）；`pipe_dec_create(unit=1)` 返回 NULL（`pipe_dec.c:237`）；`pipe_enc_create` 全返回 NULL（`pipe_enc.c:239`）；DMA 对齐 32 而非 64（`pipe_dma.c:399`）；VGA 负载检测生效（`vga.c:71-85`）|
|`false`|CRTC/plane = 4；3D/DEC 各 2 个；ENC 1 个；DMA 对齐 64 |

### 2.6 remove 顺序（`mwv207_drv.c:432-447`）与 K11

| 步 | 调用 | 行 |
|---|---|---|
| 1 | `mwv207_fbdev_fini` | `:436` |
| 2 | `drm_dev_unregister` | `:437` |
| 3 | `mwv207_kms_fini`（poll_fini → `drm_atomic_helper_shutdown` → `free_irq(va_irq)` → `drm_mode_config_cleanup`，`kms.c:198-204`） | `:438` |
| 4 | `mwv207_kctx_fini` | `:439` |
| 5 | `mwv207_sched_fini`（6 槽各 `drm_sched_fini` + `pipe->destroy`） | `:440` |
| 6 | `mwv207_ttm_fini` | `:441` |
| 7 | `mwv207_irq_fini`（`free_irq(pdev->irq)` → 释放向量 → 64 个 `irq_dispose_mapping` → `irq_domain_remove`） | `:442` |
| 8 | `mwv207_vbios_fini` | `:443` |
| 9 | `mwv207_db_fini` | `:444` |
| 10 | **`pci_clear_master(pdev)`** | `:445` |
| 11 | `pci_set_drvdata(pdev, NULL)` | `:446` |

顺序评估：

- **与 probe 基本逆序，但 `kms_fini` 先于 `irq_fini`**，这不是笔误：`va_irq` 由 kms 自己 `request_irq`（`kms.c:173`），必须在 `irq_domain` 拆除（`irq.c:184-199`）之前释放，否则 `free_irq` 会作用到已析构的域。因此**该顺序在中断耦合上是正确的**。
- **K11（⚠️ 本树仍在）**：`pci_clear_master` 位于第 10 步，即 `free_irq` 之后。`pci_clear_master` 的作用是清 `PCI_COMMAND_MASTER`、切断设备发起 DMA/中断请求的能力；放在最后意味着从 `free_irq` 到 `pci_clear_master` 之间存在一个窗口，设备仍可拉中断但没有任何 ISR 认领 → 共享中断线上出现未处理中断（`nobody cared` / `irq N: nobody cared`）。判定：**K11 = 仍在（本树确认，`mwv207_drv.c:445`）**，原报告（5.10）行号一致。
- 严格逆序还差一点：probe 中 `db_init`（步 10）早于 `vbios_init`（步 11），remove 中 `vbios_fini` 早于 `db_fini`，一致，无需调整。

### 2.7 生命周期中值得记录的不对称/缺陷

| # | 现象 | 证据 |
|---|---|---|
| A1 | `err_drm` 只 `drm_dev_unregister`，不回退 `mwv207_fbdev_fini`；但 `mwv207_fbdev_init` 失败时自身已是失败方 | `drv.c:406-411`、`:414-415` |
| A2 | `db_init`/`vbios_init` 失败直接 `return ret`，跳过清理 | `drv.c:369-373` |
| A3 | `mwv207_mmap` 未校验 `filp->private_data`/`minor` 为空 | `drv.c:83-94` |
| A4 | 悬挂于 devfreq 的 `governor_performance` 依赖 softdep；缺失时 probe 失败 | `drv.c:548-556`、`devfreq.c:192-198` |
| A5 | `mwv207_sched_fini` 只在 6 个槽上销毁，**丢弃 dec1 的 scheduler**（见 §5.6 N1） | `sched.c:369-378`、`:310-314` |

### 2.8 电源管理（`mwv207_drv.c:449-512`）

| 阶段 | 顺序 |
|---|---|
| suspend | `mwv207_kms_suspend` → **`ttm_bo_evict_mm(&jdev->bdev, TTM_PL_VRAM)`** → `mwv207_sched_suspend` → `mwv207_irq_suspend` → `pci_save_state` → `pci_disable_device` → `pci_set_power_state(PCI_D3hot)` → `udelay(200)`（`:449-480`） |
| suspend 失败 | `fail:` 回滚 `mwv207_kms_resume`（`:474-478`） |
| resume | `msleep(100)` → `PCI_D0` → `pci_restore_state` → `pci_enable_device` → `pci_set_master` → **`mwv207_iatu_init`** → `mwv207_irq_resume` → `mwv207_sched_resume` → `mwv207_kms_resume`（`:482-510`） |
| ops | `SIMPLE_DEV_PM_OPS(mwv207_pm_ops, ...)`（`:512`） |

关键点：

| 机制 | 说明 | 行 |
|---|---|---|
| `mwv207_sched_suspend` | 遍历 6 槽，`atomic_read(&sched[i]->hw_rq_count) != 0` 即返回 `-EBUSY`（有在飞 job 就不休眠）；随后 `devfreq_suspend` | `sched.c:264-284` |
| `mwv207_sched_resume` | 遍历 6 槽 `devfreq_resume` + `pipe->reset`（重写寄存器与环形缓冲指针） | `sched.c:286-304` |
| `mwv207_irq_suspend/resume` | 读/写 `0x009A8020 + i*4` 使能寄存器到影子；resume 先向 `0x009A802c + i*4` 写 `0xffffffff` 清 pending | `irq.c:83-101` |
| devfreq `suspend_freq` | 固定 `MIN_FREQ = 50`（MHz） | `devfreq.c:202`、`:21` |

---

## 3 ioctl 内核实现（逐个）

### 3.1 命令号与分发表

| ioctl | 号 | 类型 | 处理器 | 门控 |
|---|---|---|---|---|
| `DRM_IOCTL_MWV207_INFO` | `DRM_COMMAND_BASE + 0x00` | `struct drm_mwv207_info` | `mwv207_db_ioctl` | `DRM_AUTH\|DRM_RENDER_ALLOW` |
| `DRM_IOCTL_MWV207_GEM_CREATE` | `+0x01` | `union drm_mwv207_gem_create` | `mwv207_gem_create_ioctl` | 同上 |
| `DRM_IOCTL_MWV207_GEM_MMAP` | `+0x02` | `union drm_mwv207_gem_mmap` | `mwv207_gem_mmap_ioctl` | 同上 |
| `DRM_IOCTL_MWV207_GEM_WAIT` | `+0x03` | `struct drm_mwv207_gem_wait` | `mwv207_gem_wait_ioctl` | 同上 |
| `DRM_IOCTL_MWV207_CTX` | `+0x04` | `struct drm_mwv207_ctx` | `mwv207_ctx_ioctl` | 同上 |
| `DRM_IOCTL_MWV207_SUBMIT` | `+0x05` | `struct drm_mwv207_submit` | `mwv207_submit_ioctl` | 同上 |

出处：KMD 号定义 `mwv207_drm.h:150-155`；分发表 `mwv207_drv.c:110-117`。UMD 侧定义见 `mwv207_drm.h:197-209`（命令号完全一致）。全部为 `DRM_IOWR`，因此**入参由 DRM core 从用户态拷入、返回前拷回**——KMD 内部不做 ioctl 顶层 `copy_from_user`。

### 3.2 INFO — `int mwv207_db_ioctl(struct drm_device *, void *, struct drm_file *)`（`mwv207_db.c:108-144`）

| `args->op` | 语义 | 内核行为 | 返回值 |
|---|---|---|---|
| 0 | 枚举 / 问数量 | `args->recs != 0` → `copy_to_user(recs, db->db, nr * 8)`（`:130-131`）；`recs == 0` → 只回写 `args->nr_recs = db->nr`（`:136`） | 0 / `-EFAULT`（`:133`） |
| 1 | 按键查值 | 用 **`args->nr_recs` 当输入 key**（`rec.key = args->nr_recs; rec.val = 0`，`:118-119`），`bsearch`（`:120`）后把值写进 `args->recs`（`:125`） | 0 / `-ENOENT`（并把 `args->recs` 清零，`:121-124`） |
| 其它 | — | — | `-EINVAL`（`:140`） |

要点：

- **该 ioctl 不读写任何寄存器**：`mwv207_db` 是纯内核内存键值库（2048 项，`db.c:31-48`）。当前仅 5 个 key 被写入：`DRM_MWV207_ACTIVE_{3D,DEC,ENC,2D,DMA}_NR`（`sched.c:357-361`）。
- op=0/op=1 的入参复用是 ABI 的历史包袱：`nr_recs` 既是"条目数"又是"查询 key"，`recs` 既是"数组指针"又是"返回值"。**UMD 必须知道这一点**（`01-WINSYS-KMD.md` §9.8 有字段表，此处补充的内核侧事实是：op=1 时 key 来自 `nr_recs` 而非 `recs` 指向的结构）。
- K4 涉及的两点在本树都已修：`sort`/`bsearch` 显式传 `sizeof(*db->db)`（`db.c:97`、`:118`、`:120`），且 `rec.val` 被初始化（`db.c:119`）。
- 细节：`mwv207_db_add` 在已排序状态下每次插入都会重排（`db.c:56-74`），O(n log n)/次；`max_nr` 达到上限时 `WARN_ONCE("mwv207: maximum key count reached")` 并返回 `-ENOMEM`（被忽略）。

### 3.3 GEM_CREATE — `mwv207_gem_create_ioctl`（`mwv207_gem.c:89-116`）

```
int mwv207_gem_create_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
```

| 校验 | 条件 | 错误码 | 行 |
|---|---|---|---|
| size | `in.size == 0` | `-EINVAL` | `:93` |
| alignment | 必须 2 的幂：`alignment & (alignment - 1)` | `-EINVAL`（**0 可通过**） | `:96` |
| preferred_domain | `& ~0x7` 非零（只允许 SYSTEM/GTT/VRAM） | `-EINVAL` | `:99` |
| flags | `& ~(0x1\|0x2\|0x4)` 非零 | `-EINVAL` | `:102` |

| 流程 | 内容 | 行 |
|---|---|---|
| 分配 | `mwv207_gem_create(dev_private, size, align, preferred_domain, flags, &gobj)` | `:105`、`:21-50` |
| UMD/内核 flags 语义 | bit0 = CPU 可见（`MWV207_GEM_CREATE_CPU_VISIBLE`）、bit1 = CPU 不可见、bit2 = GPU 物理 32 位（`MWV207_GEM_CREATE_GPU_PHYS32`，内核用它要求必须 VRAM：`bo.c:110-112`） | `mwv207_drm.h:47-50`、`bo.c:110` |
| 句柄 | `drm_gem_handle_create(filp, gobj, &args->out.handle)` | `:109` |
| 引用释放 | `mwv207_gem_object_put(gobj)`（= `drm_gem_object_put`，`gem.h:36`） | `:112` |
| 回写 | **无 `copy_to_user`**，union 由 DRM core 按 `DRM_IOWR` 整体拷回；`out.resv` 不写（用户输入原样回传） | — |

分配失败的回退链（`gem.c:21-50`）：先原样重试一次；`-ERESTARTSYS` 以外的失败 → 去掉 flags bit0（放弃 CPU 可见）重试 → 再 `preferred_domain |= 0x1`（退回 SYSTEM）重试 → 仍失败 `DRM_DEBUG("Failed to allocate GEM object (...)")`。**因此 GEM_CREATE 成功返回的 BO，其 domain/CPU 可见性可能与请求不同**——UMD 不能用请求值推断了。

`mwv207_gem_dumb_create`（`gem.c:52-76`）：`pitch = ALIGN(width * DIV_ROUND_UP(bpp, 8), 64)`，size = pitch × height，用 align `0x10000` + VRAM + CPU 可见创建；失败路径在 `drm_gem_handle_create` 出错时只 `return ret`（此时 `gobj` 已被 put，`args->handle` 未清 —— 无害但残留）。

### 3.4 GEM_MMAP — `mwv207_gem_mmap_ioctl`（`mwv207_gem.c:118-134`）

| 步骤 | 内容 | 行 |
|---|---|---|
| 校验 | `in.pad != 0` → `-EINVAL` | `:121` |
| 查对象 | `drm_gem_object_lookup(filp, in.handle)`，失败 `-ENOENT` | `:124-126` |
| 取偏移 | `out.offset = drm_vma_node_offset_addr(&jbo->tbo.base.vma_node)`（宏 `mwv207_bo_mmap_offset`，`bo.h:69-74`） | `:129` |
| 引用 | `drm_gem_object_put(obj)`；返回 0 | `:132-134` |

用户态拿到的是 **DRM 假偏移**，实际建立映射发生在 `mmap(PROT_*, MAP_SHARED, fd, offset)` → `mwv207_mmap` → `ttm_bo_mmap`（`drv.c:83-94`）。对应 `01-WINSYS-KMD.md` 的 O 系列观察（offset 语义），内核侧补充：没有对 `offset` 做额外校验，非法 offset 由 TTM 的 `ttm_bo_mmap`/`verify_access` 拒绝。

### 3.5 GEM_WAIT — `mwv207_gem_wait_ioctl`（`mwv207_gem.c:136-155`）

| 项 | 内容 | 行 |
|---|---|---|
| 校验 | `op & ~(READ \| WRITE)` 非零 → `-EINVAL` | `:139` |
| 等待方向 | `write = op & 0x2`（`MWV207_GEM_WAIT_WRITE`） | `:142` |
| 超时换算 | `timeout = drm_timeout_abs_to_jiffies(args->timeout)`，交给 `drm_gem_dma_resv_wait(filp, handle, write, timeout)` | `:145-148` |
| 错误映射 | `-ETIME` → `args->timeout != 0 ? -ETIMEDOUT : -EBUSY` | `:150-153` |

内核实现走的是 **dma_resv（TTM/DRM 通用）等待**，不是自旋在 GPU 寄存器上；BO 上的 fence 由 §5 的 job/fence 机制填充。

### 3.6 CTX — `mwv207_ctx_ioctl`（`mwv207_ctx.c:193-210`）

| 项 | 内容 | 行 |
|---|---|---|
| 校验 | `args->resv != 0 \|\| args->flags != 0` → `-EINVAL` | `:196-198` |
| op 0（CREATE） | `mwv207_ctx_create(dev, mgr, flags, &args->handle)` | `:201`、`:132-163` |
| op 1（DESTROY） | `mwv207_ctx_destroy(dev, mgr, args->handle)` | `:204`、`:178-191` |
| 其它 | `-EINVAL` | `:207` |

| create 细节 | 内容 | 行 |
|---|---|---|
| 结构 | `kzalloc` + `kref_init` + `entity_init`（11 个实体，§5.7） | `:135-145` |
| 句柄 | `idr_alloc(&mgr->handle_table, ctx, 1, 0, GFP_KERNEL)`（**从 1 开始，0 非法**） | `:148` |
| 失败 | `out_fini_entites` 逐实体销毁 + `out_free` kfree | `:150-162` |
| destroy 细节 | `mutex_lock` → `idr_remove` → `unlock`；**表项不存在返回 `-EINVAL`**；`mwv207_ctx_put`（kref_put → `ctx_release` → entity_fini + kfree） | `:180-191`、`:165-176` |
| 进程退出 | `mwv207_ctx_mgr_fini` 遍历 idr 逐个 put + `idr_destroy` + `mutex_destroy` | `:219-229` |

**返回值语义**：CTX create 成功时 `args->handle` 是新句柄；destroy 成功返回 0。job 内部持 ctx 引用（`submit.c:293`），因此用户态 destroy 后仍在飞的 job 不会造成 UAF。

### 3.7 SUBMIT — `mwv207_submit_ioctl`（`mwv207_submit.c:612-653`）

顶层流程：

| 阶段 | 内容 | 行 |
|---|---|---|
| 校验 | `args->padding != 0` 或 `args->flags & ~0x3` → `-EINVAL`；`cmd_size == 0 \|\| cmds == 0` → `-EINVAL` | `:617-620` |
| 构建 | `mwv207_submit_init` → `mwv207_job_alloc` + `mwv207_submit_init_job`（失败 `job_put` + `ERR_PTR`） | `:622`、`:310-328` |
| 预留 | `ttm_eu_reserve_buffers(&ticket, &mjob->tvblist, true, NULL)` 失败 → `put_job` | `:627-631` |
| 校验显存 | `mwv207_submit_validate` | `:633-635` |
| 打补丁 | `mwv207_submit_patch` | `:637-639` |
| 依赖 | `mwv207_submit_attach_dependency`（写 `fence_in`、填各 tvb 的 excl/shared） | `:641-643` |
| 提交 | `mwv207_submit_commit`（`drm_sched_job_init` + out-fence fd + `drm_sched_entity_push_job`） | `:645-647` |
| 收尾 | `unreserve:` 有 fence → `ttm_eu_fence_buffer_objects(&ticket, &tvblist, fence)`，否则 `ttm_eu_backoff_reservation`；`put_job: mwv207_job_put` | `:649-652` |

各子阶段详见 §5.2–§5.5。**返回值语义**：0 表示 job 已入队；失败时返回负值，且若已挂上 out-fence fd 则不回收该 fd（见 N4 相关说明）。

### 3.8 参数校验 / uaccess 汇总

| ioctl | 校验项 | uaccess 处理 | 错误码 |
|---|---|---|---|
| INFO | op ∈ {0,1} | op=0 用 `copy_to_user`；**返回 `-EFAULT` 而非剩余字节数（K3 已修，`db.c:130-134`）** | `-EINVAL/-ENOENT/-EFAULT` |
| GEM_CREATE | size≠0、align 幂、domain、flags | 无（DRM core 代劳） | `-EINVAL` 等 |
| GEM_MMAP | `pad == 0` | 无 | `-EINVAL/-ENOENT` |
| GEM_WAIT | op 掩码 | 无 | `-EINVAL/-ETIMEDOUT/-EBUSY` |
| CTX | `resv/flags == 0`、op ∈ {0,1} | 无 | `-EINVAL` |
| SUBMIT | padding、flags 掩码、cmd_size/cmds、nr_* ≥ 0、指针域 | **5 处 `copy_from_user` 全部检查返回值并显式 `-EFAULT`（K6 已修）**：`submit.c:85-90`（reloc）、`:121-126`（bo 表）、`:181-182`（cmds）、`:216-221`（cmd_dat 描述）、`:249-254`/`:262-266`（cmd_dat 的 reloc 与 dat） | `-EINVAL/-EFAULT/-ENOENT/-E2BIG/-ERANGE/-ENOMEM/-ENODEV/-EBUSY` |

SUBMIT 的栈缓冲策略（影响最大参数）：

| 参数 | 栈上阈值 | 超出时 | 行 |
|---|---|---|---|
| `nr_bos` | 32（`ubos_stack[32]`） | `kvmalloc_array` | `submit.c:97`、`:103-118` |
| `nr_cmd_dat` | 8（`udats_stack[8]`） | `kvmalloc_array` | `submit.c:189`、`:200-214` |
| `cmd_size` | — | 直接堆分配；上限 `0x20000`（128 KiB）→ 超出 `-E2BIG` | `submit.c:163-171` |
| `cmd_dat.dat_size` | — | 上限 `0x20000` → `-E2BIG` | `submit.c:239-241` |

> 注意：`kvmalloc_array` 的返回值在 `submit.c:82`、`:243` 未判空（紧随其后的 `copy_from_user` 依赖 `access_ok` 兜住 —— 可改进项，非本树缺陷清单条目）。

---

## 4 显存管理（TTM）

### 4.1 初始化与区域（`mwv207_ttm.c:570-613`）

| 步骤 | 调用（**5.10 API**） | 行 |
|---|---|---|
| 设备初始化 | `ttm_bo_device_init(&jdev->bdev, &mwv207_bo_driver, jdev->base.anon_inode->i_mapping, jdev->base.vma_offset_manager, true)` | `:574-578` |
| GTT 区 | `ttm_range_man_init(&jdev->bdev, TTM_PL_TT, true, 0x80000000ULL >> PAGE_SHIFT)`（上限 2 GiB） | `:585-586` |
| VRAM 区 | `ttm_range_man_init(&jdev->bdev, TTM_PL_VRAM, false, jdev->vram_size >> PAGE_SHIFT)` | `:593-594` |
| 失败 | `out_no_vram:` → `ttm_bo_device_release` | `:597-604` |
| 成功日志 | `DRM_INFO("mwv207: %lldM of VRAM memory size", ...)` | `:602` |
| 反初始化 | `ttm_range_man_fini(VRAM)` → `ttm_range_man_fini(TT)` → `ttm_bo_device_release` → `DRM_INFO("mwv207: ttm finalized")` | `:607-613` |

可见性判定（`ttm.c:32-42`）：SYSTEM/TT 恒可见；VRAM 需 `(mem->start << PAGE_SHIFT) + mem->size < jdev->visible_vram_size`。

### 4.2 `ttm_bo_driver` 回调表（结构定义 `mwv207_ttm.c:554-568`，**5.10 形态**）

| 回调 | 实现 | 行 | 内核侧语义 |
|---|---|---|---|
| `ttm_tt_create` | `mwv207_ttm_tt_create` | `:146-162` | `kzalloc(mwv207_ttm_tt)` + `ttm_dma_tt_init(&gtt->ttm, bo, page_flags)`；失败 kfree + NULL |
| `ttm_tt_populate` | `mwv207_ttm_tt_populate` | `:164-172` | `ttm_dma_populate(&gtt->ttm, jdev->dev, ctx)` |
| `ttm_tt_unpopulate` | `mwv207_ttm_tt_unpopulate` | `:174-181` | `ttm_dma_unpopulate(&gtt->ttm, jdev->dev)` |
| `ttm_tt_bind` | `mwv207_ttm_tt_bind` | `:121-128` | 仅断言 `bo->ttm` 存在，返回 0 |
| `ttm_tt_unbind` | — | `:130-134` | 空实现 |
| `ttm_tt_destroy` | `mwv207_ttm_tt_destroy` | `:136-144` | `ttm_tt_destroy_common` + `ttm_dma_tt_fini` + kfree |
| `eviction_valuable` | `ttm_bo_eviction_valuable` | `:556` | 通用实现 |
| `evict_flags` | `mwv207_evict_flags` | `:45-90` | 见下 |
| `move` | `mwv207_bo_move` | `:417-472` | 见 4.5 |
| `io_mem_reserve` | `mwv207_ttm_io_mem_reserve` | `:92-119` | 见 4.6 |
| `move_notify` | `mwv207_bo_move_notify` | `:474-485` | 取 jbo 后无操作 |
| `fault_reserve_notify` | `mwv207_bo_fault_reserve_notify` | `:487-546` | **6.6 已删除的回调**；本树仍在用 |
| `verify_access` | `mwv207_bo_verify_access` | `:548-552` | `drm_vma_node_verify_access(&tbo->base.vma_node, filp->private_data)`；**6.6 已删除** |

`evict_flags`（`:45-90`）：非 mwv207 BO → 单条 SYSTEM placement（静态常量 `:50-55`）。mwv207 BO 按 **visible VRAM → invisible VRAM → GTT → SYSTEM** 取值：若当前在 VRAM 且 `visible_vram_size != vram_size` 且当前可见 → 清 flags 的 `1<<0`（CPU 可见）与 `1<<2`（pin/GPU-PHYS32）、按 domain `0x2|0x4` 重建 placements，然后**手工覆写** `placements[0].fpfn = visible_vram_size >> PAGE_SHIFT`、`lpfn = 0`（即"只允许不可见 VRAM"）、`num_placement = num_busy_placement = 1`（`:72-83`）；否则 domain `0x4`（GTT），非 VRAM → domain `0x1`（SYSTEM）。

### 4.3 `mwv207_bo` 生命周期

| 阶段 | 函数 | 行 | 要点 |
|---|---|---|---|
| 创建 | `int mwv207_bo_create(jdev, size, align, enum ttm_bo_type type, u32 domain, u32 flags, struct mwv207_bo **pbo)` | `bo.c:107-152` | flags bit2 要求 domain 必须含 VRAM（`:110`）；`size = roundup(size, PAGE_SIZE)`；`drm_gem_object_init` → `ttm_bo_dma_acc_size`（`:133`）→ `ttm_bo_init(..., align >> PAGE_SHIFT, type != kernel, acc_size, NULL, NULL, &mwv207_ttm_bo_destroy)`（`:138-141`）；**失败已补 `drm_gem_object_release` + kfree（K2 ✅，`:143-149`）** |
| 销毁 | `mwv207_ttm_bo_destroy` | `bo.c:21-30` | `WARN_ON_ONCE(map_count > 0)`；`import_attach` 时 `drm_prime_gem_destroy`；`drm_gem_object_release` + kfree |
| 类型判定 | `mwv207_ttm_bo_is_mwv207_bo` | `bo.c:32-37` | 比较 `bo->destroy` 指针 |
| 预留 | `mwv207_bo_reserve(jbo, no_intr)` / `_unreserve` | `bo.c:154-170` | 包装 `ttm_bo_reserve/unreserve` |
| pin | `mwv207_bo_pin_reserved(jbo, domain)` | `bo.c:227-246` | 置 flags bit2 → 重建 placement（`pinned=true` → `TTM_PL_FLAG_NO_EVICT`）→ `ttm_bo_validate`；`pin_count = 1` |
| unpin | `mwv207_bo_unpin_reserved` | `bo.c:248-268` | 清所有 `placements[i].flags` 的 `NO_EVICT` 后重新 validate |
| kmap | `mwv207_bo_kmap_reserved` | `bo.c:172-197` | **`if (!(jbo->flags & (1<<0))) return -EPERM;`（K1 ✅，`:180`）**；`ttm_bo_kmap` + `ttm_kmap_obj_virtual` |
| kunmap | `mwv207_bo_kunmap_reserved` | `bo.c:199-208` | `map_count` 计数归零才真正 `ttm_bo_kunmap` |
| 引用 | `mwv207_bo_ref/unref` | `bo.c:210-225` | `ttm_bo_get/ttm_bo_put` |
| 等待 | `mwv207_bo_wait(jbo, no_wait)` | `bo.c:294-307` | 预留（不可中断）→ `ttm_bo_wait` → 解除 |
| 放置 | `void mwv207_bo_placement_from_domain(struct mwv207_bo *jbo, u32 domain, bool pinned)` | `bo.c:39-105` | domain 位：`0x2` VRAM、`0x4` TT、`0x1` SYSTEM；`placements[3]`；`BUG_ON(c > 3)`（`:101`） |

`mwv207_bo.placements[3]` 的取值条件（`bo.c:56-95`）：

| 域 | flags 位 | placement 参数 |
|---|---|---|
| VRAM | `1<<2`（GPU 物理 32 位/pin） | `lpfn = 0x100000000 >> PAGE_SHIFT`（4 GiB 内） |
| VRAM | `1<<0`（CPU 可见） | `lpfn = visible_vram_size >> PAGE_SHIFT` |
| VRAM | 两者皆无 | `TTM_PL_FLAG_TOPDOWN`（从高地址取，优先不可见区） |
| TT | — | `TTM_PL_MASK_CACHING \| pflag` |
| SYSTEM | — | `TTM_PL_MASK_CACHING \| pflag` |

VRAM 的 flags 组合为 `TTM_PL_FLAG_UNCACHED | TTM_PL_FLAG_WC | pflag`（`bo.c:74`），pinned 时追加 `TTM_PL_FLAG_NO_EVICT`。

### 4.4 搬运路径（`mwv207_ttm.c:247-410`）

| 路径 | 函数 | 行 | 机制 |
|---|---|---|---|
| VRAM→GTT | `mwv207_move_vram_gtt` | `:247-274` | 构造 DMA job（src = `mem.start << PAGE_SHIFT` + VRAM 页数编码，dst = `gtt->ttm.dma_address` + RAM 页数编码），提交后 `ttm_bo_move_accel_cleanup(bo, fence, evict, true, new_mem)` |
| GTT→VRAM | `mwv207_move_gtt_vram` | `:276-303` | 反向同上 |
| VRAM→VRAM | `mwv207_move_vram_vram` | `:305-330` | 同区搬运 |
| VRAM→RAM | `mwv207_move_vram_ram` | `:332-373` | 已是 TT 则直接走 vram→gtt；否则 `ttm_bo_mem_space` 申请 TT 暂存、`ttm_tt_set_placement_caching`（`:359`，5.10 名）、`ttm_tt_populate`、vram→gtt、`ttm_bo_move_ttm` |
| RAM→VRAM | `mwv207_move_ram_vram` | `:375-410` | 反向同上 |
| 分派 | `mwv207_bo_move` | `:417-472` | `WARN_ON_ONCE(pin_count > 0)` → `-EINVAL`；SYSTEM→无 ttm 或 ram→ram 用 `ttm_bo_move_null`；失败兜底：若两侧有一方不可见则 `pr_err("mwv207: can't move invisible mem by cpu")` 放弃，否则 `ttm_bo_move_memcpy`；成功且新 mem 不可见则清 flags `1<<0` |

DMA job 的构造（`ttm.c:183-245`）：

| 项 | 内容 | 行 |
|---|---|---|
| 提交 | `drm_sched_job_init(&mjob->base, mjob->engine_entity, NULL)` → `*fence = &s_fence->finished` → `mwv207_job_get` → `drm_sched_entity_push_job` | `:183-197` |
| job 内容 | 单个 `struct mwv207_dma_cmd`：`src/dst.offset = 0`、`stride = bo->mem.size`、`width = size`、`height = 1` | `:215-220` |
| 实体 | `jdev->dma_entity`（`is_dma = true`） | `:222` |
| 依赖 | 用 `mwv207_tvb` 取 `dma_resv_get_fences_rcu` | `:225-245` |

### 4.5 mmap / io_mem_reserve / fault

| 机制 | 实现 | 行 |
|---|---|---|
| `io_mem_reserve` | SYSTEM/TT → `mem->bus` 全 0；VRAM → `bus.offset = mem->start << PAGE_SHIFT`，若 `offset + bus_size > visible_vram_size` 返回 `-EINVAL`，否则 `bus.is_iomem = true` 且 `offset += vram_bar_base`；其它类型 `-EINVAL` | `ttm.c:92-119` |
| 用户 mmap | `mwv207_mmap` → `ttm_bo_mmap(filp, vma, &jdev->bdev)`（5.10） | `drv.c:83-94` |
| 缺页修正 | `mwv207_bo_fault_reserve_notify`：非 VRAM/非本驱动 BO 直接 0；置 flags `1<<0`；可见则 0；`pin_count > 0` → `-EINVAL`；置 domain `0x2` 后 `validate`，失败（非 `-ERESTARTSYS`）改 domain `0x1` 重试；最后若仍落在 VRAM 且越界 → `DRM_ERROR("bo validation goes crazy") + -EINVAL` | `ttm.c:487-546` |
| TODO（源码自带） | 移到 RAM 不触发 swap → 低内存时 `-ENOMEM`；注："可用滑动 IATU 窗口消除该校验" | `ttm.c:517-521` |
| 访问校验 | `mwv207_bo_verify_access` → `drm_vma_node_verify_access(&tbo->base.vma_node, filp->private_data)` | `ttm.c:548-552` |

### 4.6 5.10 → 6.6 移植点：本树的实际写法

| 6.6 要求 | 本树实际写法（5.10） | 出处 |
|---|---|---|
| TTM 头合并（`ttm_bo.h`/`ttm_device.h`/`ttm_pool.h`） | 仍 `#include <drm/ttm/ttm_bo_driver.h>` 等 5.10 头 | `mwv207.h:21`、`bo.h:20-24`、`ttm.c:21-25` |
| `ttm_bo_device` → `ttm_device` | `struct ttm_bo_device bdev` | `mwv207.h:43` |
| `ttm_bo_device_init(...)` → `ttm_device_init(bdev, funcs, dev, mapping, vma, use_dma_alloc, use_dma32)` | `ttm_bo_device_init(&bdev, &driver, mapping, vma_manager, true)` | `ttm.c:574-578` |
| `ttm_bo_driver` → `ttm_device_funcs`（删 `fault_reserve_notify`/`verify_access`） | `static struct ttm_bo_driver mwv207_bo_driver`，两个回调都在 | `ttm.c:554-568` |
| `ttm_bo_init` + `ttm_bo_dma_acc_size` → `ttm_bo_alloc(bdev, bo, type, placement, alignment, ctx)` | `ttm_bo_dma_acc_size(...)` + `ttm_bo_init(..., acc_size, NULL, NULL, destroy)` | `bo.c:133`、`:138-141` |
| `ttm_range_man_init` → `ttm_range_man_init_nocheck`（返回 int） | 直接调 `ttm_range_man_init(...)`，**返回值未检查** | `ttm.c:585-586`、`:593-594` |
| `ttm_dma_tt_init/populate/unpopulate/fini` → `ttm_sg_tt_init` + `ttm_pool_*` + `ttm_tt_fini` | `ttm_dma_tt_init`、`ttm_dma_populate`、`ttm_dma_unpopulate`、`ttm_dma_tt_fini`、`ttm_tt_destroy_common` | `ttm.c:146-181`、`:136-144` |
| `ttm_tt_set_placement_caching` → `ttm_tt_set_caching` | `ttm_tt_set_placement_caching` | `ttm.c:359` |
| `gem_free_object_unlocked` → `drm_gem_object_funcs.free` | `struct drm_driver.gem_free_object_unlocked = mwv207_gem_free_object` | `drv.c:129` |
| `drm_device.pdev` 删除 | `jdev->base.pdev = pdev`；`irq.c:129`、`pipe_3d.c:493` 都在读它 | `drv.c:342`、`irq.c:129` |
| `ttm_bo_mmap` → `ttm_bo_mmap_obj` | `ttm_bo_mmap` | `drv.c:93` |
| `ttm_bo_unref` → `ttm_bo_put` | 已用 `ttm_bo_put`（这一项本树已符合） | `bo.c:210-216` |

> **结论**：本树**未做 6.6 移植**，是"5.10 源码 + 5 个缺陷修复 + softdep"。若要落到 6.6，需按 `reports/ANALYSIS-REPORT.md` §四全表替换；本文只补充了每项在本树的具体位置，便于逐点改造。

### 4.7 显存相关的可改进点

| # | 现象 | 证据 |
|---|---|---|
| M1 | `ttm_range_man_init` 返回值未检查 | `ttm.c:585`、`:593` |
| M2 | `kvmalloc` 返回值未判空（`submit.c:82`/`:243`）会让 `copy_from_user` 落到 NULL 目标（靠 `access_ok` 拦下） | `submit.c:82-90` |
| M3 | `mwv207_bo_fault_reserve_notify` 的 TODO（低内存 `-ENOMEM`） | `ttm.c:517-521` |
| M4 | `MWV207_DEBUG_BO_MIGRATION` 调试路径默认关闭（`bo.h:28` 的 `#undef`） | `bo.c:66-77` |
| M5 | `bo_placement` 有声明无实现（`bo.h:87`） | `bo.h:87` |

---

## 5 命令提交与调度

### 5.1 全景

```
UMD submit ioctl
  └─ mwv207_submit_ioctl                     submit.c:612
       ├─ mwv207_submit_init_job             submit.c:280   (ctx lookup → 引擎选择 → cmd_dat → bo 表 → reloc → cmds)
       ├─ ttm_eu_reserve_buffers             submit.c:627   (所有 tvb 一次性预留)
       ├─ mwv207_submit_validate             submit.c:513   (domain/pin/validate)
       ├─ mwv207_submit_patch                submit.c:469   (地址打补丁，写到 cmds / cmd_dat.dat，再 memcpy_toio 进 VRAM)
       ├─ mwv207_submit_attach_dependency    submit.c:549   (in-fence + dma_resv 共享 fence)
       └─ mwv207_submit_commit               submit.c:575   (drm_sched_job_init → out-fence fd → push_job)
                                                              ↓
                                            drm_gpu_scheduler (drm_sched)  →  mwv207_sched_run_job  sched.c:166
                                                              ↓
                                                  pipe->submit(pipe, mjob)  (3D/2D/DEC/ENC/DMA)
```

### 5.2 job 构建各阶段的校验

| 阶段 | 函数 | 行 | 校验与行为 |
|---|---|---|---|
| 任务名 | `mwv207_submit_init_job` | `:280-308` | `get_task_comm(mjob->comm, current)`（超时日志会打印）；`mwv207_ctx_lookup(dev, filp, args->ctx)`，NULL → `-ENOENT`；调用顺序 **select_engine → cmd_dat → bo → reloc → cmd** |
| 引擎选择 | `mwv207_submit_select_engine` | `:26-70` | `engine_type` 0→3D、1→DEC、2→ENC、3→2D、4→DMA（并置 `is_dma`）；`nr_* <= 0` → `-ENODEV`；`engine_id == 0xffffffff` 取"任意"实体 `entities[nr]`；`engine_id < nr` 取指定；否则 `-ENODEV` |
| cmd_dat | `mwv207_submit_init_job_cmd_dat` | `:186-278` | `nr_cmd_dat < 0` → `-EINVAL`；指针为空 → `-EINVAL`；每项：`nr_relocs < 0`、`dat_size <= 0 \|\| dat == 0`、`nr_relocs && !relocs` → `-EINVAL`，`dat_size > 0x20000` → `-E2BIG`；**最后为每块 dat 创建一个 VRAM + CPU 可见的内核 BO**（`mwv207_bo_create(..., ttm_bo_type_device, 0x2, 1<<0, &mdat->jbo)`，`:270-273`） |
| bo 表 | `mwv207_submit_init_job_bo` | `:94-159` | `nr_bos < 0` → `-EINVAL`；`nr_bos == 0 && nr_cmd_dat == 0` 直接成功；否则要求 `bos != 0`；**在 `spin_lock(&filp->table_lock)` 下**用 `idr_find(&filp->object_idr, handle)` 取对象，空 → `-ENOENT`；`tvb->num_shared = (flags & 0x1) ? 0 : 1`；对每个 cmd_dat 追加一个 `num_shared = 0` 的 tvb |
| reloc（顶层） | `mwv207_submit_init_job_reloc` | `:72-92` | `nr_relocs < 0` → `-EINVAL`；0 → 成功；`relocs == 0` → `-EINVAL`；`copy_from_user` 失败 → `kvfree` + 置 NULL + `-EFAULT`（K7 ✅） |
| 命令流 | `mwv207_submit_init_job_cmd` | `:161-184` | `cmd_size <= 0` → `-EINVAL`；`> 0x20000` → `-E2BIG`；`cmds == 0` → `-EINVAL`；`is_dma` 用 `kmalloc` 否则 `kvmalloc`；拷贝失败 → `-EFAULT` |

### 5.3 reloc 打补丁（`submit.c:330-511`）

`struct drm_mwv207_submit_reloc`（`mwv207_drm.h:111-117`）：`idx`、`cmd_offset`、`bo_offset`、`type`、`pad`。

| `type & 0xffff` | 语义 | 写入宽度 | 边界检查 | 行 |
|---|---|---|---|---|
| 0 | 64 位整地址 | 8 字节 | `cmd_offset + 8 > dat_size` → `-ERANGE` | `:341-352` |
| 1 | 低 32 位 | 4 | `cmd_offset + 4 > dat_size` | `:354-355` |
| 2 | 高 32 位 | 4 | 同上 | `:356-357` |
| 3 | `(addr >> 16) & 0xffff` | 2 | 同上 | `:358-359` |
| 其它 | — | — | `-EINVAL` | `:360-361` |

公共校验：`reloc->pad != 0` → `-EINVAL`；`bo_offset >= mwv207_bo_size(jbo)` → `-ERANGE`；地址 = `mwv207_bo_gpu_phys(jbo) + bo_offset`（`bo.h:76-79`，即 `tbo.mem.start << PAGE_SHIFT`）。

| 索引选择 | 判据 | 行 |
|---|---|---|
| cmd_dat 表 | `type & 0x80000000`（`MWV207_SUBMIT_RELOC_FLAG_CMD_DAT`）；要求 `idx < nr_cmd_dat`，BO = `mwv207_job_bo(mjob, idx + nr_bos)` | `:373-382` |
| bo 表 | 否则；要求 `idx < nr_bos` | `:384-389` |

写入目标（`mwv207_submit_patch`，`:469-511`）：顶层的 reloc 直接改 `mjob->cmds`；cmd_dat 的 reloc 改 `mjob->cmd_dats[i].dat`，随后对该 BO `ttm_bo_wait` → `mwv207_bo_kmap_reserved` → `memcpy_toio(ptr, dat, dat_size)` → `kunmap`（**即 reloc 结果由 CPU 拷进 VRAM，GPU 之后读的是 VRAM 副本**）。

DMA 专用提交（`is_dma = true`，`:430-467`）有更严格的形状要求：

| 要求 | 值 |
|---|---|
| `nr_bos == 2 && nr_relocs == 2 && nr_cmd_dat == 0` | `:437-438` |
| `cmd_size == sizeof(struct mwv207_dma_cmd)` | `:439` |
| 两条 reloc：bit31 未置、`(type & 0xffff) == 0`、`bo_offset == 0` | `:443-453` |
| `cmd_offset` 必须分别等于 `offsetof(struct mwv207_dma_cmd, src.base)` 与 `dst.base` | `:446-453` |
| src/dst 不得为同一 BO | `:455-456` |

DMA 条目的地址填充（`mwv207_submit_patch_dma_entry`，`:391-428`）：VRAM → `base = mwv207_bo_gpu_phys(jbo)` 且 `pg_nr_type = MWV207_DMA_NR_PAGES_VRAM(num_pages)`（bit63 = 0）；TT → `base = gtt->ttm.dma_address` 且 `MWV207_DMA_NR_PAGES_RAM`（bit63 = 1）；**其它 mem_type 直接 `BUG_ON(1)`（`:411`）**；随后三重边界校验（`offset >= bo_size`；`offset + stride*(height-1) + width` 回绕；越过 `bo_size`）与 `(offset % stride) + width > stride` 的对齐校验，均返回 `-ERANGE`。

### 5.4 提交前的显存校验（`mwv207_submit_validate`，`submit.c:513-547`）

| 情况 | 处理 |
|---|---|
| 已 `pin_count != 0` | 跳过 |
| `is_dma` 且 `mem_type == TTM_PL_SYSTEM` | `mwv207_bo_placement_from_domain(jbo, 0x4, false)` → 允许 GTT |
| 其它 | `jbo->flags \|= (1<<2)`（要求 GPU 物理 32 位/VRAM），domain `0x2`，`ttm_bo_validate` |
| cmd_dat 的 BO（`i >= nr_bos`） | 额外 `jbo->flags \|= (1<<0)`（CPU 可见） |

### 5.5 依赖与 fence（`submit.c:549-610`）

| 机制 | 内容 | 行 |
|---|---|---|
| 每个 tvb 的 excl fence | `num_shared != 0` → `dma_resv_get_excl_rcu(resv)`；`num_shared == 0` → `dma_resv_get_fences_rcu(resv, &excl, &nr_shared, &shared)`（取全部共享 fence，对应 bo 表 `flags` bit0 的"写/独占"语义） | `:556-569` |
| in-fence | `flags & MWV207_SUBMIT_FLAG_FENCE_FD_IN` → `sync_file_get_fence(args->fence_fd)`，取不到 fence → `-EINVAL` | `:571-573` |
| out-fence | `flags & FENCE_FD_OUT` → `get_unused_fd_flags(O_CLOEXEC)` + `sync_file_create(&mjob->base.s_fence->finished)` + `fd_install` → **写回 `args->fence_fd`** | `:584-598` |
| 入队 | `drm_sched_job_init(&mjob->base, mjob->engine_entity, mjob->ctx)` → `mwv207_job_get` → `drm_sched_entity_push_job(&mjob->base, mjob->engine_entity)` | `:577-609` |
| 源码自带 XXX | "job init and push should be protected with common lock, refer to drm_sched_entity_push_job documentation" | `:580-582` |

**in/out fence 共用 `fence_fd` 字段**（`mwv207_drm.h:82` 注释、`submit.c:597`）——UMD 必须先读 in、返回后读 out，正是 `01-WINSYS-KMD.md` O 系列记录的那条。

依赖消费发生在运行时（`mwv207_sched_dependency`，`sched.c:124-164`）：先 `fence_in`，再每个 tvb 的 `excl`，最后 `shared[]`，被消费的 fence 立即 put 并置空、`shared` 数组释放。

### 5.6 drm_sched 初始化与引擎划分（`mwv207_sched.c:226-367`）

| 项 | 值 | 行 |
|---|---|---|
| backend ops | `.dependency`、`.run_job`、`.timedout_job`、`.free_job` | `:219-224` |
| `drm_sched_init` 签名（本树） | `drm_sched_init(&sched->base, &mwv207_sched_ops, hw_submission, hang_limit, msecs_to_jiffies(timeout), pipe->fname)` | `:238-240` |
| `run_job` | `job->s_fence->finished.error` → `ERR_PTR(-ECANCELED)`；`devfreq_record_busy`；`pipe->submit(pipe, job)`；返回 ERR 时 `DRM_ERROR("%s: submit failed, errcode=%ld")` | `:166-181` |
| `free_job` | `mwv207_job_put`（`job_fini` 释放全部 fence、tvb、cmd_dat BO、ctx 引用、内存） | `:212-217`、`:41-79` |
| `dump_hang_job` | 打印管线状态（`pipe->dump_state`）+ 命令流（每行 4 个 dword） | `:196-199`、`:92-122` |

引擎创建清单（`sched.c:322-349`）：

| 顺序 | 管线 | regbase | 名称 | hw_submission | hang_limit | 超时 | 失败处理 |
|---|---|---|---|---|---|---|---|
| 1 | `pipe_3d_create(0)` | `0x900000` | `mwv207_3d0` | 30 | 5 | 2000 ms | 返回 `NULL` → `-ENODEV` |
| 2 | `pipe_3d_create(1)` | `0x910000` | `mwv207_3d1` | 30 | 5 | 2000 ms | 不判空 |
| 3 | `pipe_dec_create(0)` | `0x930000` | `mwv207_dec0` | 1 | 5 | 2000 ms | `goto err` |
| 4 | `pipe_dec_create(1)` | `0x940000` | `mwv207_dec1` | 1 | 5 | 2000 ms | 不判空 |
| 5 | `pipe_enc_create(0)` | `0x920000` | `mwv207_enc0` | 1 | 5 | 2000 ms | 不判空 |
| 6 | `pipe_2d_create(0)` | `0x968000` | `mwv207_2d0` | 30 | 5 | 2000 ms | `goto err` |
| 7 | `pipe_dma_create(0)` | `0x9d0000` | `mwv207_dma0` | 1 | 5 | 2000 ms | `goto err` |

数量与 INFO 键（`:351-361`）：`nr_3d = sched_3d[1] ? 2 : 1`、`nr_enc = sched_enc[0] ? 1 : 0`、`nr_dec = sched_dec[1] ? 2 : 1`、`nr_2d = 1`、`nr_dma = 1`。

**槽位布局（重要）**：`sched.c:310-314`

| 表达式 | 结果 | 实际占用 |
|---|---|---|
| `sched_3d = &sched[0]` | `sched_3d[0]=sched[0]`、`sched_3d[1]=sched[1]` | 3d0、3d1 |
| `sched_dec = &sched_3d[2]` | `sched_dec[0]=sched[0+2]=sched[2]`、`sched_dec[1]=sched[3]` | dec0、**dec1 用 sched[3]** |
| `sched_enc = &sched_dec[1]` | `sched_enc[0]=sched[3]` | **enc0 也用 sched[3]** |
| `sched_2d = &sched_enc[1]` | `sched_2d[0]=sched[4]` | 2d0 |
| `sched_dma = &sched_2d[1]` | `sched_dma[0]=sched[5]` | dma0 |

> **N1（本树新发现，待修）**：`sched_dec[1]` 与 `sched_enc[0]` 指向同一元素 `jdev->sched[3]`。第 4 步创建 dec1 的 scheduler 写入 `sched[3]`，第 5 步创建 enc0 的 scheduler **覆盖** `sched[3]`。后果：① dec1 的 `drm_gpu_scheduler` 对象失去唯一引用、`mwv207_sched_fini`（遍历 `sched[0..5]`）不会 `drm_sched_fini` 它 → 调度线程/工作队列泄漏，卸载时可能报警；② `ctx.c:97-130` 在 ctx 创建时为 DEC 引擎绑定 `sched_dec[0..1]`，此时 `sched_dec[1]` 已经是 **ENC 的 scheduler** → UMD 用 `engine_type=1(专享 DEC)、engine_id=1` 提交的 job 会被送进 ENC 调度器；③ INFO 的 `DRM_MWV207_ACTIVE_DEC_NR` 报 2（`:353`、`:359`）。该问题只在 full 芯片（非 lite）且 DEC1/ENC0 都存在时成立；lite 上 `pipe_dec_create(unit=1)` 与 `pipe_enc_create` 均返回 NULL（`pipe_dec.c:237`、`pipe_enc.c:239`），`sched[3]` 保持 NULL，不触发。

> **N2（源码注释与实现不一致）**：`sched.c:319-320` 注释要求 "sched limit of 3d should be lower than hw event count, which is 30 for 91/92"，但 `:323`/`:328` 传的 `hw_submission` 恰为 **30**；而 3D 事件位图正是 30 位（`pipe_3d.c:302-306` 的 `bitmap_zero(pipe->event_bitmap, 30)` 与 30 个 `event_fence[]` 槽）。相等而非"lower"，属边界配置，源码自述"otherwise pipe 3d will BUG_ON"。

### 5.7 ctx 的 entity 划分（`mwv207_ctx.c:54-130`）

| 项 | 内容 | 行 |
|---|---|---|
| 上限宏 | `MWV207_MAX_SCHED_PER_ENGINE 6` | `:54` |
| 栈上数组 | `struct drm_gpu_scheduler *scheds[6]`，**入口先检查 `nr > 6` → `-EINVAL`（K10 ✅）** | `:60`、`:66-67` |
| 每引擎实体数 | `nr` 个指定实体 + **1 个"任意"实体**（挂在 `scheds[0]` 上，供 `engine_id == 0xffffffff` 使用） | `:74-92` |
| 实体初始化 | `drm_sched_entity_init(entity, DRM_SCHED_PRIORITY_NORMAL, &scheds[i], 1, &ctx->guilty)` | `:77-80` |
| 布局 | `entity_3d=&entities[0]`（3 槽）、`entity_dec=&entity_3d[3]`（2 槽）、`entity_enc=&entity_dec[2]`（2 槽）、`entity_2d=&entity_enc[2]`（2 槽）、`entity_dma=&entity_2d[2]`（2 槽）= **11** | `:97-108` |
| 自检 | `WARN_ON(ctx->entity_dma[1] != ctx->entities[10])` → `-EINVAL` | `:108` |
| 内核专用实体 | `mwv207_kctx_init`：`devm_kzalloc` 一个 entity 绑 `jdev->sched_dma[0]`，guilty 传 `NULL` | `:231-244` |

### 5.8 各管线如何产出 fence

| 管线 | 完成通知机制 | fence 产出 | 行 |
|---|---|---|---|
| 3D | **硬中断**：hwirq `19 + unit`；`request_irq(..., 0, ...)` | 环形缓冲写入命令 + `loadstate(0x0e01, 0x80 \| event)` 触发事件号；ISR 读 `0x10`，逐位 `ffs` 出事件号，`pipe_3d_event_pop_irq` 取出 fence，`dma_fence_signal` + `put`，并推进 `head`/`completed_fence`；`0x80000000`/`0x40000000` 位分别报 AXI/MMU 错误 | `pipe_3d.c:588-598`、`:424-464` |
| 2D | **硬中断**：hwirq `17 + id` | 环形缓冲 + 事件号；ISR 先 `write(0x4308, 0xFFFFFFFF)` 清中断、读 `0x6000` 序列号，再按 **16 位回绕比较**（`fence16_after`）遍历 `fence_queue` 逐个 signal。`0x6000` 是硬件完成序号，因此**只按序号推进，不依赖每个事件的中断位** | `pipe_2d.c:541-547`、`:400-424` |
| DEC | **轮询内核线程** `dec_polling`（无 IRQ）：`usleep_range(1000,1001)` 轮询，`read(0x4) & 0x1100 == 0x1100` 视为完成 → 执行软后处理 `pipe_codec_sw_pp_excute` → `pipe_dec_stat_reset` → signal | 单槽 `current_fence`（`smp_load_acquire/store_release`）；提交时若已有在飞 fence 返回 `-EBUSY`；`hw_run` 为假（命令流中 `reg 4` 的值 bit0 为 0）则同步执行并立即 signal | `pipe_dec.c:81-112`、`:144-217` |
| ENC | **轮询内核线程** `enc_polling`（同 DEC 结构，完成位读 `0x4`） | 同 DEC | `pipe_enc.c:90-116`、`:234-264` |
| DMA | **无中断、无真 fence**：提交路径内 `memcpy` 式 LLI 提交 + `mwv207_timed_loop(..., 5000)` **忙等**通道完成（`0x188` bit1），随后 `pipe_dma_chan_clear(0x198, 1<<1)`；返回 **`dma_fence_get_stub()`（已 signal 的 stub fence）** | 内存搬运在 `run_job` 上下文中同步完成 | `pipe_dma.c:113-127`、`:140-187`、`:329-368` |

管线创建与资源（3D 为例，`pipe_3d.c:527-613`）：`lite && unit` → NULL；`pll_id = GU3D0/GU3D1`；`iosize = 16 KiB`；环形缓冲 = `mwv207_bo_create(0x40000, 0x1000, ttm_bo_type_kernel, 0x2, 1<<0)` → pin VRAM → kmap → `ringbuf`/`ringbuf_gpu_addr`/`end`；`devfreq_register`；`mwv207_pipe_3d_reset`；取 IRQ 映射（`== 0` 时报错并回退，**不用 BUG_ON**，`:589-592`）；`request_irq`。2D 同构（`:500-547`，环形缓冲同样 0x40000，`iosize = 32 KiB`）。DMA 用 `dmam_alloc_coherent` 分配 LLI 表（`:402-413`，devres 管理，故其 `destroy`/`reset` 为空壳 `pr_info` 不构成泄漏，`:370-375`、`:135-138`）。

### 5.9 超时处理（`mwv207_sched_timedout_job`，`sched.c:183-210`）与 K12

流程：

1. `pr_info("mwv207: job from '%s' to '%s', timed out!")`（打印 job 的 `comm` 与管线名）。
2. `drm_sched_stop(&sched->base, job)`。
3. `dump_hang_job` 为真 → `pipe->dump_state(pipe)`（3D 会读 `0x660/0x664/0x668/0x66c` 并轮询状态变化，`pipe_3d.c:397-422`；DEC/ENC/DMA 的 dump 只是 `pr_info`）+ `mwv207_sched_dump_job(mjob)`。
4. `drm_sched_increase_karma(job)`。
5. `pipe->reset(pipe)`（3D：清 30 个事件 bitmap 与引用、重写硬件、复位 tail/head、start；`pipe_3d.c:296-314`）。
6. **`list_for_each_entry(timedout_job, &sched->base.ring_mirror_list, node) mwv207_devfreq_record_idle(sched->pipe);`（`:203-204`）**。
7. `drm_sched_resubmit_jobs(&sched->base)` → `drm_sched_start(&sched->base, true)`。

**K12 复核（本树：⚠️ 语义问题仍在，且比原描述更完整）**：

| 子项 | 判定 | 证据 |
|---|---|---|
| 循环体只有 `devfreq_record_idle`，循环变量 `timedout_job` 未被使用 | **成立** | `sched.c:203-204` |
| 该写法把"每个仍在环上的 job"各记一次 idle，但 `busy_count` 的配对者是 `run_job` 里的 `devfreq_record_busy`（`:180`）与管线 ISR/轮询线程里的 `record_idle`（`pipe_3d.c:459`、`pipe_2d.c:417`、`pipe_dec.c:107`、`pipe_enc.c:116`）；环形镜像表里的 job **并非**都曾 `record_busy`（超时 job 自己那条已被 `drm_sched_stop` 摘出/或计数已被 `run_job` 加过），因此这里可能**多减** | 推断（`busy_count` 为普通 `int`，递减到负只由 `devfreq.c:276` 的 `WARN_ON(--busy_count < 0)` 报出；而 `record_idle` 在 `!pipe->devfreq` 时直接返回，`devfreq.c:270-271`） |
| `drm_sched_resubmit_jobs` 重放路径无 job 完整性校验/上下文重置（重放前只 `pipe->reset`） | **成立** | `sched.c:205-208`；对比 `drm_sched_increase_karma` 只累计 guilty |
| 管线 `reset` 是否丢弃在飞 fence | **三条管线都会丢**：① 3D 的 reset 先 `bitmap_zero(event_bitmap, 30)` 并 `dma_fence_put` 全部 30 个事件 fence，**不 signal**（`pipe_3d.c:301-307`）；② 2D 的 `mwv207_pipe_2d_reset` → `pipe_2d_hw_init` + `pipe_2d_reset_ring`（`:212-227` 遍历 `fence_queue` 只 `dma_fence_put`，**不 signal**）+ `pipe_2d_start`（`:265-272`）；③ DEC 的 reset 只 `dma_fence_put` 当前 fence，同样不 signal（`pipe_dec.c:136-141`）。→ 超时重置后，用户态若在等这些 fence，拿不到 signal（`dma_fence_put` 只减引用，且 deque 后对象可能已不可见） | `pipe_3d.c:296-314`、`pipe_2d.c:212-272`、`pipe_dec.c:136-141`（**实机复现待验证**，见 §10 U5） |

### 5.10 提交路径的锁与并发

| 保护对象 | 锁 | 行 |
|---|---|---|
| `filp->object_idr` 查 handle | `spin_lock(&filp->table_lock)` | `submit.c:132-147` |
| ctx 表 | `mutex_lock(&mgr->lock)` | `ctx.c:29`、`:148`、`:183` |
| 3D 事件表 | `spin_lock_irq(&pipe->event_lock)` | `pipe_3d.c:301`、`pipe_3d_event_push/pop_irq` |
| 2D fence 队列 | `spin_lock(&pipe->fence_queue_lock)` | `pipe_2d.c:410` |
| 管线树内 fence 初始化 | `spin_lock_init(&pipe->fence_lock)` | `pipe_3d.c:554`、`pipe_2d.c:511` |
| 滑动窗口 | `spin_lock(&jdev->win_lock)` | `drv.c:243-260` |
| DDC I2C | `mutex jdev->gpio_lock`（`i2c_pre/post_xfer`） | `i2c.c:103-122`、`vi.c:71` |
| PLL 命令通道 | `mutex jdev->vbios.vcmd_lock` | `vcmd.c:77`、`:146` |

---

## 6 中断

### 6.1 中断域与影子寄存器

| 项 | 内容 | 出处 |
|---|---|---|
| 状态寄存器（读） | `0x009A802c + i*4`（i = 0,1；共 64 位事件） | `irq.c:30` |
| 使能寄存器 | `0x009A8020 + i*4` | `irq.c:57-81` |
| 域尺寸 | `irq_domain_add_linear(NULL, 64, &mwv207_irq_domain_ops, jdev)` | `irq.c:140-143` |
| hwirq 上限 | `>= 64` → `-EPERM` | `irq.c:113-114` |
| 处理函数 | `irq_set_chip_and_handler(irq, &mwv207_irq_chip, handle_simple_irq)`；`irq_set_chip_data(irq, jdev)` | `irq.c:117-119` |
| irq_chip | `.name = "mwv207"`、`.irq_mask`、`.irq_unmask`（无 ack/EOI 回调） | `irq.c:103-107` |
| 影子 | `jdev->irq_enable_reg[2]`，mask/unmask 在 `irq_lock` 内更新 | `irq.c:64`、`:78` |

### 6.2 初始化流程（`mwv207_irq.c:127-182`）

| 步 | 内容 | 行 |
|---|---|---|
| 1 | `pdev = jdev->base.pdev`（**5.10 字段**） | `:129` |
| 2 | 向 `0x009A802c`（两组）写 `0xffffffff` 清 pending；向 `0x009A8020` 写 0 全部屏蔽 | `:133-136` |
| 3 | `spin_lock_init(&jdev->irq_lock)` | `:138` |
| 4 | 建线性 irq_domain，NULL → `-ENODEV` | `:140-143` |
| 5 | **`for (i = 0; i < 64; ++i)` 逐个 `irq_create_mapping`，任一返回 0 → `-ENOMEM` 并跳 `free_mapping_partial`（K8 ✅）**，注释明确写 "A failed mapping surfaces later as BUG_ON(va_irq == 0) in mwv207_kms.c or request_irq(0) -ENOMEM; fail cleanly here" | `:146-153` |
| 6 | `pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI \| PCI_IRQ_LEGACY)`，返回 `< 1` → `-ENODEV` | `:155-159` |
| 7 | `request_irq(pdev->irq, mwv207_isr, IRQF_SHARED, "mwv207_isr", jdev)` | `:161` |
| 回滚 | `free_vec:` 释放向量；`free_mapping_partial:` 逆序 `irq_dispose_mapping`；`irq_domain_remove` | `:166-181` |
| 反初始化 | `free_irq(pdev->irq, jdev)` → 释放向量 → 64 个 `irq_dispose_mapping(irq_find_mapping(...))`（**未判 0**）→ `irq_domain_remove` | `:184-199` |

> **N8**：第 7 步用的是 `pdev->irq`，而非 `pci_irq_vector(pdev, 0)`。在 MSI 生效时二者通常相同（单个向量），但语义上应由 `pci_irq_vector` 取得；`free_irq` 同样用 `pdev->irq`，成对自洽。

### 6.3 demux ISR（`mwv207_isr`，`mwv207_irq.c:23-51`）

| 步 | 内容 | 行 |
|---|---|---|
| 1 | `for (i = 0; i < 2; ++i)`：`stat = jdev_read(0x009A802c + i*4)` | `:29-30` |
| 2 | `stat &= jdev->irq_enable_reg[i]`（**软件影子屏蔽，未使能的位直接丢弃**） | `:31` |
| 3 | `while ((j = ffs(stat)))`：`j--`；`hwirq = i*32 + j`；`virq = irq_find_mapping(jdev->irq_domain, hwirq)` | `:32-35` |
| 4 | 有映射 → `generic_handle_irq(virq)`，返回 `< 0` 时 `pr_warn("mwv207: hwirq(%d) handled with %d")`；无映射 → `pr_warn("mwv207: no irq mapping set on %d")` | `:35-41` |
| 5 | **`jdev_write(0x009A802c + i*4, (1UL << j))` —— 逐位 W1C** | `:44` |
| 6 | `stat &= ~(1UL << j)`；`ret = IRQ_HANDLED` | `:45-46` |

### 6.4 hwirq 分配表

| hwirq | 使用者 | 注册方式 | 出处 |
|---|---|---|---|
| 17、18 | 2D 管线（`id` = 0、1） | `request_irq(..., 0, fname, pipe)` | `pipe_2d.c:541-547` |
| 19、20 | 3D 管线（`unit` = 0、1） | `request_irq(..., 0, fname, pipe)` | `pipe_3d.c:588-598` |
| 32 + idx（32..35） | HDMI 0..3 | `request_threaded_irq(irq, hard_irq, thread_fn, IRQF_SHARED, encoder.name, hdmi)` | `hdmi.c:1056-1063` |
| 42 | CRTC vblank（VA） | `request_irq(va_irq, mwv207_va_handle_vblank, 0, "mwv207_va", jdev)` | `kms.c:171-173` |
| 其它 | 未映射（映射存在但无处理者 → 命中 `pr_warn("no irq mapping set")` 之外的路径时静默） | — | — |

DEC / ENC / DMA **不注册中断**（DEC/ENC 用轮询线程，DMA 忙等），与 §5.8 一致。

### 6.5 mask / unmask 与影子竞态

| 函数 | 行为 | 行 |
|---|---|---|
| `mwv207_irq_mask` | `jdev_modify(0x009A8020 + (irq/32)*4, 1 << (irq%32), 0)`；随后**回读同位并写入 `irq_enable_reg[irq/32]`**；全程 `spin_lock_irqsave(&irq_lock)` | `:53-66` |
| `mwv207_irq_unmask` | 同址把该位写 1；同样在锁内更新影子 | `:68-81` |
| 竞态 | demux ISR **读 `irq_enable_reg[i]` 时不持 `irq_lock`**（`:31`），而 mask/unmask 在锁内写它 → 影子数组无锁共享 | `:31` vs `:53-81` |

### 6.6 K8 复核（`irq_create_mapping` 与 `BUG_ON`）

| 项 | 判定 | 证据 |
|---|---|---|
| `irq_create_mapping` 返回值是否检查 | **已检查（K8 ✅）**，失败返回 `-ENOMEM` 并逆序 dispose 已建映射 | `irq.c:146-153`、`:170-177` |
| 上游是否保证 `va_irq != 0` | 是（64 个映射全建成才继续），故 `dc/mwv207_kms.c:172` 的 `BUG_ON(jdev->va_irq == 0)` 在正常路径不会触发 | `kms.c:171-172` |
| 残留风险 | ① `kms.c:172` 的 `BUG_ON` 未降级为可回退错误；② **`dc/mwv207_hdmi.c:1057` 有同类的 `BUG_ON(hdmi->irq == 0)`**；③ 3D/2D 管线是正确写法（判 0 后 `pr_err` + 回退），可作对照 | `kms.c:172`、`hdmi.c:1057`、`pipe_3d.c:589-592`、`pipe_2d.c:542-545` |

### 6.7 K9 复核（W1C 竞态）——**与原描述不符**

原报告（`reports/ANALYSIS-REPORT.md` K9）称：`mwv207_irq.c:30-46` 的 demux 在"读状态与写清零之间新置位的 bit 被一并清掉且不处理"。

| 事实 | 结论 | 证据 |
|---|---|---|
| demux 的清零写是**逐位** `1UL << j`，不是整字回写 | **新置位 bit 不会被这次写清掉**（W1C 粒度 = 单位）→ 原 K9 描述的机制在本树 demux 层**不成立** | `irq.c:44` |
| 但 `stat &= jdev->irq_enable_reg[i]` 会丢弃"当前未使能"的位 | 该位保持 pending、本次不处理；level 触发下需依赖 unmask 后再次进入 ISR | `irq.c:31` |
| `irq_enable_reg[i]` 的无锁共享 | 真实竞态（§6.5），属**新发现** | `irq.c:31` vs `:53-81` |
| **整字回写 W1C 的真实实例在别处**：VA vblank 处理器 `fired = read(0x9907f4); write(0x9907f4, fired)`，再取 `(fired >> 12) & 0xf` | 读-写窗口内新置位的 **bit 12..15** 会被本次写清掉（虽然只关心这 4 位）；若在同一窗口内又发生一次 vblank，则这一次 vblank 事件对软件不可见 → 可能丢失一次 vblank 中断/事件 | `dc/mwv207_va.c:634-655` |
| 另一实例：2D ISR 先 `write(0x4308, 0xFFFFFFFF)` 全清 | 但 2D 的完成判定改为读 `0x6000` 序列号并回绕比较，**不依赖中断位**，故清掉新位不丢事件 | `pipe_2d.c:406`、`:408-419` |
| 3D ISR 读-改-写 `0x10` 是 `intr &= ~(1 << event)` 的**软件**变量，硬件清理由 `pipe_3d_event_pop_irq`/事件机制负责 | 3D 不属整字回写问题；但 3D 事件上限 30 与 `hw_submission = 30` 的边界见 N2 | `pipe_3d.c:430-462` |

> **K9 本树判定**：⚠️ **原描述的机制不成立（demux 是单位 W1C）**；但"中断事件可能丢失"的**后果**在 **VA vblank（整字回写、仅 4 个 CRTC 位）** 与 **`irq_enable_reg` 无锁影子** 两处成立，范围小于原报告。对应 `06-KNOWN-ISSUES.md` 中 K9 的"未复核"状态可以按此更新为"机制描述需更正"。

### 6.8 各中断来源的处理方式一览

| 来源 | 顶层 | 二次处理 | 关闭/挂起 |
|---|---|---|---|
| 芯片 64 位事件 | `mwv207_isr`（共享，`IRQF_SHARED`） | `generic_handle_irq` → 各子 ISR | `irq.c:184-199` |
| 3D 完成/错误 | `mwv207_pipe_3d_isr`（`0x10`） | 事件位图 → fence signal；AXI/MMU 错误仅 `pr_warn` | `free_irq` 在 `pipe_3d_destroy:472` |
| 2D 完成 | `mwv207_pipe_2d_isr`（`0x4308`/`0x6000`） | 序列号回绕比较 → fence signal | `pipe_2d_destroy` |
| HDMI HPD/PHY | `mwv207_hdmi_hard_irq`（读 `0x0104`，W1C `0x0184`，返回 `IRQ_WAKE_THREAD`）/ `mwv207_hdmi_irq`（`0x0104`/`0x3007`/`0x3004`） | `drm_helper_hpd_irq_event` | `free_irq` 在 `mwv207_hdmi_destroy:944` |
| vblank | `mwv207_va_handle_vblank`（`0x9907f4`） | 按 bit 12..15 派发 `drm_crtc_handle_vblank` + 事件完成 | `kms.c:202` |

---

## 7 KMS / 显示

### 7.1 CRTC / plane 数量与 lite 关系

| 项 | 值 | 出处 |
|---|---|---|
| CRTC 数 | `nr = jdev->lite ? 2 : 4`，逐个 `mwv207_va_init_single` | `va.c:707-719` |
| 每 CRTC plane | 1 个 primary + 1 个 cursor（共 4 或 8） | `va.c:657-705` |
| CRTC 寄存器窗口 | `MWV207_VA_BASE(idx) = 0x990000 + 0x100*idx + (idx>1 ? 0x600 : 0)`；窗口 `MWV207_WIN_BASE` 在同 idx 下 `+0xC00` | `va.c:29-31` |
| primary plane 可绑 CRTC | `1 << idx`（**只能绑自己的 CRTC**） | `va.c:671` |
| connector 可绑 CRTC | `(1 << num_crtc) - 1`（**任意 CRTC**） | `hdmi.c:1044`、`vga.c`、`dvo.c:167` |
| cursor 尺寸 | 64×64（mode_config） | `kms.c:137-138` |
| max framebuffer | `max_width = max_height = 4096*4 = 16384`；min 20 | `kms.c:120-126` |
| 其它 config | `preferred_depth = 24`、`prefer_shadow = 1`、`allow_fb_modifiers = true`、`async_page_flip = true` | `kms.c:127-133` |

### 7.2 plane 实现

| plane | 格式 | 关键寄存器（VA 窗口字节偏移） | 行 |
|---|---|---|---|
| primary | `rgb_formats[] = { RGB565, RGB888, XRGB8888, ARGB8888, XRGB2101010 }`；modifier 仅 `DRM_FORMAT_MOD_LINEAR` | `0x434` = pitch>>4；`0x43C` = `((src_h-1)<<16)\|(src_w-1)\|0x3`；`0x4F8` = `(src_w-1)<<16`；`0x438` = fb 地址>>6；`0x430` 格式：RGB565 → bit0、XRGB2101010/ARGB2101010 → bit1、ARGB8888 → `0x3` | `va.c:48-59`、`:163-208` |
| cursor | `cursor_formats[] = { ARGB8888 }` | `0x478` 开关（`on ? 6 : 0`）、`0x4B4` 图地址>>6、`0x454` 热点掩码、`0x4A8` 位置 | `va.c:61-63`、`:217-260` |
| 地址计算 | `gpu_phys + src_x*cpp0 + pitch*src_y` | 函数 `mwv207_plane_fb_addr` | `va.c:144-161` |
| prepare_fb | 预留 → `flags \|= 1<<0` → `pin_reserved(0x2)`（pin 到 VRAM）→ 解除 → `mwv207_bo_ref` → `drm_gem_fb_prepare_fb` | cleanup 反向 `unpin` + `unref` | `va.c:99-142` |
| atomic_check | 空返回 0（不做缩放/位置校验） | — | `va.c:92-97` |

> **N6**：`rgb_formats[]` 未列 `ARGB2101010`，但 `mwv207_primary_atomic_update` 的格式分支包含 `ARGB2101010`（`va.c:48-54` vs `:198-201`）→ 该分支对用户态不可达（除非通过其它路径设置格式）。属声明不一致，非崩溃。

### 7.3 CRTC 时序与 PLL

| 环节 | 内容 | 行 |
|---|---|---|
| 模式修正 | `adjusted_mode` 复制后把 `hdisplay/hblank_start/hblank_end/hsync_start/hsync_end/htotal/clock` **全部 ×2** | `va.c:376-387` |
| PLL | `mwv207_vbios_set_pll(jdev, idx + MWV207_PLL_GRAPH0, mode->crtc_clock)`，失败 `DRM_ERROR("failed to set crtc clock")` 并放弃配置 | `va.c:402-407` |
| `0x400`（hs begin/end） | `va.c:389-483` |
| 寄存器 | `0x400`（hs begin/end）、`0x404`（hblank）、`0x408`（htotal/hhalf）、`0x40C`+`0x458`（1 场 vblank）、`0x410`+`0x45C`（2 场 vblank）、`0x418`/`0x41C`（vs）、`0x414`（vtotal）、`0x464`（隔行标志，1440 特例 +2）、`0x420`=1，最后 `0x2a0` 取 `& ~0x6 \| 1`（`:477-480`） | `va.c:389-483` |
| 隔行 720 特例 | 临时 kmalloc 一份 native mode 再 fixup | `va.c:409-417` |
| enable | 对 `encoder_mask` 中每个 encoder 调 `mwv207_output_set_crtc` + 配置时序 + `drm_crtc_vblank_on` | `va.c:514-525` |
| disable | `vblank_off` → `0x2c4=0`、`0x2c0=0`、`0x2a0=2`、`0x2c4=1` | `va.c:527-540` |
| misc reset | `0x9b001c/0x9b0020/0x9b0024 = 0xfffff`、`0x9b0028 = 0x03020100`、`WIN_BASE+0x2c = 0` | `va.c:288-302` |

PLL 硬件命令通道（`mwv207_vcmd.c`）：

| 项 | 内容 | 行 |
|---|---|---|
| 请求寄存器 | `0x9b0140` = `cmd<<28 \| idx<<24 \| value<<1 \| enable`；`0x9b0144` bit28 启动、bit24-27 结果、bit28-31 空闲 | `:49-70` |
| 超时 | `mww207_vcmd_wait_idle` 最多 1000 次 ×1 ms；`MWV207_PLL_TIMEOUT` = 1000 ms | `:21`、`:39-47` |
| set | `mwv207_vcmd_execute(jdev, 1, pll_idx, kfreq, kfreq ? 1 : 0)`；`pll_idx >= MWV207_PLL_COUNT` → `-EINVAL` | `:125-132` |
| 错误码 | 0 成功；1 不支持类型；2 非法频率；3 PLL 忙；4 参数非法；9 命令不支持；其它未知 → 均返回 `-EIO` 并 `pr_err` | `:96-119` |
| get | 读 `FBDIV_REFDIV/FRAC/POSTDIV`（基址 `0x9b0000 + 0xA00 + 0x100*(id/6) + 0x20*(id%6)`，偏移 0/4/8），按 `100000*100000/refdiv * (fbintdiv + fbracdiv>>24) / (4 * postdiv2)` 反算 kHz；`port = pll_idx ? 0 : 1`，postdiv2 取 `state3` 的 `8*(port%4)` 位域低 7 位 | `:134-177`、`:23-28` |
| PLL ID 枚举 | `MWV207_PLL_GPU=0, GRAPH0=1, GRAPH1=2, GRAPH2=3, GRAPH3=4, GRAPH4=5, HD=6, GU3D0=7, EDP=8, GU3D1=9, RESERVED=10, GU2D_PLL=11, DDR=12, COUNT` | `vbios.h:21-38` |

### 7.4 gamma / LUT（`va.c:304-374`、`:542-560`）

| 环节 | 内容 | 行 |
|---|---|---|
| color mgmt 声明 | `drm_crtc_enable_color_mgmt(&va->crtc, 768, true, 768)`（degamma 768 项 + CTM + gamma 768 项）；`drm_mode_crtc_set_gamma_size(&va->crtc, 256)` | `va.c:698-699` |
| FIFO 等待 | 轮询 `0x450`，最多 120 次 × `usleep_range(1000,1001)`，超时 `-EBUSY` | `:304-317` |
| FIFO 激活 | `0x44C` = 通道号；`0x448` = 1 → `udelay(2)` → 0 → `udelay(2)` | `:319-328` |
| FIFO 写入 | 先等 FIFO 余量 10，再写 `0x444` | `:330-343` |
| 使能/乒乓 | `modify(0x460, 1<<31, 0)`；`WIN_BASE+0x38 = lutdata[384]*15/8 \| 0x80000000`；**双 RAM 乒乓**：读 `0x440` 得当前 RAM，逐通道（0..2）激活后写 1024 项，`mb()`，最后写 `0x440 = 1 - active_ram` | `:345-374` |
| atomic_flush | `color_mgmt_changed` 且有 `gamma_lut` 时把 256 项拆成 R/G/B 三段（`lutdata[i]/[i+256]/[i+512]`，`drm_color_lut_extract(..., 8)`）再使能 | `:542-560` |
| 校验 | `drm_color_lut_size(gamma_lut) != 256` → `-EINVAL` | `:506-512` |
| legacy | `gamma_set = drm_atomic_helper_legacy_gamma_set` | `:582` |

> LUT 深度：硬件每通道 1024 项（写入循环上限 `min(1024-5, ...)`），DRM 侧对用户暴露 256 项；`lutdata[]` 数组为 768（`va.c:44`）。

### 7.5 vblank 与 page flip

| 项 | 内容 | 行 |
|---|---|---|
| 使能 | `jdev_modify(0x9907f0, 1<<(12+idx), 1<<(12+idx))` 与 `jdev_modify(0x9907f8, ...)`；**CRTC idx 直接对应 bit 12+idx** | `va.c:485-504` |
| 状态/清位 | `0x9907f4`（读状态、写回 W1C） | `va.c:634-655` |
| 事件挂接 | `mwv207_crtc_prepare_vblank` 在 `crtc->state->active` 时 `drm_crtc_vblank_get` 并在 `dev->event_lock` 下暂存 `va->event`；非 active 直接 `drm_crtc_send_vblank_event` | `va.c:595-614` |
| 完成 | `mwv207_crtc_finish_vblank` 在锁内 send + 清，锁外 `drm_crtc_vblank_put` | `va.c:616-632` |
| commit tail 顺序 | `modeset_disables` → `commit_planes(dev, old_state, 0)` → `modeset_enables` → `mwv207_atomic_prepare_vblank` → `commit_hw_done` → `wait_for_vblanks` → `cleanup_planes` | `kms.c:91-105` |
| vblank 初始化 | `drm_vblank_init(&jdev->base, num_crtc)`（`:178`），失败 `goto free_irq`；`irq_enabled = true`（`:176`） | `kms.c:176-181` |

### 7.6 连接器总表

| 输出 | connector 类型 | encoder | mmio 窗口 | I2C 通道 | 探测方式 | 创建于 |
|---|---|---|---|---|---|---|
| eDP | — | — | — | — | **未实现**（`pr_info("mwv207_edp_init TBD")`） | `dc/mwv207_edp.c:18-22` |
| HDMI 0..3 | `DRM_MODE_CONNECTOR_HDMIA` | `DRM_MODE_ENCODER_TMDS` | `0x1200000 + 0x40000*idx`（HDMI 寄存器），输出公共窗口 `+0x9a0000` | `idx`（0..3） | `DRM_CONNECTOR_POLL_HPD`；`detect_ctx` 读 `0x3004` bit1；HPD 走硬中断 | `hdmi.c:1017-1086` |
| VGA | `DRM_MODE_CONNECTOR_VGA` | `DRM_MODE_ENCODER_DAC` | `+0x9a0000` | 4 | `POL L_CONNECT\|POLL_DISCONNECT`；i2c_probe + DAC 负载检测（仅 lite） | `vga.c:252-289` |
| DVO/DVI | `DRM_MODE_CONNECTOR_DVII` | `DRM_MODE_ENCODER_DAC` | `+0x9a0000` | 5 | `POLL_CONNECT\|POLL_DISCONNECT`；`mwv207_i2c_probe` | `dvo.c:157-180` |

创建顺序：`mwv207_vi_init` 中依次 `edp → hdmi → vga → dvo`（`vi.c:69-88`），任一失败直接返回（已注册的 connector 交给 `drm_mode_config_cleanup`）。`mutex_init(&jdev->gpio_lock)` 在此之前。

### 7.7 HDMI 细节（`dc/mwv207_hdmi.c`）

| 环节 | 内容 | 行 |
|---|---|---|
| 单元探测 | `for (i = 0; i < 4; i++)` 读 `MWV207_HDMI_BASE(i)`，`== 0x21` 才创建 → **同一判据被 drv.c:365 用于 lite 识别** | `:1072-1086` |
| PHY 配置表 | 19 组 `{min_freq_khz, max_freq_khz, bpp, config[47]}` 静态默认表（`default_phy_data`），bpp 覆盖 8/10/12 | `:39-155` |
| 优先取 VBIOS | `mwv207_vbios_vdat(jdev, 0xfff0 + idx)`，长度必须等于表大小；不满足则 `DRM_INFO("hdmi%d use default phy data")` 并 `memcpy` 默认表 | `:208-237` |
| PHY 写序列 | `write(regbase+0x70, 0xc8)`、`write(+0x74, 0x2)`，再逐字节写 `+0x4+i*4`（**跳过 `0x84`、`0x88`、`0xB8`**） | `:239-267` |
| PHY 开关 | 开：`+0x84 = 0x80` + `HDMI_CTRL` bit28 清；关：bit28 置 + `+0x84 = 0` | `:269-281` |
| PHY 锁 | 上电后 5 次轮询 `0x3004` bit0，失败 `pr_warn("hdmi PHY PLL failed to lock")` | `:283-299` |
| SCDC/scrambling | `drm_scdc_set_scrambling`、`SCDC_SOURCE_VERSION`；TMDS > 340 MHz 或 low_rates 时开启 | `:456-469`、`:533-550` |
| AVI / VSIF / HDR | AVI infoframe 拆写 `0x1019..0x1024`、`0x10E0`、`0x1017`；VSIF 写 `0x102A/0x1029/0x1030..0x1034`、`0x10B3/0x10B4/0x10B5`；HDR（DRM infoframe）写 `0x1168/0x1169/0x116A+i` 与 `0x1167`、`0x10E3` | `:615-746` |
| 视频路径 | `0x1011=12`、`0x1012=32`、`0x1013=1`、`0x1014=0x0B`、`0x1015=0x16`、`0x1016=0x21`；`mc_clkdis` 位操作写 `0x4001`；`rgb_limited_range` 时 `0x4004=1` 否则 0 | `:576-604` |
| 打包/CSC | `video_packetize` 固定 `color_depth = 4`、`output_select = 0x3` 写 `0x0801-0x0804`；CSC 系数矩阵写 `0x4100-0x4113` | `:748-812` |
| HDCP | `mwv207_hdmi_tx_hdcp_config` 写 `0x5000/0x5001/0x5009`（无实际认证流程） | `:832-841` |
| set_timing | 关溢出中断 → `drm_match_cea_mode` → 量化范围判定 → `av_composer` → **`phy_configure(hdmi, mode->clock, 8)`（bpp 恒为 8）** → 视频路径 → 音频时钟 → infoframes → packetize/csc/sample/hdcp → 清溢出 | `:843-880` |
| mode_valid | `clock >= 384000` → `MODE_CLOCK_HIGH`；`> 594000` → 同；**非 HDMI sink 且 `>= 165000` → `MODE_CLOCK_HIGH`** | `:903-916` |
| irq | `irq_find_mapping(domain, idx + 32)`，`BUG_ON(hdmi->irq == 0)`，`request_threaded_irq(..., IRQF_SHARED, ...)` | `:1056-1063` |
| 销毁 | `free_irq` → `drm_connector_unregister` → `drm_connector_cleanup` | `:940-947` |

> **N10**：`mode_valid` 的第一个条件 `clock >= 384000` 已经把 384–594 MHz 判为超范围，紧随其后的 `> 594000` 分支只对 ≥384000 的输入可达（两条都返回 `MODE_CLOCK_HIGH`），即 **384–594 MHz 的合法 HDMI 2.0 时序被拒绝**；同时 `phy_configure` 恒传 bpp = 8，导致 PHY 表中 10/12 bpp 的表项不可达。属功能限制，未确认为缺陷（可能与硬件/移植范围有关）。

### 7.8 连接器细节补充

| 输出 | 要点 | 行 |
|---|---|---|
| DVO | `switch` = `modify(0x200, 1, on)`；`config` 用 `adjusted_mode` 的 `PHSYNC/PVSYNC` 求极性写 `0x200` bit9/bit8；`select_crtc` 写 `0x200` 的 `0x7<<16` 与 NT 侧 `0x9b003c` 低 4 位；`mode_valid` 仅 `clock > 162000 → MODE_CLOCK_HIGH` | `dvo.c:18-54` |
| VGA | `set_detect_mode` 写 `0xA4/0xA8/0xAC` 与 3 通道 HPD 参数（`0x64/0x68/0x6C/0x70/0x74 + chan*0x14`）；`switch` 仅在 `lite && mwv207_vga_load_detect` 时做负载检测；`detect` 先 i2c_probe 再读 `0xA8` 三通道位；`mode_valid` `clock > 193250` | `vga.c:18-34`、`:36-157`、`:114-122` |
| I2C (DDC) | GPIO 表 `scl = {19,21,23,25,27,29,31,33}`、`sda = {20,22,...34}`；`set_addr` 分 0-5 / 6 / 7 三组寄存器（`0x9aa000/0x9aa004/0x9aa008`，6/7 用 `0x9aa010/0x9aa014/0x9aa018`）；`create` 拒绝 `chan < 0 || >= 6`（故 6/7 不被使用）；`udelay = 10`、`timeout = usecs_to_jiffies(2200)`；`probe` 用 DDC 读 8 字节 + `drm_edid_header_is_valid(buf) < 6` | `i2c.c:21-22`、`:171-212`、`:240-280`、`:282-312` |
| I2C 复用切换 | `mwv207_i2c_gpio_multi` 把 `0x9b0918/0x9b091c/0x9b0910` 上对应的 2-bit 复用域清 0（切到软件 GPIO） | `i2c.c:50-98` |
| 未声明导出 | `mwv207_i2c_put_byte`（非 static，构造 2 字节 `i2c_msg` 调 `i2c_transfer`）**未在头文件声明** | `i2c.c:214-233` |

### 7.9 双 4K 能力与限制

| 支撑点 | 出处 |
|---|---|
| `max_width/max_height = 16384` | `kms.c:125-126` |
| 每 CRTC 独立寄存器窗口（idx 0/1 与 2/3 各 `+0x600/+0xC00`） | `va.c:29-31` |
| 每 CRTC 独立 PLL（`MWV207_PLL_GRAPH0 + idx`） | `va.c:402` |
| connector 可绑任意 CRTC | `hdmi.c:1044`、`dvo.c:167` |
| 限制：primary plane 只允许 `1<<idx` 的 CRTC | `va.c:671` |
| 限制：仅 LINEAR modifier、`allow_fb_modifiers = true`、`prefer_shadow = 1`（大分辨率下走阴影/软件路径） | `va.c:56-59`、`kms.c:128-131` |
| 限制：fbdev 走 CFB 软件加速（`fb_fillrect/copyarea/imageblit`） | `fb.c:27-33` |

### 7.10 fbdev（`dc/mwv207_fb.c`）

| 项 | 内容 | 行 |
|---|---|---|
| 初始化 | `drm_fb_helper_prepare` → `drm_fb_helper_init`（包装函数**忽略 crtc_count/max_conn_count 参数**）→ `drm_fb_helper_initial_config(fb_helper, 32)` | `:127-156`、`:35-41` |
| fb 创建 | `surface_bpp 24 → 32`；pitch = `width * ((bpp+7)/8)`；`drm_mode_legacy_fb_format`；分配内核 BO（`ttm_bo_type_kernel`、VRAM、CPU 可见，align `0x10000`）；`framebuffer_init`；pin + kmap + `memset` 清零；填 `info->screen_base/screen_size/fix.smem_len`、`skip_vt_switch = true` | `:43-121` |
| 销毁 | 先 reserve 拿到 jbo → `kunmap/unpin/unreserve` + put BO + `drm_framebuffer_cleanup` → `drm_fb_helper_fini` | `:158-181` |
| 清屏/刷新 | 走 DRM fb helper 通用路径（`DRM_FB_HELPER_DEFAULT_OPS`） | `:27-33` |

---

## 8 调试接口

### 8.1 `mwv207_db`（INFO ioctl）——**不是寄存器接口**

| 问题 | 答案 | 证据 |
|---|---|---|
| 是否读写寄存器？ | **否**。全文件无 `jdev_read`/`jdev_write` | `db.c:1-144` |
| 是什么？ | 内核内存里的 key→value 能力库，容量 2048，`devm_kmalloc` 分配（结构 + 数组同块） | `db.c:23-48` |
| 谁写？ | 仅 `mwv207_sched_init` 写 5 个 `DRM_MWV207_ACTIVE_*_NR` | `sched.c:357-361` |
| 谁读？ | UMD 通过 `DRM_IOCTL_MWV207_INFO`（op=0 枚举 / op=1 按键查询） | `db.c:108-144` |
| 排序 | `mwv207_db_sort` 在 `drm_dev_register` 前调用一次；`add` 在已排序时重排；重复 key 用 `WARN_ONCE("mwv207 key: %d is duplicated")` | `drv.c:388`、`db.c:56-106` |
| K3/K4/K5 | 全部 ✅ 已修（`db.c:131-134`、`:114`+`:90`/`:97`、`:52-53`） | 同左 |

### 8.2 sysfs / devfreq（唯一的 sysfs 暴露）

| 节点 | 位置 | 内容 | 行 |
|---|---|---|---|
| `utilization` | `DEVICE_ATTR_RO(utilization)`，挂在每管线的 devfreq 设备上 | busy 占比百分比（`total_time == 0` 返回 100） | `devfreq.c:117-141` |
| `stats` | `sysfs_create_link(dev->kobj, pipe->devfreq->dev.kobj, "stats")` | 指向 devfreq 目录 | `devfreq.c:204-208` |
| 每管线设备名 | `dev_set_name(dev, "devfreq_%s", pipe->fname)` → `devfreq_mwv207_3d0`、`devfreq_mwv207_2d0` 等 | `devfreq.c:171` |
| OPP 表 | 从 `MIN_FREQ = 50` 到各管线 max（步长 50） | `devfreq.c:21-22`、`:186-190` |
| 调速器 | **固定 `DEVFREQ_GOV_PERFORMANCE`**（不调频，仅采样利用率） | `devfreq.c:192-193` |
| `max_freq` 来源（3D） | 按 `pdev->subsystem_device` 映射：`0x9103→600`、`0x9210/0x9211→800`、`0x9101/0x9102/0x910A/0x910B/0x910C/0x920C/0x930B→1000`、`0x9200/0x920A/0x920B/0x9230/0x9231/0x930A/0x930C→1200`、default `DEFAULT_MAX_FREQ_3D` | `pipe_3d.c:489-525` |
| busy/idle 记账 | `record_busy`（`run_job` 调用）/ `record_idle`（管线 ISR 或轮询线程调用）配对；`record_idle` 带 `WARN_ON(--busy_count < 0)` | `devfreq.c:251-279`、`sched.c:180` |
| 挂起 | `devfreq_suspend_device` / `devfreq_resume_device`，`suspend_freq = MIN_FREQ` | `devfreq.c:227-249`、`:202` |

### 8.3 module 参数（同 §1.2）：`selftest`、`dump_hang_job`、`mwv207_vga_load_detect`。

### 8.4 debugfs：**无**

| 事实 | 证据 |
|---|---|
| 全树仅在 `mwv207_ttm.c:20` `#include <drm/drm_debugfs.h>`，**没有任何 `debugfs_create_*` 调用** | `grep -rn "debugfs"` 结果 |
| 没有自定义 sysfs 于 drm 设备下（除 devfreq） | 同上 |
| 唯一的"看内部状态"手段是 `dump_hang_job` 触发的中断/管线寄存器打印（`sched.c:196-199`、`pipe_3d.c:397-422`）与 `pr_info/pr_err/pr_warn` | 各处 |

### 8.5 运行时日志关键字速查

| 关键字 | 位置 | 含义 |
|---|---|---|
| `mwv207: sliding window failed` | `drv.c:177` | IATU 滑动窗口校验失败 |
| `mwv207: ddr size default to 256MB` / `ddr cnt default to 1` | `drv.c:299`、`:314` | VRAM 探测兜底 |
| `mwv207: vbios indexer is not valid` | `vbios.c:170` | VBIOS 索引器校验失败（HDMI PHY 走默认表） |
| `mwv207: hdmi%d use default phy data` | `hdmi.c:232` | 同上 |
| `mwv207: hdmi PHY PLL failed to lock` | `hdmi.c:298` | PHY 未锁 |
| `mwv207: no irq mapping set on %d` | `irq.c:39` | 中断位无处理者 |
| `failed to set crtc clock` | `va.c:404` | PLL 设置失败 |
| `bo validation goes crazy` | `ttm.c:540` | fault reserve 后仍在非法 VRAM |
| `can't move invisible mem by cpu` | `ttm.c:465` | 不可见显存间搬运无法用 CPU 兜底 |
| `mwv207: job from '%s' to '%s', timed out!` | `sched.c:189` | GPU 超时（后接管线状态 dump） |
| `mwv207: engine 3d is stuck` | `pipe_3d.c:351` | 环形缓冲 1 万次（×200 µs）等不到空间 |
| `mwv207: 3d axi bus error` / `3d mmu error` | `pipe_3d.c:436`、`:440` | 3D AXI/MMU 错误（仅打印） |
| `mwv207: wait for completion of DMA timeout` | `pipe_dma.c:182` | DMA 5 s 忙等超时 |
| `unexpected event: %d` | `pipe_3d.c:449` | 事件位无对应 fence |
| `mwv207 selftest passed` / `bo access test failed` | `selftest.c:104`、`:91` | 自检结果 |
| `Unable to find governor for the device`（内核 devfreq 打印） | 由 `devfreq.c:192` 触发 | 缺 `governor_performance` → 需 softdep |

---

## 9 缺陷状态（K 系列本树复核）

### 9.1 状态总表

| 编号 | 原描述（`ANALYSIS-REPORT.md` §三） | 本树判定 | 证据 |
|---|---|---|---|
| K1 | `mwv207_bo.c:172` `!flags & (1<<0)` 优先级错误 | ✅ **已修** | `bo.c:180` `if (!(jbo->flags & (1<<0)))` |
| K2 | `ttm_bo_init` 失败未释放 shmem | ✅ **已修** | `bo.c:143-149` `drm_gem_object_release` + kfree |
| K3 | `db.c` 把 `copy_to_user` 剩余字节当返回值 | ✅ **已修** | `db.c:130-134` 返回 `-EFAULT` |
| K4 | `sort`/`bsearch` 元素大小硬编码 8；`rec.val` 未初始化 | ✅ **已修** | `db.c:97`（sort）、`:118`/`:120`（bsearch）传 `sizeof(*db->db)`；`:119` 显式 `rec.val = 0` |
| K5 | `db_fini` 后 `jdev->db` 悬垂 | ✅ **已修** | `db.c:52-53` 置 NULL（注释 "avoid a dangling pointer for in-flight ioctls"） |
| K6 | 5 处 `copy_from_user` 未转 `-EFAULT` | ✅ **已修** | `submit.c:85-90`、`:121-126`、`:181-182`、`:216-221`、`:249-254`、`:262-266` |
| K7 | `init_job_reloc` 失败时 `mjob->relocs` 悬垂 | ✅ **已修** | `submit.c:85-90`（失败即 `kvfree` + 置 NULL） |
| K8 | `irq_create_mapping` 返回值被忽略 | ✅ **已修**；残留 `BUG_ON` 见 9.2 | `irq.c:146-153`、`:170-177` |
| K9 | demux ISR 的 W1C 竞态丢事件 | ⚠️ **原机制不成立**；有真实但更小的等价问题（VA vblank 整字回写 + 影子无锁） | `irq.c:44`（逐位）vs `va.c:638-641`（整字）、`irq.c:31` |
| K10 | `ctx.c` `scheds[6]` 无上界检查 | ✅ **已修** | `ctx.c:60`、`:66-67` |
| K11 | remove 顺序：`pci_clear_master` 在最后 | ⚠️ **仍在** | `drv.c:445`（`free_irq` 之后） |
| K12 | timedout_job 循环体语义可疑；重放无完整性校验 | ⚠️ **确认仍在** | `sched.c:203-204`、`:205-208` |

### 9.2 K8 的残留与 K9/K12 的详细证据

见 §6.6、§6.7、§5.9。要点复述：

- **K8**：`irq.c` 侧已修（映射失败即回退）；但 `dc/mwv207_kms.c:172` 与 `dc/mwv207_hdmi.c:1057` 仍保留 `BUG_ON(... == 0)`。前者由 `irq.c` 的完整 64 映射保证不触发；后者依赖同一保证。建议把两处 `BUG_ON` 改为 `-ENODEV` 返回（3D/2D 管线已是正确范式）。
- **K9**：demux ISR 的硬件清位是**逐位 W1C**（`1UL << j`），因此"读-写之间新置位被一并清掉"不成立。真实等价风险落在两处：① `dc/mwv207_va.c:634-655` 的 vblank 处理器整字回写 `0x9907f4`（只影响 bit 12..15）；② `jdev->irq_enable_reg[]` 被 ISR 无锁读取（`irq.c:31`）而 mask/unmask 在 `irq_lock` 内写（`:53-81`）。
- **K12**：`sched.c:203-204` 的 `list_for_each_entry` 循环体只调用 `devfreq_record_idle`，循环变量 `timedout_job` 未被使用；且 `drm_sched_resubmit_jobs`（`:205`）之前只做 `pipe->reset`，无 job 上下文完整性校验。附带发现：3D / 2D / DEC 三条管线的 `reset` 都把在飞 fence **只 `dma_fence_put` 而不 signal**（`pipe_3d.c:301-307`、`pipe_2d.c:212-227`、`pipe_dec.c:136-141`）→ 存在"用户态等一个永不 signal 的 fence"的可能；另 2D 的 `destroy`（`pipe_2d.c:424-448`）不排空 `fence_queue`，卸载时残留节点只有 `dma_fence_put` 一个去处（实机复现待验证，见 §10 U5）。

### 9.3 本树新发现（N 系列）

| 编号 | 严重度 | 问题 | 证据 | 建议 |
|---|---|---|---|---|
| N1 | 高 | `sched[3]` 槽位被 `sched_dec[1]`（dec1）与 `sched_enc[0]`（enc0）共用 → ENC scheduler 覆盖 DEC id1；dec1 的 scheduler 泄漏；INFO 报 DEC=2；`engine_id=1` 的 DEC 提交落到 ENC 调度器 | `sched.c:310-314`、`:335-339`、`:351-355`、`ctx.c:97-130` | 分离基址（如 `sched_enc = &sched_dec[2]` 并把 `sched[]` 扩到 7），或改为显式索引 |
| N2 | 中 | 3D 的 `hw_submission = 30` 与硬件事件数 30 相等，与源码注释"must be lower"冲突 | `sched.c:319-323`、`pipe_3d.c:302-306` | 降到 < 30（如 16）或扩大事件位图 |
| N3 | 中 | DEC/ENC 用 1 ms 周期轮询内核线程推进 fence，无中断；完成延迟与 CPU 占用被动依赖轮询调度 | `pipe_dec.c:81-112`、`pipe_enc.c:90-116` | 改用事件中断（hwirq 表预留），或延长轮询间隔 |
| N4 | 中 | DMA 提交在 `run_job` 上下文中忙等最多 5 s 并返回 stub fence → 调度器工作线程被长时间占用、fence 语义退化为"同步完成" | `pipe_dma.c:177-186`、`:366` | 改为真实 fence + 中断/轮询 |
| N5 | 中 | `pipe_dma_destroy`/`pipe_dma_reset` 是 `pr_info("todo")` 空壳；LLI 表靠 `dmam_alloc_coherent`（devres）兜住才不泄漏，但 DMA 通道未被停表 | `pipe_dma.c:129-138`、`:370-375`、`:402-408` | 实现 stop/reset |
| N6 | 低 | primary plane 的 `rgb_formats[]` 未含 `ARGB2101010`，但 update 分支里有该格式 → 死代码/声明不一致 | `va.c:48-54` vs `:198-201` | 统一格式表 |
| N7 | 低 | `mwv207_drm_fb_helper_init` 忽略 `crtc_count`/`max_conn_count` 参数（恒传 `num_crtcs`） | `fb.c:35-41`、`:135-139` | 参数生效或删参 |
| N8 | 低 | `request_irq`/`free_irq` 用 `pdev->irq` 而非 `pci_irq_vector(pdev, 0)` | `irq.c:161`、`:189` | 统一取值方式 |
| N9 | 低 | `kms.c:172` 与 `hdmi.c:1057` 保留 `BUG_ON(irq == 0)`（3D/2D 已用可回退写法） | `kms.c:172`、`hdmi.c:1057` vs `pipe_3d.c:589-592` | 改为返回错误 |
| N10 | 低 | HDMI `mode_valid` 的首个条件使 384–594 MHz 全部被拒；`phy_configure` 恒传 bpp = 8，PHY 表 10/12 bpp 项不可达 | `hdmi.c:903-916`、`:858` | 按 sink 能力放宽；按色深选 bpp |
| N11 | 低 | `irq_enable_reg[]` 无锁共享（K9 的残留形态） | `irq.c:31` vs `:53-81` | ISR 内也持 `irq_lock` 读影子，或用 `READ_ONCE` |
| N12 | 低 | `mwv207_i2c_put_byte` 非 static 但无头文件声明；`mwv207_i2c_set_addr` 恒返回 0（不校验 channel，靠 `create` 限 0..5） | `i2c.c:214-233`、`:171-212`、`:252` | 补声明、加校验 |
| N13 | 低 | probe 第 14 步起的 5 条错误路径不调用 `mwv207_db_fini`（`db_fini` 的置 NULL 防护只在 `err_vbios` 生效） | `drv.c:369-373`、`:414-429` | 统一错误路径 |
| N14 | 中 | 管线 `reset` 重置后丢弃在飞 fence 而不 signal（3D/2D/DEC），且 `pipe_2d_destroy` 不排空 `fence_queue` | `pipe_3d.c:301-307`、`pipe_2d.c:212-227`、`:424-448`、`pipe_dec.c:136-141` | reset 内对在飞 fence `dma_fence_set_error(-EIO)` + signal |

（编号 N1–N14 为本轮新增，与既有 O1–O12（UMD 侧）不重叠。）

### 9.4 与 `06-KNOWN-ISSUES.md` 的差异结论

| 文档原状态 | 本文结论 |
|---|---|
| K1–K7、K10 已修 | 一致（逐条给出证据行） |
| K11 仍在（`drv.c:445`） | 一致 |
| **K8/K9/K12 未复核** | **本文补齐**：K8 已修（irq 侧）+ 两处残留 `BUG_ON`；K9 原机制不成立、但等价风险在 VA/影子两处；K12 确认仍在，另发现 3D/2D/DEC 三条管线 reset 丢在飞 fence 不 signal 的疑点（N14） |
| — | 新增 N1（sched[3] 槽位碰撞，高）等 14 条本树发现 |

---

## 10 未能确认项

| # | 项 | 原因 |
|---|---|---|
| U1 | 6.6 版实际加载的 `mwv207.ko` 源码 | 不在本仓库（`06-KNOWN-ISSUES.md` 亦已注明）；本文所述 5.10 写法与 6.6 运行时行为的差异无法据此判定 |
| U2 | `struct mwv207_lli` 的字段布局与 `DMA_MAX_LLI_COUNT` 具体值 | 只在 `mwv207_pipe_dma.c` 内部使用，未逐行读取其定义段（`pipe_dma.c:189-209` 已见 `sar/dar/ts/ctl/llp` 用法） |
| U3 | 3D 环形缓冲/2D 环形缓冲的"命令打包"格式（`pipe_2d_submit` / `pipe_3d_loadstate` 之前的指针运算） | 属 UMD↔硬件协议细节，已由 `docs/registers/` 系列覆盖；本轮只按依赖核查 |
| U4 | `mwv207_pipe_codec_common.c` 的软后处理寄存器语义（`wb[i].reg` 表） | 只确认"提交时写、完成后回读"的行为，未逐项解码 |
| U5 | 管线 `reset`（3D/2D/DEC）丢弃的在飞 fence 是否会导致用户态永久等待（N14） | 需要实机复现（构造 3D/2D/decoder 超时）才能确认；本文标为「疑点」 |
| U6 | N1 在实机上是否真的导致 DEC id1 提交错路由 | 静态代码可证指针别名（`sched_dec[1] == sched_enc[0] == &sched[3]`）；实际影响取决于 UMD 是否使用 `engine_id=1` 的 DEC（`01-WINSYS-KMD.md` O 系列记录 UMD 当前 `engine_id` 恒 0，故当前可能不触发） |
| U7 | HDMI 384–594 MHz 被拒是否有意为之（如 PHY/板级限制） | 无硬件文档；标为功能限制 |
| U8 | `0x12c0000` lite 判据在先前的 91/92 变体上是否普遍成立 | 源码只给出该魔数比较；`hdmi.c:1078` 用同一处，逻辑自洽，但未见硬件文档 |
| U9 | `irq.c:146` 循环对 64 个 hwirq 全建映射是否必要（可能浪费 virq 空间） | 属设计选择，未找到驱动侧注释说明 |
| U10 | `vbios.c` 的 `parse_edp`/`parse_cfg` 空实现对应的厂商数据结构 | 源码为空函数，无法推断 |

---

## 附录 A 关键函数签名索引

| 签名 | 文件:行 |
|---|---|
| `struct mwv207_ctx *mwv207_ctx_lookup(struct drm_device *, struct drm_file *, u32)` | `mwv207_ctx.c:24` |
| `int mwv207_ctx_ioctl(struct drm_device *, void *, struct drm_file *)` | `mwv207_ctx.c:193` |
| `int mwv207_bo_create(struct mwv207_device *, u64, u64, enum ttm_bo_type, u32, u32, struct mwv207_bo **)` | `mwv207_bo.c:107` |
| `int mwv207_bo_kmap_reserved(struct mwv207_bo *, void **)` | `mwv207_bo.c:172` |
| `void mwv207_bo_placement_from_domain(struct mwv207_bo *, u32, bool)` | `mwv207_bo.c:39` |
| `int mwv207_gem_create_ioctl(struct drm_device *, void *, struct drm_file *)` | `mwv207_gem.c:89` |
| `int mwv207_gem_mmap_ioctl(...)` / `mwv207_gem_wait_ioctl(...)` | `mwv207_gem.c:118` / `:136` |
| `int mwv207_submit_ioctl(struct drm_device *, void *, struct drm_file *)` | `mwv207_submit.c:612` |
| `static int mwv207_submit_patch_entry(struct mwv207_job *, struct drm_mwv207_submit_reloc *, struct mwv207_bo *, char *, u32)` | `mwv207_submit.c:330` |
| `static int mwv207_submit_patch_dma_entry(...)` | `mwv207_submit.c:391` |
| `int mwv207_sched_init(struct mwv207_device *)` / `void mwv207_sched_fini(struct mwv207_device *)` | `mwv207_sched.c:306` / `:369` |
| `static void mwv207_sched_timedout_job(struct drm_sched_job *)` | `mwv207_sched.c:183` |
| `int mwv207_irq_init(struct mwv207_device *)` / `void mwv207_irq_fini(struct mwv207_device *)` | `mwv207_irq.c:127` / `:184` |
| `static irqreturn_t mwv207_isr(int, void *)` | `mwv207_irq.c:23` |
| `int mwv207_db_ioctl(struct drm_device *, void *, struct drm_file *)` | `mwv207_db.c:108` |
| `int mwv207_ttm_init(struct mwv207_device *)` / `void mwv207_ttm_fini(...)` | `mwv207_ttm.c:570` / `:607` |
| `int mwv207_bo_move(struct ttm_buffer_object *, bool, struct ttm_operation_ctx *, struct ttm_resource *)` | `mwv207_ttm.c:417` |
| `int mwv207_bo_fault_reserve_notify(struct ttm_buffer_object *)` | `mwv207_ttm.c:487` |
| `int mwv207_kms_init(struct mwv207_device *)` / `void mwv207_kms_fini(...)` | `dc/mwv207_kms.c:157` / `:198` |
| `int mwv207_va_init(struct mwv207_device *)` | `dc/mwv207_va.c:707` |
| `irqreturn_t mwv207_va_handle_vblank(int, void *)` | `dc/mwv207_va.c:634` |
| `int mwv207_hdmi_init(struct mwv207_device *)` | `dc/mwv207_hdmi.c:1072` |
| `int mwv207_vi_init(struct mwv207_device *)` | `dc/mwv207_vi.c:69` |
| `int mwv207_devfreq_register(struct mwv207_pipe *, unsigned long)` | `mwv207_devfreq.c:159` |
| `int mwv207_vbios_set_pll(struct mwv207_device *, int, u32)` | `mwv207_vcmd.c:125` |
| `int mwv207_test(struct mwv207_device *)` | `selftest/selftest.c:98` |
| `int mwv207_pci_probe(struct pci_dev *, const struct pci_device_id *)` | `mwv207_drv.c:321` |
| `void mwv207_pci_remove(struct pci_dev *)` | `mwv207_drv.c:432` |

## 附录 B KMD 侧主要寄存器速查

| 地址/偏移 | 域 | 用途 | 出处 |
|---|---|---|---|
| `0x100000` 起 | NT | flash/VBIOS 读取基址（`ioread8/16/32`） | `vbios.c:21-43` |
| `0xa000` | NT | VBIOS 索引器头（`0xaa55`、count、chksum）与扇区表（`+8+idx*8+4`） | `vbios.c:49-71` |
| `0x009A8020 + i*4` | NT | 中断使能（i=0,1） | `irq.c:57-81` |
| `0x009A802c + i*4` | NT | 中断状态（W1C，逐位） | `irq.c:30`、`:44` |
| `0x9b0000 + 0xA00 + 0x100*(id/6) + 0x20*(id%6) (+0/+4/+8)` | NT | PLL 的 FBDIV_REFDIV / FRAC / POSTDIV | `vcmd.c:23-28` |
| `0x9b0140` / `0x9b0144` | NT | PLL 命令请求 / 启动+结果+空闲 | `vcmd.c:49-70` |
| `0x9b0038` | NT | VGA 的 CRTC 选择（bit16-19） | `vga.c:112` |
| `0x9b003c` | NT | DVO 低 4 位 / HDMI 的 `0b111 << (8+idx*4)` 做 CRTC 选择 | `dvo.c:44`、`hdmi.c:447-448` |
| `0x9b0190` | NT | DDR 容量档位（bit0-2） | `drv.c:289` |
| `0x9b0910/0x9b0918/0x9b091c` | NT | GPIO 复用域（DDC 切到软件控制） | `i2c.c:50-98` |
| `0x9aa000/0x9aa004/0x9aa008` | NT | DDC 通道 0-5 的 SDA 输入、输出、方向 | `i2c.c:177-198` |
| `0x9aa010/0x9aa014/0x9aa018` | NT | DDC 通道 6/7 的 SCL 侧寄存器组 | `i2c.c:196-209` |
| `0x990000 + 0x100*idx (+0x600/+0xC00)` | VA | CRTC idx 的 VA 寄存器窗口（`idx>1` 偏移不同） | `va.c:29-31` |
| `+0x2a0 / 0x2c0 / 0x2c4` | VA | 时序使能/复位序列 | `va.c:480-483`、`:534-539` |
| `+0x400/0x404/0x408/0x40C/0x410/0x414/0x418/0x41C/0x420` | VA | 水平/垂直时序 | `va.c:436-483` |
| `+0x430/0x434/0x438/0x43C/0x4F8` | VA | primary 格式/行距/地址/尺寸 | `va.c:166-190` |
| `+0x440/0x444/0x448/0x44C/0x450/0x460` | VA | gamma LUT RAM 选择、FIFO 数据、激活、通道、FIFO 状态、使能 | `va.c:304-374` |
| `+0x454/0x478/0x4A8/0x4B4` | VA | 光标热点、开关、位置、地址 | `va.c:217-251` |
| `0x9907f0/0x9907f4/0x9907f8` | VA | vblank 使能（bit 12+idx）、状态（W1C）、另一使能寄存器 | `va.c:485-504`、`:634-655` |
| `0x9b001c/0x9b0020/0x9b0024/0x9b0028` | NT | VA misc reset 值 | `va.c:288-302` |
| `0x1200000 + 0x40000*idx` | HDMI | HDMI 寄存器线性空间；`+0x1000-0x1034` 时序/infoframe、`+0x0104/0x0184` 中断、`+0x3004/0x3006/0x3007` PHY、`+0x4001/0x4002/0x4004` 时钟/溢出、`+0x4100-0x4113` CSC、`+0x5000-0x5009` HDCP | `hdmi.c:22`、`:329-880` |
| `0x400 + 0x100*idx` / `0x6000 + 0x800*idx` | VI 输出 | HDMI 控制（bit0 开关、bit8/9 极性、bit10、bit28、bit4-5 CRTC）与 PHY 窗口 | `hdmi.c:25-26`、`:257-281`、`:436-449` |
| `+0x0/0x4/0x18/0x2C/0x4C/0xA4/0xA8/0xAC` | VGA(`0x9a0000`) | DAC 开关/极性/时序/负载检测 | `vga.c:36-113` |
| `+0x64+0x14*chan` 等 | VGA | HPD 检测参数 4 组 | `vga.c:18-22` |
| `0x900000 / 0x910000` | 3D | 3D0 / 3D1 管线窗口（`iosize 16 KiB`） | `sched.c:322`、`:327` |
| `0x930000 / 0x940000` | DEC | DEC0 / DEC1（DEC 完成状态在 `+0x4`，`& 0x1100`） | `sched.c:330`、`:335`、`pipe_dec.c:97-98` |
| `0x920000` | ENC | ENC0 | `sched.c:338` |
| `0x968000` | 2D | 2D0（`iosize 32 KiB`；中断清位 `+0x4308`、完成序号 `+0x6000`） | `sched.c:341`、`pipe_2d.c:406-408`、`:502` |
| `0x9d0000` | DMA | DMA0（通道 0：`+0x120/0x124/0x128/0x12C` 配置，`+0x188` 状态，`+0x198` 清位；全局 `+0x10/0x18`） | `sched.c:346`、`pipe_dma.c:120-174` |
| `0x9a0000` | VI | 输出公共窗口（DVO/VGA 用；`BUG_ON(reg >= 0x8000)` 限制 32 KiB） | `vi.h:78-99`、`dvo.c:157`、`vga.c` |

---

*本文由 KMD 源码逐行通读产出（约 10 260 行 / 39 个源文件），所有结论均带 `文件:行号`；未能确认者集中在 §10。*