# 未修复缺陷与未实现功能 · 总清单

> 汇总截至 **2026-11-11 12:10**（运行中的 UMD 是 **r23**（`payload-42/VERSION =
> 24.1.2-fullstack-r23`，2026-11-11 12:07 安装，含 I-5；DRI `8f52e997…`、
> VA `e2ddc5bb…`）；r23 之前的 `payload-42` 依次是 r22（含 N18）、r21；
> 内核 `6.6.155-arm64-desktop-hwe` 侧运行中的是 `8DFC520FB8FCDECFBC911AA`（20 处 `BUG_ON`
> 收敛 + N16 + N4 异步 DMA/K11 + N17 引用计数，2026-11-10 17:55 重启后
> `tests/verify-installed.sh 8DFC520FB8FCDECFBC911AA --bad-submit` 全过
> `PASS=25 FAIL=0 SKIP=1`；N17 的引用计数对账 2052/2052；r23 安装后同一脚本再次
> `PASS=25 FAIL=0 SKIP=1`），
> 对应外层仓库工作树（含 `src/mwv207-umd/` 分层）。
> 取代散落在 `api/06-KNOWN-ISSUES.md`、`api/11-FIX-FEASIBILITY.md` 里的分散记录：
> 本文件是**唯一索引**，那两个文件保留历史细节与推导过程。
>
> **口径**：只列**当前仍未落地**的项。2026-11-06~11-10 新增已修项（VP9 Profile0 全链路、
> Firefox 字形/图标撕裂、Firefox 标签栏"微缩 chrome"）见 §6《已修复索引》。
> 每条给出 `文件:行号`；标【实测】的表示上机验证过，未标者为静态审计结论。

---

## 0. 一图看清卡在哪

| 层 | 改了能立刻验证吗 | 阻塞原因 |
|---|---|---|
| **UMD**（`src/mwv207-umd/`） | ✅ 能 | 源码完整在仓库；`LIBGL_DRIVERS_PATH` 可旁路加载，`umd install` 一键部署 |
| **KMD**（`src/kernel-6.6/`） | ✅ 能（**已部署**） | 自建模块已装到 `/lib/modules/…/updates/dkms/mwv207.ko` 并在运行 |

两层的卡点性质不同：**UMD 卡在"要不要改 / 改了是否诚实"**，
**KMD 已不再是卡点**（详见下）。

### KMD 部署状态（2026-11-10 重启后复核）

此前本节写作"从未真机加载"，**已过时**。实测：

```
cat /sys/module/mwv207/srcversion        -> 0EFB7F4433EA5311B08E88C（运行中）
modinfo -n mwv207                        -> /lib/modules/6.6.155-…/updates/dkms/mwv207.ko
modinfo src/kernel-6.6/mwv207.ko         -> srcversion 0EFB7F4433EA5311B08E88C
modinfo /lib/modules/…/updates/dkms/…    -> srcversion 0EFB7F4433EA5311B08E88C
```

三方一致 ⇒ **运行中的就是自建版**，且包含：
- `b0479ba` 移植的 K1 / K3 / K4 / K5 / K6 / K7 / K8 / K10；softdep 就是
  `src/kernel-6.6/mwv207_drv.c:543` 的 `MODULE_SOFTDEP("pre: governor_performance")`；
- `28bc73d` 修的 N15（零重定位提交未初始化 `drm_sched_job` → `BUG_ON(!entity)`）；
- `22145e9` 修的 N14（reset/destroy 时 signal 在飞 fence，见 §2.1）；
- `6502f19` 修的 N1 / N2 / K12 / K9-② / N9（见 §2.2，运行中 srcversion 已是
  `D5EEF36617A4D3FBF4AFD1D`，2026-11-10 13:29 重启后回归全过）；
- `fbed27f` 修的 N4 / K11（见 §2.3）：2026-11-10 14:36 重启后**已验证生效**
  （一键验收 22 项全过），运行中 srcversion 即 `0EFB7F4433EA5311B08E88C`；
- `53b3229` 收敛的 20 处 `BUG_ON`（见 §2.4）与 `c01af40` 修的 N16（`job_fini`
  对未分配的 `cmd_dats` 空指针解引用，见 §2.5）：2026-11-10 16:21 重启后**已验证生效**
  （运行中 `5A5B4018…`；畸形提交 9/9 干净返回 errno、dmesg 零 oops）；
- `1fc9b21` 修的 N17（N4 的异步 DMA 在 `queue_work()` 之后才 `dma_fence_get()`，
  worker 抢跑时对已释放 fence 做 `refcount_inc`，见 §2.6）：2026-11-10 17:55 重启后
  **已验证生效**——运行中 `8DFC520FB8FCDECFBC911AA`，一键验收 PASS=25，kprobe 压测
  submit/complete 2052·2052 平衡，`refcount_t` 告警 0（旧模块 209 s 即触发 3 条）。

因此 **K 系列、N14、§2.2 的 5 条、§2.3 的 N4/K11、§2.4/§2.5 两批、§2.6 的 N17
均已生效**。剩下的只有 N10（决定只文档化）。
自建模块会打印 `loading out-of-tree module taints kernel` 与
`module verification failed: signature and/or required key missing`（DKMS 签了名，
但 MOK 证书没有 enroll 进内核信任链），属既有现象；dmesg 里 GPU 相关错误计数为 0。

---

### 0.1 未解决项速览（2026-11-10 复核）

按"是否影响出厂可用性"排序；详细条目见对应章节。

| # | 领域 | 未解决项 | 影响 | 位置 |
|---|---|---|---|---|
| 1 | VAAPI/VP9 | **A5：Profile 2（10bit 4:2:0）已支持；4:2:2/4:4:4 定案不可实现** | 2026-11-10 `82e7b02`：Profile 2 与软件解码**逐位一致**（64x64/640x360/720p60/1080p30，含 inter 帧，零 TDR），默认声明，`MWV207_VP9_10BIT_DISABLE=1` 可关。Profile 1/3 与 H.264 422/444 同理不可实现：解码器没有色度采样格式选择字段（§1.5）。**残余**：12bit VP9 未验证（代码只放行 10bit） | 本文 §1.6 |
| 2 | VAAPI/VP9 | **B3 完成**：官方 intra-only 向量上 7 个真实解码帧全部与 libvpx 逐字节一致（`8f6e964` 修掉 I-5） | 样片与诊断脚本在 `tests/vp9-intraonly/`；只剩 `show_existing` 重放帧仍是黑图（独立限制，不影响 B3 判据） | 本文 §1.7 |
| 3 | VAAPI/VP9 | **I-6 已修（`bb5cd9e`）；B2 已完成并实测通过（`21a15db`）** | `vp90-2-14-resize-10frames-fp-tiles-1-2.webm` 硬件与 libvpx **IDENTICAL**；`vp90-2-21-resize_inter_320x180_5_1-2.webm` 此前按设计跳过（跨尺寸参考不支持），I-6 生效后 `vaSyncSurface()` 返回 `VA_STATUS_ERROR_TIMEDOUT`、ffmpeg 报 `Failed to sync surface` 停输出。2026-11-11 `351e6ed` 删掉"参考尺寸必须等于当前帧"的跳过分支并把 staging 缓冲改成可增长；厂商驱动同一向量逐位正确。**尚未实测**（本机处于 vendor 栈），需切回自研栈后跑 `tests/vp9-resize/run-vp9-resize.sh` 确认 `跳过帧=0` 且 IDENTICAL | 本文 §1.8 |
| 4 | KMD | **N14 已修，重启后已验证生效** | 修复见 §2.1（`22145e9`）；运行中 srcversion 已是 `039F972EFE3279A155EE225`，GL/query/VP9 回归全过；reset 通路已用 `tests/n14-reset/` 探针实测（6 轮 TDR、force_complete 6/6） | 本文 §2.1 |
| 4b | KMD | **N1/N2/K12/K9-②/N9 已修，重启后已验证生效** | 修复见 §2.2（`6502f19`）；2026-11-10 13:29 重启后 srcversion = `D5EEF36617A4D3FBF4AFD1D`，GL/query 4·4/VP9 18·18/H.264 8bit 逐位一致/N14 探针 6·6 全过，dmesg 无 WARN/BUG；引擎计数用 `tests/engines/` 核对（lite 机型 3d=1 dec=1 enc=0 2d=1 dma=1） | 本文 §2.2 |
| 4c | UMD 视频 | **I-4：H.264 High10 已定案（2026-11-10 收回声明）** —— 按当前寄存器配置提交 10bit H.264 会让 DEC 管线挂死（6 次 TDR、输出全 0）；缺 Hantro 的 high10p 位流预转换 | 客户端现在对该类流直接回退（`hwaccel initialisation returned error`），DEC 零超时；原厂闭源驱动同样不声明 High10 | 本文 §1.2 / `api/05-VIDEO-CODEC.md` §6.7 |
| 4d | KMD | **N4/K11 已修并重启验证生效** | 修复见 §2.3（`fbed27f`）：DMA 提交改异步完成 + 真 fence；移除路径补 `pci_disable_device` 与 `free_irq` 前硬件侧关中断。运行中 srcversion `0EFB7F4433EA5311B08E88C`，2026-11-10 14:36 重启后一键验收 `PASS=22 FAIL=0 SKIP=1` | 本文 §2.3 |
| 4e | KMD | **20 处 `BUG_ON` 已收敛为可恢复错误，重启后已验证生效** | `53b3229`：用户提交路径仍可能触发的那几处（`mem_type` 非 VRAM/TT、`reloc->idx` 越界、DMA `cmd_size` 不符、3D 事件槽耗尽）现在返回 `-EINVAL`/`-EIO`/`-EBUSY`；寄存器/ring 描述符类改 `WARN_ON_ONCE` + 跳过，由 TDR 回收 | 本文 §2.4 |
| 4f | KMD | **N16 已修，重启后已验证生效**（新发现）：畸形 submit（`nr_cmd_dat>0` 而 `cmd_dats==NULL`）会让 `mwv207_job_fini()` 空指针解引用 → 内核 oops，任何本地进程可触发 | `c01af40`：清理条件加 `&& mjob->cmd_dats`；配套 `tests/bad-submit/` 9 个用例（默认不跑，老模块会 oops）。两内核已装 `5A5B4018…` | 本文 §2.5 |
| 4g | KMD | **N17 已修，重启后已验证生效**（新发现）：N4 的异步 DMA 在 `queue_work()` 之后才 `dma_fence_get()`，worker 抢跑时对已释放 fence 做 refcount_inc（`refcount_t: addition on 0; use-after-free`，PC 在 `mwv207_pipe_dma_submit`），等待者可能永远收不到 signal —— 与 16:30 整屏卡死（Xorg 收 SIGTERM 不退、只能 SIGKILL）症状一致 | `1fc9b21`：给调度器的引用在 `queue_work` 之前取；运行中已是 `8DFC520FB8FCDECFBC911AA`，17:55 重启后 `verify-installed.sh --bad-submit` PASS=25，kprobe 压测 submit/complete 2052·2052 平衡、refcount 告警 0、X 最大延迟 5 ms | 本文 §2.6 |
| 4h | UMD 视频 | **I-5 已修并随 r23 部署验证**（`8f6e964`） | 三处根因：`vp9_setup_past_independence()` 漏 intra-only、自适应基准该用默认表、`show_existing` 污染 `saved_ctx`；安装态（r23）用 ffmpeg 完整路径解官方 intra-only 向量，**7 帧与 libvpx 全部逐字节一致**。N18（`6225e97`）为同批复用缺陷 | 本文 §1.7 |
| 4i | UMD 视频 | **I-6 已修（`bb5cd9e`）**：跳过帧（参考缺失/尺寸不符/参数不支持/分配失败）以前只清 `JMDEC_DEC_E` 就返回，PPU 输出面没编程、`vaSyncSurface()` 照样成功，应用拿到旧内容 | 现在 `end_frame()` 置 `frame_failed` + 桩 fence，`get_decoder_fence()` 返回 0 → `vaSyncSurface()` 报错；回归（VP9 P2 10bit / show_existing / res14 / HEVC8 / H264 8）全部逐位一致 | 本文 §1.8 |
| 5 | KMD | 只剩 N10（决定只文档化） | K9-① 已核实为与厂商一致（非缺陷）；N10 改为只记录 | 本文 §2.3 |
| 6 | 切换脚本 | **vendor 态端到端未实跑** | `stack vendor` 会重启，需人工确认；热切换已证不可行（refcnt 恒 2） | `switch-mwv207.sh` / §5 |
| 7 | 打包 | **DKMS 一致性警告（走 stage 时会复现，已记录处理步骤）** | `tools/dkms-sync.sh` 重建后两个内核都干净；`kmd stage` 直接覆盖仓库构建的 .ko 会让 6.6.155 重新报 `Diff between built and installed module!`，用 `dkms install --force` + `depmod` + `update-initramfs` 恢复即可 | §2.1、§5 |
| 8 | 性能 | buffer 写入等待的吞吐代价未量化 | 仅 glxgears（110 FPS）观测；重负载未测 | 本文 §4 性能 |
| 9 | 依赖补丁 | `loader_dri3` buffer age 恒 0 只在本地 mesa 树提交 | 未推厂商镜像；重建 mesa 时需保留 | `ref/oh-mesa` `248659c` |
| 10 | 测试 | 无自动化 GL 回归 | Firefox/GL 侧探针均为一次性脚本；VP9 有 `tools/vp9-regression.sh` | §7 |

## 1. UMD — 未修复缺陷

### 1.0 已修（2026-11-03，随 r12/r13 部署）

#### A 类：双面模板 / 反汇编 type / 编译器健壮性 / 失败 fence（提交 `5dee412`）

- **E5 双面模板引用双重互换**：`mwv207_set_stencil_ref()` 在 set 时已把
  `stencil_back_reference[0]` 填成背面语义（`mwv207_state.c:320-334`），
  `mwv207_config_other()` 又按 `front_ccw` 挑了一次 → `front_ccw=1` 时 0x528
  发射的是正面值。改为恒取 `[0]`（`mwv207_draw.c:1043-1050`）；0x507 的正面数组
  未预互换，保持按 `front_ccw` 取。
- **F.3.1 反汇编 type 位序**：原公式 `inst_type = bit0 | (bit1_2 << 1)` 与汇编器
  `mwv207_assemble()`（`nir/mwv207_common.c:264-326`）相反 → `type != 0` 时打印的
  类型名全错（`.f16` 打成 `.s32`）。改为 `(bit0 << 2) | bit1_2`
  （`nir/mwv207_disasm.c:639-650`）。
- **F.4 三条健壮性**：`link_shader` 恒 `return false` → 成功路径 `return true`
  （`nir/mwv207_compiler.c:695-700`；调用者本不检查返回值，语义化而已）；
  `shared_atomic` 形状匹配失败 `assert(0)`（release 下被编译掉后返回 NULL 当基址用
  → 空指针）→ 打印告警并返回 NULL（`nir/mwv207_lower_shared_atomic.c:108-115`）；
  RA 溢出循环 `while (!mwv207_ra())` 无上限且循环体只 free 不重建（use-after-free /
  死循环）→ 上限 32 轮后报错退出（`nir/mwv207_ra.c:819-834`）。
- **O5 提交失败仍造"永远就绪"fence（视频路径）**：`video_cs.c` 的 `out_fence_fd`
  初始化为 `-1`、失败时不再 `fence_create(-1)`（`mwv207_video_cs.c:119-136`）；
  `drm_cs.c` 失败时保持 `-1`（`mwv207_drm_cs.c:460-470`）。**残留**：`mwv207_flush()`
  仍无条件 `fence_create(out_fence_fd)`，见 §1.1 O5。
- 回归：随 r12 部署，验收结果见 `docs/HANDOVER.md` §1（GL 4.0 / GLES 3.2 /
  `Video memory: 2048MB` / glxgears 100.3 FPS / 火狐内容与性能正常 / 玲珑正常）；
  回归判据清单见同文档 §4.7。

#### dual-source blend 编译期空视图解引用（火狐 GitHub 崩溃）

- 现象：Firefox 打开 GitHub 后概率性 SIGSEGV（crash_address `0x48`，SEGV_MAPERR）。
- 帧链：`get_rt_texture_coord` (`nir/mwv207_lower_dual.c:115`) ← `mwv207_lower` (`mwv207_lower.c:1690`) ← `mwv207_compile_shader` ← `mwv207_set_current_shader` ← `mwv207_update_shader` ← `_mesa_DrawElementsInstancedBaseVertexBaseInstance`。
- 根因：① `mwv207_config_dual_blend()`（`mwv207_draw.c:1214`）把 RT blit 视图绑在 `num_sampler_views[FS]` 槽，但 `dsbi.sampler_index` 记的是 `num_samplers[FS]`（两计数不等，视图槽回缩后错位）；② `mwv207_lower_dual.c:115` 无条件解引用 `sampler_views[4][sampler_index]->texture`（`0x48` = `pipe_sampler_view::texture` 偏移）。
- 修复：`cbufs[0]`/`create_sampler_view` 为 NULL 时关闭 is_dual；`sampler_index` 与 `bind_sampler_states` 槽统一为 `num_sampler_views[FS]`；`get_rt_texture_coord()` 判空并回退到 `fb_state` 渲染目标尺寸。
- 回归：wrreplay VT/brush/triv 全 OK；query tests PASS=4；glxgears/GLES 3.2/2048MB 不变；Firefox GitHub 45s 静止加载崩溃 0（原崩溃概率性触发，需日常使用观察）。
- 提交：`40d5d76`（`src/mwv207-umd`，2 文件 / +42 −4）。

### 1.1 错误路径 / 健壮性（低风险，可直接改）

| # | 位置 | 问题 | 当前证据 | 建议 |
|---|---|---|---|---|
| ~~W6~~ | `winsys/mwv207/drm/mwv207_drm_bo.c` | ~~`CALLOC` 后无 NULL 检查~~ | **已修**（2026-11-02）：加 `if (!bo) goto unlock;` | — |
| ~~W7~~ | `winsys/mwv207/drm/mwv207_drm_bo.c` | ~~`GEM_MMAP` 失败仅 `printf` 不返回~~ | **已修**（2026-11-02）：失败改 `fprintf` + `return NULL` | — |
| ~~W12~~ | `winsys/mwv207/drm/mwv207_drm_cs.c` | ~~依赖 `assert`，release 下被移除~~ | **已修**（2026-11-02）：4 处分配全部判空、失败释放并返回 NULL | — |
| **O5** | `winsys/mwv207/drm/mwv207_drm_cs.c:100-118` | `fence->fd < 0` 时 `fence_wait()` 恒返回 true | **已修（2026-11-03，批次 2b）** 视频路径此前已修（`5dee412`）；`mwv207_flush()`（`mwv207_context.c:93-105`）在 `out_fence_fd < 0` 时返回 NULL fence，Mesa 的 `st_finish` / `st_manager` 对 NULL 判空 → 提交失败不再伪装成"已完成"。故障注入探针 `tests/batch2b/failsubmit.c`（拦截 `DRM_IOCTL_MWV207_SUBMIT` 返回 EIO）：glxgears 连续 92k 次失败下无崩溃/挂死，NULL 分支命中；正常路径与 query 回归均通过（提交 `4a19713`） | — |
| ~~E4~~ | `mwv207_draw.c` | ~~无深度模板时解引用 FS `shader_states` 未判空~~ | **已修**（2026-11-02）：`fs_state ? fs_state->current : NULL`，GEOMETRY 同样加 `&& ->current` | — |

### 1.2 会直接影响可观测行为（需行为验证）

