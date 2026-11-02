# 未修复缺陷与未实现功能 · 总清单

> 汇总截至 **2026-11-02**，对应外层仓库工作树（含 `src/mwv207-umd/` 分层）。
> 取代散落在 `api/06-KNOWN-ISSUES.md`、`api/11-FIX-FEASIBILITY.md` 里的分散记录：
> 本文件是**唯一索引**，那两个文件保留历史细节与推导过程。
>
> **口径**：只列**当前仍未落地**的项。2026-10-30/31 已修复的（VRAM 单位、O2、O3、
> 遮挡查询 C4）、2026-11-02 已修复的（条件渲染 C5 真实现、KMD 零重定位提交空 entity N15）
> 不在此列，见文末《已修复索引》。
> 每条给出 `文件:行号`；标【实测】的表示上机验证过，未标者为静态审计结论。

---

## 0. 一图看清卡在哪

| 层 | 改了能立刻验证吗 | 阻塞原因 |
|---|---|---|
| **UMD**（`src/mwv207-umd/`） | ✅ 能 | 源码完整在仓库；`LIBGL_DRIVERS_PATH` 可旁路加载，`umd install` 一键部署 |
| **KMD**（`src/kernel-6.6/`） | ✅ 能（**已部署**） | 自建模块已装到 `/lib/modules/…/updates/dkms/mwv207.ko` 并在运行 |

两层的卡点性质不同：**UMD 卡在"要不要改 / 改了是否诚实"**，
**KMD 已不再是卡点**（详见下）。

### KMD 部署状态（2026-11-02 复核，已更正）

此前本节写作"从未真机加载"，**已过时**。实测：

```
cat /sys/module/mwv207/srcversion        -> D507FC2774D547E7BEBA182
modinfo -n mwv207                        -> /lib/modules/6.6.155-…/updates/dkms/mwv207.ko
modinfo src/kernel-6.6/mwv207.ko         -> srcversion D507FC2774D547E7BEBA182
modinfo /lib/modules/…/updates/dkms/…    -> srcversion D507FC2774D547E7BEBA182
```

三方一致 ⇒ **运行中的就是自建版**，且包含：
- `b0479ba` 移植的 K1 / K3 / K4 / K5 / K6 / K7 / K8 / K10 + patch 0008（softdep）；
- `28bc73d` 修的 N15（零重定位提交未初始化 `drm_sched_job` → `BUG_ON(!entity)`）。

因此 **§2 里标【待部署】的条目，K 系列修复已生效**，剩下的是从未做过修复的
N1 / N14 / K11 / N2 / N4 / K9-① / K9-② / K12 / N10 / N9（见 §2）。
自建模块会打印 `loading out-of-tree module taints kernel` 与
`module verification failed: signature and/or required key missing`，属正常（未签名）；
dmesg 里 GPU 相关错误计数为 0。

---

## 1. UMD — 未修复缺陷

### 1.0 已修（2026-11-03）：dual-source blend 编译期空视图解引用（火狐 GitHub 崩溃）

- 现象：Firefox 打开 GitHub 后概率性 SIGSEGV（crash_address `0x48`，SEGV_MAPERR）。
- 帧链：`get_rt_texture_coord` (`nir/mwv207_lower_dual.c:115`) ← `mwv207_lower` (`mwv207_lower.c:1690`) ← `mwv207_compile_shader` ← `mwv207_set_current_shader` ← `mwv207_update_shader` ← `_mesa_DrawElementsInstancedBaseVertexBaseInstance`。
- 根因：① `mwv207_config_dual_blend()`（`mwv207_draw.c:1214`）把 RT blit 视图绑在 `num_sampler_views[FS]` 槽，但 `dsbi.sampler_index` 记的是 `num_samplers[FS]`（两计数不等，视图槽回缩后错位）；② `mwv207_lower_dual.c:115` 无条件解引用 `sampler_views[4][sampler_index]->texture`（`0x48` = `pipe_sampler_view::texture` 偏移）。
- 修复：`cbufs[0]`/`create_sampler_view` 为 NULL 时关闭 is_dual；`sampler_index` 与 `bind_sampler_states` 槽统一为 `num_sampler_views[FS]`；`get_rt_texture_coord()` 判空并回退到 `fb_state` 渲染目标尺寸。
- 回归：wrreplay VT/brush/triv 全 OK；query tests PASS=4；glxgears/GLES 3.2/2048MB 不变；Firefox GitHub 45s 静止加载崩溃 0（原崩溃概率性触发，需日常使用观察）。
- 提交：`40d5d76`；补丁 `patches/0020-mwv207-dual-blend-null-view-fix.patch`（85 行，`fb9c9b1`）。

