# 11 · 已知问题修复可行性评估

依据 [06-KNOWN-ISSUES.md](06-KNOWN-ISSUES.md) 的 A–I 九组清单（原报告 W1–W12 / K1–K12、
新观察 O1–O12、通读发现 E/F/G/H/I），逐条评估**能否修、怎么修、验证得了吗、卡在哪**。

所有"修复方案"均为**阅读代码得出的建议**，除标注外**未落地、未验证**。

---

## 0. 一个决定性的部署前提（先读这条）

修复可行性不等于*能不能改代码*，而是**改了能不能生效**。本机现状已核实：

| 组件 | 本机实际运行的东西 | 源码可得性 | 结论 |
|---|---|---|---|
| **UMD** | Mesa 24.1.2 的 `mwv207_dri.so`（可用 `LIBGL_DRIVERS_PATH` 旁路加载） | **完整在仓库**：`src/oh-mesa/.../mesa-24.1.2` | **改了立刻可验证**（已有 patch 0009/0010 先例） |
| **KMD** | `/lib/modules/6.6.155-arm64-desktop-hwe/kernel/drivers/gpu/drm/mwv207/mwv207.ko`，250 177 B，内建描述 `MWV207 Graphics Driver`，字符串含 `mwv207_3d0`（与开源树同名管线） | **不在本机**：内核头文件包里只有 `.ko`；仓库 `src/kernel` 是 **5.10 基线**（10 260 行） | **改了不能直接生效** |

补充事实（均已实测）：

- 运行模块时间戳 **2026-08-28 16:50**，**早于**仓库修复提交 `0ec4816`(2026-10-20)、`d94c106`(2026-10-21) → **运行模块不含仓库的任何 KMD 修复**。
- 仓库 `src/kernel` 存在 `linux-6.6` 分支，但该分支内**没有 mwv207 驱动**（`git ls-tree` 无命中）→ 它只是内核基线分支，不是移植好的 KMD。
- 厂商 6.6 实现**可用但属另一套**：DKMS 已装 `mwv207/1.8.0.uos`，产出 `/lib/modules/…/updates/dkms/jmgpu.ko`（1 246 061 B，描述 `JingjiaMicro Graphics Driver`）——**已编译但未加载**；其源码在 `/usr/src/mwv207-1.8.0.uos/`，是 `jmgpu_*` 闭源实现，且**不含本树那 6 个旧 ioctl**（`grep DRM_IOCTL_MWV207_INFO|SUBMIT` 无命中）。

**推论**：KMD 类问题要落地，必须先解决"改哪棵树"：

- **路线 K-a**：把仓库 5.10 KMD 移植到 6.6 并做成独立 DKMS 模块 → 替换内建模块。5.10→6.6 的 API 替换清单**已完整存在**（`reports/ANALYSIS-REPORT.md` §四：TTM 改名、`ttm_device`/`ttm_bo_alloc`、`gem_free_object_unlocked`、`to_pci_dev` 等）。工作量中等，但一次投入后**所有 KMD 修复都可部署**。
- **路线 K-b**：给厂商 `jmgpu_*` 源打补丁 → 6.6 现成、DKMS 现成，但那是**另一套实现**，本清单里的行号/结论**多数不适用**，且厂商头文件标注机密。
- **路线 K-c**：只改仓库 5.10 树作为"修复记录/上游化准备"，不部署 → 零风险，但用户看不到变化。

> 因此后文凡标 **【KMD·需部署路径】** 的条目，其"代码改动难度"都很低，
> 真正成本在 K-a/K-b 的取舍；未标者即 UMD，可立即验证。

---

## 1. 总览分级

| 级别 | 含义 | 条目数 | UMD | KMD |
|---|---|---:|---:|---:|
| **A** | 低风险、改动 ≤ 数行、可静态判定 → 建议直接修 | 14 | 11 | 3 |
| **B** | 能修，但必须配硬件/实验验证，或需先查证语义 | 16 | 9 | 7 |
| **C** | 信息边界或硬件边界 → 只能降级声明、或需更多厂商信息 | 6 | 4 | 2 |
| **D** | 不是缺陷修复，而是**新增功能实现** | 6 | 3 | 3 |