| # | 位置 | 问题 | 当前证据 | 建议 |
|---|---|---|---|---|
| ~~I-4~~ | H.264 10bit 解码路径（`video-codec/mwv207/mwv207_dec_h264.c`；`dec_param.bit_depth` 在 `:2330` 置位，`:597`/`:1337`/`:1892` 先用；`JMDEC_BIT_DEPTH_OUT_MINUS8` 只有 VP9 在写） | **已定案（2026-11-10，`4ecca0d`）：收回 High10 声明**。实测 10 帧 352×288 High10 样片：驱动侧 `set_regs` 正常执行 10 次（`bit_depth=10`、`high10p_mode=1`、`chroma_format_idc=1`），但内核连报 6 次 `mwv207: job from 'av:h264:df0' to 'mwv207_dec0', timed out!`（`irq state reg: 0x00010102`），靠 TDR 回收，输出面 1520640 个 u16 采样全 0；8bit High 420 同路径**零超时**、逐位一致 | 结论：**不是硬件能力缺口而是实现缺口** —— 原厂特性表对 `0x1F76` 写 `h264_high10_support=1`，但 Hantro 系的 10bit H.264 需要 high10p 位流预转换，本驱动未实现；原厂闭源驱动的 `QueryConfigProfiles` 也不声明 High10。已收回声明并在 `end_frame` 对 `bit_depth_luma_minus8 > 0` 直接跳过（避免把 DEC 拖进超时循环） | 若将来要做：实现 high10p 位流预转换（10bit 语法元素重打包）后重新申请该 profile；判据＝与软件解码逐位一致且 `timed out!` 计数 0 |
| **E1** | `mwv207_draw.c:751-754` | `color_8` 四个分量都 `<< 8` | **【实测更正】不是高位覆盖**：`/tmp/blendcolor` 用 `glBlendFunc(GL_CONSTANT_COLOR, GL_ZERO)` + `glBlendColor(0.5,0.25,0.125,1.0)` 实测，llvmpipe `(26,26,19,204)`（= 期望值），我们 `(25,25,19,204)` —— **只差 1 LSB，A 通道完全正确**，说明硬件确实按"低 8 位 + 某个位宽"解释了该寄存器，没有互相覆盖。原文档"高位互相覆盖"的判断作废 | 不改（按目标字段宽度改移位缺乏验证手段，可能引入真错）；如需精确到 0 LSB 再查 `0x509` 定义 |
| ~~E2~~ | `mwv207_draw.c` | ~~`static uint32_t draw_count` 跨 context 共享~~ | **已修**（2026-11-02）：移入 `struct mwv207_context` 的 `uint32_t draw_count;`，`mwv207_config_draw()` 读写 context 字段 | — |
| **E6** | `mwv207_query.c:108,201,284` | `TIMESTAMP` / `TIMESTAMP_DISJOINT` 用 **CPU** `os_time_get_nano()`，不是 GPU 时间戳 | 【复核】三处均 `os_time_get_nano()`；【实测】`GL_TIMESTAMP` 返回 ≈31.7 小时 = CPU 开机时长 | **不改**（复核：与 llvmpipe 同为 CPU 时钟语义，行为一致；硬件时间戳寄存器 `0x00490/0x00494` 虽存在，切换属功能改动而非缺陷修复，且需整条 query 路径验证） |
| ~~E7~~ | `mwv207_query.c` | ~~不支持的 query 类型走 `unreachable()`~~ | **已修**（2026-11-02）：`SO_STATISTICS` / `PIPELINE_STATISTICS` / `PIPELINE_STATISTICS_SINGLE` / 未知类型统一 `return false`（按 Gallium 契约＝结果不可用），不再 `unreachable()`（release 下是 `__builtin_unreachable()`＝未定义行为） | 剩余：这些类型仍无真实硬件计数 |
| **E8** | `mwv207_state.c:335-354` | 只保留单份视口/裁剪，`set_viewport_states` 忽略 `num_viewports` | **复核（2026-11-03，批次 2b）：不是缺陷**。`PIPE_CAP_MAX_VIEWPORTS = 1`（`mwv207_screen.c:160`），`GL_ARB_viewport_array` 未暴露、`GL_MAX_VIEWPORTS` 查询返回 `GL_INVALID_ENUM`（探针 `tests/batch2b/vpquery.c`）→ 应用根本提交不了多视口；忽略 `num_viewports` 与声明一致 | 不改（多视口属新功能，见 D6） |
| **E3** | `mwv207_blit.c:568-584` | 压缩格式分支里算好的 `task_info` 被随后无条件赋值覆盖 | **复核（2026-11-03，批次 2b）：路径不可达**。该分支只可能经 `mwv207_copy_layer()` 进入；`resource_copy_region` 对 `blocksize >= 8` 走 CPU 回退、压缩格式 tile 不支持且恒 LINEAR、GL 不允许压缩色彩缓冲 blit。实测 ETC2 `glCopyImageSubData` 拷贝后采样结果与源完全一致（`DIFF=0`，`tests/batch2b/etc2copy.c`） | 不改（死代码；将来若支持压缩 blit 再重算 `task_info`） |
| ~~E5~~ | `mwv207_state.c:326-331` | ~~`stencil_back_reference` 与正面索引交叉 → 背面取到正面值~~ | **已修**（2026-11-03，`5dee412`）：`mwv207_draw.c:1043-1050` 的 0x528 恒取 `stencil_back_reference[0]`（set 时已按背面语义预填）；改动物理意义明确，未附专门双面模板探针记录 | — |
| ~~O4~~ | `winsys/mwv207/drm/mwv207_drm_cs.c` | ~~`-EBUSY` 无上限 busy-loop；重试可能泄漏 fd~~ | **已修**（2026-11-02）：重试上限 2000 次（≈2s）；失败改 `fprintf` 并关闭 KMD 已 `fd_install` 的 out-fence（判据：`out_fence_fd && submit.fence_fd >= 0 && (in_fence_fd < 0 || submit.fence_fd != in_fence_fd)`，避免误关调用者的 in-fence） | — |

### 1.3 声明与实现不符（诚实性问题，需拍板）

| # | 问题 | 现状 | 三个选项 |
|---|---|---|---|
| **C1** | `R16G16_UNORM` / `R16_SNORM` / `R16G16_SNORM` **采样恒 0**。数据本身已字节精确（强制 LINEAR 修好了上传）。**【2026-11-03 定位：硬件不解码，非编码未解】**把工作正常的 `R16_FLOAT` 纹理强制换成扩展码 `0x36`（R16_SNORM）后也读 0，而同一纹理/同一描述符用码 `7`（RF16）正常；`0x32–0x36`（RGBA16/RG16/R16 归一化）全为 0。16 位**整数**码 `24/25/26` 与 16 位浮点码 `7/8/9` 均取数正确（R16UI 0x3C00 读出 0.234379=15360/65535），上传/读回字节精确；tiling 与 LINEAR、`align`、`TX_CONFIG2.NORM_*`、`TX_CONFIG3.ENABLE_NEW_MODE`、`TX_EXT_CONFIG` bit23/bit31 逐位试验均无变化 → 0x1F76 对归一化 16 位取样返回 0。**【2026-11-03】10bit 解码面已启用**：`f0c7605` 让 P010/P016 在 R16/R16G16 `SAMPLER_VIEW` 受支持时通过（VA 前端据此广播 YUV420_10BPP）；真机 HEVC Main10 硬解 + DRM_PRIME_2 导出与软件解码逐位一致。**仍剩**：这些格式的 GL 采样恒 0 → 10bit 帧经 GL 采样/缩放仍错（`hwdownload` 走导出，不受影响）；`src/mwv207-umd/drivers/mwv207/mwv207_format.c` 已加注释记录该结论 | **A（已采纳 2026-11-03，用户要求“尽可能保留完整功能”）** 保留声明并注明（采样不可用但上传/读回/渲染目标可用）/ **B** 回退到只留 `R16_UNORM` / **C** 继续追编码（已证伪） |
| **C2** | 高级混合**只有 8/15** 个方程。硬件仅实现 MULTIPLY/SCREEN/OVERLAY/DARKEN/LIGHTEN/HARDLIGHT/DIFFERENCE/EXCLUSION；`0x9–0xF` 实测**把帧缓冲清零** | 【实测】属真实硬件缺口 | **保留超额声明（已采纳 2026-11-03，用户要求“尽可能保留完整功能”）：维持原生 GLES 3.2**，文档注明 7 个方程不可用；撤销 cap 会丢 GLES 3.2 |
| **C3** | `ARB_ES3_compatibility` 建立在 C1 那 6 个格式声明上，其中 3 个实际不可采样 → 用 ETC2/EAC 的应用**渲染错误但不崩** | 依赖 C1 | **随 C1 采 A（2026-11-03）：保留声明并注明**；如改 B 则同步回退 |
| ~~I-3~~ | ~~能力表宣告 H264 含 **High422 / High444**，但解码器**硬置** `chroma_format_idc = 1`~~ | **已定（2026-11-10，`a5a938b`）：不可实现，已收回声明**。证据见 §1.5 | — |

### 1.4 编译器 / 工具链（不影响渲染正确性）

| # | 位置 | 问题 |
|---|---|---|
| ~~F.3.1~~ | `nir/mwv207_disasm.c:141-158` vs `nir/mwv207_emit.c:205-232` | ~~`_str_inst_type[]` 与 `get_alu_inst_type()` 的 type 编码互相矛盾（1 是 `.f16` 还是 `i32`）→ `type != 0` 时打印的类型名基本全错~~ **已修**（2026-11-03，`5dee412`）：按汇编器语义改为 `type = (bit0 << 2) \| bit1_2`（`mwv207_disasm.c:639-650`），`.f16` / `.s32` 不再对调 |
| ~~F.3.2~~ | `nir/mwv207_disasm.c` | ~~`mwv207_disasm()` 断言 `sizedwords % 2 == 0`，实际步长是 **4**~~ **已修**（2026-11-02，`841e300`）：断言改 `% 4`（`mwv207_disasm.c:1250-1259`） |
| ~~F.3.3~~ | `nir/mwv207_disasm.c` | ~~FS color 输出打印遇 0 截断~~ **已修**（2026-11-02，`841e300`）：用编译期记录的 `ps_color_out_num` 逐个打印（`mwv207_disasm.c:1315-1322`） |
| **F.4** | 见 `api/04-NIR-COMPILER.md` §12 | **已修 3 条**（2026-11-03，`5dee412`）：`link_shader` 返回 true、`shared_atomic` 失败改打印告警并返回 NULL、RA 溢出循环上限 32。**剩余**：`bo_size/bo_offset` 倍数关系无注释（§5.5）、三个窥孔默认全关（即 D5）、`spill` 位无宏（§11.2）、离线/运行时选项不一致（§11.3）、变体命中只查 4 个 dirty 位（§10.3）、dual-blend 采样槽硬编码 4（§4.5）、`get_ssbo_size` 常量描述符是否漏更新（§7.7）、`shader_key` 位域宽度（§2.3）、liveness 的 `~0u` 处理（§8.5）、`reg_get_base` 各 stage 偏移无依据（§7.2） |
| ~~E9~~ | `include/mwv207_state.h:143` | ~~`mwv207_context_zsa_init` 只有声明、无定义、无调用（删掉即可）~~ **已修**（2026-11-02，`841e300`）：声明已删，全树无该符号 |
| ~~E10~~ | `mwv207_query.c:342`、`include/mwv207_query.h:53` | ~~`destory` 拼写；另有 `cmd_dat_arrary`、`MWV207_IMMEDIATE_GLOBAL_MEMEORY_OFFSET`、`CACHE_TEXUTRE_L1`~~ **已修**（2026-11-02，`841e300`）：`destory→destroy`、`cmd_dat_arrary→cmd_dat_array`、`MEMEORY→MEMORY`、`TEXUTRE→TEXTURE`（连带 11 个文件） |

> F 类只影响**反汇编输出可读性**（调试工具），不影响编译与渲染。
> 但因为 ISA 常量头缺失（F.1），反汇编表是 opcode 数值的**唯一交叉验证来源**，所以修它有额外价值。
> 现况（2026-11-03）：F.3.1–F.3.3 已修，F 类只剩上表 **F.4 的残余条目**。

### 1.5 H.264 4:2:2 / 4:4:4 不可实现（I-3，2026-11-10 已收回声明）

**结论**：本机 H.264 硬解只有 **4:0:0** 与 **4:2:0** 两种色度采样，"做真正的 4:2:2/4:4:4 支持"在寄存器层面无法实现；`a5a938b` 已从 `mwv207_profile_supported()` 与
`mwv207_is_video_format_supported()` 收回 `PIPE_VIDEO_PROFILE_MPEG4_AVC_HIGH422/HIGH444`，并在 `end_frame` 加一道 `chroma_format_idc > 1` 就记 ERROR 并跳过解码的防线。

**证据链**（三条互相独立）：

1. **解码寄存器表没有采样格式选择字段**：`mwv207_video_dec_8170table.h` 全部 1840 项里与色度相关的只有 word7 bit30 的
   `JMDEC_BLACKWHITE_E`（语义＝`chroma_format_idc == 0` 单色）；没有任何 420/422/444 选择器。HEVC 侧同样没有。
2. **原厂闭源驱动自己也不声明**：从 `ref/vendor-1.8.0/mwv207-dev_1.8.0.uos_arm64.deb` 解出的 `jmgpu_drv_video.so`（1.8.0），
   `strings` 里寄存器文档写明 `chroma format idc. 0=4:0:0. 1=4:2:0`；反汇编 `jmgpu_QueryConfigProfiles`（符号 `0x127b0`）得到
   其 H.264 profile 表 = `(13, 6)` 即 ConstrainedBaseline + Main，再追加 `7`＝High；其余仅有 JPEG(12)、HEVC Main/Main10(17/18)、
   VP9 P0/P1/P2(19/20/21) 与条件性的 15/16。**没有 422/444。**
3. **libva 侧根本无法表达**：`/usr/include/va/va.h`（libva 1.22）的 `VAProfile` 枚举里不存在 `VAProfileH264High422/High444`
   （只有 `VAProfileH264High10 = 36` 与 HEVC 的 422/444 系列 24–28）⇒ 原声明在 VAAPI 上本就是**不可达的死代码**。

**实测（`LIBVA_DRIVERS_PATH=/tmp/gd3`，High422 样片 `libx264 -profile:v high422 -pix_fmt yuv422p`）**：
ffmpeg `-hwaccel vaapi` 能建 device，但 `hwdownload` 报 `Function not implemented`（exit 218）取不出画面；
给驱动 `end_frame` 临时插桩后计数为 **0** —— 该流根本走不到驱动解码入口，`5 frames decoded / 0 decode errors` 是 ffmpeg 侧的假计数。
收回声明前后此行为一致（VAAPI 前端只向驱动要 `is_video_format_supported()` 认可的 NV12/P010 面，422 面从未被广播）。

**回归**（同一次构建）：HEVC 720p 8bit/Main10 各 2 tile 列与软件逐位一致；H.264 High 420 逐位一致；VP9 18/18；
`vainfo` 只剩 ConstrainedBaseline/Main/High/High10 + HEVC Main/Main10 + VP9P0；GL 侧 glxinfo/glxgears/query 4/4 均无变化。

> H.264 的另一个色度/位深相关结论见 §1.2 的 **I-4**：High10（10bit）提交会挂死 DEC 管线，
> 已同日定案并收回声明（`4ecca0d`），缺 high10p 位流预转换。

### 1.6 A5：VP9 Profile 2（10bit 4:2:0）已支持；4:2:2 / 4:4:4 定案不可实现

**结论（2026-11-10，`82e7b02`）**：VP9 **Profile 2（10bit 4:2:0）已实现并通过逐位验证**；
Profile 1/3（4:2:2 / 4:4:4）与 H.264 422/444 同理**不可实现**——解码器没有色度采样格式选择
字段（证据链见 §1.5）。此前"Profile 2 会让 ASIC 自旋"的判断已被实测推翻。

**为什么 10bit 只差一个 gate**（不是缺一整条通路）：

| 环节 | 现状 | 位置 |
|---|---|---|
| 位深寄存器 | `JMDEC_BIT_DEPTH_OUT_MINUS8` / `_Y_MINUS8` / `_C_MINUS8` 早已按 `sh->bit_depth` 编程 | `mwv207_dec_vp9.c:1265-1267` |
| 解码参数 | `dec->dec_param.bit_depth = sh->bit_depth;` 在 set_regs 早期、PP 之前 | `mwv207_dec_vp9.c:1170` |
| 输出格式 | PP 按 `pixel_width > 8` 出 `PP_OUT_FMT_YUV420_P010`，与 HEVC Main10 同一段代码 | `mwv207_dec_ppu.c`（`out_p010` / `tiled_e && pixel_width > 8`） |
| stride | `out_w_y = NEXT_MULTIPLE(4 * … * bit_depth, …)` 与 HEVC 的 `4*out_w*pixel_width` 同构，10bit 无需改 | `mwv207_dec_vp9.c:1230 附近` |
| 缺的 | 入口处 `profile != 0 \|\| bit_depth != 8` 一刀切拒绝；profile 查询/格式支持不声明 Profile 2 | `mwv207_dec_vp9.c`、`mwv207_video_codec.c` |

**改动**：放行 `profile == 2 && bit_depth == 10`（Profile 1/3 继续拒绝）；在
`mwv207_profile_supported()` 与 `mwv207_is_video_format_supported()` 同时声明
`PIPE_VIDEO_PROFILE_VP9_PROFILE2`（VAAPI 要 P010 面 + 该 profile 的组合才肯建配置）；
新增 `mwv207_vp9_10bit_enabled()`（默认开，`MWV207_VP9_10BIT_DISABLE=1` 可整体关）。

**逐位验证**（dev 驱动，`LIBVA_DRIVERS_PATH=/tmp/gd3`，ffmpeg `-hwaccel vaapi
-hwaccel_output_format vaapi` + `hwdownload,format=p010`，与软件解码 `cmp`）：

| 样片 | 帧数 | 结果 | dmesg |
|---|---|---|---|
| 64x64（x265 同类小图） | 3 | IDENTICAL | 零超时 |
| 640x360（含 inter，g=10） | 30 | IDENTICAL | 零超时 |
| 1280x720（含 inter，g=30） | 60 | IDENTICAL | 零超时 |
| 1920x1080（g=15） | 30 | IDENTICAL | 零超时 |

回归：`tools/vp9-regression.sh` 18/18；H.264 High 420、HEVC 8bit/Main10 逐位一致；
`tests/verify-installed.sh`（dev 驱动）PASS；载荷 r20 的 `libmwv207_dri.so`/`_drv_video.so`
直接参与上述比对（`/tmp/p42` 复核 glxinfo + query 4/4 + 10bit 逐位一致）。

**残余缺口**：12bit VP9（Profile 2 语法允许）未验证，代码只放行 10bit；Profile 1/3 永久拒绝。

**装到 r20 后的实机复核（2026-11-10 16:21 重启后，运行中的 UMD 即 r20）**：

| 检查 | 结果 |
|---|---|
| `vainfo` | 已列出 `VAProfileVP9Profile2`（与 Profile 0 并存），H.264 只剩 ConstrainedBaseline/Main/High，HEVC Main/Main10 |
| 1080p30 Profile 2（30 帧，g=15） | 硬件解码 186624000 B 与软件解码 **IDENTICAL** |
| 1080p30 Profile 2 压力（120 帧，g=30） | 746496000 B **IDENTICAL**，全程约 20 s（含 `hwdownload` 回读到用户态），dmesg 零新增超时 |
| dmesg | 无新增 `timed out`（仅 `tests/n14-reset` 探针的 6 条 3D 超时），无 WARN/BUG |
| 其余 | H.264 High 420 / HEVC 8bit / HEVC Main10 逐位一致；query 4/4；VP9 回归 18/18；N14 探针 PASS |

### 1.7 B3：intra-only 非关键帧样片（I-5 已修，提交 `8f6e964`）

B3 要的是 **intra-only 非关键帧**（`show_frame = 0` 且 `intra_only = 1`；Profile 0 时
未压缩头不带 color config，按规范默认 BT.601/studio/4:2:0/8bit）。本机 libvpx 产不出
这种帧 —— `-auto-alt-ref 1` 编 320×240 40 帧，只有 4 个隐藏帧且**全是 inter**，所以在
`tests/vp9-intraonly/` 里改用 WebM 官方测试向量并做成可复跑的诊断脚本。

**样片**：`vp90-2-16-intra-only.webm`（<https://storage.googleapis.com/downloads.webmproject.org/test_data/libvpx/>，
175807 B，md5 `70d4afd231a41dd0f0364fec8121fa09`，352×288 Profile 0 8bit）。7 个包，
包 0 用驱动自带解析器确认为 intra-only 隐藏帧：

```
res=0 used=19 key=0 intra=1 show=0 showex=0 err=0 prof=0 352x288 refresh=0x01
tilecols=0 tilerows=0 seg=0 lossless=0 txmode=3 fpar=0 lf=6 sharp=0 qyac=53
```

注意 `fpar=0`（`frame_parallel_decoding_mode = 0`），而本机 libvpx 编出的关键帧是 `fpar=1`。

**比对结果（r20 载荷，`LIBVA_DRIVER_NAME=mwv207`）**：

| 帧 | 内容 | 硬件 vs libvpx |
|---|---|---|
| 0 | intra-only 隐藏帧 | **DIFF**（Y max 244，mean 19.4） |
| 1 | inter（可见） | **DIFF**（Y max 255，mean 21.5） |
| 2/3/4 | 单字节 `show_existing_frame` | **IDENTICAL** |
| 5/6 | inter（可见） | **DIFF**（Y max 255/249，mean 26.8/25.9） |

软件侧 `-c:v libvpx-vp9` 与 `-c:v vp9` 两种解码器逐帧一致，可作参考。
最小复现：只留包 0（单帧、无任何参考）→ 两边都是 1 帧 152064 B，但**不一致**。