### 1.1 错误路径 / 健壮性（低风险，可直接改）

| # | 位置 | 问题 | 当前证据 | 建议 |
|---|---|---|---|---|
| ~~W6~~ | `winsys/mwv207/drm/mwv207_drm_bo.c` | ~~`CALLOC` 后无 NULL 检查~~ | **已修**（2026-11-02）：加 `if (!bo) goto unlock;` | — |
| ~~W7~~ | `winsys/mwv207/drm/mwv207_drm_bo.c` | ~~`GEM_MMAP` 失败仅 `printf` 不返回~~ | **已修**（2026-11-02）：失败改 `fprintf` + `return NULL` | — |
| ~~W12~~ | `winsys/mwv207/drm/mwv207_drm_cs.c` | ~~依赖 `assert`，release 下被移除~~ | **已修**（2026-11-02）：4 处分配全部判空、失败释放并返回 NULL | — |
| **O5** | `winsys/mwv207/drm/mwv207_drm_cs.c:100-118` | `fence->fd < 0` 时 `fence_wait()` 恒返回 true | 【复核】`fd<0` 来源已枚举：`video_cs.c` 的 `out_fence_fd` 未初始化 → **已修**（改 `int out_fence_fd = -1;`）；`fence_create(NULL)` 由调用方判空。剩余：提交失败时 `flush(pfence)` 仍用 `fence_create(ws, -1)` 造出"永远就绪"的 fence，但失败本身现在会 `fprintf` 报错 | 低（失败已可见，不再静默） |
| ~~E4~~ | `mwv207_draw.c` | ~~无深度模板时解引用 FS `shader_states` 未判空~~ | **已修**（2026-11-02）：`fs_state ? fs_state->current : NULL`，GEOMETRY 同样加 `&& ->current` | — |

### 1.2 会直接影响可观测行为（需行为验证）

| # | 位置 | 问题 | 当前证据 | 建议 |
|---|---|---|---|---|
| **E1** | `mwv207_draw.c:751-754` | `color_8` 四个分量都 `<< 8` | **【实测更正】不是高位覆盖**：`/tmp/blendcolor` 用 `glBlendFunc(GL_CONSTANT_COLOR, GL_ZERO)` + `glBlendColor(0.5,0.25,0.125,1.0)` 实测，llvmpipe `(26,26,19,204)`（= 期望值），我们 `(25,25,19,204)` —— **只差 1 LSB，A 通道完全正确**，说明硬件确实按"低 8 位 + 某个位宽"解释了该寄存器，没有互相覆盖。原文档"高位互相覆盖"的判断作废 | 不改（按目标字段宽度改移位缺乏验证手段，可能引入真错）；如需精确到 0 LSB 再查 `0x509` 定义 |
| ~~E2~~ | `mwv207_draw.c` | ~~`static uint32_t draw_count` 跨 context 共享~~ | **已修**（2026-11-02）：移入 `struct mwv207_context` 的 `uint32_t draw_count;`，`mwv207_config_draw()` 读写 context 字段 | — |
| **E6** | `mwv207_query.c:108,201,284` | `TIMESTAMP` / `TIMESTAMP_DISJOINT` 用 **CPU** `os_time_get_nano()`，不是 GPU 时间戳 | 【复核】三处均 `os_time_get_nano()`；【实测】`GL_TIMESTAMP` 返回 ≈31.7 小时 = CPU 开机时长 | 硬件寄存器已找到：`MWV207REG_DRAW_PRIMITIVE_START/END_TIME_STAMP`(0x00490/0x00494)、`COUNTER0-3`(0x00544–0x00550) |
| ~~E7~~ | `mwv207_query.c` | ~~不支持的 query 类型走 `unreachable()`~~ | **已修**（2026-11-02）：`SO_STATISTICS` / `PIPELINE_STATISTICS` / `PIPELINE_STATISTICS_SINGLE` / 未知类型统一 `return false`（按 Gallium 契约＝结果不可用），不再 `unreachable()`（release 下是 `__builtin_unreachable()`＝未定义行为） | 剩余：这些类型仍无真实硬件计数 |
| **E8** | `mwv207_state.c:335-354` | 只保留单份视口/裁剪，`set_viewport_states` 忽略 `num_viewports` | 静态；GL `GL_MAX_VIEWPORTS` 报 0/1 → **很可能硬件本身就受限** | 先确认硬件是否支持多视口 |
| **E3** | `mwv207_blit.c:568-584` | 压缩格式分支里算好的 `task_info` 被随后无条件赋值覆盖 | 静态 | 需先确定正确值 |
| **E5** | `mwv207_state.c:326-331` | `stencil_back_reference` 与正面索引交叉 → 背面取到正面值 | 静态 | 需双面模板测试确认期望 |
| ~~O4~~ | `winsys/mwv207/drm/mwv207_drm_cs.c` | ~~`-EBUSY` 无上限 busy-loop；重试可能泄漏 fd~~ | **已修**（2026-11-02）：重试上限 2000 次（≈2s）；失败改 `fprintf` 并关闭 KMD 已 `fd_install` 的 out-fence（判据：`out_fence_fd && submit.fence_fd >= 0 && (in_fence_fd < 0 || submit.fence_fd != in_fence_fd)`，避免误关调用者的 in-fence） | — |