---

## 2. A 类：建议直接修（改动 ≤ 数行）

| # | 位置 | 修复方案 | 难度 | 风险 | 验证 |
|---|---|---|---|---|---|
| A1 **W6** | `winsys/mwv207/drm/mwv207_drm_bo.c:382-383`（CALLOC）、`:405-406`（解引用） | 分配后判空，`NULL` 即返回 | 1 行×2 | 极低 | 静态 + 可注入分配失败 |
| A2 **W7** | 同文件 `:283-284`：`GEM_MMAP` 失败仅 `printf` 后用 `args.out.offset`（可能为 0）继续 `os_mmap()` | 失败即 `return NULL`，不再 mmap | 3 行 | 低（仅错误路径） | 静态；可用非法 handle 触发 |
| A3 **W12** | `mwv207_drm_cs.c:125-147` 依赖 `assert` | 改为返回错误码，调用方检查 | 小 | 低 | 静态 |
| A4 **O2** | `mwv207_drm_bo.c:293` `return ptr;` → `return bo->logical;` | 1 行（与厂商 22.3.7 树一致） | 1 行 | **零**：`MWV207_FLAG_SUBALLOC` 全树无人写入 → slab 路径是死代码，当前不可触发 | 静态即可定论 |
| A5 **O3** | `mwv207_drm_cs.c:267` 之前补 `memset(dest_dat->data + dest_dat->size, 0, 0x100);` | 1 行；**不要**改 `:268` 为 `+= src_dat->size` | 1 行 | 低；修的是"每槽尾 0x64 字节未初始化数据发往 GPU" | 静态 + 可读 dat BO 比对 |
| A6 **E1** | `mwv207_draw.c:751-754` `color_8` 四个分量都 `<< 8` | 按目标字段宽度修正移位 | 小 | 低；属"blend color 高位互相覆盖" | 需 blend-color 测试 |
| A7 **E2** | `mwv207_draw.c:888,907` 函数内 `static draw_count` | 移入 `mwv207_context` | 小 | 低；修"跨 context 共享计数" | 两个 context 交替绘制 |
| A8 **E4** | `mwv207_draw.c:1012` FS `shader_states` 未判空 | 加判空（并确认无 FS 时的正确行为） | 小 | 低 | 无 FS 绑定的 FBO 绘制 |
| A9 **E9** | `include/mwv207_state.h:143` `mwv207_context_zsa_init` 只有声明 | 删除声明 | 1 行 | **零**（无定义、无调用） | 编译 |
| A10 **E10/拼写** | `mwv207_query.c:342`、`include/mwv207_query.h:53` `destory`；另 `cmd_dat_arrary`、`MWV207_IMMEDIATE_GLOBAL_MEMEORY_OFFSET`、`CACHE_TEXUTRE_L1` | 改名（内部符号，无 ABI 影响） | 小 | 极低 | 编译 + 回归 |
| A11 **F.3.2** | `nir/mwv207_disasm.c` 断言 `sizedwords % 2 == 0` 而步长是 4 | 修正断言 | 1 行 | 零（仅调试工具） | 反汇编任一着色器 |
| A12 **F.3.3** | 同文件：FS color 输出遇 0 截断 | 修 printf 逻辑 | 小 | 零 | 同上 |
| A13 **N9** | KMD `mwv207_kms.c:172`、`mwv207_hdmi.c:1057` 残留 `BUG_ON` | 改为可回退错误路径（3D/2D 已有范式） | 小 | 低 | **✅ 已做（2026-11-10 `6502f19`）：`pr_err` + `-ENODEV`，重启后启动无报错** |
| A14 **K11** | KMD 移除路径：`free_irq` 后设备仍可拉中断（`0x009A8020/24` 使能位未清）、`pci_remove` 缺 `pci_disable_device` | 在 `irq_fini()` 的 `free_irq` 前硬件关中断 + 清挂起；`clear_master` 后补 `pci_disable_device`（`clear_master` 位置保持与厂商一致） | 小 | 中（次序敏感） | **已做（2026-11-10 `fbed27f`）**；卸载路径无法在桌面运行中实测，以代码审查 + 厂商对照为准 |