**已排除**：
- 比对方法：同批次的 `vp90-2-10-show-existing-frame.webm` 硬件与软件 **IDENTICAL**
  （7 帧 2585088 B）⇒ `hwdownload` 通路、隐藏帧与 `show_existing` 都正常。
- 尺寸/对齐：本机 libvpx 编的 **352×288 关键帧** 硬件与软件 **IDENTICAL**。
- 解析层：`src/mwv207-umd/drivers/mwv207/video-codec/common/mwv207_decoder_vp9_slice_header.c:958-1000`
  的 intra_only 分支与 libvpx `read_uncompressed_header()` 一致。
- **intra-only 帧类型本身**、部分刷新、高低 QP、`frame_parallel_decoding_mode`：
  用 `tests/vp9-intraonly/make-intra-only.py` 把本机关键帧改写成内容等价的 intra-only
  帧后逐位比对（见下节），全部 **IDENTICAL**。

**受控实验（自产 intra-only 样片，2026-11-10 19:45）**：`tests/vp9-intraonly/` 新增
`vp9-parse-probe.c` + `build-probe.sh`（链接驱动自带的 VP9 头解析器）与
`make-intra-only.py`（把关键帧的未压缩头换成 intra-only 形态，保留原压缩头与 tile；
Profile 0 的 intra-only 头不带 color config，`reset_frame_context` 取 3）。用同一编码器、
同一内容生成样片，硬件（r20）与 libvpx 逐位比对：

| 样片 | 关键参数 | 结果 |
|---|---|---|
| 64×64 testsrc2 | refresh 0xFF/0x01/0x02/0x80，rfc=3，fpar=1 | IDENTICAL |
| 352×288 testsrc2 | refresh 0xFF 与 0x01，rfc=3，fpar=1 | IDENTICAL |
| 352×288 testsrc2 CRF 50/55 | qyac 123/145 | IDENTICAL |
| 352×288 testsrc2 `-frame-parallel 0` | **fpar=0** | IDENTICAL |

⇒ 触发条件既不是 intra-only 帧类型、也不是 `fpar=0`/部分刷新/QP，而是官方样片特有的
参数组合（`txmode=3`、`lf=6`、单帧 118 KB 的真实画面内容），范围已从"整类帧"收窄到
"某类流"。**注意**：官方样片的 `txmode=3`（ALLOW_32X32）与本机编码器的 `txmode=4`
（SELECT）不同，这是目前最大的未验证差异。

**当前最可疑处**：`show_existing` 帧（2/3/4）逐字节一致，说明**参考槽里的重建是对的**，
出问题的是"把重建结果写进应用 surface"这一路（PP/输出面），而不是参考管理；但
inter 帧（1/5/6）也 DIFF、且它们的预测来自正确参考，说明问题更可能在**输出面写入**或
**intra 帧的重建在写入输出面时才出错**。目视对比（`tests/vp9-intraonly/` 脚本产出的
`evidence/*.png`）：布局、色调、位置都对，一批 64×64 级区块整块是乱的，不是色度/范围
变换，也不是整帧偏移。

**2026-11-11 取证：官方包是 superframe，且问题只出现在"用 intra-only 帧当参考的 inter 帧"上**

- 官方样片的首包 117963 B 其实是 **superframe，含 4 个子帧**（尺寸
  `30299/35391/40875/11384`，和 117949 B + 14 B 索引正好对上），ffmpeg 会把它拆成 4 个
  独立帧逐个送进驱动（临时插桩 `TMPBS/TMPBX/TMPEF` 证实每帧一次 begin/decode/end，
  `streams_len[0]` 等于本次子帧长度）。⇒ 此前"驱动把 118 KB 截成 30 KB"的怀疑不成立：
  `JMDEC_STREAM_LEN=30306` 就是第一个子帧的大小，`JMDEC_STRM_BUFFER_LEN=304080` 只是
  `352*288*3-48` 的窗口。插桩用完已删除，工作树干净。
- 4 个子帧的未压缩头（`tests/vp9-intraonly/vp9-parse-probe`）：
  sub0/1/2 都是 `key=0 intra=1 show=0`，`refresh=0x01/0x02/0x04`，`txmode=3`；
  sub3 是 **inter 帧**（`key=0 intra=0 show=1 refresh=0x08`，探针因缺参考尺寸报
  `w=0 h=0`，属测试台局限）。所以这个向量的结构是"3 个 intra-only 隐藏帧轮流刷新
  slot 0/1/2 → 第 4 个 inter 帧显示"。
- **单独解 sub0（再跟一个 `0x88` show_existing 让它可见）→ 硬件与 libvpx IDENTICAL**
  （152064 B）⇒ 单个 intra-only 帧的解码本身没有问题。
- **逐个刷新位对照**：`refresh = 0x01/0x02/0x04/0x08/0x10/0x80` 各配一个对应的
  `show_existing(idx)` → 六组全部 **IDENTICAL** ⇒ `refresh_frame_flags` 的
  bit i → slot i 映射、以及从参考槽重建输出都正确。
- **只解 [sub0, sub1, sub2, sub3]（prefix4）→ 只有 1 帧输出（就是这个 inter 的 sub3），
  且 DIFF**；而"自产 intra-only(refresh 0xFF) 后跟一个 inter 帧"是 IDENTICAL。
- 帧序检查：7 帧输出里 `sw2==hw2`、`sw3==hw3`、`sw4==hw4`（完全为 0），其余两两误差
  14–27 ⇒ **不存在输出顺序错位**，差异就落在"这几帧的重建值"上。
- 结论（收窄）：I-5 不是 intra-only 帧自身的解码问题，而是**"intra-only 帧作为参考、
  随后 inter 帧使用它们"这条路径**在该流上的表现。下一步要在自产样片上复现这条路径
  （本机编码器可控），再对照 `vp9_ref_slot_*` 的尺寸/stride 记账与
  `vp9_write_ref_regs()` 的 `ref_slot_luma`/`refw`/`refh` 推导。

**2026-11-11 01:05 取证：错误发生在 inter 帧的解码本身，且已排除参考记账/自适应/txmode**

- `tests/vp9-intraonly/vp9-parse-probe.c` 现在把 `active_ref_idx/ref_frame_map/sign_bias`
  打到 **stderr**（打到 stdout 会破坏 `make-intra-only.py` 的 `key=value` 解析），并支持
  `[fallback_width fallback_height]` 两个可选参数给缺失参考尺寸的帧用。
- sub3 的探针（带 fallback）：`active_ref_idx=0,0,0`、`ref_frame_map=0,0,0`、
  `sign_bias=0,0,0` ⇒ 三路参考**全指向 slot 0 同一张图**，"三路参考各需自己的 MV 图"
  这条假设作废。
- 概率自适应（`vp9_adapt_probs()`，仅 `!error_resilient && !frame_parallel_decoding`
  时运行）：官方 sub3 是 `fpar=0`，而此前自产样片几乎都是 `fpar=1`。补做两组实验：
  `fp0_10.ivf`（本机 352×288、10 帧、`-frame-parallel 0`）hw/sw **IDENTICAL**；
  `[intra-only, inter]`（fpar=0）与 `[关键帧, inter]` 的 **inter 输出完全相同（mean 0.0）**
  ⇒ 自适应与上下文重置无误，此假设排除。
- `txmode=3` 的 **inter** 帧：`vpxenc --good --cpu-used=5 --end-usage=cq --cq-level=20`
  编得 10 帧流（pkt1/pkt2 均为 `key=0 txmode=3`），hw/sw **IDENTICAL**（1520640 B）
  ⇒ txmode（含 inter 帧）彻底排除。
- **关键新证据**：把 `[sub0,sub1,sub2,sub3]` 尾部再加一包 `0x8b`（show_existing 显示
  slot 3，即刚解完的 inter 帧）→ 两帧输出（直接输出 + 槽重放）**逐字节相同**，且都与
  libvpx 差 mean 19.6。⇒ 参考槽里存的就是这份错图，**错误发生在解码阶段本身**，不是
  PPU/输出路径；同时说明"用 show_existing 校验槽内容"只对 intra-only 槽有效。
- 反向尝试（`[sub0,sub3]`、`[sub0,sub1,sub3]`）无效：libvpx 侧 0 帧输出（参考/色彩状态
  不完整），无法作为对照。
- 现状：I-5 收敛为"官方流里这个 inter 帧的解码结果错"，自产的同尺寸、同 txmode、
  同 fpar 流都复现不出来；下一步需要逐宏块对照（segment id / last-partition / 参考帧
  尺寸记账），或自写 VAAPI 客户端控制 surface 复用以隔离 ffmpeg 的影响。

**寄存器 dump 已恢复（2026-11-10 20:05）**：`mwv207_video_dec_reg.c` 的
`mwv207_9200_dump_register()` 里那句无条件 `return;` 已去掉（上限放宽到 400 帧），
并在 `mwv207_dec_vp9_set_regs()` 末尾加了一次 `reg_buf->dump_regs()`。用
`mwv207_debug=video` 跑官方样片包 0 与自产 352×288 intra-only 样片，把两份
`mwv207_video_reg.dump` 对齐比较：1802 个共有寄存器里 78 个取值不同，绝大多数是
内容/码率差异（`JMDEC_QUANT_SEG*`、`JMDEC_FILT_LEVEL*`、`JMDEC_STRM_START_BIT`、
`JMDEC_STREAM_LEN`、`JMDEC_INIT_RLIST_*` 等）。**唯一的结构性差异是
`JMDEC_TRANSFORM_MODE`：官方样片 = 3（ALLOW_32X32），自产样片 = 4（SELECT）**；
`JMDEC_SECOND_TRANSFORM_E`/`JMDEC_NONSQUARE_TRANSFORM_E`/`JMDEC_DMH_E` 看着也有差异，
但查规格表（`mwv207_video_dec_8170table.h`）它们是同一个 word 11 的相邻位
（shift 27/28/29，与 `TRANSFORM_MODE` 的 3 位字段重叠），只是别名，不是独立缺陷。
所以下一步最该验的就是 `txmode=3`，而本机 ffmpeg/libvpx 都不暴露 `-tx-size`，
造不出这种流 —— 要么装 `vpxenc`（Debian 的 `vpx-tools`）试 `--tx-size=32x32`，
要么在同内容上手工比对"只差 txmode"的两条流。

**txmode 已排除（2026-11-10 20:25）**：`vpxenc` 虽没有 `--tx-size`，但**编码模式会决定
tx_mode** —— `--rt --cpu-used=8` 出 `tx_mode=2`（ALLOW_16X16），
`--good --cpu-used=5 --end-usage=cq --cq-level=20` 出 `tx_mode=3`（ALLOW_32X32）。
两条流的关键帧在硬件上解码都与 libvpx **逐位一致**；再用 `make-intra-only.py` 把
tx_mode=3 的关键帧改写成 intra-only（`refresh=0x01`、352×288、101 KB 高码率），
硬件同样 **IDENTICAL**。⇒ `tx_mode` 1/2/3、intra-only 与 `tx_mode=3` 的组合、
分槽刷新、高码率都不再是嫌疑，I-5 只在这一份官方样片（`vp90-2-16-intra-only`）上复现。

**误差形态分析（2026-11-10 20:35，纯像素统计）**：把 `only0` 的软/硬 Y 平面对齐比较，
① 按 4/8/16/32/64 网格边缘 vs 内部统计平均绝对误差，全部约 26–28，**没有边缘聚集**
⇒ 不是去块滤波（loop filter）问题；② 按行带搜索水平平移（±40 像素）最优位移在
−20/+4/−8/−8 之间跳，**不存在固定错位** ⇒ 不是 stride/重排问题；③ 16×16 块的误差
直方图两极分化（最好 0.1、最差 136.7），空间上分布在画面下半部与右中部，既非"整帧
乱"也非"局部几个块"。这条形态与"部分超级块没解对、其余正确"更接近，下一步应拿
逐块（64×64 超级块）的误差图与码流里的 partition/tx 决策对照，或恢复寄存器 dump
逐帧比对 A 的 CTU 级参数（`JMDEC_*_PARM`/`JMDEC_*_ILF` 系列）。

**2026-11-11 取证（自写 VAAPI 客户端，`tests/vaapi-client/vadec.c`）**：为绕开
ffmpeg 的包重组，直接走 libva 解码，把问题收窄到三个事实：

- 官方 `vp90-2-16-intra-only.webm` 的**包 0 是 superframe**，内含 4 个子帧
  `30299/35391/40875/11384` 字节（和 117949 + 14 B 索引 = 117963）；ffmpeg 会把它拆成
  4 次送驱动，之前"`JMDEC_STREAM_LEN` 只有 30306、包被截断"的猜测即由此作废 —— 那是
  第一个子帧的长度。
- **单个 intra-only 子帧单独解码是逐字节正确的**（`sub0` + 尾部单字节 `0x88`
  `show_existing`，与 libvpx 比 IDENTICAL）。所以 I-5 不是"intra-only 帧解不对"，
  差异只在"用 intra-only 帧当参考的 inter 帧"上。
- 用 `[sub0..sub3]` 顺序解码，第 4 帧（inter 的 `sub3`）与软件差 mean 19.61，与 ffmpeg
  路径同量级 ⇒ 可稳定复现，且与容器/客户端无关。

**同批修掉的三处实现缺陷（N18，`6225e97`）**：直连 VAAPI 的客户端送单字节
`show_existing_frame` 包时它会被丢掉 —— ① `mwv207_video_decoder.c` 的
`decode_bitstream()` 在 `num_buffers == 1` 分支里 `if (sizes[i] != 1)` 把它静默丢弃；
② `mwv207_vp9_parse_frame()` 入口按 `size < 4` 拒绝；③ 即使放行，位读取器按 32 位字
推进，1 字节会读越界（`mwv207_sw_turn_around` 断言）。三处都已改。另把
`show_existing_frame` 分支里误加的 `frame_skipped` 去掉（它有正常输出画面，不该按
跳过上报失败），并补上该分支的输出面地址/步长/位深编程。

**仍未解决**：`show_existing` 的**重放画面**目前是全黑（`[关键帧, 0x88]` 时
va1 与 va0 差 mean 105.85、非零采样 522/1568）。原因已查清：整条解码+PPU 管线由
`JMDEC_DEC_E` 启动，`show_existing` 不需要解码单元，把 `DEC_E` 关掉后 PPU 也不会
执行（补上 `DEC_MODE`/公共配置/`mwv207_pp_set_regs()` 后画面仍是黑的，实测确认）。
要真正支持得给 PPU 单独找一条"只做格式转换"的启动路径，属独立课题；B3 的判据不
依赖它（单帧 intra-only 已与 libvpx 逐字节一致）。参考槽副本本身内容正常
（`vp9_refresh_slots()` 的源缓冲非零）。

**驱动不认 superframe（新登记的限制，暂不影响内置播放链）**：把官方首包（117963 B 的
superframe）当成**一个**缓冲直接送 `vaRenderPicture`，驱动会把它当单帧解，输出与
libvpx 差 mean 17.37 —— 说明 `mwv207_dec_vp9_set_regs()` 没有拆 superframe。ffmpeg 在
送驱动之前就按 superframe 索引拆好了，所以播放链路不受影响；但**直连 VAAPI 的客户端
必须自己拆**（`tests/vp9-intraonly/extract-webm-packets.py` 里的 IVF 拆分逻辑可复用）。

**降低范围的一条反证**：本机自造的 "关键帧 / 改写后的 intra-only + inter" 对照
（`io_fp0` 序列）里，硬件把 intra-only 帧当参考解出的 inter 帧与软件**逐位一致**
（`hw A[0] == B[1]`、`sw A[0] == B[1]`），所以"intra-only 当参考"本身没有问题，
官方向量里出错的应是该流特有的属性。另注意：把官方向量拆成 `[sub0, sub3]` 两帧后，
**libvpx 自己也会拒绝**（`Referenced frame has incompatible color format`，因为
Profile 0 的 intra-only 帧不携带 color config，缺了前面关键帧给它定调）——这类
拆分样本不能用来判定驱动。

**待查方向**：① ~~`fpar=0`~~、~~`refresh_frame_flags=0x01`~~、~~高 QP~~ —— 均已被自产
样片实验排除；② **`transform_mode`**：官方样片 `txmode=3`（ALLOW_32X32），本机编码器
给 4（SELECT），驱动 `JMDEC_TRANSFORM_MODE` 直接写 `sh->transform_mode & 0x7`，需确认硬件
对 3 的语义与"非 SELECT 时块尺寸固定"的处理；③ `round-trip`：把官方样片的 tile 数据用
本机编码器重编同内容、同 `txmode` 的流做对照；④ 硬件寄存器回读（当前
`mwv207_9200_dump_register()` 里有一句 `return;` 把整段 dump 关掉了，
`MWV207_DEBUG=video` 只能拿到空的 `mwv207_video_reg.dump`）——要 dump 得先恢复该函数。

**复跑**：`tests/vp9-intraonly/run-vp9-intraonly.sh`（首次需联网下载样片；`--local DIR`
可复用已下载文件）；自产样片用 `tests/vp9-intraonly/build-probe.sh` +
`tests/vp9-intraonly/make-intra-only.py`。

### 1.8 B2 侦察：官方 resize 向量一批能解、一批跳过；I-6 已修（`bb5cd9e`）

**2026-11-11 补充：把保护性跳过临时放开后的实测**（`MWV207_VP9_ALLOW_RESIZE`，代码已撤回）。
`vp90-2-21-resize_inter_320x180_5_1-2.webm`（30 包，352×288 的 inter 帧引用 320×180 的
槽）在放开后能一路走到 winsys 提交，但**提交失败**：

```
mwv207_drm_cmdbuf_commit: submit failed: -34 (Numerical result out of range)
```

`-34` 是 KMD `src/kernel-6.6/mwv207_submit.c` 里 `mwv207_submit_patch_entry()` 的
`-ERANGE`（`reloc->cmd_offset + {2,4,8} > dat_size` 或 `reloc->bo_offset >= bo_size`），
即"跨尺寸帧的寄存器流/地址表超出了为等尺寸帧准备的缓冲或槽几何"。同时 `vadec` 仍
按第一阶段给的 352×288 建 surface，输出的 30 帧与软件（320×180、20 帧）不可比。
⇒ B2 真正实现需要三件事，都不是一行改动：
① 参考槽按"每槽自己的几何"记账（现在 `st->ref_stride` 是首帧 `frame_bytes` 固定值，
`vp9_write_ref_regs()` 用 `vp9_frame_geometry(refw,refh)` 算步长但槽偏移仍用 `slot*ref_stride`）；
② 输出/应用 surface 在帧尺寸变化时重建（VAAPI 前端在 `vaCreateContext` 之后不会自己换 surface 尺寸）；
③ 寄存器流的容量要按最大帧而非首帧分配（上面的 `-ERANGE`）。
在没做完这三件事之前，保留"尺寸不一致即跳过 + `vaSyncSurface()` 报错"的行为是对的。


**2026-11-11 `351e6ed`：第一版实现（未实测）**

商家对照（vendor 栈 `jmgpu` 1.8.0）在同一向量上是逐字节正确的，说明硬件能力没问题，缺的只是
我们的实现。基于取证明细（`pkt_0` 320×180 关键帧 `refresh=0xff`、`pkt_10` 160×90 `refresh=0x03`、
其余 inter 帧不带尺寸），改动两处：

1. 删掉 `mwv207_dec_vp9_set_regs()` 里"参考尺寸 != 当前帧尺寸就跳过"的分支。硬件本来就按
   每个参考槽自己的几何编程（`vp9_write_ref_regs()` 的 `ref_w_reg`/`ref_h_reg`/`ref_y_stride_reg`
   与 14 位定点 `HOR/VER_SCALE`），这条分支是 B2 之前为了规避硬件挂死加的粗保护
   （§1.7 记忆：旧布局下跨尺寸会让单元空转，N14 之后有 TDR 兜底）。保留"槽从未解码过就跳过"。
2. staging 缓冲（`buf->res`，只是本地暂存，PP 的输出目标是 `buf->resources[]`＝应用 surface）
   在 `frame_bytes` 变大时先释放再按新 `buff_size` 重建，避免越界写。

**2026-11-11 `21a15db`：B2 完成（参考槽跨解码器实例移交）——已实测通过**

切回自研栈后用 dev 驱动实测 `351e6ed` 版本仍跳过 9 帧，日志为
`vp9: reference 0 (slot 0) has no decoded picture; frame skipped`。加临时打印后
真相清楚了：分辨率从 320×180 变到 160×90 的那一帧，**解码器状态指针整个换了**
（`st=…1eb0` → `st=…0f00`）——客户端（ffmpeg vaapi）在分辨率变化时重建了 VA
context，Mesa 随之销毁并重建 `pipe_video_codec`，我们私有的 `ref_store`
（参考槽副本，存的是 ASIC 内部布局的像素）跟着新实例一起归零。