### 1.3 声明与实现不符（诚实性问题，需拍板）

| # | 问题 | 现状 | 三个选项 |
|---|---|---|---|
| **C1** | `R16G16_UNORM` / `R16_SNORM` / `R16G16_SNORM` **采样恒 0**。数据本身已字节精确（强制 LINEAR 修好了上传），但 6 位采样器格式码 `0x00–0x36` **全扫无解**；厂商文档给的 RG16 码 `0x34/0x35` 读出全 0 | 【复核】`mwv207_format.c:144-147` 已声明这 4 个格式；`:462-465` BLT 码已加 | **A** 保留声明并注明 / **B** 回退到只留 `R16_UNORM` / **C** 继续追采样编码 |
| **C2** | 高级混合**只有 8/15** 个方程。硬件仅实现 MULTIPLY/SCREEN/OVERLAY/DARKEN/LIGHTEN/HARDLIGHT/DIFFERENCE/EXCLUSION；`0x9–0xF` 实测**把帧缓冲清零** | 【实测】属真实硬件缺口 | 保留超额声明并注明（现状，**解锁原生 GLES 3.2 的必要代价**），或撤销该 cap（代价：丢 GLES 3.2） |
| **C3** | `ARB_ES3_compatibility` 建立在 C1 那 6 个格式声明上，其中 3 个实际不可采样 → 用 ETC2/EAC 的应用**渲染错误但不崩** | 依赖 C1 | 同 C1 的 A/B/C |
| **I-3** | 能力表宣告 H264 含 **High422 / High444**，但解码器**硬置** `chroma_format_idc = 1`（`video-codec/mwv207/mwv207_dec_h264.c:1184`）→ 实际只走 4:2:0 | 【复核】硬置行仍在 | 诚实路线：取消两个 profile 声明；实现路线：改硬置值（大，需硬件验证） |

### 1.4 编译器 / 工具链（不影响渲染正确性）

| # | 位置 | 问题 |
|---|---|---|
| **F.3.1** | `nir/mwv207_disasm.c:141-158` vs `nir/mwv207_emit.c:205-232` | `_str_inst_type[]` 与 `get_alu_inst_type()` 的 type 编码**互相矛盾**（1 是 `.f16` 还是 `i32`）→ `type != 0` 时打印的类型名基本全错 |
| **F.3.2** | `nir/mwv207_disasm.c` | `mwv207_disasm()` 断言 `sizedwords % 2 == 0`，实际步长是 **4** |
| **F.3.3** | `nir/mwv207_disasm.c` | FS color 输出打印遇 0 截断 |
| **F.4** | 见 `api/04-NIR-COMPILER.md` §12 | `link_shader` 恒 `return false`；`shared_atomic` 形状匹配失败即 `assert(0)`；RA 溢出循环无迭代上限；变体命中只查 4 个 dirty 位 等 7 条 |
| **E9** | `include/mwv207_state.h:143` | `mwv207_context_zsa_init` 只有声明、无定义、无调用（删掉即可） |
| **E10** | `mwv207_query.c:342`、`include/mwv207_query.h:53` | `destory` 拼写；另有 `cmd_dat_arrary`、`MWV207_IMMEDIATE_GLOBAL_MEMEORY_OFFSET`、`CACHE_TEXUTRE_L1` |

> F 类只影响**反汇编输出可读性**（调试工具），不影响编译与渲染。
> 但因为 ISA 常量头缺失（F.1），反汇编表是 opcode 数值的**唯一交叉验证来源**，所以修它有额外价值。

---

## 2. KMD — 未修复缺陷