> A 类里 A4/A9/A11/A12 基本是"零风险修正"；A5 是本清单里**性价比最高的一条**（1 行换来 GPU 不再读到堆残留）。

---

## 3. B 类：能修，但必须验证 / 需先查证

| # | 位置 | 修复方案 | 难度 | 卡点 |
|---|---|---|---|---|
| B1 **O5** | `mwv207_drm_cs.c:89-101` `fence->fd < 0` 时 `fence_wait()` 恒 `true` | 让失败提交不再被当作"已完成" | 小 | **已修（2026-11-03，`4a19713`）**：视频路径 `5dee412`；`mwv207_flush()` 改为失败返回 NULL fence（Mesa 前端判空），故障注入探针验证 |
| B2 **O4** | `mwv207_drm_cs.c` commit 的 `-EBUSY` 无上限重试；KMD `mwv207_submit.c:591-599` 失败路径似不回收入 fence fd | 加上限 + 失败回收 fd | 中 | 需先读 KMD submit.c 确认是否真泄漏 |
| B3 **E3** | `mwv207_blit.c:568-584` 压缩格式分支里算好的 `task_info` 被无条件赋值覆盖 | 恢复预期值 | 中 | **2026-11-03 复核：不可达，不改**——压缩拷贝走 CPU 回退、压缩格式 tile 不支持、GL 不允许压缩 blit；ETC2 `glCopyImageSubData` 实测一致 |
| B4 **E5** | `mwv207_state.c:326-331` `stencil_back_reference` 与正面索引交叉 | 修正索引 | 小 | **已修（2026-11-03，`5dee412`）**：`mwv207_draw.c` 的 0x528 恒取背面数组首元素（set 时已按背面语义预填） |
| B5 **E6** | `mwv207_query.c:108-112` `TIMESTAMP` 用 CPU `os_time_get_nano()` | 改用真 GPU 时间戳 | 中 | **2026-11-03 复核：不改**——与 llvmpipe 同为 CPU 时钟语义，行为一致；硬件寄存器 `0x00490/0x00494` 虽存在，切换属功能改动且需整条 query 路径验证 |
| B6 **E7** | `mwv207_query.c:156-163,279-282` `PIPELINE_STATISTICS`/`SO_STATISTICS` 不发射寄存器，`get_result` 走 `unreachable()` | 最小可行：`begin/end` 报 `available=false`，消除 `unreachable()` 崩溃 | 小 | 完整实现需把硬件计数器映射到各统计项，中-大 |
| B7 **E8** | `mwv207_state.c:335-354` 只保存单份视口，忽略 `num_viewports` | 存数组并逐视口编程 | 中-大 | **2026-11-03 复核：非缺陷**——cap=1、`ARB_viewport_array` 未暴露（`GL_MAX_VIEWPORTS` 查询 INVALID_ENUM）；多视口属新功能 D6 |
| B8 **N2** | KMD 3D `hw_submission = 30` 与硬件事件数 30 相等，违背源码注释 "must be lower than 30" | 降到 29 或按事件数推导 | 小 | **✅ 已做（2026-11-10 `6502f19`）：`MWV207_3D_HW_SUBMISSION = 28`，重启后无事件表满 `BUG_ON`**；压满场景验证见 `docs/UNRESOLVED.md` §2.2 |
| B9 **N10** | KMD HDMI `mode_valid` 阈值（6.6 树为 594 MHz）与 `phy_configure` 恒传 `bpp=8` | 调整判据、按真实 bpp 传参 | 中 | **决定不做（2026-11-10，只文档化）**：6.6 树只挡 6K/8K，本机 1080p@100Hz 正常，需 4K60+ 实测且收益极低 |
| B10 **N14** | KMD `mwv207_pipe_3d.c:301-307`、`mwv207_pipe_2d.c:212-227`、`mwv207_pipe_dec.c:136-141`：`reset` 只 `dma_fence_put` 在飞 fence 而**不 signal**；`mwv207_pipe_2d.c:424-448` `destroy` 不排空 `fence_queue` | reset 时以 `-EIO` signal 在飞 fence；destroy 排空队列 | 中 | **已修（2026-11-10，`22145e9`）**：四条管线统一 `dma_fence_set_error(-EIO)`+signal，destroy 排空；DEC/ENC 用 `xchg` 抢槽防双重 put。模块已于 2026-11-10 重启后生效（`039F972E…`，重启后 GL/query/VP9 回归全过）；3D reset 通路已用 `tests/n14-reset/` 探针实测（6 轮 TDR、`pipe_3d_force_complete` 6/6、用户 fence 12.3 s 完成、无 WARN），2D/DEC/ENC 与 destroy 路径未覆盖 |
| B11 **K12** | KMD `mwv207_sched.c:203-204` 超时循环体只有 `devfreq_record_idle`，循环变量未用，`drm_sched_resubmit_jobs`(:205) 前无 job 完整性校验 | 补真正的 job 检查与信号 | 中 | **✅ 已做（2026-11-10 `6502f19`）：新增 `mwv207_devfreq_reset_busy()` 直接清零 `busy_count`（`drm_sched` 侧的完整性由 6.6 的 stop/start 负责），重启后 6 轮 TDR 无 `busy_count` WARN** |
| B12 **K9-①** | KMD `dc/mwv207_va.c:634-655` vblank 处理器整字回写 `0x9907f4` | 与厂商源码对照后确认 | — | **定为非缺陷（2026-11-10）**：厂商 `jmgpu_package.c:1158` 的 `j9_handle_attribute_commandery()` 同样是读 `0x9907f4` 再原值写回，该寄存器就是 W1C 状态寄存器；唯一差别是厂商还处理 bit26–29（overlay），本驱动无 overlay 面 |
| B13 **K9-②** | KMD `mwv207_irq.c:31` ISR 无锁读 `irq_enable_reg[]`，而 mask/unmask 在 `irq_lock` 内写 | 加锁或改用影子/原子 | 小-中 | **✅ 已做（2026-11-10 `6502f19`）：`READ_ONCE`/`WRITE_ONCE`（不加锁，避免 ISR 里持锁调 `generic_handle_irq`），重启后全流程无 IRQ 报错** |
| B14 **N1** | KMD `mwv207_sched.c:310-314`：`sched_enc = &sched_dec[1]` 使 **`sched_dec[1]` 与 `sched_enc[0]` 同指 `&sched[3]`**；而 `:353` 仍报 `nr_dec = 2` | 扩大槽数组到 7 并修正偏移（`sched_enc = &sched_dec[2]`），同步改 `fini` 循环 | 小 | **✅ 已做（2026-11-10 `6502f19`）：`MWV207_MAX_SCHED_NR 7` + 显式下标（3d=0..1、dec=2..3、enc=4、2d=5、dma=6），三处遍历同步；UMD 无需改（`engine_id` 语义不变），重启后 `tests/engines/` 核对 lite 机型 3d=1 dec=1 enc=0 2d=1 dma=1** |
| B15 **N4** | KMD `mwv207_pipe_dma.c:177-186,366`：DMA 提交在 `run_job` 里忙等最多 5 s 并返回 **stub fence** | 真 fence + 独立完成工作队列（睡眠轮询，1500 ms 上限） | 大 | **已做（2026-11-10 `fbed27f`，重启后已验证生效）** |
| B16 **F.3.1** | `nir/mwv207_disasm.c:141-158` 的 `_str_inst_type[]` 与 `nir/mwv207_emit.c:205-232` 的 `get_alu_inst_type()`/`ops[].type` 编码矛盾（1 是 `.f16` 还是 `i32`） | 以 emit 侧为准统一 | 小 | 建议**先用厂商枚举判明** type 编码（见 C 类 F.1） |