修法（`21a15db`，`src/mwv207-umd/.../mwv207/mwv207_dec_vp9.c`）：
- 新增进程级 `vp9_ref_carry`：`vp9_destroy_state()` 把 `ref_store` 连同 16 个槽的
  `w/h/luma/bytes` 与 `stride/ref_frame_bytes` 移交到缓存（所有权一并交出，避免重复释放），
  新建状态时 `vp9_ref_carry_adopt()` 直接接手。
- 合法 VP9 流总以关键帧开头（`refresh_frame_flags=0xff` 覆盖全部 8 槽），所以不会把
  上一个流的旧内容当成新流的参考；分辨率变化的流（本向量）正好接上 320×180 的槽。
- 参考槽内容无法从客户端 surface 反推（那是线性/PP 布局，不是 ASIC 内部布局），
  所以只能"移交"而不是"按客户端 surface 重算"。

实测（KMD `8DFC520FB8FCDECFBC911AA`，dev 驱动）：

| 向量 | 修前 | 修后 |
|---|---|---|
| `vp90-2-21-resize_inter_320x180_5_1-2.webm` | hw=864000 `SKIPPED-AS-ERROR` 跳过 9 | **hw=2592000 `IDENTICAL` 跳过 0** |
| `vp90-2-14-resize-10frames-fp-tiles-1-2.webm` | IDENTICAL 4665600 | IDENTICAL 4665600 |
| `vp90-2-16-intra-only.webm` | IDENTICAL 1064448 | IDENTICAL 1064448 |
| `vp90-2-10-show-existing-frame.webm` | IDENTICAL 2585088 | IDENTICAL 2585088 |
| `fp0g5.ivf`（I-5 复现样本） | 20/20 | 20/20 |

`tools/vp9-regression.sh` 18/18、query 4/4，`dmesg` 无 `timed out`/WARN/BUG —
跨尺寸参考并没有让 ASIC 自旋（之前 §1.8 里"旧布局下会挂死"的担心在当前寄存器
配置下不成立，TDR 也只是兜底）。

**已知未解（若实测仍跳过，按此顺序查）**：
- 跳过计数 9 的另一种解释：客户端在分辨率变化时重建了 VA context（ffmpeg 对 `vaCreateContext`
  之后不会自己换 surface 尺寸），我方解码器状态（参考槽副本）随之清空，于是"有参考但槽空"。
  厂商驱动之所以不受影响，是因为它直接用 `pic_param->ref[]`（客户端 surface）当参考，
  没有第二份参考副本。若这一条成立，B2 的正确做法是把参考槽改成"引用客户端 surface"，
  而不是复制；那是结构性改动（见 `vp9_refresh_slots()` 的 `ref_store`/`ref_stride` 设计）。
- 输出尺寸：`vp9_program_pp()` 用 `decoder->base.width/height`（创建时固定）作 PPU 输入，
  `mwv207_check_pp_unit_config()` 在未设 `scale` 时把 `scale = crop = in_width/in_height`
  ⇒ PPU 恒 1:1。分辨率变化后要么由客户端重建 surface，要么显式设
  `ppu_ext_cfg->scale.width/height = target->width/height`（`pipe_video_buffer` 带
  `width/height`，见 `p_video_codec.h:54-55`）。
- 寄存器流容量：临时放开保护时曾见 `mwv207_drm_cmdbuf_commit: submit failed: -34 (ERANGE)`，
  来自 `src/kernel-6.6/mwv207_submit.c` 的 `mwv207_submit_patch_entry()`；若实测再现，
  需要把命令流缓冲按"最大帧"预分配。

B2 要的是"跨尺寸参考"（参考帧尺寸与当前帧不同）。官方测试向量里有两族现成的：

| 向量 | 说明 | 硬件 vs libvpx |
|---|---|---|
| `vp90-2-14-resize-10frames-fp-tiles-1-2.webm` | 432×240，10 帧，tile 1×2 | **IDENTICAL**（4665600 B） |
| `vp90-2-21-resize_inter_320x180_5_1-2.webm` | 320×180，30 帧，首包 `K__` 关键帧 | **DIFF**（两边都是 2592000 B） |

第一族说明"带 tile 的普通 resize 序列"在本驱动上是通的。第二族触发驱动的保护性跳过：

```
mwv207_dec_vp9_set_regs() ERROR vp9: reference 0 (slot 0) has no decoded picture; frame skipped
```

（每帧一行。）libvpx 软件解码这条流**没有任何告警**，即它是合法流；但驱动把帧判为
"参考缺失"并 `frame skipped` 之后，**这些帧仍然被当成正常帧输出**（硬件输出字节数与
软件一致 2592000 B，内容 DIFF）。也就是说：

- 现有"缺参考/尺寸不同即拒绝"（`72f9523`）确实拦住了不该解的帧；
- 但拦截**没有传到应用**：`mwv207_dec_vp9_set_regs()` 之后调用方继续走 PP/输出，
  用户拿到的是脏帧而不是错误。

这既是 B2 的真实内容（要么支持跨尺寸参考，要么让"跳过"对上层可见），也是 I-6 候选：
**解码失败必须让 surface 标记失败/让 fence 带错误，而不是静默出图**。

跳过时到底发生了什么（代码层面）：三处"跳过"都是 `reg_buf->set_reg(JMDEC_DEC_E, 0);
return;`（`mwv207_dec_vp9.c:1017-1050`），而 **PPU 的输出面配置在函数末尾
（`mwv207_pp_set_regs()`，约 :1433）**，提前返回后根本没编程 —— 也就是硬件既不解码、
也不写目标 surface；应用拿到的"画面"其实是那块 surface 的**旧内容/未初始化显存**。
所以修复有两条路：

1. **占位填充**：跳过时在 CPU 侧把目标 surface 填成可识别的中性色（例如 0x80 灰）。
   要注意 NV12/P010 与 64B 对齐/tiled 布局，必须先确认 `target` 的实际内存布局。**未做。**
2. **失败上报**：让 `vaEndPicture`/`vaSyncSurface` 看到错误。**已做（`bb5cd9e`）。**

**I-6 修法（2026-11-10，`bb5cd9e`）**：不需要动 Mesa 的 `frontends/va` —— Mesa 24.1 的
`pipe_video_codec` 已经有 `get_decoder_fence(codec, fence, timeout)`，而
`_vlVaSyncSurface()` 正是把它的返回值翻译成 `VA_STATUS_SUCCESS` / `VA_STATUS_ERROR_TIMEDOUT`
（返回 0 即报错）。原来本驱动根本没实现这个 vfunc，`surf->fence` 始终为 NULL，
`vaSyncSurface()` 在"没有未完成操作"的分支直接返回成功。所以：

- codec 层在每个跳过点置 `decoder->frame_skipped`（VP9 13 处、H.264 的 4:2:2 与
  High10 两处、未支持格式的 default 分支）；
- `end_frame()` 看到它就不提交作业，改调 `mwv207_video_decoder_mark_frame_failed()`：
  置 `frame_failed`，并给 `*picture->fence` 一个 fd = -1 的桩 fence
  （Mesa 只在 fence 非空时才调 `get_decoder_fence()`）；
- 新增 `mwv207_video_decoder_get_decoder_fence()`：`frame_failed` 置位时返回 0，
  其余情况正常 `fence_wait()`。

实测（dev 驱动）：`vp90-2-21-resize_inter_320x180` 现在 ffmpeg 报
`Failed to sync surface 0x2b: 38 (unknown libva error)` 并停止输出，不再是 2592000
字节的脏数据；`vp90-2-14`、VP9 Profile2 10bit、show_existing、quantizer-00、
HEVC 8bit、H.264 8bit 回归全部逐位一致。`tests/vp9-resize/run-vp9-resize.sh` 的判据
相应改成 `SKIPPED-AS-ERROR`（好）/`SKIPPED-MUTE`（I-6 复发）。

副作用（有意为之）：被跳过的帧会让 ffmpeg 报 `Input/output error` 并放弃该序列。
真机播放时这是"明确失败"替代"静默脏帧"。
复跑：`ffmpeg -hwaccel vaapi -hwaccel_output_format vaapi -i <向量> -vf hwdownload,format=nv12`
与 `-c:v libvpx-vp9` 逐字节比对；向量在
<https://storage.googleapis.com/downloads.webmproject.org/test_data/libvpx/>。

**2026-11-11 用自写 VAAPI 客户端复核（`tests/vaapi-client/vadec.c`）**：为了排除
ffmpeg 的帧对齐/拆包干扰，写了一个直连 libva 的最小客户端（`vaGetDisplayDRM` +
`vaCreateConfig(VP9Profile0)` + 逐包 `vaRenderPicture`/`vaSyncSurface` +
`vaExportSurfaceHandle` 读回）。它内置 VP9 superframe 拆分（官方首包 117963 B
拆成 `30299/35391/40875/11384` 四个子帧），实验结论：

- 单解官方 sub0（+尾部 `0x88` 重放）→ 第 0 帧与 libvpx **逐字节一致**；
  `[本机关键帧, 官方 sub1, 0x89]` → 解码出的 sub1 也与软件一致。
  ⇒ **intra-only 帧本身的解码路径是正确**，I-5 不是"intra-only 必错"。
- 但 `[sub0, sub1, sub2, sub3]` 里 sub3（inter，三路参考同指 slot 0）与软件差
  mean 19.5；把 4 个子帧外层再加 `0x8b`（重放 slot 3）得到的画面与 sub3 输出
  逐字节相同、同样差 19.5 ⇒ 差异落在"**用 intra-only 写出的参考槽做帧间预测**"
  这条路上，且发生在解码阶段（不是 PP/输出）。
- `show_existing_frame` 重放目前是黑的（`va1 vs va0` mean 105.85，非零采样
  522/1568）：整条"解码 + PPU"由 `JMDEC_DEC_E` 启动，重放帧不需要解码单元，
  补上 `DEC_MODE`/`dec_set_common_config_regs`/`DEC_OUT_DIS=0`/
  `DEC_OUT_EC_BYPASS=1`/`vp9_program_pp()` 之后仍黑 ⇒ 需要单独一条"只做格式
  转换"的启动路径，未实现（见 `73ecbde` 的注释）。
- 顺带确认一个接口限制：驱动**不拆 superframe**，直连 VAAPI 的客户端必须自己
  按索引拆（整包送进去会按单帧解，输出与软件差 mean 17.37）；ffmpeg 侧无影响。

**重放帧（show_existing_frame）出黑图的尝试与否定结论（2026-11-11）**：
- 只开 `JMDEC_PP_E`/`JMDEC_PP_PIPELINE_E`（关 `DEC_E`）、并把 PP 输入地址指到
  参考槽，输出仍是黑（mean 106，非零采样 522/1568）⇒ **PPU 不是靠这两个位
  自启动的**，整块视频解码器仍以 `DEC_E` 为启动条件。
- 反向验证"目标 buffer 是不是显示用的那块"：把重放帧写入的 `buf->res->bo`
  直接 memset 成 0x80，导出后该帧有 1/3 采样变成 128 ⇒ **写对了 buffer，但
  布局不对**（内部是 ASIC 的 tiled 布局，应用 surface 期望线性 NV12；正常帧
  由 PPU 做这一步转换，重放帧没有 PPU 参与就被直接读了）。
- 结论：要支持重放必须二选一 —— ① 找到 PPU 单独的启动方式（寄存器语义没
  文档，厂商 VAAPI 驱动是闭源的）；② 在 CPU 侧实现 tiled→线性转换（当前
  驱动里没有这样的 helper，tiling 公式还只从 `vp9_frame_geometry()` 反推得到
  `out_w = NEXT_MULTIPLE(4*ceil8(w)*bit_depth, align*8)/8` 这一层）。两条都
  不在 B3 的判据路径上（单帧 intra-only 已逐字节一致），故留作待办。

**用参考槽重放做"交叉对齐"后 I-5 的定位（2026-11-11，关键）**：ffmpeg 不输出
隐藏帧，逐帧索引对不上；但 `vadec` 会输出每一帧，于是可以用"官方流里的
`show_existing` 包"充当 libvpx 的槽内容探针：官方 7 个包的重放分别吐出
slot0/1/2 的画面，把它们与我们的解码帧做成矩阵比较：

| 我们的输出 | == libvpx 的哪一帧 | 均值差 |
|---|---|---|
| va0（sub0 intra-only） | sw4（重放 slot 0） | **0.0** |
| va1（sub1 intra-only） | sw3（重放 slot 1） | **0.0** |
| va2（sub2 intra-only） | sw2（重放 slot 2） | **0.0** |
| va3（sub3 inter） | 最近的 sw 帧也差 15.4+ | **≈19.5** |

⇒ **三个 intra-only 帧的解码结果与 libvpx 写进参考槽的像素逐字节相同**，
参考内容没有错；差的是 inter 帧本身的解码（sub3 与任何 sw 帧都对不上，而
不是"某处偏移"）。同一份客户端解本机 libvpx 造的 30 帧关键帧+inter 流
（`own.ivf`）**30/30 逐帧一致**，说明 inter 解码整体没问题，只有这份官方
向量上的 inter 帧不同。

已经排除/试过而不成立的解释：superframe 截断、txmode（intra 与 inter 两侧都
单独验证过）、refresh 位映射、槽拷贝、帧序、概率自适应的有无（临时关掉
`vp9_adapt_probs()` 后 va3 的差从 15.4 变 16.3，只是量变，不改变结论）、
PP/输出布局（intra 帧逐字节一致已证明这条路正常）。

尚未做的：把 sub3 的熵解码结果（模式/MV/残差）与 libvpx 逐块比对；或用
"同内容重编"造出参数一致但编码器不同的 inter 帧做对照。

**局部化到"整帧都不一致"（2026-11-11 本轮最后一步）**：把 va3（我们解的 sub3）
与 sw0（libvpx 解的同一条 inter 帧）按 16×16 块逐个比对，**396 块里 0 块
完全一致**，而且没有"前一半对、后一半错"的边界 —— 差异是全局的，不是某一
行/某一块之后熵解码走飞。同时 va3 与三张参考帧的距离分别是 15.4（slot0）、
18.2（slot1）、16.0（slot2），最近的是它真正引用的 slot 0，说明**参考选对了、
预测方向没跑偏**，像是"预测做对了、残差没加对"。这条最可疑的方向是量化参数
（`JMDEC_QUANT_SEG*`）或残差/变换的配置在"无关键帧的流"里被算错 —— 本机自造
的关键帧+inter 流 30/30 一致，唯一差别就是官方这份流**从头到尾没有关键帧**。

**更正（2026-11-11）："用本机内容复现 I-5"这个结论是错的，已撤回。**
当时拼出的 `own_nokf.ivf` 是 31 个包（实际应为 30 个：1 个 intra-only + 29 个
inter），包里多了一个由拼装脚本写坏的包，导致逐帧对位整体错了一帧，才出现
"30 个 inter 帧全部不一致"。用原始 `own.ivf` 的包列表重建（把第 0 个包换成
`make-intra-only.py --refresh 0xff` 的结果，其余 29 个包原样保留）后，
`own_nokf2.ivf` = 30 个包，vadec 解 30 帧、libvpx 解 29 帧（隐藏帧不出），
**va[1..29] 与 sw[0..28] 逐帧完全一致（0 差异）** ⇒ **本机内容即使"没有关键帧、
首帧是 intra-only"，解码也完全正确**，I-5 没有用本机内容复现。这条更正也解释了
同一轮里"寄存器 1801 个全同、概率表全同、参考帧缓冲全同"却是"输出了不同画面"
的怪现象：那两组对照（`twoA.ivf`/`twoB.ivf`，都是 2 帧）本来就输出**完全相同**
（与 libvpx 逐字节一致），是我把"本机 30 帧流"的错误结论套到了寄存器实验上。
教训：拼装测试流之后必须核对帧数与包数，再用一个已知正确的样本做正对照。

**两条接口事实（2026-11-11，用于解释"为什么自造样本难复现"）**：
① Profile 0 的 intra-only 帧**不携带 color config**，libvpx 按规范回落成
`CS_BT_601` + studio range + 4:2:0 + 8bit（`/tmp/vp9dec.c:2722-2731` 有原文注释）；
本机 `libvpx-vp9` 编出的关键帧默认写 `color_space=0`（未指定），于是"关键帧 +
intra-only 混用"时 libvpx 会报 `Referenced frame has incompatible color format`。
用 `ffmpeg … -colorspace bt470bg -color_range tv` 重编后关键帧写 `cs=1`，两边才
一致（此时"首帧 intra-only + 29 个 inter"的自造流仍是 **29/29 逐帧一致**）。
② 官方 sub3 实际用到了 **slot 0/1/2 三个参考槽**（驱动日志：`reference 1 (slot 1)
has no decoded picture; frame skipped`），所以不能把官方向量简化成 `[sub0, sub3]`
—— libvpx 也拒绝这种缺槽的流。要复现必须保留 `sub0/1/2` 三个 intra-only 帧。

**对上面这条的否定（同日，同帧正对照）**：在**本机自造、解码完全正确**的流
（`own_cs.ivf`，30/30 与 libvpx 一致）上做同样的 dump，参考槽内容与显示帧也
相差 **mean 67.4** ⇒ **"槽内容 ≠ 显示帧"是 ASIC 的正常布局（槽里是解码单元的
内部布局，显示帧是 PPU 转换后的结果），不是缺陷**。官方流那次量到 23.6 只是
内容不同造成的幅度差异，不能当证据。此前的"参考槽布局可疑"因此降级为"已否定"，
I-5 仍未定位。

**可复现的本机样本（2026-11-11，I-5 的缩小版，尚未解释）**。用
`ffmpeg -f lavfi -i testsrc2=size=352x288:rate=30 -frames:v 20 -c:v libvpx-vp9
-b:v 1M -g 5 -frame-parallel 0 -pix_fmt yuv420p -colorspace bt470bg
-color_trc bt709 -f ivf fp0g5.ivf` 造 20 帧（每 5 帧一个关键帧、
`frame_parallel_decoding_mode=0`）：vadec 与 `-c:v libvpx-vp9` 比对，
**差异帧 = 6,7,8,9, 11,12,13,14, 16,17,18,19**，即第 2 个 GOP 起的所有
inter 帧（第 1 个 GOP 的 1–4 帧完全一致，关键帧本身也都一致）。同样的流用
`-frame-parallel 1`（`default`）编时 **20/20 全对** ⇒ 触发条件是
**fpar=0 + 多个 GOP**。把第 2 个 GOP 的 5 个包单独拆成 `seg2.ivf` 解码
**5/5 全对** ⇒ 是**上一段留下的状态**（概率上下文/计数/槽）在关键帧之后没有
被正确重置，与帧内容本身无关。

已尝试但**无效**的修法（各做过一次、均已撤回）：① 在概率上下文重置条件里补
`intra_only`（`mwv207_decoder_vp9_slice_header.c` 两处）；② 把
`frame_context_idx` 在 key/intra_only/error_resilient 时强制为 0（对应 libvpx
`vp9_setup_past_independence()`）；③ 关键帧也清段映射缓冲区；④ 每帧清
`VP9_COUNTER_OFFSET` 的计数缓冲。**反向实验**：关掉 `vp9_adapt_probs()`
（临时 env 门控）会让第 1 个 GOP 也全错 ⇒ 自适应本身是必需的，且在 GOP 1 里是
对的，问题在"跨 GOP 的状态"。

**状态 dump 的实测结果（2026-11-11）**：在 `mwv207_dec_vp9_set_regs()` 入口按帧打印
`st->saved_ctx` 的 FNV 哈希与 `prev_was_inter/prev_was_shown`，并在解析后打印
`key/intra_only/fci/rfc/fpar/refresh_entropy_probs` 与 `sh->entropy`、
`sh->entropy_last[0..3]` 的哈希。对 20 帧流与"只解第 2 个 GOP"两例对照：

- 关键帧（第 6 帧 vs seg2 的帧 0）**解析结果完全相同**（`ent=ac4d21e5ba5342bd`、
  `l0..l3=29b85013eef86153`），说明解析阶段没有历史泄漏；
- 但**关键帧处理完之后**两例的 `saved_ctx` 哈希不同（长流 `de9b33a04e26aa96`、
  seg2 `ff3667d7dfeb1edf`）⇒ **差异产生在"关键帧之后的上下文落盘"这一步**
  （`vp9_adapt_probs()` 写回 `saved_ctx` 的数据依赖了本帧之前的状态）。
  本流的关键帧是 `er=0, fpar=0, refresh_entropy_probs=1`，与"关键帧必然
  error_resilient"的假设不同，因此 `vp9_setup_past_independence()` 的路径需要
  按"intra-only、但刷新上下文"来模拟。