> 行号以 **6.6 树** `src/kernel-6.6/` 为准（与 5.10 树的行号不同）。
> **自建 KMD 已经在跑**（`srcversion D507FC2774D547E7BEBA182`，见 §0），
> 所以下表全部是**真实存在于当前系统上**的未修缺陷，不是"待部署"的纸上问题。
> 下表之外，K1–K8/K10 与 N15 均已修好并在运行。

| # | 级别 | 位置 | 问题 |
|---|---|---|---|
| **N1** | **高** | `mwv207_sched.c:353-355` + `mwv207.h:61` | **比原记录更严重**：`sched[6]` 只有 **6 槽**，但拓扑需要 **7 个**（3d×2 + dec×2 + enc×1 + 2d×1 + dma×1）。`sched_3d=&sched[0]`、`sched_dec=&sched_3d[2]`、`sched_enc=&sched_dec[1]` → **`sched_dec[1]` 与 `sched_enc[0]` 同为 `&sched[3]`**，且 `sched_dma[0]` 落在**最后一槽**无余量。后果：enc0 的 scheduler 覆盖 dec1，而 `:390` 仍据 `sched_dec[1]` 非空报 `nr_dec = 2` → `INFO` 谎报；`engine_id=1` 的 DEC 提交会被 ENC 调度器执行。**当前 UMD `engine_id` 恒 0，故未触发**（高严重、零现网影响） |
| **N14** | 中 | `mwv207_pipe_3d.c`、`pipe_2d.c`、`pipe_dec.c` | 三条管线 `reset` 都只 `dma_fence_put` 在飞 fence **而不 signal**；2D 的 `destroy` 还不排空 `fence_queue` → **用户态可能永久等待**。**唯一可能让应用卡死的条目**；若将来出现"某次 hang 后卡死"，先怀疑它 |
| **K11** | 中 | `mwv207_drv.c:439` | `pci_clear_master` 位于移除流程最后；且与 `kms_fini` 必须先于 `irq_fini` 的次序耦合 |
| **N2** | 中 | `mwv207_sched.c` `hw_submission=30` | 与硬件事件数 30 **相等**，违背源码注释 "must be lower than 30" → 事件位图满时有越界/`BUG_ON` 风险 |
| **N4** | 中 | `mwv207_pipe_dma.c` | DMA 提交在 `run_job` 工作线程内**忙等最多 5 s** 并返回 **stub fence** → 调度器工作线程被长占、fence 语义退化 |
| **K9-①** | 中 | `dc/mwv207_va.c:634-655`（约） | vblank 处理器**整字回写** `0x9907f4`（影响 bit12–15），而 demux 清位本该是逐位 W1C |
| **K9-②** | 中 | `mwv207_irq.c:31` | ISR **无锁**读 `irq_enable_reg[]`，而 mask/unmask 在 `irq_lock` 内写 |
| **K12** | 中 | `mwv207_sched.c` 超时路径 | 循环体只有 `devfreq_record_idle`、循环变量未使用；`drm_sched_resubmit_jobs` 前无 job 完整性校验 |
| **N10** | 低 | `dc/mwv207_hdmi.c:908-918` `mode_valid` | 6.6 树的判据是 `clock > 594000` → `MODE_CLOCK_HIGH`，另对非 HDMI sink 加 `clock >= 165000` 限制；`phy_configure` 恒传 `bpp=8` 使 PHY 表 10/12 bpp 项不可达。**实测本机 HDMI-A-1 走 1920x1080@100Hz**，未受影响。⚠ 原记录的"384–594 MHz 全被拒"出自 **5.10 树**，6.6 树判据已不同（阈值 594000，非 384000） |
| **N9** | 低 | `dc/mwv207_kms.c:188`、`dc/mwv207_hdmi.c:1064` | 仍保留 `BUG_ON`（3D/2D 已改成可回退范式，显示侧没跟上） |
| 其它 | — | 全树 | `BUG_ON`（不含 `BUILD_BUG_ON`）仍有 **20 处** |

> **已移植到 6.6 树、待部署**：K1（flags 位运算）、K3（copy_to_user 返回）、
> K4（sort 比较符）、K5（bsearch 记录初始化）、K6/K7（copy_from_user 错误路径 +
> `engine_id` 的 u32 判据）、K8（`irq_create_mapping` 检查与回退）、
> K10（`MWV207_MAX_SCHED_PER_ENTITY 6` 边界 + `BUG_ON` 改 `WARN_ON`）、
> patch 0008（`MODULE_SOFTDEP("pre: governor_performance")`）。
> **刻意未移植 K2**：6.6 的 `ttm_bo_init_reserved()` 失败走 `err_put: ttm_bo_put(bo)`
> → `bo->destroy` = 本驱动 `mwv207_ttm_bo_destroy()`（内部已 release+free），再补一次是 double free。