---

## 4. C 类：信息边界 / 硬件边界

| # | 条目 | 判定 | 可做的最优动作 |
|---|---|---|---|
| C1 | **C1 遗留**：`R16G16_UNORM`/`R16_SNORM`/`R16G16_SNORM` 采样恒 0。**【2026-11-03 已定位】**不是编码未解，而是 0x1F76 不解码 `TX_EXT_CONFIG.FORMAT` 码 `0x32–0x36`：把工作正常的 `R16_FLOAT` 纹理强制换码 0x36 亦读 0，同纹理码 7 正常；16 位整数码 `24–26`、浮点码 `7–9` 取数正确，上传/读回字节精确，tiling/LINEAR、align、NORM、new-mode、bit23/31 全无效 | **硬件边界**（解码单元缺口），非笔误 | ① 反查字段表（2026-11-03 已走完：无残留候选字段，确认无解）；② **已拍板（2026-11-03）保留声明并注明** |
| C2 | **C2 高级混合只有 8/15**：`0x9–0xF` 使帧缓冲清零，证明**混合单元只实现 8 个扩展方程** | **硬件边界**，不可修 | **已拍板（2026-11-03）：保留超额声明并注明**（维持原生 GLES 3.2）；撤销 cap 会丢失 GLES 3.2。着色器模拟理论上可行但语义/代价都差 |
| C3 | **C3** `ARB_ES3_compatibility` 建立在这 6 个格式声明上，其中 3 个实际不可采样 | **决策问题** | **已拍板（2026-11-03）：随 C1 取 A，保留并注明** |
| C4 | **F.1** ISA 常量头 `INST_OPCODE_*`/`INST_TYPE_*`/`INST_EXT_OPCODE_*` 在本树无定义（`include/hw/*.h` 是空壳） | **部分可救** | 扩展操作码**已有厂商枚举**（`SH_EXTENDED/CMPLX/IMG_ATOM/VISION_OPCODE`）→ 可据此**生成** `INST_EXT_OPCODE_*` 头，给 101 条 opcode 里的扩展部分一个厂商依据；**基础 ALU 操作码仍未找到厂商枚举** |
| C5 | **I.4** 视频解码块的 `word → 厂商名` 无法反查（厂商 1392 条寄存器里**没有视频解码块**，`MWV207REG_DEC*` 只有 2D/3D 的 `DEC400`） | **数据源缺口**，不可修 | 只能沿用驱动自己的三张表（1840 行，已全量还原） |
| ~~C6~~ | **I-2** `JMDEC_BITPLANE1_E` 与 `JMDEC_BLACKWHITE_E` **同占 word7 bit30** | **已做（2026-11-10，`a5a938b`）** | 查证结果＝**真复用**：原厂 `{word,width,shift}` 表里 BITPLANE0/1/2 与 CABAC/BLACKWHITE/DIR_8X8_INFER 在 word7 上逐位同名同址（31/30/29），是按 codec 复用的物理位；H.264 不走 bitplane，故删掉那句 `set_reg(BITPLANE1_E, 0)`（它等于取消单色模式） |