尝试过但**使 GOP 1 也变错**的改法（已撤回）：把自适应基准从
`st->pre_ctx[fci]`（帧头更新前的表）改成 `sh->entropy`（帧头更新后的表），以及
把重置时的 `saved_ctx[i]` 从 `sh->entropy` 改成"默认表"。⇒ 现有实现的自适应基准
就是"帧头更新前"的表，与实际需要的语义只差在"哪些帧的更新该被落下"。

下一步方向：在关键帧前后各 dump 一次 `st->saved_ctx[0..3]`、计数缓冲与
`st->ref_store` 槽内容，找出第一处不符的字节；或对照 libvpx 的
`vp9_setup_past_independence()` 完整清单（它还重置 `lf_ref_delta`/`lf_mode_delta`、
段特征、参考槽映射）。

**新疑点：参考槽拷的是"PP 之前的原始缓冲"（2026-11-11）**。给
`vp9_refresh_slots()` 加临时 dump（已撤回）后发现：同一次运行里，官方 sub0 写进
参考槽的字节与它**显示出来**的画面（`va0`，与 libvpx 逐字节相同）差 mean 23.6；
也就是说参考槽里存的不是最终图像，而是 PPU 之前的原始缓冲。`frame_bytes` 实际
是 **152064**（线性 NV12 大小），而 `vp9_frame_geometry()` 推算的 tiled 布局只有
**19008** 字节 —— 两者不一致，说明"槽里到底是什么布局"这件事目前没有对齐。
如果槽内容布局/长度不对，帧间预测就会整体略偏（与 I-5 的 0/396 块一致、均差
约 19 的形态吻合）。**但本机自造流（含 alt-ref、intra-only 首帧）全部正确**，
所以这条要么只在"三张不同参考图"时暴露，要么与官方流的其它细节叠加，需要进一步
用同一次运行的正对照验证（下一步：把"参考槽内容"与"PP 输出"做同帧对照，
并用本机 keyframe + inter 流确认槽内容是否本来就该等于显示帧）。

**sub3 的真实参考槽（2026-11-11，驱动侧实测）**：在驱动里临时打印解析结果后确认
官方 sub3 是 `refs=0,1,2 map=0,1,2`（`active_ref_idx` 三个值分别是 slot 0/1/2），
即**三路参考指向三张不同的图**（sub0/sub1/sub2）。之前探针打印 `0,0,0` 是因为
探针每次都从空的 `ref_frame_map` 起解析（驱动是跨帧维护的）。这条推翻了
"三路同指 slot 0"的判断，也使"inter 帧用多张不同参考"成为 I-5 最可疑的方向。
本机自造的样本（首帧 intra-only、alt-ref 隐藏帧、30 帧 inter、`--lag-in-frames=8`
+ `--auto-alt-ref=1`）全部逐帧一致，**唯独"三个参考槽装着三张不同的图"这个组合
还没造出来**（本机编码器产不出"三个 intra-only 帧轮流刷新 slot 0/1/2"的结构）。

**探针精度问题（2026-11-11）**：`tests/vp9-intraonly/vp9-parse-probe` 调
`mwv207_vp9_parse_frame()` 时把 `saved_ctx` 传 **NULL**，而驱动实际传的是
`st->saved_ctx`（已自适应的上下文数组）。两者会走不同的参考槽解析路径：
探针对 sub3 打印 `active_ref_idx=0,0,0`（三个参考都当 slot 0），但驱动在只解过
sub0 的流上却报 `reference 1 (slot 1) has no decoded picture` ⇒ sub3 的第二个
参考实际落在 slot 1。结论：**之前用探针得出的"三路同指 slot 0"不可信**，
它把"官方 sub3 只用一张参考图"这个判断带偏了；后续要用探针给出的结论，必须先
让它按真实 `saved_ctx` 解析（或在驱动侧把解析结果打印出来）。

补充（同一轮的否定实验）：给解析后的 `sh->frame_context_idx` 强行赋值
（0/1/2/3）再解同一个流，va3 的最好匹配分别变成 40.3/83.9/83.9/83.9，都比
自然运行的 15.4 差 —— 但这条实验本身不成立：`frame_context_idx` 在解析之后
才被改，而概率更新已经写进了原本的上下文，且自适应会把结果写进被强制的那个
槽，后续帧被污染。要做这个判定必须在**解析之前**换基上下文（例如把
`saved_ctx[k]` 拷到 `saved_ctx[sh->frame_context_idx]` 再解析），本轮未做。

复跑：`LIBVA_DRIVERS_PATH=<构建目录> LIBVA_DRIVER_NAME=mwv207 vadec <向量.ivf> out.yuv`
与 `ffmpeg -c:v libvpx-vp9 -i <向量.webm> -pix_fmt nv12 -f rawvideo` 比对；注意
ffmpeg 不输出隐藏帧，逐帧索引不能直接对位（要按 show_existing/可见帧对齐）。

**I-5 已修（2026-11-11，提交 `8f6e964`）**。最终定位到三处与 libvpx 语义不符：

1. **`vp9_setup_past_independence()` 漏了 intra-only**。libvpx 的调用条件是
   `frame_is_intra_only(frame_type) || error_resilient_mode`，它对**非关键帧的 I 帧**
   同样把段特征、`lf_ref_delta`/`lf_mode_delta` 清回默认，把 coef/mode/mv 概率表恢复
   成默认值并按 `reset_frame_context` 广播到各 frame context，**最后强制
   `frame_context_idx = 0`**。我们原来只在 `key_frame || error_resilient ||
   reset_frame_context == 3` 时做，intra-only 漏掉，紧随其后的 inter 帧于是从上一
   GOP 的概率表和 lf delta 继承，重建结果逐 64×64 块偏掉（与观测到的 0/396 块完全
   一致、均差约 19 的形态吻合）。
2. **自适应基准取错**。libvpx 的 `adapt_coef_probs()` 以
   `cm->frame_contexts[frame_context_idx]` 为基准做 `merge_probs()`，而该槽在上面那
   步刚被重置成默认表；我们固定用 `pre_ctx`（帧头更新前的保存表）。现在
   key/intra-only/error-resilient 帧改用 `default_entropy` 作基准。
3. **`show_existing_frame` 污染概率上下文**。解析器在未压缩头里遇到 `show_existing`
   就返回 `MWV207_OK`，但 `mwv207_vp9_parse_frame()` 见 `ret == 0` 后继续把补零缓冲
   当压缩头解码，函数末尾执行 `saved_ctx[frame_context_idx] = sh->entropy`，用垃圾覆盖
   保存的概率表；同时重放帧不解码却不清系数计数器，下一帧的 `merge_probs()` 会把上一
   帧的旧统计当成自己的。libvpx 遇到 `show_existing` 直接返回，既不动
   `frame_contexts` 也不上传统计。现在解析器在 `show_existing` 处立即返回、
   `vp9_adapt_probs()` 跳过重放帧、重放分支清零 `VP9_COUNTER_OFFSET`。

定位手段（可复跑）：`tests/vaapi-client/vadec.c` 直连 libva 消除 ffmpeg 变量；先找
"最小可复现"——`/tmp/v5/r1.ivf`（在正确解码的 `r0.ivf` 中间插入**一个** 1 字节重放包）
就足以让下一帧变错；再用三组临时 dump 逐层排除：寄存器（`mwv207_debug=video` 的两份
`mwv207_video_reg.dump` **6 个块逐块 0 差**）、上传的概率表（同帧差 300/1872 个 u16）、
`saved_ctx`（重放前后差 397 字节），最后落到解析器多走了一段。

验证（dev 驱动，与 libvpx 逐字节比对）：

| 样本 | 修前 | 修后 |
|---|---|---|
| 官方 `vp90-2-16-intra-only.webm`（10 帧输出） | 真实解码帧 4/4 DIFF（15.43 / 15.46 / 22.34 / 14.79） | **7 个真实解码帧全部 EXACT** |
| 本机 352×288 20 帧、每 5 帧一个关键帧、`-frame-parallel 0` | 12 帧 DIFF（第 2 个 GOP 起） | **20/20 EXACT** |
| `tools/vp9-regression.sh` | 18/18 | 18/18 |
| H.264 High 420 / HEVC 8bit / Main10 / VP9 Profile 2 10bit / query | — | 全 IDENTICAL / 4·0 |

**随 r23 装到安装栈后的复核（2026-11-11 12:08）**：`switch-mwv207.sh umd status` 显示
活动栈 `oss-24.1.2-fullstack-r23`（DRI `8f52e9972ffb78f9949f0a92c07b6c61`、VA
`e2ddc5bb769a7bd7d54d73e78a90ae69`）；`sudo ./tests/verify-installed.sh
8DFC520FB8FCDECFBC911AA --bad-submit` → **PASS=25 FAIL=0 SKIP=1**（含 N14 探针、畸形
提交、H.264/HEVC/VP9 P2 解码逐位一致）；**官方 intra-only 样本走完整 ffmpeg 路径**
（`ffmpeg -hwaccel vaapi -hwaccel_output_format vaapi -i vp90-2-16-intra-only.webm -vf
hwdownload,format=nv12 -pix_fmt yuv420p`）与 `-c:v libvpx-vp9` 逐帧比对：
**7 帧全部 IDENTICAL（0 差异帧）**；同批 `vp90-2-10-show-existing-frame.webm`（6 个 inter 隐藏帧
+ 6 个单字节重放）**整文件 2585088 B 全等**；`tools/vp9-regression.sh` 18/18；
`tests/vp9-resize/run-vp9-resize.sh --local /tmp/vp9b2` → `vp90-2-14 … IDENTICAL 跳过帧=0`、
`vp90-2-21 … SKIPPED-AS-ERROR 跳过帧=9`。

**仍然存在的独立限制**：官方流里 3 个 `show_existing` 重放帧（`va5/6/7`）仍是黑图，
原因见上文"整条解码+PPU 由 `JMDEC_DEC_E` 启动"，与解码正确性无关；`tests/vp9-intraonly/`
的判据据此分两段（解码帧必须 EXACT、重放帧单列）。

---

## 2. KMD — 未修复缺陷

> 行号以 **6.6 树** `src/kernel-6.6/` 为准（与 5.10 树的行号不同）。
> **自建 KMD 已经在跑**（`srcversion 5A5B401802B79D560151210`，见 §0），
> 所以下表全部是**真实存在于当前系统上**的未修缺陷，不是"待部署"的纸上问题。
> 下表之外，K1–K8/K10、N15、N14、N1/N2/K12/K9-②/N9 已修好并在运行
> （N14 见 §2.1、`6502f19` 一批见 §2.2，均已于 2026-11-10 重启后验证）；
> **N4/K11 已修（`fbed27f`，见 §2.3，2026-11-10 重启后验证生效）**；K9-① 经与厂商源码对照
> 确认**不是缺陷**（见 §2.3），N10 决定只文档化。

| # | 级别 | 位置 | 问题 |
|---|---|---|---|
| **N10** | 低 | `dc/mwv207_hdmi.c:908-918` `mode_valid` | 6.6 树的判据是 `clock > 594000` → `MODE_CLOCK_HIGH`，另对非 HDMI sink 加 `clock >= 165000` 限制；`phy_configure` 恒传 `bpp=8` 使 PHY 表 10/12 bpp 项不可达。**实测本机 HDMI-A-1 走 1920x1080@100Hz**，未受影响。⚠ 原记录的"384–594 MHz 全被拒"出自 **5.10 树**，6.6 树判据已不同（阈值 594000，非 384000）。**决定只文档化**：现有阈值只挡 6K/8K 与深色非 HDMI sink，改判据需要显示侧实测，收益极低（见 §2.3） |
| 其它 | — | 全树 | `BUG_ON`（不含 `BUILD_BUG_ON`）仍有 **20 处** |

> **已修好并在运行**：K1（flags 位运算）、K3（copy_to_user 返回）、
> K4（sort 比较符）、K5（bsearch 记录初始化）、K6/K7（copy_from_user 错误路径 +
> `engine_id` 的 u32 判据）、K8（`irq_create_mapping` 检查与回退）、
> K10（`MWV207_MAX_SCHED_PER_ENTITY 6` 边界 + `BUG_ON` 改 `WARN_ON`）、
> softdep（`src/kernel-6.6/mwv207_drv.c:543`）。
> **刻意未移植 K2**：6.6 的 `ttm_bo_init_reserved()` 失败走 `err_put: ttm_bo_put(bo)`
> → `bo->destroy` = 本驱动 `mwv207_ttm_bo_destroy()`（内部已 release+free），再补一次是 double free。

### 2.1 N14：reset/destroy 时 signal 在飞 fence（已修，已重启生效，reset 通路已实测）

**提交 `22145e9`**（`src/kernel-6.6`），改法按 `docs/api/08-KMD.md:1067` 的规定
"reset 内对在飞 fence `dma_fence_set_error(-EIO)` + signal"：

| 管线 | 原实现 | 现实现 |
|---|---|---|
| 3D | `mwv207_pipe_3d_reset()` 清 `event_bitmap` 并 `dma_fence_put` 30 个 `event_fence`，**不 signal**；`destroy()` 不排空 | 新增 `pipe_3d_force_complete()`：`event_lock` 内快照 30 槽 + 清 bitmap，锁外 `set_error(-EIO)` + signal + put；`reset()` 先完成 fence 再复位硬件；`destroy()` 在 `free_irq()` 后调用 |
| 2D | `pipe_2d_reset_ring()` 遍历 `fence_queue` 只 `dma_fence_put`，**不 signal**；`destroy()` 不排空 | 新增 `pipe_2d_force_complete()`：锁内 `list_splice_init()` 到本地链表，锁外完成；`reset()` 在 `hw_init()` 之前调用（hw_init 失败也要完成）；`destroy()` 在 `free_irq()` 后调用 |
| DEC/ENC | 单槽 `current_fence`，`reset()` 只 `dma_fence_put`；`destroy()` 只 `kthread_stop` | 轮询线程与 reset 用 `xchg(&current_fence, NULL)` 抢槽（避免双重 put/UAF）；新增 `pipe_dec_force_complete()`/`pipe_enc_force_complete()`，reset/destroy 均调用 |

行为变化只在 reset/卸载路径：原本永远不完成的 job 现在以 `-EIO` 结束；
`drm_sched_stop()` 已摘除 job 回调，不会误触发 job done。

**部署状态（2026-11-10 已重启验证生效）**：`tools/dkms-sync.sh` 重建并安装到
6.6.155 与 6.6.143 两个内核；`switch-mwv207.sh kmd stage` 暂存到 `updates/dkms`
并刷新 initramfs（srcversion `D507FC2774D547E7BEBA182` → `039F972EFE3279A155EE225`）。
重启后实测 `/sys/module/mwv207/srcversion` = `039F972EFE3279A155EE225`，
`modinfo -n mwv207` 指向 `updates/dkms`，`dkms status` 两个内核均干净。

> **备注（DKMS 状态）**：`kmd stage` 是把仓库构建的 .ko 直接 `install` 到
> `/lib/modules/$KVER/updates/dkms/`，与 DKMS 自己构建/签名的副本内容不同，
> 因此 `dkms status` 会重新出现 `Diff between built and installed module!`。
> 已用 `dkms install -m mwv207-oss -v 1.0.0 -k 6.6.155-arm64-desktop-hwe --force`
> + `depmod -a` + `update-initramfs -u -k 6.6.155-arm64-desktop-hwe` 恢复干净
> 状态：两处 srcversion 均为 `039F…`，initramfs 内那份也已刷新为 `039F…`。
> 以后每次 `kmd stage` 后若要 `dkms status` 干净，重复这三步。

**重启后回归（2026-11-10 10:39–10:43）**：
- `glxinfo -B`：JingjiaMicro MWV207 / Mesa 24.1.2 / GL 4.0 / `Video memory: 2048MB`，accelerated yes；
- `glxgears` 117 FPS；
- `tests/query/run-query-tests.sh`（`DRI_DIR=/usr/local/lib/mwv207-oss`）4/4 PASS；
- `tools/vp9-regression.sh`（系统安装驱动）18/18 OK；
- `dmesg` 无新增 WARN/BUG/Call Trace；只有 boot 时的 out-of-tree 与未签名告警，
  外加一条与驱动无关的 `sched: RT throttling activated`（上一轮启动同样出现）。

**reset 通路实测（2026-11-10，`tests/n14-reset/`）**：用 GL pbuffer + 长循环
片元着色器制造 >2 s 的 3D 作业，触发 6 轮超时复位（`hang_limit` 5）；kprobe 记录
`mwv207_sched_timedout_job` 6 次、`pipe_3d_force_complete` 6 次（一一对应），
用户态 fence 在 12.3 s 后完成而非永久阻塞，复位后短作业与 `glxinfo`/`glxgears`
均正常，dmesg 无 WARN/BUG。详见 `tests/n14-reset/README.md`。

**残余验证缺口（如实记录）**：
- 读 6.6 `drm_sched` 源码可知，超时路径上 `drm_sched_stop()` 会摘掉 job 回调、
  `drm_sched_start(full_recovery=true)` 对无 parent 的 job 直接以 `-ECANCELED`
  完成，所以"reset 后用户态永久等待"这个旧场景**无法用修复前对照复现**；N14 的
  收益是 fence 状态正确（带错误信号而非被静默丢弃）、驱动内等待者不被挂住、
  卸载路径不留永远 unsignaled 的 fence。
- 2D / DEC / ENC 三条管线的 force_complete 与 destroy 路径未被探针覆盖
  （需分别让 2D/解码引擎挂死或在有在飞 fence 时卸载驱动，风险更高）。

### 2.2 调度器槽位/在飞上限/超时计数与显示侧 BUG_ON（已修，2026-11-10 重启后验证生效）

**提交 `6502f19`**（`src/kernel-6.6`，5 条一起改，都在"结构性/边界"这一类）：

| 项 | 级别 | 原问题 | 改法 |
|---|---|---|---|
| **N1** | 高 | `sched[6]` 只有 6 槽，拓扑要 7 个；`sched_enc = &sched_dec[1]` 与 dec1 撞槽 → enc0 scheduler 覆盖 dec1（dec1 的 scheduler+pipe 泄漏），`nr_dec` 仍报 2，`engine_id=1` 的 DEC 提交会进 ENC 调度器 | `mwv207.h` 加 `MWV207_MAX_SCHED_NR 7`，数组扩到 7；`mwv207_sched_init()` 改用显式下标（3d=0..1、dec=2..3、enc=4、2d=5、dma=6）；suspend/resume/fini 三处循环改用该宏 |
| **N2** | 中 | 3D `hw_submission=30` 等于 `event_bitmap` 槽数（30），满负荷时下一次 `pipe_3d_event_push()` 命中 `BUG_ON(event >= 30)` | 新增 `MWV207_3D_HW_SUBMISSION 28`（3d0/3d1 都用），留 2 槽余量 |
| **K12** | 中 | 超时路径用"遍历 `pending_list` 每个 job 记一次 `devfreq_record_idle`"补偿复位丢掉的减一；队列里的 job 未必都 `record_busy` → 多减 → `busy_count` 变负触发 `WARN_ON` | 新增 `mwv207_devfreq_reset_busy()`：复位后在 `devfreq_lock` 下把 `busy_count` 清零（复位语义＝无在飞 job），resubmit 的 job 重新 `record_busy` |
| **K9-②** | 中 | ISR 无锁读 `irq_enable_reg[]`，mask/unmask 在 `irq_lock` 下写 | 读写都改 `READ_ONCE`/`WRITE_ONCE`（含 `irq_suspend`/`irq_resume`）；不加锁以免在 ISR 里持锁调 `generic_handle_irq` |
| **N9** | 低 | 显示侧 `BUG_ON(va_irq == 0)`（`kms.c`）、`BUG_ON(hdmi->irq == 0)`（`hdmi.c`） | 改成 `pr_err` + `-ENODEV` 回退，与 3D/2D 的 K8 范式一致；`irq.c` 里引用旧 BUG_ON 的注释同步更新 |

**部署状态**：`tools/dkms-sync.sh` 重建并安装到 6.6.155；6.6.143 另外
`dkms build/install --force` + `depmod`；两个内核的 initramfs 都已
`update-initramfs -u`（解包核对 `updates/dkms/mwv207.ko` 的 srcversion 均为
`D5EEF36617A4D3FBF4AFD1D`），`dkms status` 两个内核均干净。2026-11-10 13:29
重启后运行中已是 `D5EEF36617A4D3FBF4AFD1D`，**验证结果见下方小节**。

> 注：`update-initramfs -u -k 6.6.143…` 第一次调用时 `update-initramfs`
> 自身 segfault（exit 139），目标文件未被改动（mtime 未变），重跑一次即成功；
> 已核对解包后的模块 srcversion。属工具偶发问题，与驱动无关。