---

## 3. 未实现功能（新功能，不是修缺陷）

| # | 目标 | 可行性 | 状态 / 缺口 |
|---|---|---|---|
| **D1** | **VP9 解码** | **中-高**（最可行） | 切片解析器 + bool 解码器**已编入库但零调用者**；唯一缺口是 `mwv207_video_dec.c` 里 `mwv207_dec_vp9_set_regs()` **无 VP9 分支**（`grep vp9 mwv207_video_dec.c` 仅命中注释/头文件）。有 H264（10 个 `set_regs`）/HEVC（7 个）作模板，1840 行解码寄存器表已全量还原。**本机芯片档位已确认**：`mwv207_dec_h264.c:2321` 与 `mwv207_dec_hevc.c:1231` 都硬编码调用 `mwv207_video_dec_get_hw_features_by_id(0x00001F76, ...)`，而 `mwv207_video_dec_vpu_features_list.h:29-36` 的 `0x00001F76` 档 `vp9_support=1`、`vp9_profile2_support=1` → **硬件确实支持 VP9（含 Profile 2）**。需真实 VP9 码流验证 |
| **D2** | **H264 4:2:2 / 4:4:4** | 二选一 | 见 1.3 的 I-3：要么取消声明，要么改 `chroma_format_idc` 硬置并做硬件验证 |
| **D3** | **编码（H264/HEVC）** | 大（从零） | UMD 侧 `base.encode_bitstream` **从未安装**（全树 0 处）；KMD 有 `mwv207_pipe_enc.c`。硬件侧有 HEVC 编码证据（闭源串含 `jmenc/source/hevc/hevcencapi.c`） |
| **D4** | 4 个未实现的扩展 opcode：`WAIT_GROUP_EVENT`(0x11) / `TD_OP`(0x12) / `TEX_INVALIDATE`(0x14) / `PRE_NORM`(0x15) | 小-中 | 厂商枚举齐全（`MWV207REG_SH_EXTENDED_OPCODE`），需逐个确定语义 |
| **D5** | 开启三个窥孔 `OPT_DIV` / `OPT_FFMA` / `OPT_SET`（默认全关） | 小改动 + 大验证 | 潜在性能收益，需全量着色器回归 |
| **D6** | 多视口 | 与 E8 同 | 取决于硬件是否支持（`GL_MAX_VIEWPORTS` 现报 0/1） |
| **D7** | **Vulkan** | 不可达 | 官方 1.8.0 **也没有**（无 libvulkan / `vulkan/icd.d`；KMD 的 syncobj/vm_bind/gpuva/sparse 全 0）。官网"Vulkan 1.1"至今未交付 |

---

## 4. 信息 / 硬件边界（不该承诺修好）

| # | 条目 | 判定 | 可做的最优动作 |
|---|---|---|---|
| **C-1** | 16 位采样恒 0（同 1.3 的 C1） | 信息边界 | 用厂商完整字段表（1392 寄存器 / 3954 字段）反查所有与纹理通道宽度相关、而驱动恒写 0 的字段逐项试；无解则降级声明 |
| **C-2** | 高级混合 8/15 | **硬件边界**，不可修 | 保留超额声明并注明（现状），或撤销 cap（丢 GLES 3.2） |
| **C-4** | ISA 常量头 `INST_OPCODE_*` / `INST_TYPE_*` 在本树无定义（`include/hw/*.h` 是空壳） | 部分可救 | **扩展**操作码已有厂商枚举（`SH_EXTENDED/CMPLX/IMG_ATOM/VISION_OPCODE`）→ 可据此生成 `INST_EXT_OPCODE_*`；**基础 ALU 操作码仍未找到厂商枚举** |
| **C-5** | 视频解码块 `word → 厂商名` 无法反查 | 数据源缺口 | 厂商 1392 条寄存器里**没有视频解码块**（`MWV207REG_DEC*` 只有 2D/3D 的 `DEC400`）。只能沿用驱动自己的三张表（1840 行，已全量还原） |
| **C-6** | 开源驱动 219 个命令寄存器号里 **49 个** 在厂商库中查不到（含 `0xE00–0xEFF` 段） | 部分可解 | 32 个已由闭源 HAL **函数名**给出语义推断（`0x191`=ProgramIndex、`0x401–0x404`=着色器计数、`0x594/0x595/0x596`=tile status 三件套、`0xE05`=绘制使能…）；**17 个闭源也不用**（`0x192/0x281/0x283/0x284/0x28B/0x301–0x306/0x502/0x507/0x509/0x50A/0x6D0/0xE13`）→ 标未确认 |
| **C-7** | `I-2`：`JMDEC_BITPLANE1_E` 与 `JMDEC_BLACKWHITE_E` **同占 word7 bit30** | 可查证 | 用已还原的 1840 行表核对 bit30 归属；若属笔误则改动极小，若真复用只能择一 |