---

## 5. D 类：新增实现（不是修复）

| # | 目标 | 可行性 | 依据 / 成本 |
|---|---|---|---|
| D1 | **VP9 解码** | **已完成（2026-11-10）** | Profile0 8bit 自 2026-11-06 起逐位一致（回归 18/18）；Profile2（10bit 4:2:0）2026-11-10 完成（`82e7b02`）——10bit 通路与 HEVC Main10 共用，缺的只是入口 gate 与 profile 声明，4 组样片（64x64~1080p，含 inter 帧）与软件解码逐位一致、零 TDR。载荷 r20 已安装（2026-11-10 16:17），重启后实机复核 1080p30 逐位一致、零超时。Profile1/3 定案不可实现（解码器没有色度采样格式选择字段） |
| D2 | **H264 4:2:2 / 4:4:4** | 二选一 | 诚实路线：取消 `High422`/`High444` 声明（小）；实现路线：改 `mwv207_dec_h264.c:1184` 硬置的 `chroma_format_idc = 1`（大，且需硬件支持验证） |
| D3 | **编码（H264/HEVC）** | 大 | UMD 侧 `base.encode_bitstream` 从未安装；KMD 有 `mwv207_pipe_enc.c`。属从零实现 |
| D4 | **4 个未实现的扩展操作码** `WAIT_GROUP_EVENT`(0x11)/`TD_OP`(0x12)/`TEX_INVALIDATE`(0x14)/`PRE_NORM`(0x15) | 小-中 | 厂商枚举齐全，需逐个确定语义 |
| D5 | **开启三个窥孔** `OPT_DIV`/`OPT_FFMA`/`OPT_SET`（默认全关） | 小改动 + 大验证 | 潜在性能收益，但需全量着色器回归确认正确性 |
| D6 | **多视口（E8）** | 与 B7 同 | 取决于硬件是否支持 |