**这一批为什么能改、怎么验证**：
- N1 只动槽位分配，不改变任何对外接口，UMD 的 `engine_id` 语义不变（所以不需要
  改 UMD）；`nr_*` 由"对应 sched 是否建成"推出，lite 机型上仍是 1/1/0/1/1
  （见下方验证）。重启后用 `tests/engines/run-engines.sh` 读
  `DRM_IOCTL_MWV207_INFO` 的引擎计数，并用 `engine_id=1` 的自写提交验证 DEC1
  走的是 dec1 而不是 enc0（这一条本机做不了，见残余缺口）。
- N2 只降低在飞上限，判据是"压测 28+ 个 3D 提交不再出现
  `kernel BUG at ...mwv207_pipe_3d.c` 的事件表满"；正常桌面负载在飞数远小于 28。
- K12 的判据：`tests/n14-reset/` 探针再跑一遍（它会连做 6 轮超时复位），
  修复前每轮都可能打 `WARNING ... busy_count`，修复后 `dmesg` 无该 WARN。
- K9-②/N9 属防御性改动，判据是编译 + 常规启动无回归；`BUG_ON` 一旦命中
  已是不可恢复，改后最多是 probe 失败退驱动，不再 panic。

**重启后验证（2026-11-10 13:29 重启，模块 `D5EEF36617A4D3FBF4AFD1D`）**：

| 项 | 证据 | 结论 |
|---|---|---|
| 部署 | `cat /sys/module/mwv207/srcversion` = `D5EE…`；`modinfo -n` 指向 `updates/dkms`；`dkms status -m mwv207-oss` 两内核 `installed`、无 `Diff between built and installed module!` | ✅ |
| 基线 | `glxinfo -B` → MWV207 / Mesa 24.1.2 / GL 4.0 / accelerated yes / 2048MB；`glxgears` 99.98 FPS | ✅ |
| GL/GLES | `DRI_DIR=/usr/local/lib/mwv207-oss tests/query/run-query-tests.sh` → PASS=4 FAIL=0 | ✅ |
| VP9 | `tools/vp9-regression.sh` → pass=18 fail=0 | ✅ |
| H.264 8bit | 640×360 30 帧 `libx264` 样片 → 与软件解码**逐位一致**，`timed out!` 计数 0 | ✅ |
| N14 reset 通路 | `tests/n14-reset/run-n14-reset.sh` → TDR 6 次、`pipe_3d_force_complete` 6 次、探针未阻塞 | ✅ |
| K12 | 上述 6 轮超时复位期间 `dmesg` 无 `busy_count`/`WARN`/`BUG`（只有一次无关的 `hwinfo` wireless 警告） | ✅（修复前该路径会打 `WARN_ON(--busy_count < 0)`） |
| N1 | 新增 `tests/engines/run-engines.sh`（经 `DRM_IOCTL_MWV207_INFO` 读引擎计数）→ `3d=1 dec=1 enc=0 2d=1 dma=1`，与 lite 机型能力一致 | ✅ |
| K9-② / N9 | 启动与全流程无 IRQ/显示侧报错，`BUG_ON` 未命中（改后为 `pr_err`+`-ENODEV`） | ✅（防御性，无正向触发场景） |

**这条验证带来一个重要事实**：本机（JM9100）是 **lite** 版 —— `jdev->lite` 由
`mwv207_drv.c:344` 读寄存器 `0x12c0000` 是否等于 `0x21` 决定，本机不等于，故
`mwv207_pipe_3d_create()`/`mwv207_pipe_dec_create()` 对 `unit > 0` 直接返回 NULL、
`mwv207_pipe_enc_create()` 也返回 NULL ⇒ **dec1 / enc0 / 3d1 的 pipe 根本不存在**。
因此 N1 的槽位撞车在 lite 机型上没有运行时可观测后果（`nr_dec` 之前也只有 1），
真正受影响的是非 lite 的完整 MWV207（2×3D、2×DEC、1×ENC）；这次修的是**布局正确性**，
不是本机的功能回归。新加的 `tests/engines/` 让这条以后可以一键核对。

**残余验证缺口**：① N2 的 `credit_limit=28` 无法直接观测，也没有做 28+ 在飞压测
（正常桌面在飞数远小于 28，`BUG_ON` 未命中即为间接证据）；② `engine_id=1` 的
DEC1 提交路径在 lite 机型上不存在，无法实测，只能靠非 lite 硬件或代码审查。

### 2.3 N4/K11 修复与 K9-①/N10 定案（2026-11-10，`fbed27f`；已重启验证）

| 项 | 原实现 | 现实现 |
|---|---|---|
| **N4 DMA 忙等 + stub fence** | `pipe_dma_commit()` 在 drm_sched 的 `run_job` 工作线程里 `cpu_relax()` 自旋最多 5 s 等引擎完成，`mwv207_pipe_dma_submit()` 最后返回 `dma_fence_get_stub()`（提交即已 signal） | `commit` 拆成 `pipe_dma_start()`/`pipe_dma_wait()`/`pipe_dma_clear_chan()`；submit 只编程 LLI 并启动引擎，分配真实 `dma_fence`（自有 `fence_ops`）经 xchg 放入单槽 `current_fence`，再 `queue_work` 到本管线自己的 `WQ_MEM_RECLAIM` 有序队列；worker 睡眠式轮询（前 200 次短自旋走快路径，之后 `usleep_range`，上限 **1500 ms**，故意小于调度器 2000 ms 超时）→ 清通道 → 失效目的端缓存 → xchg 抢槽 signal+put；`reset`/`destroy` 按 N14 范式 `cancel_work_sync()` + xchg 后以 `-EIO` 完成 |
| **K11 移除路径/中断窗口** | ① `mwv207_pci_remove()` 只做 `pci_clear_master(pdev)`，缺 `pci_disable_device(pdev)`；② `mwv207_irq_fini()` 直接 `free_irq()`，设备的使能位还在（`0x009A8020/0x24`），共享中断线上可能出一次 `nobody cared` | ① `pci_clear_master()` 之后补 `pci_disable_device()`（与厂商 `jmgpu_insert.c` 的 remove/probe 错误路径同序）；② `free_irq()` **之前**先把两路使能位清零、按 W1C 清掉 `0x009A802c/0x34` 的挂起状态，彻底关掉窗口。`clear_master` 仍留 teardown 末尾（与厂商一致，不再视为缺陷）；注释写明 `kms_fini()` 必须早于 `irq_fini()` 不能动 |
| **K9-① vblank 整字回写** | 记为疑点：`dc/mwv207_va.c` 里读 `0x9907f4` 后整字写回，被怀疑会误清 bit12–15 之外的状态 | **核实不是缺陷，不改**。厂商源码 `ref/vendor-1.8.0` 解出的 `jmgpu_package.c` 里 `j9_handle_attribute_commandery()`（第 1158 行）正是"读 `0x990000 + J9_MURRHAS`（= `0x9907f4`，`mwv207_driver.h:434`）→ 把读到的值原样写回"，随后按 `(mask >> 12) & 0xf` 逐 crtc `drm_crtc_handle_vblank()`；写回读值就是 W1C 状态寄存器的标准清位。唯一差别：厂商还处理 bit26–29（overlay），我们丢弃这些事件——本驱动没有 overlay 面，无用户可见影响 |
| **N10 HDMI `mode_valid`** | 记为低级别缺陷 | **决定只文档化，不改**：6.6 树阈值已是 594 MHz，只影响 6K/8K；`bpp=8` 硬编码使 PHY 的 10/12 bpp 项不可达，但改判据要显示侧实测，本机 1080p@100Hz 正常，收益极低 |

**判断依据（N4 为什么值得改）**：DMA 管线承载 TTM 的所有搬移，原来"自旋 5 s + stub
fence"意味着引擎一旦挂住，调度器工作线程被占死、fence 又永远显示"已完成"，问题会被
静默吞掉；改成真 fence 后，超时以 `-ETIMEDOUT`、复位以 `-EIO` 显式上报。
LLI 数组是复用的（`0x400000 / sizeof(lli)` 条），超过约 64 MB 的单次搬移会走中间批次，
那条路径仍必须同步等待（否则会覆盖引擎正在读的 LLI），代码里保留了
`pipe_dma_commit()` 并注明原因。

**部署状态（2026-11-10，已重启验证生效）**：`tools/dkms-sync.sh` 重建并安装到 6.6.155，
6.6.143 用 `dkms install --force` 补装；两内核 `dkms status` 均 `installed`、无 `Diff`
警告；两个 initramfs 都已刷新，解包核对
`updates/dkms/mwv207.ko` 的 srcversion 均为 **`0EFB7F4433EA5311B08E88C`**
（143 的 `update-initramfs` 又出现既有的 `dracut-install` segfault/exit 139，
重跑即成功，与上次同因，目标文件未被破坏）。**这个 139 与我们的驱动无关，已有证据**：
对应时刻的内核日志是 `note: cp[246066] exited with irqs disabled` + `preempt_count 1`
以及随后 idle 上的 `WARNING at kernel/context_tracking.c:128 ct_kernel_exit`，
而同参数的 `cp --reflink=auto --sparse=auto -fL` 在命令行手跑 3/3 成功 —— 属该内核
（`vfs_monitor(OE)` 等树外模块在场、`Tainted: D`）在 initramfs 工具链大批量搬文件时的
既有不稳定性，不是 GPU 管线触发。运行中已是 `0EFB7F4433EA5311B08E88C`（2026-11-10 14:36 重启）。

**一键验收**：`printf '666\n' | sudo -S ./tests/verify-installed.sh 0EFB7F4433EA5311B08E88C`
（`tests/verify-installed.sh`，2026-11-10 新增；`--no-n14` 可跳过 TDR 探针）——它把下面的
判据全部脚本化，含 srcversion 比对、`modinfo -n` 来源、glxinfo/query/引擎计数、
VAAPI profile 归位、三类解码逐位一致、High10 回退与 DEC 零超时、TDR 探针、
以及"只看本次运行新增"的 dmesg 告警检查（`kernel.dmesg_restrict=1` 时普通用户跑会
明确 SKIP 而不是假 PASS）。重启前在 `D5EE…` + r19 上预跑：普通用户
`PASS=18 FAIL=0 SKIP=3`、root `PASS=21 FAIL=0 SKIP=1`；重启后的正式结果是 22 项全过，
见下。

**重启后验证结果（2026-11-10 14:36，均实测通过）**：`/sys/module/mwv207/srcversion`
= `0EFB7F4433EA5311B08E88C`、`modinfo -n` 指向 `updates/dkms`；`glxinfo -B` MWV207/GL4.0/
accelerated yes；`glxgears` 102.7 FPS（vsync 同步）；query `PASS=4 FAIL=0`；
`tests/engines` `3d=1 dec=1 enc=0 2d=1 dma=1`；`tools/vp9-regression.sh` **18/18**；
H.264 High 420 / HEVC 8bit / HEVC Main10 **逐位一致**（这三类每次搬移都走 N4 改过的
异步完成路径）；H.264 High10 正确回退、DEC 零超时；`tests/n14-reset` PASS（TDR 6 次、
`force_complete` 6 次）；dmesg 无 `DMA timeout`/`WARN`/`BUG`/`nobody cared`，
也无 `WARN_ON(pipe->current_fence)` 命中；一键验收 `PASS=22 FAIL=0 SKIP=1`
（SKIP 为探针后预期的 6 条 `timed out`）。
**路径确实被走到（kprobe 计数）**：对 `pipe_dma_start` 与 `pipe_dma_complete_work`
挂 kprobe 后跑一次 HEVC Main10 解码，两者各命中 **45 次**（一一对应）——说明新异步
完成路径是真的在执行，不是被绕过或只跑同步兜底分支。

**原定判据**（保留作背景）：`cat /sys/module/mwv207/srcversion` 应为 `0EFB7F4433EA5311B08E88C`；
随后 `glxinfo -B`、`tests/query/run-query-tests.sh`（`DRI_DIR=/usr/local/lib/mwv207-oss`）、
`tools/vp9-regression.sh`、HEVC 8bit/Main10 与 H.264 High 逐位一致（**这三类测试的每一次
缓冲搬移都走这条改动过的路径，是 N4 的主要功能覆盖**）、`tests/engines/run-engines.sh`
仍为 `3d=1 dec=1 enc=0 2d=1 dma=1`、`tests/n14-reset/run-n14-reset.sh` 仍 PASS；
`dmesg | grep -iE "DMA timeout|WARN|BUG"` 应为空（`WARN_ON(pipe->current_fence)`
是本次新加的重入防御，命中即说明调度器串行假设被破坏）。

**残余验证缺口**：① 异步完成路径已随 2026-11-10 14:36 重启验证（见上），但只在
桌面常规负载下走过，未做高吞吐压力；② 没有构造"DMA 引擎真的挂住"的场景（需要让引擎停在半路，风险高），
`-ETIMEDOUT` 分支只有代码审查；③ 中间批次（>64 MB 搬移）仍走同步等待，未单独压测。

### 2.4 20 处 `BUG_ON` 收敛为可恢复错误（2026-11-10，`53b3229`；已重启验证）

**判断**：这些 `BUG_ON` 里有一部分在**用户态提交路径**上（`mem_type` 非 VRAM/TT、
`reloc->idx` 越界、DMA `cmd_size` 不符、3D 事件槽耗尽），一旦触发就是整机 panic；
其余是 ring 描述符/寄存器访问器的内部不变量，panic 只会把可恢复的驱动 bug 变成死机。
统一原则：能返回错误的返回 `-EINVAL`/`-EIO`/`-EBUSY`；不能返回的改
`WARN_ON_ONCE` + 跳过该动作，让受影响的 job 由 2 s TDR 回收（N14 已保证在飞 fence
会被 signal）。

| 位置 | 触发条件 | 改法 |
|---|---|---|
| `mwv207_submit.c` patch_dma_entry default | BO 的 `mem_type` 既非 VRAM 也非 TT | `-EINVAL`（原 `BUG_ON(1)`） |
| `mwv207_ttm.c` evict_flags | `placements[0/1]` 不是 VRAM/TT | `WARN_ON_ONCE` + 放弃本次 evict 特化 |
| `mwv207_bo.c` placement_from_domain | `c > 3`（不可达） | `WARN_ON_ONCE` + 夹紧到 `ARRAY_SIZE` |
| `mwv207.h` `jdev_write`/`jdev_modify` | `reg <= 0x100000`（把 VRAM 地址当寄存器） | `WARN_ON_ONCE` + 丢弃；modify 把检查提到读之前 |
| `dc/mwv207_vi.h` `output_read/write/modify` | `reg >= 0x8000` | read 返回 0、write/modify 直接返回 |
| `mwv207_sched.h` `mwv207_job_bo` | `idx` 越界 | 返回 NULL，4 个调用点补 NULL 检查（dma reloc 路径另补 `reloc->idx` 上界） |
| `mwv207_pipe_2d.c` `pipe_2d_start` | 重置后 ring 跨度 ≠ 32 | `WARN_ON_ONCE` + 停止后续写入 |
| `mwv207_pipe_2d.c` submit | 跨度非 16 对齐 / > `size` | `WARN_ON_ONCE` + `-EIO`（tail 不回滚，fence 由 ISR/reset 回收） |
| `mwv207_pipe_3d.c` `pipe_3d_event_push` | 30 个事件槽全在飞 | 返回 -1，调用点回滚 tail + 释放本地引用 + `-EBUSY` |
| `mwv207_pipe_3d.c` link/wait/wtol/loadstate/stall | ring 越界/对齐 | `WARN_ON_ONCE` + 跳过该描述符 |
| `mwv207_pipe_3d.c` submit | 跨度 > `size` | `WARN_ON_ONCE` + `-EIO` |
| `mwv207_pipe_dma.c` `cursor_advance` | 越过行尾 | len 夹紧到行尾（保证推进、不死循环） |
| `mwv207_pipe_dma.c` submit | `cmd_size` 不符 | `-EINVAL` |
| `dc/mwv207_va.c` | crtc 索引 ≠ `va->idx` | `WARN_ON_ONCE` 后继续 |

**部署状态**：最终部署版本是 `5A5B401802B79D560151210`（`53b3229` + 随后的 N16
`c01af40`，见 §2.5），两内核与两个 initramfs 均已解包核对为同一 srcversion
（143 的 `update-initramfs` 仍偶发 exit 139，重试后成功，属既有内核问题，见 §2.3
的证据链）。2026-11-10 16:21 重启后**运行中即是该版本，已验证生效**。

**重启后判据**：`/sys/module/mwv207/srcversion` 应为 `5A5B401802B79D560151210`；
`sudo ./tests/verify-installed.sh 5A5B401802B79D560151210 --bad-submit` 全过
（该脚本覆盖 GL/引擎/VAAPI profile/三类解码逐位一致/TDR 探针/畸形提交/dmesg 告警）；
重点确认 `glxgears`、`tests/query`、`vp9-regression.sh` 与重启前一致——本批只动错误
路径，**正常路径的行为必须完全相同**。

**2026-11-10 16:21 重启后的实测**：`PASS=25 FAIL=0 SKIP=1`（SKIP 是 N14 探针那 6 条
预期 `timed out`）；`tests/bad-submit` 9/9 用例全部干净返回
`ENODEV`/`EINVAL`/`E2BIG`/`ENOENT` 且末段 INFO 正常，`dmesg | grep -cE "Oops|NULL pointer"`
为 0；glxinfo/query/vp9-regression 与重启前一致。

**残余缺口**：① 3D ring 越界时"跳过描述符"依赖 TDR 回收，实际场景未构造；
② `WARN_ON_ONCE` 意味着同一位置只报一次，长期运行的 dmesg 里不会重复，
但也更难统计频率。（原先"没有构造真实 malformed 提交"这条缺口已由 §2.5 的
`tests/bad-submit/` 补上——而且它第一次跑就抓到一个真正会 oops 的缺陷。）

### 2.5 N16：`job_fini` 对未分配的 `cmd_dats` 做空指针解引用（2026-11-10，`c01af40`；已重启验证）

**怎么发现的**：写 §2.4 的配套测试 `tests/bad-submit/`（畸形 submit 全被拒 +
驱动仍可用）时，第 9 项"`nr_cmd_dat=1` 但 `cmd_dats=NULL`"直接把内核打出了 oops：

```
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000010
Internal error: Oops: 0000000096000004 [#11] PREEMPT SMP
pc : mwv207_job_fini+0x12c/0x25c [mwv207]
Call trace:
 mwv207_job_fini+0x12c/0x25c [mwv207]
 mwv207_job_put+0x78/0xa0 [mwv207]
 mwv207_submit_ioctl+0x1e8/0x31c [mwv207]
```

**根因**：`mwv207_submit_init_job()`（`src/kernel-6.6/mwv207_submit.c`）一进门就把
`mjob->nr_cmd_dat = args->nr_cmd_dat` 赋了值，而 `mjob->cmd_dats` 数组要等
`mwv207_submit_init_job_cmd_dat()` 里 `kvcalloc()` 成功才有内容；那个函数中途
报错返回（`cmd_dats == NULL`、`dat_size <= 0`、`nr_relocs != 0 && relocs == NULL`
等）时，`mwv207_job_fini()` 的 `if (mjob->nr_cmd_dat) { dat = &mjob->cmd_dats[i]; }`
会拿 NULL 基址 + 偏移 → 空指针解引用。

**影响**：任何本地进程（有 `renderD128` 权限即可，通常就是桌面用户）一次畸形
`DRM_IOCTL_MWV207_SUBMIT` 就能让内核 oops。虽然 oops 后进程被杀、系统通常继续跑，
但内核处于不确定状态（本次连续触发两次 `[#11]`/`[#12]`，`Tainted: G D W OE`）。

**修法**（`c01af40`）：条件改成 `if (mjob->nr_cmd_dat && mjob->cmd_dats)`；
数组为空时后面的 `kvfree(mjob->cmd_dats)` 本来就安全（`kvfree(NULL)` 允许），
其余清理项（`relocs`/`cmds`/`mtvb`）同理。

**验证**：干净编译，srcversion `5A5B401802B79D560151210`（与 §2.4 同批部署）。
2026-11-10 16:21 重启后实测：`tests/bad-submit/run-badsubmit.sh` → 9/9 `ok`
（第 9 项返回 `EINVAL`），末段 INFO 正常；`dmesg | grep -cE "Oops|NULL pointer"`
为 0；`tests/verify-installed.sh … --bad-submit` 全过 `PASS=25 FAIL=0 SKIP=1`。
对比：修前同一用例在 `0EFB7F44…` 上直接打出 `[#11]`/`[#12]` 两次 oops。