### 统计口径提醒（避免重蹈覆辙）

反编译提取寄存器时，宽松口径曾得出"413 个寄存器被 HAL 引用"，但零假设检验显示：
候选空间 55297、随机命中率 2.45%，实测命中 1165 次 vs 随机期望 367.5 → **信噪比仅 3.2×**
（约 1/3 可能是巧合）。发布数已收敛为最严口径 **379**（只取 `Load/Flush/Set/Write`
类函数内且 `v >= 0x18` 的立即数）。见 `reverse/03-REGISTERS.md` §3.1。
**教训：总量显著 ≠ 单条可信**，单条必须靠函数名 + 开源驱动自身用法双重印证。

---

## 5. 建议的推进顺序

| 批次 | 内容 | 前置 | 代价 |
|---|---|---|---|
| ~~**1**~~ | ~~1.1 全部（W6/W7/W12/E4）+ E9/E10 拼写 + F.3.2/F.3.3~~ | — | **已完成（2026-11-02）** |
| ~~**2**~~ | ~~E2 / E7 / O4 + GBM-Wayland `err=-13`~~ | — | **已完成（2026-11-02）** |
| **2b** | E1（已降级为 1 LSB，暂不改）/ E6 / E8 / E3 / E5 / O5 | 每条配一个针对性探针 | 低-中，需行为验证 |
| **3** | **拍板** 1.3 的 C1/C2/C3/I-3 | 用户决策 | 不是技术难度，是诚实性取舍 |
| **4** | **真机 `kmd install`**（解锁全部 KMD 条目） | 用户同意黑屏风险；备 SSH/TTY | 一次性风险，之后所有 KMD 修复可生效 |
| **5** | VP9 解码（D1） | 真实 VP9 码流 | 中-大，但模板齐全、可行性最高 |
| **6** | 编码（D3） | 需真实编码管线（UMD 侧从零） | 大 |

> 批次 1 之外都建议在**能上机验证**的前提下做。
>
> **UMD 验证方法（2026-11-02 分层后已变简单）**：驱动源码已迁到独立目录
> `src/mwv207-umd/`（`ref/oh-mesa/.../drivers/mwv207` 与 `winsys/mwv207` 现在是
> 指向它的符号链接），改代码 → `./tools/build-umd.sh build` → 用
> `LIBGL_DRIVERS_PATH=ref/oh-mesa/User_source_code/mesa-24.1.2/build-full/src/gallium/targets/dri` +
> `MESA_LOADER_DRIVER_OVERRIDE=mwv207` 直连跑探针即可。版本戳取自 mesa 参考树的
> `git HEAD`（现为 `b2ab2c9da2`，与已安装 `libGL.so.1` 一致），所以**不再需要
> worktree**；只有在 mesa 树里新提交过东西后，才会重新出现
> `DRI driver not from this Mesa build` 握手问题。

---

## 6. 已修复索引（不在本清单范围内）

| 日期 | 项 | 提交 |
|---|---|---|
| 2026-11-02 | 条件渲染真实现（`render_condition` 钩子 + CPU 侧谓词判断，GL 4.0 恢复） | `patches/0015`（`src/mwv207-umd`） |
| 2026-11-02 | KMD：零重定位提交未初始化 `drm_sched_job` 导致 `BUG_ON(!entity)` 内核崩溃（N15） | `28bc73d` / `patches/0014` |
| 2026-11-02 | 分层：`src/mwv207-umd/` 独立存放适配代码，oh-mesa 只作参考 | 结构变更 |
| 2026-10-31 | 遮挡查询失真（按簇计数求和）、条件渲染 SIGSEGV（cap 回退） | `b2ab2c9` |
| 2026-10-30 | VRAM cap 单位（`1024<<20` → `1024`）、O2（bo_map 返回语义）、O3（每槽尾 100 字节未初始化上 GPU） | `5499360` |
| 2026-10-30 | GLES 3.2 + 鲁棒缓冲访问 + 16 位格式上传字节精确 | `cd90d9f` |
| 2026-10-31 | 工具：合并 UMD/KMD 切换脚本、新增 `all install` | `f4b1221`、`af15fda` |
| 2026-10-31 | 文档：`docs/reverse/` 反编译五篇、`06-KNOWN-ISSUES` C4/C5 | `e63fcf3`、`fd31971` |