---

## 6. 建议的修复批次

**批次 1 — UMD 零/低风险（可当天完成并验证）**
`A4 → A9 → A11 → A12 → A1 → A2 → A3 → A5 → A10`
全部是错误路径、死代码或单行补丁。验证：重建 `libmwv207_dri.so` → `tests/run-all.sh` 回归 + `tri-test`/`fbo-test`/`egl-smoke`。
**其中 A5（O3 未初始化数据上 GPU）建议优先**，它是唯一"影响面覆盖每一次带纹理采样的 draw"的条目。

**批次 2 — UMD 需行为验证**
`A6/E1、A7/E2、A8/E4 → B1/O5、B3/E3、B4/E5、B5/E6、B6/E7`
每条配一个针对性探针（我已在本轮建过 `es32probe`/`blendtest2`/`fmtprobe2` 这类小工具，可复用同一套路）。

**批次 3 — 决策先于动手**
C1/C3（16 位格式 A/B/C）、C2（混合 8/15 是否保留超额声明）、D2（4:2:2/4:4:4 是否取消声明）。
这三条**不是技术难度问题，而是诚实性取舍**，需要你拍板。

**批次 4 — KMD（必须先解决部署路径）**
先做 **K-a**（把 5.10 KMD 移植成 6.6 DKMS 模块），之后 `A13/N9、A14/K11、B8/N2、B9/N10、B10/N14、B11/K12、B12/K9-①、B13/K9-②、B14/N1` 才谈得上生效。
**其中 B14/N1 优先级最高**（对象与管线泄漏 + `INFO` 谎报 DEC=2），但**当前 UMD 恒用 `engine_id=0` 所以未触发**——即"高严重、零现网影响"。A13/N9、B8/N2、B11/K12、B13/K9-②、B14/N1 已于 2026-11-10 一起修掉（`6502f19`，同日重启后验证生效）；B10/N14 更早已修并实测（`22145e9`）；B15/N4、A14/K11 也已修（`fbed27f`，同日重启后验证生效）；B12/K9-① 定为非缺陷；B9/N10 决定不做。
**B10/N14 已修并实测 3D reset 通路**（`22145e9`）；若将来出现"某次 hang 后应用卡死"，仍应首先怀疑它在 2D/DEC/ENC 与 destroy 路径上的残余风险。