**附带产物**：`tests/bad-submit/badsubmit.c`（9 个用例：未知 `engine_type`、
`engine_id` 越界、`cmd_size` 为 0/超限、`cmds` 空、`bos` 里 handle 不存在、
`nr_relocs>0` 而 `relocs` 空、`reloc.idx` 越界、`nr_cmd_dat>0` 而 `cmd_dats` 空）
+ `run-badsubmit.sh`；已作为 `tests/verify-installed.sh --bad-submit` 的可选步骤
接入（**默认不跑**：老模块没有 N16 修复时跑它会 oops 内核）。

### 2.6 N17：DMA 完成 fence 的引用顺序错导致 UAF（2026-11-10，`1fc9b21`；已重启验证生效）

**怎么发现的**：§2.3 的 N4 把 DMA 提交从"忙等 5 s + stub fence"改成"启动后立刻返回真
fence、完成与 invalidate 交给 workqueue"，构建出 `5A5B4018…` 并在 2026-11-10 16:21 重启。
之后本机在**普通桌面负载**下（16:41 前后，只是桌面合成器/窗口的 GEM 搬移）内核连续打出
三条引用计数告警，第一条栈直接落在这次改动的代码上：

```
refcount_t: addition on 0; use-after-free.
CPU: 2 PID: 321 Comm: mwv207_dma0 Tainted: G  OE
Call trace:
 refcount_warn_saturate+0xd0/0x14c
 mwv207_pipe_dma_submit+0x77c/0x788 [mwv207]
 mwv207_sched_run_job+0x40/0xb4 [mwv207]
 drm_sched_main+0x19c/0x428 [gpu_sched]

refcount_t: saturated; leaking memory.        （drm_sched_fence_scheduled）
refcount_t: underflow; use-after-free.        （drm_sched_fence_scheduled）
```

**根因**：`mwv207_pipe_dma_submit()` 的顺序是
`pipe->current_fence = fence; queue_work(...); return dma_fence_get(fence);`。
`pipe_dma_fence_alloc()` 用 `dma_fence_init()` 给出初始引用（=1），这份引用算作"管线
自留"，而 worker 完成时会 `dma_fence_signal()` 之后 `dma_fence_put()` → 引用数归零、fence
被 `kfree_rcu` 回收。**如果 worker 恰好在 `queue_work()` 返回与 `dma_fence_get()` 之间
跑完**（DMA 搬移通常只有几十微秒，这个窗口很宽），`dma_fence_get()` 就在已释放对象上做
`refcount_inc()` —— 正是日志里的 `addition on 0; use-after-free`，随后 s_fence 的
`saturated`/`underflow` 是连锁污染。

**后果**：轻则引用计数被写坏、本该释放的对象泄漏（"saturated; leaking memory"）；重则
调度器拿到一个已经被回收的 parent fence —— `drm_sched_job_done_cb` 挂到已释放对象上，
job 永远收不到完成通知，该管线看起来"卡住"，等它的用户态进程（例如 Xorg 的 GEM/TTM
搬移、合成器提交）会停在不可中断等待里。

**与 16:30 那次整屏卡死的关系**：卡死那轮（boot `-1`，16:19:35–16:36:46）的内核日志里
**没有** oops/TDR/hung task，也没有这三条 refcount 告警，只有日志在 16:30:02 之后完全
停止；`lightdm.log` 显示 16:36:14 收到 SIGTERM 后给 X 发 SIGTERM，X 5 s 内没有退出、
被 SIGKILL（`[+1004.06s] Sending signal 9 to process 1125`），随后用户硬重启。**这个
症状（用户态永久等在驱动里、内核无告警）与 N17 的"parent fence 被回收导致 job 永不
完成"一致**，但当时没有打到 refcount 断言，所以只能算最可疑的单一解释，不能算实证。

**修法**（`1fc9b21`）：把给 `drm_sched` 的那份引用提到 `queue_work()` **之前**取
（`ref = dma_fence_get(fence);` → 存 `current_fence` → `queue_work` → `return ref`），
引用计数变成 2（初始 + 返回），worker put 后剩 1，调度器 put 后归零，任何时机都不再
碰到已释放对象。另把 `queue_work()` 的返回值纳入检查（串行假设下不该失败，失败时只记
一条 rate-limited 告警，工作项仍在队列里会来收这个 fence）。

**部署状态**：新模块 srcversion `8DFC520FB8FCDECFBC911AA`，两内核 `installed`、两个
initramfs 已刷并解包核对（`modinfo -F srcversion` 均为 `8DFC52…`）。

**重启后验证（2026-11-10 17:55，已通过）**：

1. `/sys/module/mwv207/srcversion` = `8DFC520FB8FCDECFBC911AA`，`modinfo -n` 指向
   `updates/dkms`；
2. `sudo ./tests/verify-installed.sh 8DFC520FB8FCDECFBC911AA --bad-submit`
   → **PASS=25 FAIL=0 SKIP=1**（SKIP 是 TDR 探针后预期的 6 条 `timed out`）；
3. **引用计数平衡（kprobe 实测）**：给 `mwv207_pipe_dma_submit` 与
   `pipe_dma_complete_work` 各挂一个 kprobe，用 4 路并发 1080p `tests/swap-thrash`
   （每路 178 FPS / 1.48 GB/s）+ `glmark2` 压 2 分钟，命中数 **submit=2052、
   complete_work=2052、差 0**；全程 `dmesg | grep -c refcount_t` = **0**，无
   `BUG/WARNING/Oops/timed out/nobody cared`。旧模块（`5A5B4018…`）在同一桌面负载下
   209 秒就打出 3 条 refcount 告警，对比明确；
4. 负载期间每 0.5 s 探一次 `xdpyinfo`：**最大延迟 5 ms**，X 全程正常响应。

**残余缺口**：① `pipe_dma_force_complete()` 与 worker 的竞态只有代码审查
（`cancel_work_sync` + `xchg` 抢槽），没有构造 reset 与之并发的实测；②
`pipe->pending_cmd` 的读写依赖"调度器串行 + worker 已在 cancel_work_sync 后结束"这条
既有假设，若将来放开并发提交需重新审视；③ lightdm 重启能否正常退出 Xorg 未复测
（会在当前会话下杀掉本 agent，需用户在自己终端验证）。

**残余缺口**：worker 与 `force_complete()` 的竞态（`cancel_work_sync` + `xchg`）只有代码
审查，没有做故障注入；`pending_cmd` 在"worker 还没来得及读、新的 submit 已覆盖"这条
理论路径上仍是裸写（调度器按引擎串行，实际上到不了）。

---

---

## 3. 未实现功能（新功能，不是修缺陷）

| # | 目标 | 可行性 | 状态 / 缺口 |
|---|---|---|---|
| **D1** | **VP9 解码** | **已完成：Profile0 + Profile2 均落地** | **Profile0 8bit** 已实现并上机验证（`mwv207_dec_vp9.c` 的切片/bool 解析 + `mwv207_video_dec.c` 的 `mwv207_dec_vp9_set_regs()` 分支；`tools/vp9-regression.sh` 18/18 逐位一致）。**Profile2（10bit 4:2:0）** 2026-11-10 完成（`82e7b02`，见 §1.6）：4 组样片与软件解码逐位一致、零 TDR。Profile1/3（4:2:2/4:4:4）定案不可实现——解码器没有色度采样格式选择字段 |
| ~~D2~~ | ~~**H264 4:2:2 / 4:4:4**~~ | **已定案（2026-11-10）：不可实现** | 原 I-3 二选一已终结 —— 硬件只有 4:0:0/4:2:0，已收回声明（`a5a938b`），见 §1.5；`chroma_format_idc` 硬置保持 1，另加 `chroma_format_idc>1` 即跳过解码的防线 |
| **D3** | **编码（H264/HEVC）** | 大（从零） | UMD 侧 `base.encode_bitstream` **从未安装**（全树 0 处）；KMD 有 `mwv207_pipe_enc.c`。硬件侧有 HEVC 编码证据（闭源串含 `jmenc/source/hevc/hevcencapi.c`） |
| **D4** | 4 个未实现的扩展 opcode：`WAIT_GROUP_EVENT`(0x11) / `TD_OP`(0x12) / `TEX_INVALIDATE`(0x14) / `PRE_NORM`(0x15) | 小-中 | 厂商枚举齐全（`MWV207REG_SH_EXTENDED_OPCODE`），需逐个确定语义 |
| **D5** | 开启三个窥孔 `OPT_DIV` / `OPT_FFMA` / `OPT_SET`（默认全关） | 小改动 + 大验证 | 潜在性能收益，需全量着色器回归 |
| **D6** | 多视口 | 与 E8 同 | 取决于硬件是否支持（`GL_MAX_VIEWPORTS` 现报 0/1） |
| **D7** | **Vulkan** | 不可达 | 官方 1.8.0 **也没有**（无 libvulkan / `vulkan/icd.d`；KMD 的 syncobj/vm_bind/gpuva/sparse 全 0）。官网"Vulkan 1.1"至今未交付 |

---

## 4. 信息 / 硬件边界（不该承诺修好）

| # | 条目 | 判定 | 可做的最优动作 |
|---|---|---|---|
| **C-1** | 16 位采样恒 0（同 1.3 的 C1） | **已定位（2026-11-03）**：0x1F76 的 `TX_EXT_CONFIG.FORMAT` 码 `0x32–0x36` 解码返回 0；16 位整数码 `24–26`、浮点码 `7–9` 正常，上传/读回字节精确 | 保留声明并文档注明（A），或降级声明（B）；反查字段表路线已走完（tiling/LINEAR、align、NORM、new-mode、bit23/31 全无效），无需再试 |
| **C-2** | 高级混合 8/15 | **硬件边界**，不可修 | 保留超额声明并注明（现状），或撤销 cap（丢 GLES 3.2） |
| **C-4** | ISA 常量头 `INST_OPCODE_*` / `INST_TYPE_*` 在本树无定义（`include/hw/*.h` 是空壳） | 部分可救 | **扩展**操作码已有厂商枚举（`SH_EXTENDED/CMPLX/IMG_ATOM/VISION_OPCODE`）→ 可据此生成 `INST_EXT_OPCODE_*`；**基础 ALU 操作码仍未找到厂商枚举** |
| **C-5** | 视频解码块 `word → 厂商名` 无法反查 | 数据源缺口 | 厂商 1392 条寄存器里**没有视频解码块**（`MWV207REG_DEC*` 只有 2D/3D 的 `DEC400`）。只能沿用驱动自己的三张表（1840 行，已全量还原） |
| **C-6** | 开源驱动 219 个命令寄存器号里 **49 个** 在厂商库中查不到（含 `0xE00–0xEFF` 段） | 部分可解 | 32 个已由闭源 HAL **函数名**给出语义推断（`0x191`=ProgramIndex、`0x401–0x404`=着色器计数、`0x594/0x595/0x596`=tile status 三件套、`0xE05`=绘制使能…）；**17 个闭源也不用**（`0x192/0x281/0x283/0x284/0x28B/0x301–0x306/0x502/0x507/0x509/0x50A/0x6D0/0xE13`）→ 标未确认 |
| ~~C-7~~ | `I-2`：`JMDEC_BITPLANE1_E` 与 `JMDEC_BLACKWHITE_E` **同占 word7 bit30** | **已查证并已修（2026-11-10，`a5a938b`）**：原厂 `{word,width,shift}` 表里 BITPLANE0/1/2 与 CABAC/BLACKWHITE/DIR_8X8_INFER 逐位同名同址（7/31、7/30、7/29），是**按 codec 复用的同一物理位**，不是笔误；H.264 路径末尾那句 `set_reg(BITPLANE1_E, 0)` 相当于取消单色模式，已删除 | — |

### 统计口径提醒（避免重蹈覆辙）

反编译提取寄存器时，宽松口径曾得出"413 个寄存器被 HAL 引用"，但零假设检验显示：
候选空间 55297、随机命中率 2.45%，实测命中 1165 次 vs 随机期望 367.5 → **信噪比仅 3.2×**
（约 1/3 可能是巧合）。发布数已收敛为最严口径 **379**（只取 `Load/Flush/Set/Write`
类函数内且 `v >= 0x18` 的立即数）。见 `ref/reverse-source/docs/03-REGISTERS.md` §3.1。
**教训：总量显著 ≠ 单条可信**，单条必须靠函数名 + 开源驱动自身用法双重印证。

---

## 5. 建议的推进顺序

| 批次 | 内容 | 前置 | 代价 |
|---|---|---|---|
| ~~**1**~~ | ~~1.1 全部（W6/W7/W12/E4）+ E9/E10 拼写 + F.3.2/F.3.3~~ | — | **已完成（2026-11-02）** |
| ~~**2**~~ | ~~E2 / E7 / O4 + GBM-Wayland `err=-13`~~ | — | **已完成（2026-11-02）** |
| ~~**2b**~~ | ~~E1（降级为 1 LSB，不改）/ E6（复核不改）/ E8（复核不改）/ E3（复核：不可达）/ O5（flush 失败路径已修）~~ | 每条配一个针对性探针 | **已完成（2026-11-03）**：O5 修（`4a19713`）；E1/E6/E8/E3 复核后不改 |
| **3** | 1.3 的 C1/C2/C3/I-3 | **C1/C2/C3 已拍板（2026-11-03）：全部取“保留声明 + 注明”**（用户要求“尽可能保留完整功能”）；**I-3 已定案（2026-11-10）**：用户先选“做真正的支持”，但寄存器/原厂驱动/libva 三重证据表明 4:2:2、4:4:4 在本机 H.264 上不可实现 → 已收回声明，见 §1.5 | C1/C2/C3 是取舍；I-3 是物理不可能 |
| ~~**4**~~ | ~~真机 `kmd install`（解锁全部 KMD 条目）~~ | — | **已完成（2026-11-02）**：自建模块已在运行（§0），§2 剩余条目均为真实待修缺陷 |
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
| 2026-11-10 | KMD：N17 —— DMA 异步提交在 `queue_work()` 之后才 `dma_fence_get()`，worker 抢跑时对已释放 fence 做 `refcount_inc`（`refcount_t: addition on 0; use-after-free`，PC 在 `mwv207_pipe_dma_submit`），等待者可能永远收不到 signal；与 16:30 整屏卡死症状一致 | `1fc9b21`（`src/kernel-6.6`；2026-11-10 17:55 重启后验证：`verify-installed.sh --bad-submit` PASS=25、kprobe 压测 2052·2052 平衡、`refcount_t` 告警 0） |
| 2026-11-10 | KMD：N16 —— `mwv207_job_fini()` 对未分配的 `cmd_dats` 空指针解引用（畸形 submit 可让内核 oops） | `c01af40`（`src/kernel-6.6`；与下面 `53b3229` 同批部署为 `5A5B4018…`，2026-11-10 16:21 重启后验证：9/9 畸形提交返回 errno、dmesg 零 oops）＋ 新增 `tests/bad-submit/` |
| 2026-11-10 | KMD：20 处 `BUG_ON` 收敛为可恢复错误（提交路径给 errno、内部不变量改 `WARN_ON_ONCE` + 跳过） | `53b3229`（`src/kernel-6.6`；两内核与两个 initramfs 已装 `5A5B4018…`，2026-11-10 16:21 重启后一键验收 `PASS=25 FAIL=0`） |
| 2026-11-10 | KMD：DMA 提交忙等 5 s + stub fence（N4）、移除路径缺 `pci_disable_device` 与 `free_irq` 前未在硬件侧关中断（K11） | `fbed27f`（`src/kernel-6.6`；2026-11-10 重启后验证生效，一键验收 22 项全过） |
| 2026-11-10 | KMD：sched 槽位撞车（N1）、3D 在飞上限撞事件表（N2）、超时路径 devfreq 多减（K12）、ISR 影子寄存器数据竞争（K9-②）、显示侧两处 `BUG_ON(irq == 0)`（N9） | `6502f19`（`src/kernel-6.6`；两内核已装、initramfs 已刷，重启后生效） |
| 2026-11-10 | KMD：3D/2D/DEC/ENC `reset`/`destroy` 对在飞 fence 只 `dma_fence_put` 不 signal（N14），2D `destroy` 不排空 `fence_queue` | `22145e9`（`src/kernel-6.6`；模块已暂存，重启后生效） |
| 2026-11-10 | UMD：HEVC `tile_buffer_size` 用位深当帧高（I-1），tile 缓冲按错误高度分配 | `385ed81`（`src/mwv207-umd`） |
| 2026-11-10 | UMD：收回 H.264 High422/High444 声明（不可实现，见 §1.5）＋ word7 bit30 复用导致的单色标志被清零（I-2/I-3） | `a5a938b`（`src/mwv207-umd`） |
| 2026-11-10 | UMD：收回 H.264 High10 声明（I-4：10bit 提交会挂死 DEC 管线，缺 high10p 位流预转换）＋ `end_frame` 拒绝高位深码流 | `4ecca0d`（`src/mwv207-umd`）；载荷 `payload-42`（r19）已重新生成并通过 `umd check`/预验证，**安装需 `sudo ./switch-mwv207.sh umd install`（会重启 lightdm）** |
| 2026-11-11 | UMD：VP9 跨尺寸参考第一版（B2）—— 删掉"参考尺寸必须等于当前帧"的跳过分支（硬件按每槽自己的几何 + 14 位 hscale/vscale 编程），保留"槽从未解码过就跳过"；staging 缓冲帧变大时释放重建。厂商 `jmgpu` 同向量逐字节正确；我方**待实测** | `351e6ed`（`src/mwv207-umd`）；判据 `tests/vp9-resize/run-vp9-resize.sh` |
| 2026-11-11 | UMD：VP9 跨尺寸参考真正生效（B2 完成）—— 参考槽副本在解码器重建时移交（`vp9_ref_carry`），跨尺寸 inter 帧不再被"槽空"挡住；`vp90-2-21` 由 `SKIPPED-AS-ERROR` 变为 **IDENTICAL 2592000 B、跳过 0**，其余 VP9 向量与 `fp0g5`/18 项回归全过 | `21a15db`（`src/mwv207-umd`）；判据 `tests/vp9-resize/run-vp9-resize.sh --local /home/admin/vvp` |
| 2026-11-11 | UMD：VP9 intra-only 参考帧之后 inter 帧解码错（**I-5 已修**）—— `vp9_setup_past_independence()` 漏 intra-only（概率上下文/lf delta 未重置、`frame_context_idx` 未归零）、自适应基准改用默认表、`show_existing` 不再拿补零缓冲当压缩头解析（污染 `saved_ctx`）并清零系数计数；官方 intra-only 向量的 7 个真实解码帧与 libvpx 逐字节一致 | `8f6e964`（`src/mwv207-umd`） |
| 2026-11-11 | UMD：VP9 `show_existing_frame` 能真正送到解码器（N18）—— 1 字节包不再被 `decode_bitstream()` 丢弃、解析器放宽到 `size >= 1` 并补零到 16 字节、`show_existing` 不再误标记为"跳过"；新增 `tests/vaapi-client/vadec.c` | `6225e97`（`src/mwv207-umd` ＋ `tests/vaapi-client`） |
| 2026-11-10 | UMD：被跳过的帧不再静默返回脏图（I-6）—— 跳过点置 `frame_skipped`，`end_frame()` 不提交并给桩 fence，新增 `get_decoder_fence()` 让 `vaSyncSurface()` 报错（跨尺寸参考一族现在显式失败） | `bb5cd9e`（`src/mwv207-umd`）＋ `tests/vp9-resize/run-vp9-resize.sh` 判据升级 |
| 2026-11-10 | UMD：VP9 Profile 2（10bit 4:2:0）硬解落地（A5）——放行 `profile==2 && bit_depth==10`、声明 `VP9Profile2` + P010 面、`mwv207_vp9_10bit_enabled()` 开关；4 组样片（64x64/640x360/720p60/1080p30，含 inter 帧）与软件解码逐位一致、零 TDR，VP9 回归 18/18 | `82e7b02`（`src/mwv207-umd`）；载荷 `payload-42` 升为 r20（DRI `39f38c6f…`、VA `fa63148d…`） |
| 2026-11-10 | Firefox 标签栏"微缩 chrome"：buffer 写入前等待 GPU（`mwv207_resource.c`） | `6ec1e17`（`src/mwv207-umd`） |
| 2026-11-10 | mesa：`loader_dri3_query_buffer_age()` 恒 0（我们不做交换后缓冲保留） | `248659c`（`ref/oh-mesa`，本地） |
| 2026-11-07 | Firefox 字形/图标撕裂：上传暂存 BO 延迟释放（`retired_staging` 环） | `f8f693a`（`src/mwv207-umd`） |
| 2026-11-06~10 | **VP9 Profile0 8bit 硬解全链路**（tile 列/行、量化、分段、show_existing、概率自适应、时间 MV、ALTREF、跨尺寸保护），回归 18/18 | `5c6d05e`→`f8f693a`（`docs/HANDOVER-VP9.md`） |
| 2026-11-03 | 校验器路径失效（迁出 oh-mesa 后符号链接不进 `os.walk`）：`check_coverage.py`、`validate_register_map.py` | `d056efc`（`docs/tools/`） |
| 2026-11-03 | A 类：E5 双面模板双重互换、F.3.1 反汇编 type 位序、F.4 三条健壮性、O5 视频路径失败 fence | `5dee412`（`src/mwv207-umd`） |
| 2026-11-03 | 10bit 解码面启用：P010/P016 在 `R16_UNORM`/`R16G16_UNORM` 可采样时接受（真机 Main10 硬解 + 导出逐位一致） | `f0c7605`（`src/mwv207-umd`） |
| 2026-11-03 | 批次 2b：`mwv207_flush()` 提交失败不再造"永远就绪"fence（返回 NULL，Mesa 前端判空）；故障注入探针通过 | `4a19713`（`src/mwv207-umd`） |
| 2026-11-03 | dual-source blend 编译期空视图解引用（火狐 GitHub 崩溃） | `40d5d76`（`src/mwv207-umd`） |
| 2026-11-02 | 批次 1/2 健壮性（W6/W7/W12/E4/E9/E10/F.3.2/F.3.3）+ GBM/Wayland `err=-13` | `841e300`（`src/mwv207-umd`） |
| 2026-11-02 | 条件渲染真实现（`render_condition` 钩子 + CPU 侧谓词判断，GL 4.0 恢复） | `216a96a`（`src/mwv207-umd`） |
| 2026-11-02 | KMD：零重定位提交未初始化 `drm_sched_job` 导致 `BUG_ON(!entity)` 内核崩溃（N15） | `28bc73d`（`src/kernel-6.6`） |
| 2026-11-02 | 分层：`src/mwv207-umd/` 独立存放适配代码，oh-mesa 只作参考 | 结构变更 |
| 2026-10-31 | 遮挡查询失真（按簇计数求和）、条件渲染 SIGSEGV（cap 回退） | `b2ab2c9` |
| 2026-10-30 | VRAM cap 单位（`1024<<20` → `1024`）、O2（bo_map 返回语义）、O3（每槽尾 100 字节未初始化上 GPU） | `5499360` |
| 2026-10-30 | GLES 3.2 + 鲁棒缓冲访问 + 16 位格式上传字节精确 | `cd90d9f` |
| 2026-10-31 | 工具：合并 UMD/KMD 切换脚本、新增 `all install` | `f4b1221`、`af15fda` |
| 2026-10-31 | 文档：`ref/reverse-source/docs/` 反编译五篇、`06-KNOWN-ISSUES` C4/C5 | `e63fcf3`、`fd31971` |