补丁：`patches/0009`（GLES3.2+混合）、`0010`（16 位格式）、`0011`（VRAM+O2/O3）、
`0012`（KMD 6.6 移植）、`0013`（遮挡查询+条件渲染）、`0014`（KMD 空 entity）、
`0015`（条件渲染真实现）。

---

## 7. 相关文档

- 缺陷细节与推导：[api/06-KNOWN-ISSUES.md](api/06-KNOWN-ISSUES.md)
- 逐条可行性评估：[api/11-FIX-FEASIBILITY.md](api/11-FIX-FEASIBILITY.md)
- 反编译与寄存器：[reverse/README.md](reverse/README.md)、
  [reverse/05-OCCLUSION-QUERY.md](reverse/05-OCCLUSION-QUERY.md)
- KMD 6.6 移植：[../reports/KMD-6.6-PORT-REPORT.md](../reports/KMD-6.6-PORT-REPORT.md)
- 验证工具：`tests/query/run-query-tests.sh`、`tests/run-all.sh`

## 火狐浏览器自身 UI 的文字错位（r8 起可见）

- 现状：页面内容（WebRender）在 r8 下已完全正确；浏览器界面（标签栏 / 地址栏）
  的文字呈"字符被打乱"状（`file:///tmp/diagpage.html` → `fi fhi;ap: t.lp|/tg/e d l`）。
  r7 时整个 chrome 区域全黑（完全不渲染），r8 修复后才暴露出来。
- 已排除：双源混合（`/tmp/dsrc`）、亚像素抗锯齿逐通道覆盖率（`/tmp/dsrc2`），
  两者与 llvmpipe 逐字节一致。
- 疑点方向：chrome 文字用 R8 字形图集，其 UV / 取样位置可能有问题；与
  128 位纹理路径无关（那条路径已修好并有回归）。
- 参考：`reports/FF-RENDER-20261102.md` 第 11.7 节。

## 玲珑（linyaps）应用打不开：CDI 挂载源被我们删掉（已修）

- 现象：微信 / 日历 / 看图等玲珑应用全部打不开，`ll-cli run <id>` 报
  `[FATAL] container.cpp:1630 clone_fn: child process error: No such file or directory`
  + `container process exited before reaching expected stage exec_ready`。
- 根因：`switch-mwv207.sh` 为了停用厂商闭源 EGL ICD，把
  `/usr/share/glvnd/egl_vendor.d/10_mwv207.json` **`mv` 成 `.oss-disabled`**；而
  厂商 1.8.0 的 CDI 规格 `/etc/cdi/jmgpu.yaml` 把它当作 `rbind` 的 `hostPath`
  （38 个挂载源里唯一缺失的那个）。源文件不存在 → `ll-box` 建容器失败 →
  所有玲珑应用启动不了。strace 的最后一根稻草就是
  `openat(AT_FDCWD, "/usr/share/glvnd/egl_vendor.d/10_mwv207.json", O_PATH) = -1 ENOENT`，
  紧接着 `sendto(4, "\0\0.\0\0\0child process error: No su"...)` 后 `exit_group(1)`。
  **与显卡无关，是我们的安装脚本与厂商 CDI 规格冲突。**
- 修法（`switch-mwv207.sh`）：不再改名/删除，改为**原地改写**该 json
  的 `library_path` 指向我们的 `libEGL_jm.so.0`（原件备份 `.vendor-orig`）；
  同时清掉旧版遗留的 `20_jm.json`（两条 json 指向同一 ICD 会被 glvnd 重复枚举）。
  `umd verify` 新增 `[OK] CDI 挂载源存在` 断言，防止回归。
- 验证：三个玲珑应用均可启动；`eglinfo -B` 只枚举 1 个 platform device；
  火狐 WebRender 页面渲染不受影响（直方图与修复后一致）。

## 火狐 WebGL 页仍未到 vsync 上限（性能，剩余项）

- 现状（r9，零 env）：CSS 动画页 **96.6 FPS / 10.35ms**（已到 100Hz 面板上限），
  WebGL 动画页 **42.0–44.5 FPS / 23.5ms**（llvmpipe 同条件 28.5 FPS / 35.05ms，
  即我方 1.5–2.0×）。