**批次 5 — 视频**
`I-1`（`mwv207_dec_hevc.c:1060` 位深当图像高）与 `I-2` 都是**一行级修复**，已完成
（`385ed81` / `a5a938b`，验证＝HEVC 8bit/Main10 2 tile 列与软件解码逐位一致、VP9 18/18）。
`I-3`（H.264 High422/444）**不可实现**，已收回声明：解码寄存器表里没有色度采样格式
选择器、原厂 `jmgpu_drv_video.so` 的文档串只写 `0=4:0:0 1=4:2:0` 且 `QueryConfigProfiles`
不声明 422/444、libva 也没有对应 VAProfile —— 三条互相独立（详见 `UNRESOLVED.md` §1.5）。
`I-4`（H.264 High10）已定案并收回声明：原厂特性表对 `hw_build_id 0x1F76` 写
`h264_high10_support = 1`，但按当前寄存器配置提交 10bit 码流会让 **DEC 管线挂死**
（实测 6 次 `job from 'av:h264:df0' to 'mwv207_dec0', timed out!`、输出面全 0；
8bit 同路径零超时）。Hantro 系的 10bit H.264 需要 **high10p 位流预转换**，本驱动
未实现，原厂闭源驱动同样不声明 High10 → `4ecca0d` 收回 `MPEG4_AVC_HIGH10` 并在
`end_frame` 跳过高位深码流。真正实现属"大"：需要先做位流重打包，再重调
`BIT_DEPTH_*`/`DEC_MODE`/输出位深寄存器。
`D1`（VP9）已完成：Profile0 8bit 自 2026-11-06 起逐位一致；Profile2（10bit 4:2:0）
2026-11-10 补齐（`82e7b02`），走的是与 HEVC Main10 共用的 P010 通路。

---

## 7. 验证方案（通用）

| 层次 | 手段 | 现状 |
|---|---|---|
| 静态 | 编译 + `docs/tools/validate_*.py` + 本次新增的逐行核对 | 已有 |
| UMD 功能 | `tests/run-all.sh`；`tri-test`/`fbo-test`/`egl-smoke`（源码即用） | 已有，本轮已跑通 |
| UMD 行为 | 针对性临时代码（已验证这套做法可行）：ES/GL 探针、混合方程比对、格式采样探针 | 已有多个可复用 |
| 性能 | `glmark2 -b :duration=1` | 已有基线（422/383/372 vs 401/468，噪声级） |
| KMD | `dmesg` + 模块加载/卸载 + 挂起恢复；reset/fence 类需构造场景 | **缺**：无 6.6 构建与部署链路 |
| 视频 | ffmpeg 生成/解码比对 + VA-API 探针（本轮已写过 4K/8K 校验） | 部分已有 |

---

## 8. 一句话结论

- **UMD 侧**：A 类 11 条中的 4 条属零风险，**立即可以修并验证**；B 类 9 条都能修，成本主要在**先查清语义**与**补测试**。
- **KMD 侧**：**代码改动都很小，卡点是"运行的不是我们能改的那棵树"**。要先投入一次 6.6 移植（替换清单现成），否则所有 KMD 修复都只能停留在仓库里。
- **C 类**（16 位采样、8/15 混合、视频寄存器名）：属**信息/硬件边界**。16 位采样已于 2026-11-03 定位为**硬件不解码**（码 0x32–0x36 返回 0，整数/浮点 16 位正常），能做的只有"降级声明"或保留并注明；混合 8/15 同理，不该承诺修好。
- **D 类**（VP9、编码、4:2:2/4:4:4）：**VP9 已完成**（Profile0 8bit + Profile2 10bit 4:2:0，
2026-11-10 逐位验证）；编码完全未实现；4:2:2/4:4:4 **定案不可实现**（解码器没有色度
采样格式选择字段，见 `UNRESOLVED.md` §1.5）。

> 本文件全部为静态评估。文中"已找到硬件寄存器""厂商枚举齐全"等均为**在厂商头文件中读到的事实**，
> 但不代表对应功能已在硬件上验证可用。