补丁已废止（2026-11-03，`patches/` 删除）：上述改动以 git 提交为唯一记录。

---

## 7. 相关文档

- 缺陷细节与推导：[api/06-KNOWN-ISSUES.md](api/06-KNOWN-ISSUES.md)
- 逐条可行性评估：[api/11-FIX-FEASIBILITY.md](api/11-FIX-FEASIBILITY.md)
- 反编译与寄存器：[ref/reverse-source/docs/README.md](ref/reverse-source/docs/README.md)、
  [ref/reverse-source/docs/05-OCCLUSION-QUERY.md](ref/reverse-source/docs/05-OCCLUSION-QUERY.md)
- KMD 6.6 移植：[../reports/KMD-6.6-PORT-REPORT.md](../reports/KMD-6.6-PORT-REPORT.md)
- 验证工具：`tests/verify-installed.sh`（**一键验收已安装栈**：KMD srcversion/来源、
  glxinfo/query/引擎计数、VAAPI profile 归位、H.264-High/HEVC 8/10bit 逐位一致、
  High10 回退且 DEC 零超时、TDR 探针、dmesg 新增告警；`sudo ./tests/verify-installed.sh <srcversion>`
  全跑，普通用户跑会自动跳过需要 root 的两项）、`tests/query/run-query-tests.sh`、
  `tests/engines/run-engines.sh`（引擎计数，N1）、
  `tests/n14-reset/run-n14-reset.sh`（reset 通路，N14/K12）、
  `tests/vp9-intraonly/run-vp9-intraonly.sh`（B3 诊断：官方 intra-only 向量硬件 vs libvpx
  逐帧比对，见 §1.7）、
  `tests/bad-submit/run-badsubmit.sh`（畸形提交防御，N16；**老模块上跑会让内核
  oops**，故默认不跑，只在确认模块含 `c01af40` 后用
  `tests/verify-installed.sh --bad-submit` 带上）、
  `tests/vaapi-client/vadec.c`（**不经 ffmpeg 的最小 VAAPI 解码客户端**：IVF 输入、
  NV12/P010 输出，读回走 `vaExportSurfaceHandle` + 逐 plane `mmap`；I-5/B2 取证用，
  `cc -O2 -o /tmp/vadec tests/vaapi-client/vadec.c -lva -lva-drm`）、`tests/run-all.sh`

## `umd install` 后屏幕卡死、必须手动重启 lightdm —— 已修（2026-11-11）

**现象**：14:43:02 执行 `sudo ./switch-mwv207.sh umd install`（载荷 r25），桌面直接卡在
最后一帧不动；用户手动 `systemctl restart lightdm` 之后恢复正常。

**日志证据**：

```
14:43:02.988  dde-session: X connection closed, logging out
14:43:13.201  systemd[1]: lightdm.service: State 'stop-sigterm' timed out. Killing.
14:43:13.203  systemd[1]: lightdm.service: Failed to kill control group ..., ignoring: Invalid argument
14:43:19.572  systemd[1]: lightdm.service: Failed with result 'timeout'.
14:44:39.770  lightdm[227894]: not first run gen_upgrade_check_config.sh   ← 用户手动拉起
```

**根因（两条，第二条是主因）**：

1. **顺序错**：`umd_install` 是"先 `umd_deploy`（会 `rm -rf` 安装目录再拷 11 个文件、
   82 MB 的 `libmwv207_dri.so` 在内）→ 再重启 lightdm"。已经加载的进程靠旧 inode 继续跑，
   但**拷贝窗口里的任何 lazy `dlopen` 都会撞上缺失/半拷贝的 .so**（Xorg 的 glamor、
   Qt/Chromium 的 EGL 都是按需加载），切换正好落在这个窗口里。
2. **`systemctl restart lightdm` 在本机不可靠**：`/usr/lib/systemd/system/lightdm.service`
   带 `TimeoutStopUSec=10s`，而 DDE 会话拆解（dde-session/kwin/dde-shell 一串 unit）要十几秒：
   systemd 在 10 s 时 SIGKILL 主进程 → `Failed to kill control group (EINVAL)` →
   `Failed with result 'timeout'`。此时 **`restart` 的 start 半边不会执行**，没有 greeter，
   屏幕就停在旧帧上。原来的 `|| die` 也救不了（die 之后照样没桌面）。
   （同一个 10 s 超时也解释了 2026-11-10 16:30 那次"整屏卡住、Xorg 不响应 SIGTERM"。）

**修法（`switch-mwv207.sh`，2026-11-11）**：

- 新增 `lightdm_ensure_stop_timeout()`：写 `/etc/systemd/system/lightdm.service.d/20-mwv207-switch.conf`
  （`[Service] TimeoutStopSec=40`）并 `daemon-reload`，让优雅停止有时间跑完（幂等，已在本机生效，
  `systemctl show lightdm -p TimeoutStopUSec` 现为 `40s`）。
- 新增 `lightdm_stop_wait()`：`stop --no-block` + 轮询最多 40 s，仍活着才 `kill -s SIGKILL`，
  最后 `reset-failed`（否则 failed 状态会挡住下一次 start）。
- 新增 `lightdm_start_wait()`：`start --no-block` + 轮询 60 s，起不来就 `reset-failed` 再拉一次。
- 新增 `lightdm_restart_recover()` = stop_wait + start_wait，替换脚本里三处裸
  `systemctl restart lightdm`（`umd_activate`、回滚路径、`umd_do_rollback`）。
- `umd_install` 改为**先停 lightdm 再部署**（停不下来也继续，只是退回"边跑边换"并记日志）。
- `stop_display()`/`start_display()`（`stack`/`kmd` 路径用的）也换成上面两个 helper，
  避免 `kmd install` 撞同一个 10 s 超时。

**未验证**：修复后的 `umd install` 需要用户下次装载荷时实测（会停一次 lightdm，
本会话跟着中断，属预期）。

## 火狐浏览器自身 UI 的文字错位（r8 起可见）—— 已修（2026-11-07）

- 现象：页面内容正常，但浏览器 chrome（标签标题、地址栏、导航按钮、会话恢复条、
  各处 16×16 图标）的文字/图标呈"字符被打乱、图标变成别的像素"状；同一驱动下
  `gfx.webrender.software=true`（SWGL）完全正常，llvmpipe 也正常。
- **根因：上传暂存 BO 的复用竞态。** `mwv207_transfer_unmap()` 的对齐路径把 CPU
  数据经 staging 资源 `blit` 进 tiled 纹理后，立刻 `pipe_resource_reference(&staging, NULL)`
  放回了 `pb_cache`；此时 GPU 还没执行完这次 blit，同一次绘制里下一次
  `glTexSubImage2D` 就可能拿到同一块内存并被 CPU 覆盖 → 纹理内容被"撕裂"。
  WebRender 用 `glTexSubImage2D` 频繁往字形图集（R8 2048×2048）和图像图集里打补丁，
  于是字形/图标看起来是"别的字符、别的像素"。因为缓冲复用时机依赖驱动上报的
  GL 版本（4.0 与 3.3 下 WR 的批处理节奏不同），症状会随版本/时序变化 —— 这也是
  此前一直误判成"UV 错、128 位取样错"的原因。
- 修法：新增 `mwv207_retire_staging()`（`mwv207_resource.c`）与上下文里的
  `retired_staging[16]` 环（`include/mwv207_context.h`），把 staging 的引用转交给环，
  环满时才 `mwv207_wait_resource()` 等待最老的一个再释放；`mwv207_context_destroy()`
  逐个等待并释放。没有引入全局 `glFinish`，`glxgears` 仍 110 FPS。
- 验证：Firefox chrome 文字/图标全部恢复（`使用百度搜索，或者输入网址`、`新标签页`、
  关闭 ×、导航箭头）；`gfx.webrender.software=false` 与 `=true` 两条路径一致；
  GL 探针（`aln2` R8 5×4、`mask1g` WR 蒙版链路）全对；VP9 回归 18/18；
  `switch-mwv207.sh umd verify` 全通过（载荷 `payload-38` / `24.1.2-fullstack-r14`）。
- 参考：`reports/FF-RENDER-20261102.md` 第 11.7 节（当时的错误假设已在此更正）。

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

## `present` 每帧多花 ~0.55 ms：根因是线性表面的采样带宽（性能，未修）

- 现象（2026-11-11，r25，`:1`）：`glmark2 -b build:duration=3` 我方 **1049 FPS（0.953 ms）**，
  厂商同条件 5982–14856 FPS（0.067–0.167 ms）。逐场景差距集中在"便宜帧"
  （`build`/`texture`/`conditionals` 4–10×），GPU 重场景只差 1.25–1.3×
  （`terrain` 105/155、`refract` 96/123）。
- **拆分开销**：同场景 `--off-screen` 3607 FPS（0.277 ms，无 present）→
  `--frame-end=finish` 2447 FPS（0.409 ms）→ 默认 swap 1049 FPS（0.953 ms）
  ⇒ **present ≈0.55 ms/帧，占整帧 58%**。
- **根因（已定位，三层证据）**：
  1. 纯 `glXSwapBuffers`（不画）800×600 就要 **0.653–0.661 ms**，
     1920×1080 要 2.304 ms；1×1 时 0.125 ms ⇒ 固定开销 0.11 ms + 与像素成正比的拷贝。
  2. `qdbus org.kde.KWin /Compositor suspend` 前后 **0.770 → 0.767 ms（无变化）**，
     且 `org.kde.kwin.Compositing.active` = **false** ⇒ 成本在 **X 的 glamor**
     合成拷贝，不在 kwin；CPU 账也不支持 CPU 拷贝（Xorg 0.057 ms/帧、客户端 0.026）。
  3. GPU 能力实测：`glBlitFramebuffer` **19.0 GB/s**（线性/平铺同速）；
     而**采样绘制**对**平铺**源为 **20.9 GB/s**（0.184 ms），对**线性**源只有
     **6.7 GB/s**（0.576 ms，3.1× 慢，且与 stride 3072–4352 对齐无关）。
     0.576 ms + 0.11 ms 固定开销 ≈ 实测 0.65 ms。
- **为什么源是线性的**：`src/mwv207-umd/drivers/mwv207/mwv207_resource.c` 的
  `mwv207_can_tile()` 里 `const bool enable_scanout_tile = false;`，
  凡带 `PIPE_BIND_SCANOUT` / `PIPE_BIND_LINEAR` 的资源一律 `MWV207_LINEAR`；
  GLX 窗口缓冲与 X 屏幕 pixmap 都带 `PIPE_BIND_SCANOUT` ⇒ glamor 每帧都用
  6.7 GB/s 的线性采样速率做一次全屏合成。
- **线性慢的是读写两端（2026-11-11 补测）**：同一程序只切换目标缓冲布局——
  clear + 不采样的三角形写 1.9 MB，平铺目标 **0.185 ms（10.4 GB/s）**、
  线性目标 **0.582 ms（3.3 GB/s）** ⇒ 3D 访问线性表面不论读还是写都只有平铺的 1/3。
  因此"只平铺窗口 pixmap、屏幕保持线性"几乎无收益（写入端仍 0.58 ms 级）。
- **2026-11-11 追加取证（推翻"厂商用平铺扫描出"的假设）**：
  - 反汇编厂商 KMD（`jmgpu.ko` 的 `j9_handle__attribute_chalkstone`，即 plane atomic update）
    显示它**只写 5 个寄存器**，与我们 `mwv207_primary_atomic_update()`
    （`src/kernel-6.6/dc/mwv207_va.c:170-206`）完全相同：`0x434 = pitch>>4`、
    `0x43C = ((h-1)<<16)|(w-1)|0x3`、`0x4F8 = (w-1)<<16`、`0x438 = addr>>6`、
    `0x430` 按 fourcc 置格式位。**没有任何平铺相关位**。
  - 厂商 KMD 有 `DRM_IOCTL_JM_GEM_SET_TILING`（`DRM_JM_GEM_TILING_SUPERTILED`），
    但发布源码里 `tilingMode` 只被 set/get ioctl 存取、无消费者；厂商 DDX
    `mwv207_drv.so` 也没 import `drm_jmgpu_bo_set_tiling`。
  - ⇒ **厂商的扫描出并不是平铺的**，它快是因为 DDX 用 2D 引擎（EXA）做合成拷贝，
    而 glamor 用 3D 着色器采样。
- **2026-11-11 追加测量（blit 比着色器快 2.6×）**：同一对 800×600 FBO，只切换驱动布局：

  | 操作 | 平铺目标 | 线性目标 |
  |---|---|---|
  | `glBlitFramebuffer`（FBO→FBO） | **0.100 ms（38.3 GB/s）** | **0.222 ms（17.3 GB/s）** |
  | 着色器采样拷贝 | 0.184 ms（20.9 GB/s） | 0.576 ms（6.7 GB/s） |

  ⇒ 线性目标上 blit 0.222 ms 明显快过着色器 0.576 ms；差别在"谁来做这次拷贝"
  （2D/blit 引擎 vs PE 着色器）。
- **X server 侧核实**：glamor 21.1.12 的 `glamor/glamor_copy.c` 里 `glamor_copy_gl()`
  只有 `needs_temp`/`fbo_fbo_temp`/`fbo_fbo_draw`/`cpu_fbo`/`fbo_cpu` 五条路径，
  **没有 blit 路径**（`glamor.c:770` 的 `has_fbo_blit` 置位后全树再无使用）⇒
  glamor 的合成拷贝必然走着色器；我方 GL 是广播 `GL_EXT_framebuffer_blit` 的，
  属 glamor 缺路径而非驱动缺能力。
- **可选改法（按性价比重排）**：
  ① **A. 让 glamor 用 blit**：打 X server 补丁（在 `glamor_copy_gl()` 里对
     同格式、无变换的拷贝走 `glBlitFramebuffer`）——present 0.65 → ≈0.30 ms、
     `build` ≈0.65 ms（≈1500 FPS）；不动驱动与 KMD，风险最低，但要把 xorg-server
     纳入我们的构建/打包流程。
  ② **C. 先查我方 3D"写线性"为何只有 3.3 GB/s**：若只是 PE 写策略/寄存器问题，
     修好后可直接接近 blit 速率（0.22 ms），收益最大且只动 UMD；目前尚无证据链。
  ③ **B. 扫描出平铺（原方案②）**：证据基础已被上面的反汇编削弱（厂商自己也没这么做），
     需 KMD 编 DE supertile 且布局不一致会花屏，风险最高、收益 ≈2200 FPS（仍差厂商 2.7×）。
  ④ 只平铺窗口 pixmap —— 收益被写线性端抵消，基本无效；开合成同样要采样线性纹理
     （无收益），只有全屏 direct-scanout 才变真 flip。
- 详细数据与复现命令：`reports/PRESENT-PATH-20261111.md`。
- 关联：`reports/GLMARK2-VENDOR-VS-OSS-20261111.md`（同条件总分与逐场景对照）。
- 另注：`glxgears` 的 `vblank_mode=0` 我方 3462–5275 FPS vs 厂商 15134–17621，
  与 present 结论一致（glxgears 每帧只画几个三角形，几乎全是 present 开销）。

## 火狐 WebGL 页仍未到 vsync 上限（性能，剩余项）

- 现状（r9 测得，零 env；r12 未重新测数）：CSS 动画页 **96.6 FPS / 10.35ms**（已到 100Hz 面板上限），
  WebGL 动画页 **42.0–44.5 FPS / 23.5ms**（llvmpipe 同条件 28.5 FPS / 35.05ms，
  即我方 1.5–2.0×）。
- 已修的两个大头（见 `reports/FF-RENDER-20261102.md` §13、提交 `d7c52a3`）：
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

## 火狐标签栏"微缩 chrome"错乱 —— 已修（2026-11-10，r16）

- 现象：上一节修完字形/图标撕裂后，**标签栏**仍残留错乱：标签标题尾部出现
  `− × ●`、`←`、Bing 图标、彩色块等一簇"微缩的整套 chrome"，静止不动（同一时刻
  反复抓图 md5 相同），页面正文与地址栏正常。只在 GPU WebRender 出现，SWGL/llvmpipe
  正常。
- 定位：把活动 profile（`q3gcvghy.default-release-1`）拷到 `/tmp/ffx/prof_r` 后可
  稳定复现；逐项关闭 partial present / SSBO / GL 版本降级 / dual-source blending
  都**无效**（此前的"开关"结论都是巧合）；`xdotool windowmove` 强制整幅重绘后地址栏
  恢复但标签栏仍错 ⇒ 不是旧内容残留，是这一帧就画错。4× 放大可见是某批 prim 被
  缩放/平移到一小块（几何/变换层面），实例属性抓取本身经 `inst3/inst4` 探针验证正确
  （含单 VBO 混排与 `gl_InstanceID`）。
- **根因：`PIPE_BUFFER` 写路径不等 GPU。** `mwv207_transfer_map()` 对 buffer 只在
  `PIPE_MAP_READ` 或写入区间与 `valid_buffer_range` 相交时才 `mwv207_wait_resource()`，
  纯写直接把 BO 的 CPU 地址返回。WebRender 每帧往 UBO（`glBufferSubData`）写入大量
  每-draw 的变换矩阵/参数，上一批命令还在流水线里时 CPU 覆盖了同一块 BO，在途 draw
  于是用了新矩阵 → 某一批图元被缩放到别处。实验佐证：`MWV207_SYNC_DRAW=1`（每 draw
  同步提交）与"buffer 写前等待"两种方式都能立刻修好，`glxgears` 仍 ~100–117 FPS。
- 修法（`mwv207_resource.c`）：buffer 的 `PIPE_MAP_WRITE`（且非 `UNSYNCHRONIZED`）
  路径先 `mwv207_wait_resource()` 再 `bo_map`，避免 CPU 覆盖 GPU 仍在读取的 BO。
- 验证：复现实例标签栏完全正常（favicon、×、+、书签栏图标、地址栏全对）；系统驱动
  （无任何 env）同样正常；`glxgears` 110 FPS；VP9 回归 18/18；
  `switch-mwv207.sh umd verify` 全通过（载荷 `payload-41` / `24.1.2-fullstack-r16`）。
- 相关：mesa 侧 `loader_dri3_query_buffer_age()` 恒 0 的改动（`ref/oh-mesa` 树，
  见 r15 payload）用于避免 WebRender 依赖我们并不保证的"交换后缓冲保留"。