- 已修的两个大头（见 `reports/FF-RENDER-20261102.md` §13、`patches/0018`）：
  ① 每帧重编译 VS+FS（约 100 次/秒、每次 ~1.4ms）——根因是
  `mwv207_set_current_shader()` 拿 `ctx->dirty` 当"状态变过"的判据，而
  `mwv207_submit()` 末尾把 `dirty` 重置成 `MWV207_DIRTY_ALL`；改为把四个
  状态指纹并入 `state->shader_list` 的缓存查找条件。
  ② 128 位纹理 split/merge 对 WC 显存逐标量读写（~100MB/s）——改为逐行
  `memcpy` 中转，平均 2192µs → 258µs。
- 剩余瓶颈的判据：WebGL 页 5 秒内**总 CPU 只有 120–180ms（≈2.4–3.6% 单核）**，
  说明剩下的时间不是驱动在算，而是 WebRender 自身的帧节奏/合成与呈现等待。
  下一步应查 WebRender 的 `RenderBackend`/`Compositor` 帧调度，而不是继续优化
  GL 侧。
- 参考：`reports/FF-RENDER-20261102.md` §13。

## ~~GBM / Wayland 平台不可用：`err=-13`（EACCES）~~ —— **已修（2026-11-02）**

- 现象：`eglinfo -B` 的 **GBM platform** 与 **Wayland platform** 全部报
  `mwv207: failed to create kernel context, fd=5 err=-13`，随后回落到
  `kms_swrast`/`swrast`（我们没装，于是这两个平台完全不可用）。
  X11 / Surfaceless / Device 平台正常。
- 出错点：`src/mwv207-umd/winsys/mwv207/drm/mwv207_drm_winsys.c:139-144`
  的 `mwv207_ws_init()` 里 `drmCommandWriteRead(ws->fd, 0x4, ...)`
  （= `DRM_IOCTL_MWV207_CTX`，op=0 建内核上下文）返回 `-EACCES`。
- **根因（2026-11-02 实测，`strace -f -y`）**：`/tmp/gbm2.strace` 里
  `ioctl(5</dev/dri/card0>, ...0x44..., ...) = -1 EACCES (权限不够)` ——
  即走 GBM 平台时 Mesa 把**主节点 `/dev/dri/card0`** 的 fd 交给赢思层，
  而我们的 6 个私有 ioctl 在 `src/kernel-6.6/mwv207_drv.c:102-108` 都带
  `DRM_AUTH|DRM_RENDER_ALLOW`；Linux 的 `drm_ioctl_permit()` 对
  `DRM_AUTH` 的放行条件是「已认证 **或** render 客户端」，主节点上既非
  master 也未 `drmAuthMagic` 的进程直接吃 `-EACCES`。
  自写探针 `/tmp/ctxtest.c` 逐节点验证：
  `/dev/dri/renderD128: ioctl ret=0 handle=0` / `/dev/dri/card0: ioctl ret=-1 errno=13`。
  → **同一份 UMD，拿 render 节点能过，拿主节点必失败。**
- **修法（已实施，UMD 侧）**：`mwv207_ws_init()` 里 `drmCommandWriteRead(ws->fd, 0x4, ...)`
  返回 `-EACCES` 时，用 `drmGetRenderDeviceNameFromFd(ws->fd)` 取同卡 render 节点，
  `open(O_RDWR|O_CLOEXEC)` + `fcntl(F_DUPFD_CLOEXEC, 3)` 另开我们**自己 dup 出来的**
  `ws->fd` 后重试（不动调用者的 fd）；成功时打一行
  `mwv207: 私有 ioctl 被拒(EACCES)，已改用 render 节点 /dev/dri/renderD128`。
  X11 路径 fd 本来就是 render 节点，不会进这个分支，行为不变。
- **验证（`LIBGL_DRIVERS_PATH=/tmp/gd3 MESA_LOADER_DRIVER_OVERRIDE=mwv207 eglinfo`）**：
  GBM platform 从"`err=-13` 后回落 swrast"变成
  `EGL driver name: mwv207` + `OpenGL core profile renderer: MWV207 (JingjiaMicro Corporation)`
  + `4.0 (Core Profile) Mesa 24.1.2 (git-9466c83cfc)`，完整 EGL 扩展串正常列出，
  `EGL_MESA_platform_gbm` / `EGL_KHR_platform_gbm` / `EGL_KHR_platform_wayland` 都在。
- 备选（未采用）：KMD 侧把 `mwv207_ioctls_drm[]` 的 `DRM_AUTH` 去掉只留
  `DRM_RENDER_ALLOW`。能一次解决，但放宽了权限（未认证客户端也能分配显存/提交），
  且要重新部署模块才生效 —— 既然 UMD 侧已验证可行，就不动 KMD ABI。
