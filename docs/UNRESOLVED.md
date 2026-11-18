# 未修复缺陷与未实现功能 · 总清单

> 汇总截至 **2026-11-13 12:40**（运行中的 UMD 是 **r33**（`payload-46/VERSION =
> 24.1.2-fullstack-r33`，2026-11-13 12:35 安装；DRI `568baf7f566f3a0b478a59533fd590f4`、
> VA `f26ab271d71ad005b9a0c38a6f0ca780`）。r33 在 r32 基础上新增：`show_existing`
> 重放帧改走线性副本（官方 intra-only 向量 7/7 逐字节一致）。r32 含 N19
> （PERSISTENT/UNSYNCHRONIZED buffer 不再误等）；r31 含：修法 1（VP9 参考槽别名 +
> MV 双缓冲）、修法 3（纹理上传暂存复用）、修法 4（buffer 改放 GTT）、回读缓冲复用、
> `mwv207_video_buffer.h` 守卫修复、JPEG 三缓冲 SOI 判定修复。
> 内核 `6.6.155-arm64-desktop-hwe` 侧运行中 srcversion `C588E9673F984406AF46A2F`
> （20 处 `BUG_ON` 收敛 + N16 + N4 异步 DMA/K11 + N17 引用计数 + DE `peek` 只读口）；
> `tests/verify-installed.sh C588E9673F984406AF46A2F` → PASS=21 FAIL=0 SKIP=3，
> `tools/gl-regression.sh`（安装栈）→ pass=7 fail=0（glmark2 1069），`tools/vp9-regression.sh`
> → pass=20 fail=0，`tests/vp9-intraonly/run-vp9-intraonly.sh` → 7/7 IDENTICAL。
>
> **性能基线（r31 安装栈）**：`glmark2 -b :duration=2` **1039–1046**；present 300×300
> 0.17 ms/帧、1080p 1.54 ms；纹理上传（1024²/小块）**0.05 ms/次**、buffer `glBufferSubData`
> 4 MB **0.30 ms（12.9 GB/s）**、`glMapBufferRange` 读 **0.33 ms**；VP9 1080p 解码
> **220 fps**、H.264 **234 fps**、HEVC **269 fps**（均不下载帧）；`glReadPixels` 4 MB 4.8 ms。
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
| 1 | VAAPI/VP9 | **A5：Profile 2（10bit 4:2:0）已支持；4:2:2/4:4:4 定案不可实现** | 2026-11-10 `82e7b02`：Profile 2 与软件解码**逐位一致**（64x64/640x360/720p60/1080p30，含 inter 帧，零 TDR），默认声明，`MWV207_VP9_10BIT_DISABLE=1` 可关。Profile 1/3 与 H.264 422/444 同理不可实现：解码器没有色度采样格式选择字段（§1.5）。**残余**：12bit VP9 明确不支持（硬件 P010 通路止于 10-bit）—— 解析器在 profile 2 的 `bit_depth=12` 处直接返回失败并打印一次性提示，客户端经 I-6 收到 `vaSyncSurface` 错误而不是旧画面；样片 `~/vp9-assets/p12bit.webm`（libvpx 编码，320×180）实测 | 本文 §1.6 |
| 2 | VAAPI/VP9 | **B3 完成**：官方 intra-only 向量上 7 个真实解码帧全部与 libvpx 逐字节一致（`8f6e964` 修掉 I-5） | 样片与诊断脚本在 `tests/vp9-intraonly/`；`show_existing` 重放帧也已逐字节一致（线性副本方案，见本文 §1.7） | 本文 §1.7 |
| 3 | VAAPI/VP9 | **I-6 已修（`bb5cd9e`）；B2 已完成并实测通过（`21a15db`）** | `vp90-2-14-resize-10frames-fp-tiles-1-2.webm` 硬件与 libvpx **IDENTICAL**；`vp90-2-21-resize_inter_320x180_5_1-2.webm` 此前按设计跳过（跨尺寸参考不支持），I-6 生效后 `vaSyncSurface()` 返回 `VA_STATUS_ERROR_TIMEDOUT`、ffmpeg 报 `Failed to sync surface` 停输出。2026-11-11 `351e6ed` 删掉"参考尺寸必须等于当前帧"的跳过分支并把 staging 缓冲改成可增长；厂商驱动同一向量逐位正确。**已实测**：`跳过帧=0`；resize-10frames IDENTICAL，resize_inter 为 **KNOWN-DIFF(滤波)**（帧 10 起码流改 160×90、参考 320×180，硬件 MC 缩放核与 libvpx 8-tap 不同；平均 1.2–1.5、最大 <64），详见 §1.9 | 本文 §1.8 |
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
| 6 | 切换脚本 | **vendor 态端到端已实跑（2026-11-13）** | 两次切栈（`stack vendor` 跑 `tools/perf-suite.sh vendor` 与 kprobe 抓寄存器、`stack oss` 切回）均成功，`/run/mwv207-stack-result` 与 `lsmod` 正确；热切换仍不可行（模块 refcnt 恒 2），脚本会自动走重启释放 | `switch-mwv207.sh` / §5 |
| 7 | 打包 | **DKMS 一致性警告（2026-11-13 已修）** | 根因是 `kmd stage` 只覆盖 `/lib/modules/$KVER/updates/dkms/mwv207.ko`，而 `dkms status` 拿它和 `/var/lib/dkms/mwv207-oss/1.0.0/$KVER/$arch/module/mwv207.ko` 比较。现在 `kmd_place_module()` 会把同一份字节同步写进 DKMS 构建产物目录（目录不存在时跳过），status 保持干净；`tools/dkms-sync.sh` 仍可用于全量重建 | §2.1、§5 |
| 8b | 功能/UMD | **JPEG baseline 解码：厂商 DEC 窗口全文已取到，`0x008` 差异也试完，仍 `STRM_CORRUPTED`（2026-11-13 15:35 收口）** | 厂商 `vainfo` 有 `VAProfileJPEGBaseline : VAEntrypointVLD` 且 `ffmpeg -hwaccel vaapi -i t1080.jpg` 成功出图（输出 limited range）；我方实现让 DEC 进 `DEC_ERROR_INT` + 2 s TDR、输出面零字节。寄存器编程细节问题，有厂商输出可做基准 | 本文 §4「JPEG baseline 解码调研」 |
| 8c | 性能/UMD | **N19：client-array 的 buffer map 每 draw 强制提交+无限等待（2026-11-13 已修并随 r32 部署验证）** | 改前 VBO 0.45 µs/draw、client arrays 26–44 µs/draw；根因是 `mwv207_transfer_map()` 对 `PERSISTENT`/`UNSYNCHRONIZED` 的写映射也按 `valid_buffer_range` 相交就等待（Mesa 的 1 MB 常量环每帧换区间，恒相交）。改为 CPU 管理的映射不做等待后 **0.42 µs/draw（约 60×）**，`ubosync`/`uborace` 逐帧写读校验 PASS | 本文 §4「每 draw CPU 与 present 拷贝」 |
| 8 | 性能 | **已量化并关闭（2026-11-13）** | buffer 改放 GTT 后 `glBufferSubData` 4 MB 0.30 ms（12.9 GB/s）、`glMapBufferRange` 读 0.33 ms；create（写入）路径本身有 `mwv207_wait_resource` 等 GPU 吃完，但 glmark2 buffer/build/jellyfish 场景 A/B 无回退（655/655、1526/1522、666/686） | 本文 §4 性能 |
| 9 | 依赖补丁 | `loader_dri3` buffer age 恒 0 只在本地 mesa 树提交 | 未推厂商镜像；重建 mesa 时需保留 | `ref/oh-mesa` `248659c` |
| 10 | 测试 | **已有自动化 GL 回归（2026-11-13）** | `tools/gl-regression.sh` 串起 query / fbo / texcheck / texbench / bufbench / rbbench / glmark2，`DRI_DIR=` 指定驱动目录即可对开发构建或安装栈跑；与 `tools/vp9-regression.sh` 并列。Firefox/WebGL 侧仍是一次性脚本 | §7 |

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

**残余缺口**：12bit VP9（Profile 2 语法允许）**已验证为"优雅拒绝"**（2026-11-16，见 §1.11），Profile 1/3 永久拒绝。

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

**2026-11-13 晚：show_existing 黑图的技术定性（PPU 无法单独启动）→ 2026-11-13 深夜已修**

把 PPU 的整个寄存器窗口 0x500–0x540 逐字段列出来（python 对齐
`mwv207_dec_register_array_7020.h` + `mwv207_video_dec_8170table.h`）后确认：
**PPU 只有输出地址寄存器（`PP_OUT_LU/CH/R/G/B/BOT_BASE`），没有任何输入地址寄存器**
（`PP_IN_WIDTH/HEIGHT`、`PP_IN_FORMAT`、`PP_IN_Y_STRIDE` 等只是格式/几何），
输入固定来自解码单元的内部输出通路。这就是"关掉 `DEC_E` 后 PPU 不跑、重放帧全黑"
的硬件层原因，不是我方编程缺失。

**修法（路线 2 的变体，已落地）**：可见性帧里 `show_frame=0` 的那些正是会被
`show_existing` 重放的内容，所以让**它们的 PPU 输出直接写进一份线性副本**
（`vp9_lin_create()` 用 `mwv207_video_buffer_create_ex()` 建 NV12 `PIPE_USAGE_STAGING`，
GTT 缓存、CPU 读得动），`lin_of[槽]` 记住"这个参考槽的内容在哪份副本里"。
重放时不再动硬件，直接把副本逐行拷进应用 surface（`vp9_lin_blit_to_target()`）。
每帧多 3.1 MB 写（1080p，约 0.3 ms），只对不可见帧发生。

一个额外的坑：`ffmpeg -hwaccel vaapi` 会把**不可见帧也当画面输出**（它按 surface
顺序出帧，不看 show_frame），所以作业完成后还要把副本拷回应用 surface
（`mwv207_dec_vp9_submit_done()` 里的 `lin_target` 分支），否则这类客户端的
前几帧会变黑。NV12 色度平面是 U/V 交错的 **w 字节/行**（h/2 行），不是 w/2
——第一版按 w/2 拷导致重放帧 Y 完全正确、色度偏 2.6 级，改成 w 后才逐字节一致。

验收：`tests/vp9-intraonly/run-vp9-intraonly.sh` 的 7 帧（含 3 个 show_existing
重放帧）与 libvpx **全部 IDENTICAL**；`tools/vp9-regression.sh` 20/20；
`tools/gl-regression.sh` 7/7。可用 `MWV207_VP9_REPLAY_LINEAR=0` 退回旧行为。

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

### 1.9 2026-11-13 复核：resize_inter 由 IDENTICAL 变 KNOWN-DIFF（跨尺寸参考的滤波差异）

`tests/vp9-resize/run-vp9-resize.sh` 在 r33 安装栈上复跑，第二族向量不再逐位一致：

| 向量 | 结果 |
|---|---|
| `vp90-2-14-resize-10frames-fp-tiles-1-2.webm` | **IDENTICAL**（4665600 B） |
| `vp90-2-21-resize_inter_320x180_5_1-2.webm` | **KNOWN-DIFF**：帧 0–9 完全一致；帧 10–29 全部差，`Y mean 1.2–1.5`、`Y max 47–52`、`U mean ~0.6`，误差遍布全画面 |

原因链：

- `ffprobe -show_entries frame=width,height` 显示该流**前 10 帧 320×180、第 10 帧起
  160×90**（码流中途真的改分辨率），帧 10 的 `refresh=0x3`、`ref0=320/180`，即跨尺寸参考。
- 我方两处把"容器/surface 尺寸"当帧尺寸：解析器
  `mwv207_decoder_vp9_slice_header.c` 的 `setup_frame_size_with_refs()` 在
  `use_prev_frame_size` 命中时用 `pic_param_passed_width/height` 覆盖；
  `vp9_program_pp()` 的 in 尺寸取 `decoder->base.width/height`。
- 修法（本提交）：`struct mwv207_vp9_slice_header` 增加 `ref_slot_w/h[16]`，驱动在解析前
  从 `st->ref_slot_w/h[]` 填入；`setup_frame_size_with_refs()` 优先用参考槽的真实尺寸，
  槽未知时才退回调用方尺寸；`vp9_program_pp()` 增加 `coded_w/coded_h` 参数，in 尺寸用
  码流尺寸、out 仍由 outsurface 决定（PPU 负责缩放，走
  `mwv207_dec_ppu.c` 的 `HOR/VER_SCALE_MODE` 分支）。
- 修后输出与修前**逐字节相同**（`cmp` 一致）：ffmpeg 在分辨率变化处会重建 VA context，
  VA 尺寸本来就是 160×90，所以这两处属于"防御性正确化"——对"VA 尺寸滞后"的客户端
  （例如 `-f null` 且不重建 context 的路径）才避免把 160×90 码流当 320×180 解。
- 残差成因：帧 10 引用 320×180 的旧槽，硬件按 `HOR/VER_SCALE` 的 1.14 定点做 MC 缩放，
  滤波器核与 libvpx 的 8-tap 缩放核不同；后续帧从该参考继承差异，量级稳定不累加。
  判据因此从"DIFF"改为 **KNOWN-DIFF(滤波)**：整帧平均 < 2.0 且最大 < 64。

`tools/vp9-regression.sh` **20/20**、`tools/gl-regression.sh` **7/7**（glmark2 1056）、
`tests/vp9-intraonly` **7/7**、`tests/verify-installed.sh` **PASS=21 FAIL=0 SKIP=3**，
`dmesg` 无 `timed out`/WARN/BUG。

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
6.6.155（当时还有 6.6.143；2026-11-12 起只维护 6.6.155 —— 143 的 headers 已残缺、
无法再编译，`updates/dkms/mwv207.ko` 已从该内核删除）；`switch-mwv207.sh kmd stage` 暂存到 `updates/dkms`
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
- `tools/gl-regression.sh` —— GL/GLES 回归（query/fbo/texcheck/texbench/bufbench/rbbench/glmark2）
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

**部署状态**：`tools/dkms-sync.sh` 重建并安装到 6.6.155（2026-11-12 起不再为
6.6.143 编译）；6.6.143 当年另外
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
| **D3** | **编码（H264/HEVC）** | **本 SKU 不可做：没有编码硬件块** | 三重证据：① `jdev->lite = jdev_read(0x12c0000) == 0x21 ? false : true`（`src/kernel-6.6/mwv207_drv.c:379`），实测 `peek 0x012c0000 = 0` ⇒ lite；`lite` 时 `mwv207_pipe_enc_create()` 直接 `return NULL`（`mwv207_pipe_enc.c:251`）⇒ `sched_enc[0]` 不存在；② 用 KMD `peek` 读引擎窗口：`0x900000`（3D0）=`0x00040100`、`0x930000`（DEC0）=`0x80018000` 正常，而 **`0x920000`（ENC）=`0xffffffff`**，与已知缺席的 `0x910000`（3D1）/`0x940000`（DEC1）完全一致；③ `tests/engines/` 早就核对过 lite 机型 **enc=0**（本文 §2.2）。另：我们没有任何 `JMENC_*` 寄存器定义（UMD 只有 `JMDEC_*` 1840 条），vendor 编码走闭源 UMD + `libdrm_jmgpu.so` 的 `jmo_HD_WriteEncReg` HAL，vendor KMD 的 `jmgpu_enc.c` 只是引擎管道。完整证据与"若有硬件需做哪些事/工作量"见 `reports/ENCODE-FEASIBILITY-20261113.md` |
| ~~D4~~ | 4 个未实现的扩展 opcode（结论见下方 D4（结论）行）：`WAIT_GROUP_EVENT`(0x11) / `TD_OP`(0x12) / `TEX_INVALIDATE`(0x14) / `PRE_NORM`(0x15) | 小-中 | 厂商枚举齐全（`MWV207REG_SH_EXTENDED_OPCODE`），需逐个确定语义 |
| **D5** | 开启三个窥孔 `OPT_DIV` / `OPT_FFMA` / `OPT_SET`（默认全关） | 小改动 + 大验证 | 潜在性能收益，需全量着色器回归 |
| **D6** | 多视口 | 与 E8 同 | 取决于硬件是否支持（`GL_MAX_VIEWPORTS` 现报 0/1） |
| **D7** | **Vulkan** | 不可达 | 官方 1.8.0 **也没有**（无 libvulkan / `vulkan/icd.d`；KMD 的 syncobj/vm_bind/gpuva/sparse 全 0）。官网"Vulkan 1.1"至今未交付。**2026-11-13 复核补充**：① 厂商 Linux 包 `mwv207-dev_1.8.0` 只有 `/etc/OpenCL/vendors/jmgpu.icd`（指向 `libJmgpuOpenCL30.so`），没有 `/usr/share/vulkan/icd.d/*.json`、没有 `libvulkan*`；对包内全部 `.so` 做 `strings` 检查，**没有任何 `vkCreateInstance`/`vkGetInstanceProcAddr` 入口**；② 官网 JM9100 页正文提"Vulkan1.1"，但同页"API接口"指标只列 OpenGL4.0/OpenCL3.0/OpenGLES3.2（JM1100 的表里才明确写 Vulkan1.3）；③ 我们继承的 Mesa 24.1.2 构建树里 Vulkan 只有 `src/vulkan`（运行时）与 `src/virtio/vulkan`，**没有 mwv207 的 Vulkan 驱动**；④ `src/kernel-6.6/mwv207_drm.h:90-103` 的 submit ioctl 里 syncobj 字段是**注释掉的**，注释原文"syncobj support, reserved for vulkan"，`struct drm_mwv207_submit_syncobj` 也标 "reserved for vulkan" ⇒ 当初只留了坑没实现 |
| **D8** | **H.264 Multiview / StereoHigh 解码** | 硬件有、客户端栈不可达 ⇒ 不做 | 特性表 `mvc_support=1`、枚举里有 `JMDEC_MVC_E`；但 Mesa 24.1.2 的 `src/gallium/include/pipe/p_video_enums.h` **没有** MVC profile，`src/gallium/frontends/va/va_private.h` 的 `PipeToProfile()` / `ProfileToPipe()` 也不映射 `VAProfileH264MultiviewHigh(15)` / `VAProfileH264StereoHigh(16)`（走 `default: assert(0)` / 返回 -1）⇒ 客户端连 `vaCreateConfig` 都过不了。要支持需同时改 Mesa 枚举 + VA 前端 + 本驱动解析（subset SPS、prefix NAL、`nal_unit_type==20` 扩展 slice），而手上没有 MVC 样片做逐位验收（x264/ffmpeg 都不能编码 MVC）⇒ **不做，只记录** |
| **D9** | **AVS / AVS+ / AVS2 / AVS2-10bit 解码** | 硬件有、客户端栈不可达 ⇒ 不做 | 特性表 `avs_support=1`、`avs_plus_support=1`、`avs2_support=1`、`avs2_main10_support=1`；但 libva 1.22 没有 AVS profile 枚举、Mesa VA 前端也无映射 ⇒ 没有客户端能调用，**不做** |
| **D10** | **VPP（`scale_vaapi` / 色彩空间转换）** | **已可用，无需改驱动** | `vlVaCreateConfig(VAProfileNone, VAEntrypointVideoProc)` 走 Mesa VA 前端的通用 `vl_compositor` 路径（`src/gallium/frontends/va/postproc.c`），`context.c:345` 对 `PIPE_VIDEO_ENTRYPOINT_PROCESSING` **不创建驱动侧 codec**，所以 ours 不需要实现 video proc codec。实测（r31，1080p VP9 120 帧）：纯解码 **220.5 fps**、`scale_vaapi=w=1280:h=720` **170.9 fps**、`scale_vaapi=w=3840:h=2160` **97.3 fps**、`scale_vaapi=format=nv12` **216.5 fps** ⇒ 720p 缩放约 1.3 ms/帧、4× 面积缩放约 5.8 ms/帧，都是 3D 引擎在做。质量对照：`scale_vaapi` 1080p→720p 与 swscale 逐帧比 **PSNR 平均 47.0 dB（y 49.5）**，几何与颜色正确，差异来自双线性 vs 双三次核 |
| **D4（结论）** | 4 个扩展 opcode `WAIT_GROUP_EVENT`/`TD_OP`/`TEX_INVALIDATE`/`PRE_NORM` | 不做（死代码） | `grep -rn "WAIT_GROUP\|TEX_INVALIDATE\|PRE_NORM\|EXTENDED_OPCODE" src/` **零命中** ⇒ 本驱动编译器从不发射这四条，实现它们没有任何可观测收益；与 E3 同类，将来若做 compute/tessellation 后端再按需补 |

---

### u_threaded_context 尝试（2026-11-16）：接入契约补齐后仍崩溃，收益也小于 present 开销，回退

试过的四层改动（`threaded_resource`/`threaded_transfer`/`threaded_query` 子类化、
`threaded_context_create`、Mesa 侧补 `set_hw_atomic_buffers` 包装）与两处崩溃
（`malloc(): invalid size` → `slab_alloc: magic` 断言）记录在
`reports/THREADED-CONTEXT-20261116.md`。放弃的决定性理由是量化结果：build 带 swap
0.653 ms 里 **present 占 0.395 ms**，TC 最多把渲染部分从 0.258 压到约 0.20 ms，
最好情况让 build 从 1532 升到约 1700 fps，而厂商的 14537 fps 主要来自"根本不拷贝
整帧就上屏"。present 的拷贝是结构性差异（2D/DE 两条路都已收口），所以不再投入。

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

### 重场景（terrain/refract）比厂商慢 1.3–1.4× —— 判为 ISA/硬件边界（2026-11-12）

结论先行：**差距落在"每像素的超越函数指令"上，不在 present、不在提交、不在降频**，
本轮实测把可动的杠杆逐个量过，都不足以补上，因此归入硬件/ISA 边界。

已排除（都有数据）：

| 假设 | 实测 | 结论 |
|---|---|---|
| present 拖累 | 两个场景 on/off-screen 差 +0.3 / +0.7 ms（jellyfish 是 +3.24） | 不是主因（已在 §16 修掉 present） |
| CPU 提交瓶颈 | 每 draw 0.17 µs（原 0.86），`strace` 显示 ioctl 总量可忽略 | 已修（`ccd9d9c`） |
| 降频 | `devfreq_mwv207_3d0/cur_freq = 1000` = max，governor `performance` | 不是 |
| 着色器编译期缓存 | 指纹脏标记已让 `update_shader` 0.76 → 0.08 µs | 已修（同上） |
| 超越函数可向量化 | 把 ISA 源操作数的 `swiz_scalar` 强制去掉（`MWV207_VEC_TRIG`，见下）后输出**逐字节相同**、耗时**无变化**（10.38 vs 10.12 ms） | 硬件就是单 lane 串行，无 vec4 形式 |
| 用多项式替换 sin/cos | 实测 sin 每个分量 ≈ 3–4 个 FMA 的代价（64 行 sin 9.15 ms vs 64 行 mul+add 2.4–3.3 ms）；多项式 sin 需 ~7 个 FMA + 范围归约 | 替换**只会更慢**，此路作废 |

剩下的差距成因：`glmark2` 这两个场景是**超越函数密集**的——
`terrain.frag` 每个点光每像素 `pow(nDotH, shininess)`（= `exp2(y*log2(x))`，
两个超越函数）+ 雾的 `exp2`；`light-refract.frag` 同样 `pow` + `refract/reflect/normalize`
（`rsqrt`）。而本 ISA 的超越函数（`0xC frcp`/`0xD frsq`/`0x21 fsqrt`/`0x22 fsin`/
`0x23 fcos`/`0x12 flog2`/`0x11 fexp2`）在 `mwv207_opcode.c:68-71` 里只有单 lane 形式
（`SRC_X_X_0`），`mwv207_emit.c:315` 还强制 `swiz_scalar`；实测其代价约为一次 FMA 的
3–4 倍，也就是说这两类场景平均每像素要多付十几个 ALU 周期。

厂商同场景只快 1.3–1.4×（155 vs 115、123 vs 97），说明它用的是同一套 ISA，只是
每像素少几条指令（更紧的寄存器复用/更少的中间归一化）。这类差距只能靠"与厂商同等
质量的着色器后端"来补，而我们手上既没有它的编译器产物也没有硬件计数器，无法反向
验证任何改写是否真的更优——上一轮试图用 `OPT_FFMA`/`OPT_DIV`/`OPT_SET` 这些 peephole
开关也是因为它们默认关闭、release 构建里根本不生效。

**结论**：`terrain`/`refract` 的 1.3–1.4× 不作修复目标，记录为本机 ISA 的着色器执行
效率边界；`jellyfish`/`buffer` 这类"重叠绘制 + 中等着色器"的场景已通过
平铺扫描出（§19–§21）拿到 1.5–2.9×。

### 性能差距盘点（2026-11-12 晚）：真正的瓶颈在"CPU 读映射 BO"与 VP9 参考槽拷贝

这一轮把 present/重场景之外的地方按"单项微基准 + 栈内实测"扫了一遍，结论按优先级：

| # | 项 | 实测 | 判断 |
|---|---|---|---|
| 1 | **VP9 硬件解码 1080p** | **7.1–8.5 fps**（同文件软件解码 **427–545 fps**） | **真缺陷级**：比软件慢 50–60× |
| 2 | 映射 BO 的 **CPU 读**带宽 | **0.28 GB/s**（写 4.4 GB/s） | 根因所在：GEM mmap 是 write-combined，读极慢 |
| 3 | VP9 参考槽拷贝 | 每帧 `vp9_refresh_slots()` **8–112 ms**（360p/1080p），其中单个 345 KB 槽的 `memcpy` 就 **1.0 ms ≈ 0.34 GB/s** | 直接后果：从 WC 映射的 BO 读 |
| 4 | 纹理上传 `glTexSubImage2D` | 1024² 4 MB **8.2 ms（0.51 GB/s）** + 约 1 ms 固定开销 | 视频/浏览器上传帧是慢路径 |
| 5 | 回读 `glReadPixels` | 64×64 **2.8 ms** | 固定同步开销为主 |
| 6 | 缓冲上传 `glBufferSubData` | 4 MB **0.99 ms（4.2 GB/s）**、`Map+write` 4.4 GB/s | 尚可（写路径本身是 WC 友好的） |
| 7 | 硬件解码 HEVC/H.264 1080p | **254 fps**（软件 545） | 可用，但同样被 CPU 侧拖累 |
| 8 | 小窗口 present 固定开销 | `glxgears` **4.9k FPS**（厂商 15.1–17.6k） | 每帧约 0.14 ms 固定开销 |
| 9 | `u_threaded_context` | 我方 UMD **未启用**（全树零引用） | 可选项，收益未知 |

关键链条（已用插桩证实，`MWV207_DECTIME` 临时开关，测完已回退）：
- `mwv207_dec_vp9_submit_done()` 里 `vp9_adapt_probs()` = **0.00 ms**、`vp9_refresh_slots()` = **8.0 ms/帧**（360p，`flags=0x1`、345600 B）。
- 拆到槽级：`map=0.00 ms`、`memcpy=1.02 ms`/345 KB ⇒ **0.34 GB/s**，正是 WC 映射读的典型值。
- 1080p 每帧 8 槽 × 3.1 MB ≈ 25 MB ⇒ 约 **70–90 ms**，与 7–8 fps 完全对得上。
- 独立微基准：`glMapBufferRange(READ)+memcpy` 4 MB = **15.1 ms（0.28 GB/s）**，而写同样的量是 0.95 ms（4.4 GB/s）。

可能的两条修法（未做）：
1. **CPU 侧不读 BO**：`vp9_refresh_slots()` 改成用 GPU 引擎把槽内容拷进 `ref_store`（同一个 3D/2D 管道，按序提交，CPU 完全不碰像素），风险是引入一次额外的提交与同步；
2. **KMD 侧让系统内存 BO 走 cached 映射**：`mwv207_gem_fault()` 现在对所有 BO 都用 `vmf->vma->vm_page_prot`（DRM 已按 WC 设好），若对 `TTM_PL_SYSTEM/TT` 改用 cached prot，读带宽能上到几 GB/s，但要处理 CPU cache 与 ASIC 写入的一致性（这正是当初选 WC 的原因），风险更高。

纹理上传的 0.51 GB/s 与 `glReadPixels` 的 2.8 ms 也都在同一条"映射 BO 慢"的链上，修 1 或 2 会一并改善。

### 修法 1 已落地（2026-11-12 深夜）：VP9 解码 8.4 → 81 fps

`mwv207_dec_vp9.c` 三处改动，全部是"别让 CPU 碰像素"：

1. **参考槽改别名**（不再每帧拷贝）。解码单元的输出面（`JMDEC_DEC_OUT_YBASE/CBASE`）现在是
   参考图片环 `ref_store` 里的一张图，`refresh_slots` 只把逻辑槽的 `ref_off[i]` 指向它 ——
   VP9 的参考帧内容不可变，多个槽指向同一张图是安全的。环有 16 个位置、最多 8 个逻辑槽，
   `vp9_pic_pick()` 每次挑一个没被任何槽引用的位置，长寿命参考（golden/altref）不会被覆盖。
   环初始按 `stride*16` 分配，尺寸增长时把每张仍被引用的图搬到自己新位置（罕见）。
2. **刷新缓存只冲刷写过的那几段**。原来每帧 `__builtin___clear_cache(scratch, scratch+mv_offset)`
   在 1080p 覆盖约 4.4 MB（2×2 MB 段映射 + 2 MB 滤波系数表，这两块本帧根本没写），
   ARM 上要 **30–45 ms/帧**。现在只冲刷概率表、计数区、tile 表；段映射只在
   `intra_only || error_resilient` 真正 memset 之后单独冲刷。MV 缓冲在 `mv_offset` 之后，
   本来就是 ASIC 写的，不参与冲刷。
3. **MV 图双缓冲**。`vp9_write_ref_regs()` 原来给三路参考各拷一份上一帧的 MV 图
   （`memcpy(scratch + prev, scratch + mv_offset, mv_bytes)`，1080p 每份约 522 KB、三次，
   走 WC 映射读写，实测 **25–30 ms/帧**）。改为两个 MV 槽轮换：当前帧写 `mv_write_off`，
   三路参考的 `DBASE` 都指向另一半（上一帧的 MV），`submit_done` 里对调。
   `VP9_MV_SLOTS` 因此从 3 降到 1，scratch 少占 2×3.1 MB（1080p）。

实测（`/tmp/vadec` 含每帧写 3.1 MB 到文件，所以它比纯解码慢；ffmpeg `-f null` 是不回读的净吞吐）：

| 场景 | 修前 | 修后（含回读写文件） | 不下载帧（`-hwaccel_output_format vaapi`） |
|---|---|---|---|
| VP9 360p | 63.4 fps | 300–320 fps | **433 fps** |
| VP9 1080p | 7.1–8.5 fps | 53–57 fps | **224 fps** |

口径说明：`ffmpeg -f null` 若不带 `-hwaccel_output_format vaapi`，每帧仍会 **下载回系统内存**
（走 0.28 GB/s 的 WC 映射读，1080p 3.1 MB ≈ 10 ms/帧），测出来只有 73–80 fps —— 那仍是
同一条"WC 读慢"链，不是解码器吞吐。不下载时的 224 fps 才是解码器真实能力
（H.264 1080p 同口径 **259 fps**）。

正确性：`tools/vp9-regression.sh` **18/18**；`tests/vp9-resize/`（B2 跨尺寸）**IDENTICAL**；
`tests/vp9-intraonly/`（I-5/N18）逐帧 IDENTICAL；改动前后解码输出 `cmp` **逐字节相同**
（360p 与 1080p 各 120 帧）。

遗留：`glTexSubImage2D` 0.51 GB/s、`glReadPixels` 2.8 ms、以及 HEVC/H.264 的 `buf->res`
读回仍在同一条"WC 映射读慢"的链上；修法 2（KMD cached 映射）未做，风险仍在。

### 修法 3 已落地（2026-11-13 凌晨）：纹理上传的 5–15 ms 固定开销 → 0.03 ms

现象：`glTexSubImage2D` 每次约 5 ms，且**与上传块大小无关、与纹理总大小成正比** ——
64×64 上传到 1024² 纹理 4.9 ms、到 2048² 纹理 19.6 ms，传到 64² 纹理只要 1.0 ms
（`tests/tex-upload/texbench.c`）。

根因：`mwv207_transfer_map()` 对平铺纹理每次都 `resource_create()` 一份**全尺寸
STAGING+LINEAR 影子**，把数据 blit 进平铺纹理后再延迟释放。内核要为这块 4–16 MB 的新 BO
分配并填充页，代价 ∝ 纹理大小；`retired_staging[16]` 环只是延迟释放，从未复用。
链路上的等待也印证：`[TX] wait=5–14 ms`，其中 `submit≈0`、`bo_wait=4–15 ms`
（等的是"上一份暂存最后一次 blit"的 fence），第二次 `bo_wait` 立刻返回。

改法（`src/mwv207-umd/drivers/mwv207/mwv207_resource.c`）：

1. `mwv207_transfer_map()` 建暂存前先在同尺寸/同格式/同层数的 `retired_staging` 环里找一份，
   取走后**有界等待**它上一次 blit 读完（`MWV207_STAGING_REUSE_TIMEOUT_NS` = 100 ms），
   超时就把它放回槽位并新建一份 —— GPU 卡死时宁可多分配，也不能把进程挂住。
2. `mwv207_retire_staging()` 的等待也从无限改为 2 s 上限；新增 96 MB 缓存预算，
   超预算的暂存等 GPU 读完直接释放，避免大纹理把内存占满。
3. 抽出 `mwv207_wait_resource_timeout()`（winsys `bo_wait` 返回 bool：true = 已完成），
   `mwv207_wait_resource()` 仍是无限等待，保持原有语义。
4. `MWV207_STAGING_REUSE=0` 可关闭复用做 A/B。

实测（安装栈 r28 的 UMD 换成开发构建，`:0`，同一进程）：

| 场景 | 修前 | 修后 | 倍率 |
|---|---|---|---|
| 2048² 纹理，64×64 上传（8 张轮换） | 14.95 ms/次 | **0.03 ms/次** | 500× |
| 1024² 纹理，64×64 上传（8 张轮换） | 3.21 ms/次 | **0.03 ms/次** | 100× |
| 1024² 纹理，整张 4 MB 上传 | 7.09 ms/次（0.53 GB/s） | **1.54 ms/次**（2.4 GB/s） | 4.6× |

正确性：`tests/tex-upload/texcheck.c` **PASS**（12 轮整张 A→B 覆盖 + 8 纹理 × 4 轮
小块背靠背上传的串数据检测）；`tests/query` 4/4；`tests/fbo-test` PASS；
`glmark2 -b :duration=2` 1048（安装栈 1073，未回归）；`tools/vp9-regression.sh` 19/19。

坑（都已记录）：① winsys `bo_wait` 返回 **bool（true = 成功）**，第一版按"0 成功"判断，
结果每次都被当成超时、复用形同关闭；② EGL 上下文不指定 `EGL_CONTEXT_CLIENT_VERSION`
时按规范得到 ES1 上下文（`OpenGL ES-CM 1.1`），**没有任何 shader 能力**，探针必须显式要 ES2，
否则 `glCompileShader` 全是 `INVALID_OPERATION`；③ `~/vp9-assets/p1080_sw.yuv` 是旧编码的
参考，导致 `p1080.webm` 一度判 FAIL —— 硬件解码与**当场重新软件解码**逐字节一致，
测试资产必须与源样片同批生成。

### 修法 4 已落地（2026-11-13 凌晨）：buffer 从 VRAM 改放 GTT，CPU 侧读写 6–80 倍

现象：`glBufferSubData` 4 MB **1.12–1.52 ms（2.6–3.5 GB/s）**、`glMapBufferRange(READ)` 4 MB
**约 15 ms（0.28 GB/s）** —— 写还行、读极慢，典型的 write-combined 特征。

定位：新写 `tests/bo-mmap/bo-mmap-bench.c`（直接用 `DRM_IOCTL_MWV207_GEM_CREATE/MMAP` 建 BO、
mmap 后测 memcpy 读/写与稀疏读），三个 domain 在同一台机器、同一个进程里对比：

| 放置 | 4 MB mmap 读 | 4 MB mmap 写 | 稀疏读（每 512 B 取一个 8 B） |
|---|---|---|---|
| `system(0x1)` | 12.06 GB/s | 11.23 GB/s | 0.04 ms |
| **`GTT(0x4)`** | **13.02 GB/s** | **11.73 GB/s** | 0.04 ms |
| `VRAM(0x2)` | **0.18 GB/s** | 1.97 GB/s | 5.58 ms |

⇒ 系统内存 / GTT 的 mmap 是 **cached**（`mwv207_ttm_io_mem_reserve()` 的 SYSTEM/TT 分支
不设 `bus.caching`，TTM 走非 iomem 的默认页属性），而 VRAM 显式设了
`ttm_write_combined` + `is_iomem`。buffer 全部建在 VRAM（`resource_create()` 默认
`MWV207_DOMAIN_VRAM`），于是 CPU 侧的每一次 SubData/map 都在和 WC 映射打交道。

改法（`src/mwv207-umd/drivers/mwv207/mwv207_resource.c`，8 行）：`target == PIPE_BUFFER`
且未设 `MWV207_BUFFER_VRAM=1` 时，domain 用 `MWV207_DOMAIN_GTT`。

实测（同一份代码、同一进程，仅切环境变量；`tests/tex-upload/buffer-bench.c`，ES3 pbuffer，4 MB buffer）：

| 操作 | VRAM（旧） | GTT（新） | 倍率 |
|---|---|---|---|
| `glBufferSubData` 4 MB | 2.10 ms（1.86 GB/s） | **0.34 ms（11.64 GB/s）** | 6.2× |
| `glMapBufferRange(READ)` + memcpy 4 MB | 24.78 ms（0.16 GB/s） | **0.31 ms（12.44 GB/s）** | **80×** |
| `glMapBufferRange(WRITE)` + memcpy 4 MB | 2.21 ms（1.77 GB/s） | **0.34 ms（11.55 GB/s）** | 6.5× |

GPU 侧没有回归（同一驱动 A/B，`:0`，`-b <scene>:duration=2`）：`buffer` 655 / 655、
`build` 1522 / 1526、`jellyfish` 686 / 666、全套 `glmark2 -b :duration=2` **1031**
（安装栈 r29 为 1041，噪声内）。正确性：`tests/query` 4/4、`tests/fbo-test` PASS、
`tests/tex-upload/texcheck.c` PASS、`tools/vp9-regression.sh` 19/19。

遗留：① 顶点数据远超 LLC 的重几何场景，从系统内存取顶点可能比 VRAM 慢，本轮 glmark2
未见差异，若后续有场景退化再用 `MWV207_BUFFER_VRAM=1` 定点关；② 导出成 dmabuf 的
buffer 现在 memory type 是 TT，跨进程共享仍走 dma-buf，未见异常。

### 着色器窥孔 OPT_DIV / OPT_FFMA / OPT_SET 评估（2026-11-13）：保持默认关闭

背景：`nir/mwv207_lower.c:1562` 的 `#define do_by_default(OPT) (debug_get_bool_option("OPT_"#OPT, false))`
让三个窥孔（`DIV`→`mwv207_nir_opt_peephole_fdiv`、`FFMA`→`…_ffma`、`SET`→`…_set`）
可以在**不重新构建**的情况下用环境变量打开（`mwv207_lower.c:1622/1677/1680`）。
它们都是标准改写：`fdiv` 除常数 → `frcp`+`fmul`、`fmul`+`fadd` → `ffma`、
`b2f32(cmp)` → `SET`。

评估方法：新增 `tests/shader-peephole/peep.c`（EGL pbuffer 256²，着色器里同时放
除常数、除变量、`x*y+c`、比较转浮点，走 `fract()` 后输出 RGBA8），在
`LIBGL_DRIVERS_PATH=/home/admin/dev-dri MESA_LOADER_DRIVER_OVERRIDE=mwv207` 下
分别用默认 / `OPT_DIV=1` / `OPT_FFMA=1` / `OPT_SET=1` / 三者全开渲染，再逐字节比较。

结果：

| 配置 | 与默认逐字节比较 | 说明 |
|---|---|---|
| `OPT_DIV=1` | **完全相同**（0/262144 字节不同） | 除法本来就已在别处降成 `RCP`（转储里默认就有 4 条 `RCP`），此窥孔未再改变 ISA |
| `OPT_FFMA=1` | **完全相同** | 探针里的 `x*y+c` 没触发该窥孔（转储里默认/开都无 `FFMA`） |
| `OPT_SET=1` | **完全相同** | 转储里 `CMP.lt`+`AND.u16 1.0` 被合并成单条 `SET.lt`，每处省 1 条指令 |
| 三者全开 | **完全相同** | — |

着色器转储（`mwv207_debug=nir`）对比确认窥孔确实生效：`OPT_SET` 让 ISA 从
`CMP.lt.denorm` + `AND.u16 … 1.000000` 变成 `SET.lt`（少一条），其它 opcode 计数不变。

性能：9 个着色器相关场景 A/B（`glmark2 -b <scene>:duration=2`，默认 vs 三者全开）
全部在噪声内：`shading` 1355/1354、`conditionals` 1223/1229、`function` 1231/1217、
`loop` 1221/1222、`bump` 1600/1590、`refract` 101/101、`terrain` 113/116、
`jellyfish` 678/662、`pulsar` 1384/1376。

结论：**保持默认关闭**。收益只体现在个别着色器的指令数（`SET` 省一条），在这些场景里
量不出来；`FFMA`/`DIV` 还会改变浮点舍入，收益不明而需全量着色器回归，不值得为此改默认值。
探针留在 `tests/shader-peephole/peep.c`，将来若有真实应用的着色器受此影响可直接复测。

### JPEG baseline 解码调研（2026-11-13）：硬件报错，不实现

背景：`hw_build_id 0x1F76` 的能力表（`video-codec/mwv207/mwv207_video_dec_vpu_features_list.h`）
里 `jpeg_support=1`、`jpeg_esupport=1`、`jpeg_prog_support=0`，而**同一张表**里
`mpeg2_support=0`、`mpeg4_support=0`、`vc1_support=0`、`vp8_support=0`（熔断）——
即 MPEG-2/MPEG-4/VC-1/VP8 在这颗 SKU 上根本不存在，JPEG 是唯一“硬件声称有、我方没实现”
的解码缺口。寄存器名在 `mwv207_video_dec_8170enum.h` 里齐全，mainline 有同代
Hantro G1 的参考实现（ST 的 `v6.6-stm32mp` 分支，`hantro_g1_jpeg_dec.c`），
于是按它实现了一版：`mwv207_dec_jpeg.c`（头解析、ECS 定位、544 B QP/AC/DC 表上传、
`DEC_MODE=3`/`JPEG_MODE=2`/`STREAM_LEN`/`STRM_START_BIT` 等寄存器、平铺输出 + PPU），
并声明 `PIPE_VIDEO_PROFILE_JPEG_BASELINE`（仅 BITSTREAM）。

结果（证据链）：

1. **地址寄存器全 0** → 定位到 `set_addr_reg_hi(id_lsb + 1, …)`：JM 的地址寄存器是
   “MSB 在前”（`YBASE_MSB 0x100` / `YBASE_LSB 0x104`），`+1` 写到的是 LSB 后面那个
   寄存器（例如把 YBASE 高半写进了 `REFER0/JPG_CH_OUT_BASE_MSB`）。改成 vp9 那样的
   显式 LSB→MSB 映射后，用 KMD 的 `peek` 口确认 `0x930104 = 54067200`、
   `0x93010c = 56140800`、`0x9302bc = 618987520`，地址确实被 patch 上了。
2. **硬件报错**：DEC0 状态 `0x930004 = 0x00010102` = bit8（`DEC_IRQ`）+
   bit16（`DEC_ERROR_INT`）。KMD `pipe_dec_polling` 要求 `(stat & 0x1100) == 0x1100`
   （bit8 + bit12），JPEG 永不满足 ⇒ fence 不 signal、PPU 不执行、2 s 后被调度器
   TDR 复位；dmesg 每帧一条 `job from 'dec0:0:mjpeg' to 'mwv207_dec0', timed out!` +
   `irq state reg: 0x00010102`（与 I-4 的 H.264 High10 是同一失效模式）。
3. **输出面零字节**：同一 JPEG 连解两帧，在第二帧 `set_regs` 开头统计上一帧的自建
   解码面 `[JPG] prev out size=2592000 nonzero=0 head=00000000` ⇒ 不是完成位掩码
   或缓存时序的假象，DEC 连一个字节都没写。错误详情寄存器（`0x414` 的 ERROR_ADDR/SLICE、
   `0x0c0`、`0x428`）读回全 0，无可用信息。
4. **试过的变体全部同样报错**：① `PJPEG_E`/`PJPEG_LAST_SCAN_E`/`JPEG_INTERLEAVED_E`
   + `PJPEG_SS/SE/AH/AL`；② 输出改线性 NV12 直接写目标面（跳过 PPU）；③ 平铺输出
   （vp9 的 7680 stride 几何）+ PPU；④ ECS 单独拷进 256 B 对齐缓冲、`STRM_START_BIT=0`。
5. **当时的"厂商也没有解码参照"判断已被推翻（同日下半天）**：切到厂商栈后
   `vainfo` 显示 `VAProfileJPEGBaseline : VAEntrypointVLD`，且
   `ffmpeg -hwaccel vaapi -vaapi_device /dev/dri/renderD128 -i t1080.jpg -f rawvideo -pix_fmt nv12`
   **成功出图**（3,110,400 B）。与软解 `libjpeg` 的输出逐字节不同，但差异只是范围映射：
   `(76-16)*255/219 ≈ 70`、`(41-16)*255/219 ≈ 29` 与厂商输出实测的 70/29 完全吻合，
   即厂商输出是 limited range。我们先前在 `jmgpu_drv_video.so` 里只找到编码器字符串，
   是因为解码参数走 `jmo_HD_*` HAL（`libdrm_jmgpu.so` 提供）而非字符串常量。
   对照见 `reports/VENDOR-BASELINE-20261113.md`。

结论（2026-11-13 晚上再修正）：**硬件可用、厂商栈可用；我方已把厂商 502 条寄存器写入
完整对齐，仍报 `STRM_CORRUPTED`，缺的是厂商的缓冲内容/布局**，不是寄存器。

取证方法（成果已入仓）：厂商 `jmgpu.ko` 未 strip，在 `dec_write_reg.isra.0` 上挂 kprobe
（5 个参数 `a1=jdev a2=id a3=type a4=offset a5=value`），跑一帧 JPEG 即可拿到整个 DEC 窗口
0x000–0x7d0 的写入序列 → `tools/vendor-capture/vendor-jpeg-regs.txt`（502 行）。
用 LD_PRELOAD 拦 ioctl 抓不到，因为寄存器数组在 `struct core_desc` 的指针里。

厂商序列的三个反直觉点（详见 `reports/JPEG-VENDOR-TRACE-20261113.md`）：

1. **`DEC_OUT_DIS=1`（0x00c bit15），且完全不写 `0x104`/`0x108`/`0x10c`/`0x18c`**
   —— JPEG 解码不落显存面，经内部通路进 PPU。我方一直是"给解码器指显存面"，所以
   输出面永远零字节。
2. `PIC_MB_WIDTH=(w+127)/128`（1920→15，不是 120）、高度 `(h+15)/16`；
   `HUFFMAN_E=0`（码长走 0x40..0x68 寄存器）、`STREAM_LEN`/`STRM_BUFFER_LEN` 都等于 ECS 长度。
3. PPU 输入按 **960×540（宽高各一半）** 配，`PP0_DUP_HOR/VER=1` 做 2x2 复制，
   输出（1920×1080）直接写应用 surface 的 Y/C 面（`PP_OUT_LU/CH_BASE`）。

按上表复刻后：帧能提交，但仍 `irq state 0x00010102`（`STRM_CORRUPTED`+`DEC_IRQ`+`DEC_ERROR_INT`）
+ 2 s TDR。已排除：`DEC_MODE` 取值、表字节序（`AUXSWAB`×`TABSWAP` 4 组）、流字节序
（`STRMSWAP` 0/1/2/3/8）、整段 JPEG 上传 + ST 的页/位偏移公式、`STREAM_ALL=0`；
寄存器写入本身已用 KMD `peek` 验证生效。⇒ 剩余差异只在**缓冲区内容/布局**
（ECS 是否带填充、544 B 表的 QP zigzag 方向与 AC/DC 交换条件、stream/qtable 的放置域）。

当前处置：**不启用 `PIPE_VIDEO_PROFILE_JPEG_BASELINE`**（否则客户端会拿到零帧或 2 s 卡顿），
取证产物与抓取脚本保留在仓库，实现代码搁置在 `~/jpeg-parked/`。重开这项时的下一步是
在厂商栈上用 kprobe 抓 `DecWriteRegs` 的 `core->regs` 指向的内容，或直接 dump 厂商
staging BO 的页面，拿到 ECS/表的确切字节。

#### 火狐"打开网页就崩溃 / 文字整页乱码"：着色器 arena 只放得下 8 个变体（2026-11-17，已修）

- 现象：Firefox 159 打开 `https://cnb.cool/...` 一类的页面，3.6 s 内即崩溃；不崩时整页文字被裁边/串字（几何与图标正常）。排障模式（禁用附加组件/硬件加速）下正常。
- 崩溃取证（`~/.mozilla/firefox/Crash Reports/pending/*.extra` 的 `StackTraces`，按模块基址算偏移再 `addr2line` 到 r37 的 `libmwv207_dri.so`）：
  - `SIGABRT / SI_TKILL`，帧链 `libc abort ← __assert_fail ← libmwv207_dri.so+0x7131ec ← +0x6f6e1c ← +0x6f85d8 ← +0x2ae4e4`
    → `mwv207_record_code`（`nir/mwv207_compiler.c:405`）← `mwv207_set_current_shader` ← `mwv207_update_shader` ← `_mesa_DrawElements…`
  - 另有 `SIGSEGV / SEGV_MAPERR` 落在 `src/mesa/main/get.c` 的 `find_value`（堆已被破坏的后果）。
- 根因：`mwv207_record_code()` 把每个着色器变体的 GPU 二进制放进 context 内的"着色器 arena"BO：容量按 `code_size * 4 * 16` 申请，而每个变体实际占用 `code_size * 4 * 2` 字节 ⇒ **只放得下 8 个变体**。双源混合把混合状态（因子/方程/blend color）烘焙进着色器后，变体数量随页面内容暴涨，第 9 个变体的 `memcpy` 就越界写入（先污染其他显存 → 文字乱码），紧接着那句 `assert(state->bo_offset < state->bo->base.base.size)` 触发 `abort`（→ 崩溃）。
- 复现探针：`tests/gl-probe/dualarena.c`（每 draw 改一次 `glBlendColor` 并让 dst 因子用 `CONSTANT_COLOR_EXT`，画 64 次）。修复前驱动 **abort**：`dualarena: ../src/gallium/drivers/mwv207/nir/mwv207_compiler.c:405: mwv207_record_code: Assertion 'state->bo_offset < …' failed`（rc=134，与火狐崩溃同一条断言）；修复后 `RESULT: PASS`。
- 修复（`src/mwv207-umd/drivers/mwv207/nir/mwv207_compiler.c`）：
  1. arena 起步容量 `0x200000`（2 MB，约数百个变体），仍保留 `code_size * 4 * 16` 的下限；
  2. 越界从 `assert` 改成**一次性告警**（`[mwv207] 着色器 arena 溢出 …`），不再 abort；
  3. 顺带把 lower_dual 使用的"渲染目标读回"槽号（`ctx->info.dsbi.sampler_index`）并进 `fp_dual_blend` 指纹 —— 该槽号随每次 draw 的视图数量变化，原来不进缓存键会让同一变体在"绑 1 张 / 绑 2 张纹理"的 draw 之间复用（`tests/gl-probe/dualtex.c`）。
- 验证：`tools/gl-regression.sh` 新增第 8 项 `dualarena`，全套 **pass=9 fail=0**；火狐用开发驱动打开原崩溃页面渲染完全正常、无崩溃、无 arena 告警。

#### 追加取证（2026-11-13 深夜）：寄存器已 100% 对齐，仍 `STRM_CORRUPTED`

1. **字级比对**：给 `mwv207_dec_jpeg.c` 的厂商路径加 `MWV207_JPEGREGS` 原始字打印
   （`reg_buf->regbuf[]`），与 `tools/vendor-capture/vendor-jpeg-regs.txt` 的 501 个写入字
   逐字比较，一开始只剩 **1 处语义差异**：word `0x010` 我方 `0x07822000`、厂商 `0x07802200`。
2. **差异原因**：厂商的 `0x010 = PIC_MB_WIDTH(15)<<23 | PIC_HEIGHT_IN_CBS(136)<<6`
   —— 也就是 **`PIC_MB_HEIGHT_P` 写 0**，高度只放 8 像素行单位（`mb_h*2`）。
   我们照 H.264 的写法把 `mb_h`（68）塞进 `PIC_MB_HEIGHT_P`，多出 bit17。
   改 `PIC_MB_HEIGHT_P=0` + `PIC_HEIGHT_IN_CBS=mb_h*2` 后，**我方寄存器窗口与厂商逐字一致**
   （除 4 个地址字，它们在 submit 时才由 relocation patch，dump 里显示 0 属正常：
   `0x2a4` STREAM_BASE、`0x2bc` QTABLE_BASE、`0x518` PP_OUT_LU_BASE、`0x520` PP_OUT_CH_BASE）。
3. **表内容自查**：把 544 B 辅助表导出（`MWV207_JPEG_AUXDUMP=1` → `/tmp/jaux.bin`），
   用 python 直接从 JPEG 的 DQT/DHT 段独立重建 ST 布局后比对 —— QP 之字形、AC1/AC2、
   DC1/DC2 全部逐字节正确；发现并修掉一个真 bug：Mesa 的
   `VAHuffmanTableBufferJPEGBaseline.ac_values[162]` 只填 `sum(num_ac_codes)` 个值，
   其余是未初始化栈内容，直接 `memcpy(…,162)` 会把垃圾写进硬件表 → 改成"先清零再拷、
   按 count 截断"（DC 同理 12 字节）。
4. **流内容自查**：导出 `st->stream` 与 JPEG 文件 ECS 起点逐字节比对，前 8 字节
   `b5d35fea1e6f5ae4` 与文件一致；长度 116,516 = ECS(116,514) + Mesa 追加的 EOI(2)，
   与厂商 `STREAM_LEN=0x1c724` 完全相同。
5. **放置域**：加 `MWV207_JPEG_VRAM=1` 把 stream/qtable 从 GTT(cached) 改到 VRAM
   （厂商抓包的 `STREAM_BASE=0x0ce23000`、`QTABLE_BASE=0x00d24000` 都是低地址显存），
   两组结果完全一致，仍是 `nz=0`、`irq 0x00010102`。

⇒ 结论更新：**寄存器与表/流内容都已对齐，失败点只剩"硬件还要求某个我们没看到的
条件"**（可能是厂商在提交前对 DEC 做的 abort/reset 脉冲、`jmgpu.ko` 里 `DEC_E` 之外的
时钟/门控写、或 JPEG 专用的 AXI 配置寄存器块 0x700–0x7d8 里的非零项）。
下一步必须拿**厂商自己的 BO 页面内容 + 完整提交序列**：在厂商栈上用 LD_PRELOAD 拦
`mmap`、退出时 dump 512 B–2 MB 的映射，再配合已入仓的 kprobe 寄存器序列做交叉验证
（需要切栈重启两次，即 `stack vendor` → 抓 → `stack oss`）。

#### 最后一轮取证（2026-11-13 15:00–15:35）：拿到厂商 DEC 窗口全文，仍未找到启动条件

1. **厂商不是用 ioctl 写寄存器，而是把 16 KB 的 DEC 窗口 mmap 进用户态做普通内存写**
   —— 这解释了两件事：`strace` 里整次 JPEG 解码只有 69 个 ioctl（31 个在 `/dev/jmgpu`
   上、都是 8 字节结构，2 个在 DRM 节点上），且拦 `mmap` 也看不到映射（映射是 KMD 在
   `ioctl(0x650c)` 里建立的，不产生 `mmap` 系统调用，`/proc/self/maps` 里才可见）。
   新工具 `tools/vendor-capture/vfollow.c`：拦 ioctl，跟它传的用户指针把窗口内容读出，
   得到 `tools/vendor-capture/vendor-jpeg-window.{bin,txt}`（16 KB，201 个非零字）。
2. **窗口里唯一与之前 501 条不同的字是 `0x008`**（kprobe 那张表从 `0x00c` 才开始，所以
   一直没比到它）：
   | word | 厂商 JPEG | 厂商 H.264 | 我方（r35 前代码） |
   |---|---|---|---|
   | `0x004` | `0x00000001` (DEC_E) | `0x1` / `0x1100` | 同 |
   | `0x008` | **`0x00003400`** = `CLK_GATE_E(4b@12)=3` + `TILED_MODE_LSB(bit10)=1` | `0x00003480`（同上 + `DRM_E(bit7)=1`） | `CLK_GATE_E=1`、TILED 不设 |
   另外从窗口读出：`STREAM_BASE` 可以**不 16 字节对齐**（第二帧 `0x0cf7a722`）；`QTABLE_BASE`
   4 KB 对齐；PPU 的 Y/C 输出在**两块独立缓冲**（`0x0cc61000` / `0x0ce5f000`，stride 均 `0x780`）。
3. **把 `0x008` 做成现场开关并跑完矩阵**（`MWV207_JPEG_CLKGATE` / `MWV207_JPEG_TILED` /
   `MWV207_JPEG_DRM`，8 种组合、1080p/320×240/64×64 三种尺寸）：
   **全部 `nz=0`、`irq state reg 0x00010102`、2 s TDR**（`dmesg` 累计 63 次）。
4. 厂商 KMD 的 `dec_subsys_array[]`（`~/vendor-src/usr/src/mwv207-1.8.0.uos/jmgpu_remote.c:141`）
   显示 DEC 有三个实例（`0x930000` / `0x950000` / `0x940000`），本机 `peek` 到
   `0x930000` 与 `0x950000` 都是 `0x80018000`（两个可用 DEC）、`0x940000 = 0xffffffff`；
   每个实例的 `HW_DECODER` 窗口就是我们已对齐的 `503*4 = 2012 B`（`0x00..0x7d8`），
   不是"JPEG 走另一个核"。
5. ⇒ **最终结论：本 SKU 的 JPEG 解码需要在 `DEC_E` 之外由厂商闭源 KMD/UMD 联手做的某个
   准备动作（寄存器窗口与码流/表内容都已 100% 对齐仍 `STRM_CORRUPTED`），我们没有足够信息
   复现。不实现**，只在 `~/jpeg-parked/` 保留实现（含 vendor 模式、`0x008` 开关、CTRL1 编码
   修正），实现细节与证据链留在 `reports/JPEG-VENDOR-TRACE-20261113.md` 与本文件。


保留的两处真修复（与 JPEG 无关，随本次提交上）：

- `video-codec/mwv207_video_buffer.h`：include guard 写成了
  `#ifndef mwv207_video_buffer_h` + `#define jmpgu_video_buffer_h`（名字不一致，
  守卫形同虚设），二次 include 会 `redefinition of 'struct mwv207_video_buffer'`。
- `mwv207_video_decoder_decode_bitstream()`：三缓冲布局里跳过 buffer 0 是给
  H.264/HEVC 用的（buffer 0 是长度头），JPEG 的 VA 前端同样用 3 个缓冲但 buffer 0
  是合成头（SOI + DQT + SOF + DHT + SOS），丢掉会报 `no SOS marker`。
  改为只在 buffer 0 **不以 `FF D8` 开头**时才跳过（对 H.264/HEVC 行为不变）。

回归：`tools/vp9-regression.sh` **pass=19 fail=0**、`tests/query` **PASS=4 FAIL=0**。

### 每 draw CPU 与 present 拷贝的实测（2026-11-13 下午）：差距在 present，不在每 draw

针对"draw 密集场景比厂商慢 5–14×"的说法，做了三组实验后修正归因（详见
`reports/VENDOR-GAP-20261113.md` §7）：

- **每 draw CPU**（新探针 `tests/draw-bench/drawbench.c`，8×8 视口）：VBO 形态
  **0.45–0.53 µs/draw**（~200 万 draws/s），与厂商同级；驱动内部插桩也只有
  `config=0.92 µs + submit=0.04 µs`。**所以 `u_threaded_context` 不是主要答案**
  （试接过，`glmark2 build` 堆损坏崩溃，收益预期也低，已回退）。
- **真正的 CPU 异常**：client-array（客户端内存指针）形态 **26–44 µs/draw**，根因是
  `mwv207_transfer_map()` 的 `PIPE_BUFFER` 分支每次 map 都 `mwv207_wait_resource()`
  （强制提交 + 无限 `bo_wait`），`strace` 实测约 2 ioctl/draw；
  `MWV207_NOBUFWAIT=1` 时 0.52 µs/draw（**50×**）。这条等待是为修 WebRender UBO
  覆写（P6）加的，不能直接删——正确修法要按 BO 的在途栅栏判断。**登记为待修项（N19）**。
- **N19 已修（2026-11-13）**：实测该 map 的 `usage=0x62`（`WRITE | PERSISTENT | 0x20`），
  Mesa 的上传缓冲是 1 MB 环，每次 map 的 `box.x` 递增 24 B，而 `valid_buffer_range`
  恒为整块 ⇒ 旧逻辑"区间相交就等"对每次 draw 都成立，纯属误判。
  改为：`cpu_managed = usage & (PIPE_MAP_PERSISTENT | PIPE_MAP_UNSYNCHRONIZED)`，
  只有非 CPU 管理的写映射才保留原来的两种等待（`glBufferSubData`、WebRender UBO
  仍走等待，P6 不回退）。效果：client arrays **26–44 µs → 0.42 µs/draw（60×）**，
  VBO 0.44 µs 不变；新增 `tests/ubo-sync/ubosync.c`（逐帧写读）与 `uborace.c`
  （128 帧无同步覆盖竞争）均 **PASS**，`tools/gl-regression.sh` 7/7（glmark2 1672）。
- **glmark2 build 的每帧构成**：`MWV207_STATS` 计数 `draws=3065 / submits=3073 /
  bufmap_wait=2`（2 秒）⇒ 每帧 1 draw、1 提交，帧内 CPU 不是瓶颈；
  `--off-screen` 3881 FPS（0.258 ms）vs 带 swap 1532 FPS（0.653 ms）
  ⇒ **present 约 0.4 ms**。
- **present 拷贝上限**：`glBlitFramebuffer`（同格式 RGBA8，走 PE `mwv207_copy_image`）
  800×600 批量提交 **0.215 ms（8.9 GB/s）**、1080p **0.881 ms（9.4 GB/s）**；
  present 实测 800×600 **0.476 ms/帧**、300×300 0.172 ms，每帧约 1.5 次拷贝。
  厂商 present 0.014 ms ⇒ **它根本不做这份拷贝**（2D 引擎或客户端缓冲直接扫描出）。
- 结论：廉价场景的差距是**结构性**的——我方 2D 引擎启动位依赖厂商未发布函数、
  DC 平铺扫描出不可达，present 必然要把窗口内容用 PE 拷进线性屏幕（约 9 GB/s 上限）。
  该项不再作为优化目标；只有 client-array 的 map 等待是真正可修的性能缺陷。

### Firefox 的 WEBRENDER_COMPOSITOR / DMABUF_SURFACE_EXPORT 被 blocklist（2026-11-17）：浏览器策略，不是驱动缺陷

现象（Firefox 159.0.1，`about:support` → 图形 → 特性）：两条都是 blocklisted，
`WEBRENDER_COMPOSITOR` 的 failure id 是 `FEATURE_FAILURE_WEBRENDER_COMPOSITOR_DISABLED`，
`DMABUF_SURFACE_EXPORT` 的 failure id 是 `FEATURE_FAILURE_BROKEN_DRIVER`。

Firefox 侧的证据（upstream 源码，逐条可查）：

| 位置 | 规则 | 影响 |
|---|---|---|
| `widget/gtk/GfxInfo.cpp`（`APPEND_TO_DRIVER_BLOCKLIST`，`OPERATING_SYSTEM=Linux`、`DeviceFamily=All`） | `FEATURE_WEBRENDER_COMPOSITOR` → `FEATURE_BLOCKED_DEVICE`，failure id `FEATURE_FAILURE_WEBRENDER_COMPOSITOR_DISABLED` | Linux 上**对所有设备**关闭，与显卡无关 |
| 同文件，`FEATURE_DMABUF_SURFACE_EXPORT` 段 | `DriverVendor::MesaAll`、`DeviceFamily::All` → `FEATURE_FAILURE_BROKEN_DRIVER`（注释列出 Mesa issue 6666/6796/6688/6988） | **对所有 Mesa 驱动**关闭 |
| `gfx/thebes/gfxPlatformGtk.cpp` 的 `InitWebRenderConfig()` | `#ifdef MOZ_WAYLAND` … `IsWaylandDisplay()` … 否则 `ForceDisable("Not available on X11", FEATURE_FAILURE_NO_WAYLAND)` | WR compositor 还要求 **Wayland 会话**；X11 上直接不可用 |

我们为什么被归到 `MesaAll`：`widget/gtk/GfxInfo.cpp` 判定 `mIsMesa = glVersion.Find("Mesa") != -1`，
而我们的 `GL_VERSION` 是 `4.0 (Compatibility Profile) Mesa 24.1.2 (git-9466c83cfc)` ⇒ `mIsMesa = true`、
`mDriverVendor = "mesa/mwv207"`，于是上面这条"Mesa 全族"规则命中原样生效（GL_VENDOR 虽然是 `JingjiaMicro`，但不参与这个判定）。

结论：**这两项在驱动侧改不掉**。可行途径只有 Firefox 侧：
① 用 Nightly 构建 + `widget.dmabuf-export.force-enabled=true`（代码里该逃生开关包在 `#ifdef NIGHTLY_BUILD` 里，release/beta 无效）+ `gfx.webrender.compositor.force-enabled=true`，且 WR compositor 仍要求 Wayland；
② 自行打补丁删掉这两条 blocklist 规则；
③ 向上游提 issue（Mesa 全族关闭是有历史的，需要带我们的实测证据）。

#### 顺带修掉的真实驱动缺口：dmabuf 导出能出 fd，导入却必然失败

即使 Firefox 放行，我们的 dmabuf 往返也是断的：`eglExportDMABUFImageMESA` 给出
`fourcc=0x34324241 (AB24)`、`modifier=0x600000000000002`（`DRM_FORMAT_MOD_VIVANTE_SUPER_TILED`）、
`stride=256`，但把同一个 fd 用 `EGL_LINUX_DMA_BUF_EXT` 导入回来时 `eglCreateImageKHR` 返回
`EGL_BAD_MATCH (0x3009)`。

根因：Mesa 的导入校验（`src/gallium/frontends/dri/dri2.c` 的 `dri2_get_modifier_num_planes()`）会调
`pipe_screen::is_dmabuf_modifier_supported()`；驱动没实现这个钩子时它返回 0 个 plane ⇒ `BAD_MATCH`。
我们此前只实现了导出侧的 `query_dmabuf_modifiers()`。

修法（`src/mwv207-umd/drivers/mwv207/mwv207_resource.c`）：
- 新增 `mwv207_screen_is_dmabuf_modifier_supported()`：`LINEAR` 恒支持（压缩格式除外）、
  `VIVANTE_SUPER_TILED` 交给 `mwv207_tile_supported_format()` 判定，YUV 置 `external_only`；
- 新增 `mwv207_screen_get_dmabuf_modifier_planes()`（`util_format_get_num_planes()`）；
- 在 `mwv207_screen_create()` 里挂到 `pscreen->is_dmabuf_modifier_supported` / `pscreen->get_dmabuf_modifier_planes`。

验证探针 `tests/gl-probe/dmabuf.c`（EGL pbuffer 渲染 → `eglCreateImageKHR(GL_TEXTURE_2D)` →
`eglExportDMABUFImageQueryMESA`/`...MESA` → 用 `EGL_LINUX_DMA_BUF_EXT` 导回 → 挂 FBO 读回比对）：

```
EXPORT: fourcc=0x34324241 (AB24) planes=1 modifier=0x600000000000002
FDS: 7 stride=256 offset=0
IMPORT: px=64,128,191,255 期望 64,128,191,255
RESULT: PASS
```

回归：`gl-regression` **8/8**（glmark2 902，当时有桌面负载）、`vp9-regression` **20/20**。

### 2D 引擎：已能执行命令流，但 draw op 不产生写（2026-11-16）

之前记录的"2D 引擎提交成功但硬件不执行"已经推进一大步：**waitlink 链能被引擎真正执行**，
证据是链里的同步写命令（`0x40000000|sync_reg, sync_val`）会落到 `0x96E004` 等寄存器
（`0x00005555`/`0x00007777`/`0x00008888`），且 `link` 能把引擎从上一段 wait 跳到下一段继续跑。

修掉的具体缺陷（`src/kernel-6.6/mwv207_pipe_2d.c`）：

| 缺陷 | 修法 |
|---|---|
| `struct mwv207_job.cmds` 是 `char *`，按 `u32` 索引读到的只是字节，`END(0x81000000)` 替换循环被编译器整体优化掉（`.ko` 里根本没有比较指令） | 改成 `const u32 *cmds = (const u32 *)mjob->cmds;` + `WRITE_ONCE(payload[i-1], 0x80000000u)`；用 `-save-temps` 与 `objdump` 复核 |
| 用户流末尾的 `0x81000000`（END）让引擎停止，其后的 fence 同步写与 wait 尾永远不执行 ⇒ fence 必超时 | 提交时把尾部 END 换成 NOP |
| 启动链只有 40 字节，`byte16_cnt` 与真实长度不符 | 按厂商 `j9_nipas()` 补齐为 48 字节（`{nop,2,nop,nop, sync(8B), 2×nop, wait(16B)}`） |
| fence 同步只用共享的 `0x6000` 槽 | 每个 fence 独占 `0x6004 + (seqno&15)*4`，值按 `^0x5555|1` 变换；ISR 逐个比对 |
| 只改 link 不踢引擎 | 每次提交都写 `0x4C00 = 1`（`j9_gelatinizable()` 行为） |
| 环放在系统内存并开了 2D 私有 MMU | waitlink 模式（`j2dmode=1`，本机默认）厂商**从不使能**该 MMU，只在 ringbuffer 模式建表；环回 VRAM、MMU 只建表不使能 |

仍然失败的关键点：**fill/copy 操作码被引擎接受（不报错、后续同步写继续执行），但不产生任何像素写**。
用厂商抓包里的原样操作数（`0x82000054`/`0x8200001c`，desc `0x00040600`/`0x00010080`）手工构造
命令链同样零像素；而 2D 窗口 `0x968000+` 的**读端口未实现**（读任何偏移都返回最后一次写入的值，
如全窗口读回 `0x8888`），无法用回读判断引擎内部状态。最可能的缺口是 DDX 在提交前通过 mmap
直接写的 2D 配置寄存器（不经过 ioctl 命令流，抓包看不到）。

结论：2D 仍不能用于 present 的整帧拷贝；本次改动让"引擎永不执行"变成"能执行命令流"，
是后续继续逆向 DDX 寄存器写（`tools/j2d/capture-vendor-engregs.sh` + kprobe `engine_write_reg`）
的基础。每次试验前必须做完整厂商复位序列（`0x96C200=FROM_AHB` → NOC idle → `0x9B0048`/`0x9B0014`
清置 → `0x96C800` CUREMASTER 握手），否则引擎一直不响应、并且反复 TDR 会拖死桌面。

### 2D 引擎补充：draw 语义已完全解出，卡点在引擎 bring-up（2026-11-16 晚）

厂商 DDX `~/vendor-src/usr/lib/xorg/modules/drivers/mwv207_drv.so` 反汇编后，填充/拷贝的
**全部编码**都已确定（`objdump -d -j .text` 落 `/tmp/ddx.asm`，20,286 行）：

| 路径 | 符号 | 编码要点 |
|---|---|---|
| 快速填充 | `jmgpu2dFillRectQuick 0x17be0` | `0x40001008,0` / `0x4000100c,depth_table[d-8]<<3` / `0x40001014,table` / `0x40001020,颜色` / `0x40001054,目标地址`，然后 `0x82000054` + `0` + `(pitch/16)<<16\|偏移` + `x\|y<<16` + `w<<16\|h` + `0x81000000`（26 字） |
| ROP 填充 | `jmgpu2dFillRectRop 0x17d60` | 同上，opcode 换成 `0x8200001c`，下一个字是 `.rodata 0x1f760` 里按 op 索引的 rop 值 |
| 拷贝 | `jmgpu2dCopyArea 0x17900` | `0x01008,0` / `0x0100c,fmt<<3` / `0x01014,fmt` / **`0x01010,目标地址`** / **`0x01018,源地址`** / `0x01054,偏移`，然后 `0x8200001c` + 7 个操作数（`fmt\|0x20000`、目标 pitch/坐标/宽高、源 pitch/坐标/宽高）+ `0x81000000` |

`depth_table`（`.rodata 0x1f6f8`）只对 8bpp 给 `2`、24bpp 给 `1`，其余 `0xffffffff`（无效）；
`jmgpu2dFillRect` 以 `[ctx+44]==3` 分派 Quick，否则 Rop。命令缓冲：`cmds[]` 从 `ctx+8` 起、
**字数在 `[ctx+0x1008]`**、`count > 0x3ac` 时 `drm_jmgpu_j2d_send_cmd(fd, ctx+8, count*4)` 后清零。

手工实验（BAR1/BAR2 直接写，不经 KMD）进一步确认了链格式的硬性规则：

- **链头必须是 4 个字（16 字节）** `{0x80000000, byte16_cnt, 0x80000000, 0x80000000}`：
  只写 2 字头时引擎会把第一个"寄存器 id/值"对当成头吃掉（实测 `0x6018` 未写、`0x601c` 写了）。
- **`byte16_cnt = 字数/4 - 1`，且整链长度必须是 16 字节倍数**：链 56 字节 + `byte16_cnt=2`
  时引擎静默不执行（不报错、不写同步寄存器）。
- **`0x81000000`（END）会终止执行**，其后的命令（含 fence 同步写）永远不跑 —— 这也是
  "提交总超时、输出全零"的直接原因，已在 KMD 里把尾部 END 换成 NOP。
- **link 目标必须在环内**（`0x40000–0x7ffff`）：指向环外（如 `0x300000`）时引擎静默不动。

测试探针也已修正：`tests/j2d-copy/j2dtest.c` 的 `submit()` 原来在 `poll()` 超时后仍然
`return 0`，把"ioctl 成功"当成"引擎执行成功"；现在超时返回错误并打印
"引擎没有执行本提交"。

**剩下的唯一卡点是引擎 bring-up，不是 draw 语义**：引擎把启动链跑完（`0x6004` 落到
`0x5555`）并停在 wait 上之后，驱动自己的 link patch + `0x4C00` 踢不再能唤醒它；手工
patch + 踢只在"刚被驱动 TDR 复位过的短暂窗口"里成功过两次（`0x6014=0x5571`、
六个同步写全落），随后同样失效；直接重写 `0x4200`（CMDADDR 指向另一条链 + MODE=WAITLINK）
也不执行。唯一能恢复执行的是完整厂商复位序列（`0x96C200=FROM_AHB` → NOC idle →
`0x9B0048`/`0x9B0014` 清置 → `0x96C800` CUREMASTER），而这条路径会同时冻结桌面
（本轮已两次导致重启），不能在生产环境里反复使用。

### 2026-11-16 深夜：唤醒路径的最后一轮对照（结论：驱动侧唤醒写不生效）

| 实验 | 结果 |
|---|---|
| 冷复位加 `0x96C200=FROM_AHB`（probe 时做一次） | 启动链照常执行（`0x96E004=0x5555`），**提交仍全部超时** |
| 在途 dump ring（BAR2 只读） | `0x40020: 89000000 00040030`（驱动已把 link 写进显存）、链头 `count=6`、`0x01054` 已被 reloc 成 `0x00b20000`、fence sync `40006020 00005553` 都在 ⇒ **链本身完全正确** |
| 驱动提交挂起期间，从用户态再踢一次 `0x4C00` | 不执行（同步寄存器不变） |
| 等 TDR 复位后再从用户态踢 | **执行**（`0x6014` 写成对应 fence 的同步值） |
| 用户态把 `0x4200` 改指另一条链 + 踢 | 不执行 |

结论：链、地址重定位、link 写入都已验证正确且**在显存里可见**；差异只剩"踢"这一条 MMIO 写。
驱动用的 `pipe_write()` 是 `writel_relaxed()`（`src/kernel-6.6/mwv207_sched.h:127`），厂商
`j9_antiroyalist()` 用的是带屏障的 `writel()`；用户态经 BAR mmap 写是 nGnRnE 强序。
所以最可能的剩余原因是**踢引擎的那一下没有真正排序/落地**（或引擎要求 GROUNDWAVE 的
边沿而非电平）。这也解释了"每次复位后的第一次唤醒能成功、驱动自己的提交永远失败"。

当晚最后一版（暂存 `F75C286B4A346C14AD85B53`，未再验证）把 link 的两个字改成经
`ioremap()` 的 Device 映射写（`pipe->ring_mmio`），仍保留 `writel_relaxed()` 的踢；
如果要继续，下一步应把踢改成 `writel()` + 回读 `0x4100` 强制完成，或把 GROUNDWAVE 写成
0→1 的显式边沿，再验证一次。

**最终对照（同日深夜，手工链）：**把厂商原样编码的填充链（`0x01008/0x0100c/0x01014/0x01020/0x01054`
+ `0x82000054` + 4 个操作数）写进环、patch link、踢 → **两个同步写都落到 `0x96E040/0x96E044`**，
说明引擎把整条链（含 fill opcode）从头执行到尾；但目标区域 `0x200000–0x240000` 里
**没有出现任何 `0x00ff00ff` 像素**（两种操作数形态都试过：厂商抓包的 `(0,dst,0x00040600,wh)` 与
DDX 反汇编的 `(0,pitch/16<<16,xy,wh)`）。

⇒ 结论：2D 的**命令语义已 100% 解出、引擎也会完整执行命令流**，但 draw op 的数据通路不写显存，
与 JPEG 解码同型——缺厂商闭源的那一步（最可能是 2D 数据通路要开私有 MMU，或某个未公开的门控）。
面板上的收益上限是 present 每帧约 0.29 ms，为此继续投入需要冒着冻结桌面的风险并反复重启，
已决定**收口不再推进**；`0x4200/0x4C00/0x4308/0x4800/0x6000` 的行为、链格式、DDX 编码、
探针与取证工具全部留档（`tools/j2d/README.md`、`tools/vendor-capture/`、本文件本节）。

留给以后的口子：`src/kernel-6.6/mwv207_pipe_2d.c` 目前暂存版（`B44D4CCC93E860CAE389DB7`）
把链内容与 link 都改为经 Device 映射写（`ring_mmio`），用于验证"驱动侧链写入可见性"；
该改动只影响 2D 管线（当前没人用），未在真机验证。

### 2026-11-16 傍晚：真正的唤醒条件是"link 目标必须 4 KB 对齐"（已修）

上面"只有完整复位序列才能唤醒"的判断是**错的**。给 `pipe_2d_wtol()` 加 `diag` 打印后
（`wtol off/gpu` + CPU/Device 两侧回读 + `kick` 后的 `globals/adstat/sync`）看到：

```
wtol off=0x20 gpu=0x40030 cpu=89000000/00040030 mmio_rd=89000000/00040030
kick: globals=20004000 adstat=00000000 sync=00005555
```

link 确实写进了显存（两侧一致）、globals 正确（CMDADDR=0x40000、WAITLINK、FROM_MEM），
但 `adstat` 恒 0、sync 不变。随后用手工链做二选一对照：

| 链起始地址 | 对齐 | 结果 |
|---|---|---|
| `0x41000` | **4 KB** | `0x96E004..0x96E010 = aaa1/aaa2/aaa3/aaa4`，整条链执行 |
| `0x40600` / `0x40500` / `0x40030` | 仅 16 字节 | 静默不执行，sync 不变 |

⇒ 引擎 follow link 时**要求 link 目标 4 KB 对齐**，此前所有"驱动链正确却不执行"的现象都由
此解释。修复：`src/kernel-6.6/mwv207_pipe_2d.c` 新增 `PIPE_2D_SLOT_SIZE 0x1000`、
`pipe_2d_slot_align()`，`pipe_2d_wait_for_space()` 返回 4 KB 对齐的槽基址，link 目标即槽基址
（单条链 > 4 KB 直接拒绝）。已验证 `wtol gpu=0x41000`、`kick: adstat=00000400`（2DBUSY 置位，
引擎开始解析）。

但**draw op 仍然不写像素**：

| 实验 | 结果 |
|---|---|
| 驱动提交（4K 对齐、链 28 字、fence sync 在 0x41050） | fence 仍超时，像素全 0 |
| 手工按厂商 sem1 抓包编码（`0x01008/0x0100c/0x01014/0x01020/0x01054` + `0x82000054` + `0,dst,0x00040600,wh`），链长 28 字、16 字节倍数 | sync 写入（`0x96E004=0x7c7c`）⇒ 整条链含 fill opcode 执行到底，目标 `0x43000` 仍全 0 |
| 三种寄存器布局（dst 放 `0x01054` / `0x01010`+`0x01018` / `0x01008`） | 只有 sync 写入，像素全 0 |
| 颜色改 `0xffff00ff`（alpha=0xff） | 仍 0 |
| 使能 2D 私有 MMU（`0x9E83B4=0x800`、`0x9E8388=0x15`，页表 0x80000 恒等映射 2048 MB） | sync 仍写入，像素仍 0；未冻结桌面，实验后已写回 0 关闭 |

⇒ 最终定性：2D 引擎现在**确实会执行命令流**（链路条件已全部查清：4 字头、16 字节倍数、
4 KB 对齐 link、END 终止、环内目标），但 fill/copy 的**数据通路不落显存**，与 JPEG 解码
同型——缺闭源的那一步。2D 仍不能用于 present，收口不变；本次的 4 KB 对齐修复与 `diag`
日志保留（它们让"提交永不执行"变成"能执行命令流"，是后续任何人接手的前提）。



### 2026-11-16 收尾：present 去重的 A/B 结论（不做）

2D 收口后，present 只剩"两次整帧拷贝"这条线索：客户端 `flush_resource` 把平铺面拷到
线性影子、Xorg 再拷到屏幕 pixmap。为了判断"消掉影子这一次拷贝"是否值得，直接做了
A/B（同一构建，`MWV207_SCANOUT_TILE` 控制客户端是否平铺；关掉即完全线性、没有影子拷贝）：

| 配置 | present 800×600（clear+swap） | glmark2 build |
|---|---|---|
| `MWV207_SCANOUT_TILE=1`（平铺 + 影子拷贝） | **0.399 ms/帧（2503 FPS）** | 1523 |
| `MWV207_SCANOUT_TILE=0`（全线性、无影子拷贝） | 0.483 ms/帧（2070 FPS） | 1564 |

⇒ 影子拷贝并不"白花"：平铺渲染省下的写带宽（PE 写平铺 38 GB/s vs 线性 6.7 GB/s）
比影子拷贝的代价更大，两套配置的 build 分数也在噪声内。**因此不做"去重"**；present 与
厂商的差距（0.014 ms）是架构级的——厂商走的是"客户端缓冲直接翻转、零拷贝异步 present"，
要做等价物得改 Xorg present/DRI3 通路，超出驱动可改范围。该项到此关闭。

### 回读路径（glReadPixels / VAAPI 下载）分析（2026-11-13 更正）：瓶颈是 DMA 引擎本身

`glReadPixels` 4 MB（1024×1024 RGBA8，r31 安装栈）稳定约 **4.7–4.9 ms**（≈0.87 GB/s）。
`MWV207_RBTIME`（临时插桩，已删）分段实测：

| 段 | 实测 | 说明 |
|---|---|---|
| 线性表面的 staging 分支 | 不触发 | pbuffer 表面本身是 `MWV207_LINEAR` |
| DMA 引擎拷贝 + `bo_wait`（4 MB） | **1.7–3.1 ms**（≈2 GB/s） | `mwv207_dma()`，`engine_type=0x4` |
| CPU 直接 memcpy 该 DMA 缓冲 | **0.5–0.7 ms**（≈8 GB/s） | 缓存被拷热之后 |
| Mesa 打包到用户缓冲 | **0.7–0.9 ms** | `mesa_copy` |
| 合计 | ≈4.2–4.8 ms | 与 `glReadPixels` 总时间吻合 |

**更正一处此前的错误结论**：先前记录"本机 CPU 大工作集读只有 1.94 GB/s"来自一个单位算错的探针
（`/tmp/mc2.c` 打印的 1939 GB/s）。用 `/tmp/mb3` 重测：**64 MB memcpy 6.38 ms ⇒ 9.80 GB/s、
256 MB 27.6 ms ⇒ 9.05 GB/s**（`memset` 32 GB/s）。所以 CPU 侧不是瓶颈，**真正的限速项是 GPU 的
DMA 拷贝通道（到系统内存约 2 GB/s）**。

**替代路径 A/B**：把回读拷贝从 DMA 引擎换成 3D/PE blit（`mwv207_blit_resource_box`，
`MWV207_RB_BLIT=1` 临时开关），实测 `dma_wait` 3.2–3.7 ms、随后 CPU 读该缓冲 18–25 ms
⇒ **明显更差**（PE 写系统内存比 DMA 还慢），不采用。

结论：`glReadPixels` / VAAPI 下载的 4 MB 读回 ≈ `DMA 2 ms + CPU 1.4 ms`，其中 DMA 段已接近
该引擎的极限（2 GB/s），驱动侧无低风险改法；已保留回读缓冲复用（省掉每次 4–16 MB 的 BO 分配，
实测 5–8%）。回读是截图/导出路径，不在实时渲染链路上，接受现状。

探针坑：① `memcpy` 结果不读会被编译器提出循环（`buffer-bench`/`bo-mmap-bench` 已加 `sink` 校验）；
② 4 MB 级工作集受缓存影响极大，引用带宽必须注明工作集与冷热；③ `bo_wait` 返回 bool（`true`=成功）。

### 后续（同日）：改走"平铺渲染 + 线性影子"，已修并实测

DE 不可达之后，换了个不依赖显示侧的做法：**窗口缓冲在驱动内按平铺+TS 渲染，
`flush` 时把内容拷到一份线性影子给 X/显示**。这套影子机制本来就在代码里
（`res->linear`、`resource_get_handle` 导出影子、`flush_resource` 同步），只是
`mwv207_can_tile()` 的 `enable_scanout_tile = false` 从未启用。

- 提交 `f486a1b`：`can_tile` 允许 SCANOUT 平铺；`flush_resource` 在 `sync_needed`
  时把平铺内容拷到影子；`mwv207_submit()` 对当前 framebuffer 的平铺附件置脏。
  **去重很关键**——一次 swap 里 `flush_resource` 会被调多次（`dri_flush` 的
  back_left 与 kopper `flush_frontbuffer` 的 front_left），不去重每帧约 3.8 次拷贝，
  800×600 多花 0.5–0.7 ms，全套 1173 vs 线性 1841（净亏）；去重后每帧一次。
- 实测（800×600，交替 A/B）：`jellyfish` 248→**738**（2.98×）、`buffer` 616→**995**
  （1.61×）、`pulsar` 1.35×、`conditionals` 1.22×、`function` 1.17×、`shading` 1.16×；
  `build`/`texture`/`bump`/`terrain` 各慢 2–5%（全是"写但不重叠"的场景，省不下过绘制）。
  `glmark2` 全套 1109–1236 → 1252–1254。
- 正确性：同场景截屏 md5 完全一致，`tests/query` 4/4。代价：窗口缓冲约 2× 显存。
- 载荷 `payload-43`（r26）已生成，安装后生效。

### 实测结论（2026-11-12，KMD `C588E9673F984406AF46A2F`，用 peek 口读取）

| 偏移 | 读出 | 判读 |
|---|---|---|
| `0x991400` | `0x00500030` | 与 `0x990400` **完全相同** ⇒ 地址镜像 |
| `0x991408` | `0x04100820` | 与 `0x990408` 完全相同 |
| `0x991518` | `0x00090004` | 与 `0x990518` 完全相同 |
| `0x991430`/`0x991440` | `0x0` | 若 DC 在位，HDISPLAY/VDISPLAY 不该是 0 |
| `0x990434`/`0x990438` | `0x1e0`/`0x41c00` | 主平面真实值：`0x1e0<<4 = 7680 B` = 1920×4 ✓ |
| `0x9A0434` vs `0x9A0434+0x1000` | 两者恒等 | **4 KB 解码窗口、bit12 被忽略**构成镜像 |

结论：`0x99 1xxx` 与 `0x99 0xxx` 互为镜像，厂商头文件里的
`DCREG_FRAME_BUFFER_CONFIG 0x1518` 在本机**根本不可达**；DC 的
`HDISPLAY/VDISPLAY` 读出 0，说明 DC 显示管道没有被编程（也没被厂商 KMD 编程，
75 个 `.c` 零引用）。而真正扫描出的是 VA 窗口块（`0x990434` pitch = 7680 B
精确等于 1920×4），**该块的寄存器表里没有任何 tile 位**。

⇒ **DE 平铺扫描出这条路在本机不通**：没有可达的 TILE_MODE 寄存器，也没有
可切换的显示管道。要拿到"窗口缓冲平铺"带来的 3–5× 写带宽，只剩
"驱动内先渲染到平铺 FBO、再 blit 上屏"（等价于在 UMD 里做一层小型合成器），
工作量与风险都远大于收益，暂不做。

保留 `peek` 调试口（只读、root 专属），后续查显示/引擎寄存器仍可用。

则平铺只能靠 VA 块，而 VA 块没有 tile 字段 ⇒ 此路不通，需改走「驱动内置
平铺 FBO + 上屏 blit」或接受结构性差距。

### 2026-11-17：厂商栈实测 2D 拷贝的真实命令形状（7 操作数，含两个额外面地址）

在厂商栈上（`jmgpu` KMD + 厂商 DDX）用 `bash tools/j2d/vendor2d-probe.sh` 抓
寄存器与 VRAM，再从 2D 环 `0x213000` 里直接读到 DDX 下发的拷贝命令（槽内 67 个非零字）：

```
0x0090: 40001010 01e001ae   40001018 02b90100   40001054 ffffffff
0x00a8: 8200001c 000200f0   01e0022c 02bd0700   00400700 01e001ae
0x00b8: 02b90100 00400700
0x00dc: 81000000
```

判读：`0x01010` = 目标裸地址、`0x01018` = 源裸地址、`0x01054` = `0xffffffff`；
`0x8200001c` 之后是 **7 个操作数**，形状为

| 序号 | 值（示例） | 判读 |
|---|---|---|
| 1 | `0x000200f0` | `fmt \| 0x20000`（固定） |
| 2 | `0x01e0022c` | 与 `0x01010` **相同**（目标面地址） |
| 3 | `0x02bd0700` | 目标面 pitch/坐标派生值（pitch 0x1e00 = 7680） |
| 4 | `0x00400700` | `(h<<16)|w`（64×1792 一类的条带） |
| 5 | `0x01e001ae` | **目标面基址**（比序号 2 小 0x7e） |
| 6 | `0x02b90100` | **源面基址**（比序号 3 小 0x600） |
| 7 | `0x00400700` | 与序号 4 相同 |

即一条拷贝命令里**同时出现"当前偏移地址"和"面基址"两组值**，我们此前只给了
`fmt + 地址 + 描述 + 宽高` 四个操作数，语义从一开始就错了。

为在厂商栈上做受控实验，新增两个工具：

- `tools/j2d/vsubmit.c`：直接走厂商提交接口
  `struct { void *cmd; u32 size; u32 flags } arg = { cmd, size, 1 };
  ioctl(fd, 0xc010644b, &arg)`，**fd 必须是 `/dev/dri/card0`**（`/dev/jmgpu` 会返回
  `Inappropriate ioctl for device`）。
- `tools/j2d/copytest.py`：写源/目标面 → 按上表形状发链 → 回读校验。

实测结论（两轮，`matched=0`）：**按这个形状复现仍然不写像素**；把环里抓到的
厂商原样命令逐字重放（`40001010 01e001ae … 81000000`，15 字）后目标窗口
字节数变化也是 **0**。说明还缺"命令流之外的每提交状态"，或引擎要求
源/目标面先经过某个 2D 侧资源登记（`0x01008/0x0100c/0x01014` 这三个 WREG 在
厂商命令里都是 0，`0x01054` 恒 `0xffffffff`）。

⇒ 现阶段 2D 只解到"命令编码 + 环格式"，**draw 数据通路仍未打通**；present 走 2D
仍不可行。后续若要继续，唯一有价值的方向是拿厂商 DDX 的 `jmgpu2dCopyArea` 入参
（EXA pixmap 私有结构里的 pitch/bpp/offset）与这两组地址对上，确认"面基址"到底
是 VRAM 地址还是某种句柄。

### 2026-11-17 凌晨：2D 数据通路取证收口（编码已通，数据面未解）

在厂商栈上用 `tools/j2d/livecap.py` 边跑 `XCopyArea` 边 dump 2D 环（1 MB / 256 槽），
稳定抓到每帧多条拷贝命令。把所有环里的 WREG 汇总，厂商每次拷贝只写这几个寄存器：

| 寄存器 | 值 |
|---|---|
| `0x01008` / `0x0100c` / `0x01014` | 恒 `0` |
| `0x01010` | 目标面地址（如 `0x01e0012f`） |
| `0x01018` | 源面地址（如 `0x022d0600`） |
| `0x01054` | 恒 `0xffffffff` |
| `0x006000` | `0xffffffff`（每次链首） |
| `0x006004..0x006020` | 同步值（`0x5555` 等） |

`0x8200001c` 的 7 个操作数规律：第 5 个 = `0x01010`、第 6 个 = `0x01018`、
第 4 个 = 第 7 个（几何量），第 2/3 个是另一组面地址（与 dst/src 差 `0xFD`/`0x1080` 量级）。

重放实验（全部 `matched=0`，目标面零字节变化）：

| 变体 | 说明 |
|---|---|
| 逐字重放抓到的命令 | 连厂商自己的参数也不写 |
| 自己的 src(标记图)/dst(清零) + 7 操作数 | 无写入 |
| 补 `o2=dst+0xFD`、`o3=src+0x1080` | 无写入 |
| 只写操作数 / 只写寄存器 | 无写入 |
| 加厂商链首 `0x81000000, 0x40006000, 0xffffffff, 0x40006004` | KMD 直接返回 `EAGAIN`（2D busy） |

对照：同一通道只发 `0x40006004, 0x1234` 时 `0x96E004 = 0x1234` ⇒ **提交通路本身没问题**，
缺的是 draw 数据面在引擎侧的上下文（最可能是厂商 UMD 提交前做的面地址重定位/窗口登记）。

**运维教训（重要）**：在厂商栈上用 `vsubmit` 反复发 2D 命令会把厂商 2D 引擎挂住，
`Xorg` 立刻进入 `R` 状态自旋、桌面冻结（本轮已发生一次，靠 `systemctl restart lightdm`
恢复）。⇒ **不要在厂商栈的在线桌面上做 2D 命令注入实验**；要试也只能在停掉 X 的
文本模式、且准备好重启的前提下做。

结论：2D 的"命令编码 + 环格式 + 提交通路"三项都已确认，唯独 **draw 数据面寻址**
仍是闭源知识；present 走 2D 不可行，该项到此封档。

### 1.11 VP9 12-bit 行为验证（2026-11-16）

自建样片：`tests/vp9-12bit/make-samples.sh` 用 `vpxenc --profile=2 --bit-depth=10/12`
生成 `~/vp9-assets/t10.webm` / `t12.webm`（320×180，各 12 帧）。

| 样片 | 结果 |
|---|---|
| `t10.webm`（10bit） | `ffmpeg -hwaccel vaapi` 正常出 1,036,800 B（12 × NV12 320×180），166–191 ms |
| `t12.webm`（12bit） | rc=251、输出 0 B、126–144 ms；stderr 一条 `mwv207: VP9 12-bit stream is not supported`，随后 libva `Failed to sync surface`、ffmpeg 正常报错退出；**无 TDR、无脏图、无挂死** |

顺带修了刷屏：12bit 的关键帧被拒后，后续每一帧都会命中"reference has no decoded
picture"检查；该日志改为**只播报一次**（`mwv207_dec_vp9.c` 的 `warned_no_ref`），
避免掩盖真正的首因。复跑入口 `tests/vp9-12bit/run-vp9-12bit.sh`。

### 火狐渲染异常（2026-11-17）：A/B 定位手段

**现象**：火狐特定页面（本地 GUI 的段落文字、侧栏标签）出现竖直彩色条纹、黑条叠加，重绘后不一定消失。复现依赖具体页面与滚动/重绘时序，agent 侧无法稳定触发。

**已排除**：字形图集小矩形上传与采样路径。探针 `tests/tex-upload/subrect.c`（1024²/2048²/4096² RGBA、每轮 400 次 2–20 px 不对齐 `glTexSubImage2D`、CPU 侧对照、FBO 读回 + 1:1 采样比对）在安装栈 r36 上连续 `RESULT: PASS (total mismatch=0)`。

**待定的三条候选**：
1. 客户端窗口缓冲"平铺 + 线性影子"（`mwv207_flush_resource()` 的单份影子拷贝）与 X 合成之间的时序；
2. N19 的持久映射免等待（`MWV207_BUFWAIT_ALL`）导致 WebRender 常量/顶点环回绕；
3. X 合成时机或 WebRender 自身。

**A/B 手段**：`tools/ff-ab.sh {base|notile|bufwait|all}`，每个模式独立 profile 启动火狐：
- `notile` = `MWV207_SCANOUT_TILE=0`（客户端窗口缓冲保持线性、无影子拷贝）；
- `bufwait` = 开发驱动 + `MWV207_BUFWAIT_ALL=1`（回退到 N19 之前"映射写前一律等待"）；
- 判读：`notile` 消失 ⇒ 影子路径；`bufwait` 消失 ⇒ 映射写竞态；都不消失 ⇒ 候选 3。


### 零拷贝异步翻转移植：C2 尺寸档已落地，C1 止损（2026-11-17）

按 `docs/HANDOVER-ZEROCOPY-PRESENT.md` 的方案先做 C2（纯 UMD）。改动在
`src/mwv207-umd/drivers/mwv207/mwv207_resource.c` 的 `mwv207_can_tile()`：新增
`MWV207_SCANOUT_LINEAR_MIN`（默认 `1280*720` 像素），客户端 `PIPE_BIND_SCANOUT`
资源达到阈值即判线性，跳过"平铺 + 线性影子"，`get_handle` 直接导出本体
（`DRM_FORMAT_MOD_LINEAR`）；Xorg 进程与阈值以下小窗口仍走平铺+影子。
`MWV207_SCANOUT_LINEAR_MIN=0` 可关掉分流做 A/B。

实测（r38 + 新构建，vblank_mode=0）：全屏 `glmark2 build:duration=8 --fullscreen`
从 **769 FPS / 1.301 ms**（平铺+影子）提升到 **943 FPS / 1.061 ms**（线性），
**+22.6% / −0.24 ms**，与"去掉影子拷贝净赚 ~0.25 ms"吻合；窗口模式 present
因合成器 glamor 拷贝掩盖基本打平（§2.2 已述）；`300²` 小窗口保留平铺不受伤。
`tools/gl-regression.sh` **10/10 PASS**，无正确性退化。

第二轮更新（2026-11-17）：C2 已用 `payload-49`（active-stack `oss-24.1.2-fullstack-r39`）
正式装进系统栈，C2 marker=1；系统栈全屏 glmark2 956 FPS/1.047ms vs OFF 788 FPS/1.270ms
（+21%/−0.22ms），`GL_DURATION=1 ./tools/gl-regression.sh` 10/10 PASS。

C1 的 KMD 前置条件已就绪：`switch-mwv207.sh kmd install` 后加载 srcversion
`C3A62E5C00106C402380ADE`（6.6.159，路径 `/lib/modules/6.6.159-arm64-desktop-hwe/updates/dkms/mwv207.ko`），
`overlay`=Y，`modetest` 确认每 CRTC 多一个 type=0 Overlay plane（id 35/42，AR24/XR24 LINEAR）。
`modetest`/`kmscube` 已 apt 装好。WIN0 与 >64×64 光标冲突风险**仅对厂商 jmgpu 驱动成立**——
OSS KMD 的 cursor 走独立寄存器（0x478 + 0x4B4/0x454/0x4A8），与 WIN0（0x990000+ 系列）无关。

**上屏硬件验证已通过（2026-11-18）**：用户在文本 VT 手动运行 `overlay-test`，屏幕出现
品红矩形 ⇒ overlay plane 可见、位置/尺寸正确、与主平面共存。至此 C1 的 KMD 硬件链路全部闭环。

**X 侧 present→overlay 补丁评估后不实施（止损）**：
- Xorg 用 modesetting DDX（`xserver-xorg-core 2:21.1.16`），`nm -D modesetting_drv.so`
  确认其**不导入 `drmModeSetPlane`**，Present flip 只走 `drmModePageFlip`（翻 PRIMARY）；
  让客户端窗口 flip 挂 WIN0 必须自编整个 xserver（改 present 路径），非 UMD/KMD 小改。
- 每 CRTC 只有一个 overlay plane，只能服务一个窗口；遮挡/不在顶层时显示错误内容，
  需额外 damage/可见性/z-order 跟踪。厂商 KMD 的 `DRM_PLANE_TYPE_OVERLAY` 也是空分支
  （`PRESENT-PATH-20261111.md` §13.1），即厂商亦未走通全链路。
- 工程量 2–4 周 + 中高风险，投入产出比远低于已落地的 C2（+21%/−0.22ms、10/10 PASS）。

最终交付维持 C2；C1 的 overlay 仅作为"能力原型"保留（模块参数默认 N）。详见交接文档 §10.4。

### 性能回归根因：glamor blit 快路径补丁被 ostree 部署切换冲掉（2026-11-18）

`glmark2 -b :duration=3`（800×600，vblank_mode=0）实测 **784**，厂商 1.8.0 记录 5209；
`present-bench` 800×600 swap-only **0.904 ms**，而本报告 §15.2 带补丁时为 0.248 ms。

根因不是 C2 或驱动回退，而是 **X 侧 `libglamoregl.so` 被换回发行版原版**：
运行中的 `/usr/lib/xorg/modules/libglamoregl.so` md5 `60ecd5a4…` 与较新 ostree
deploy `70b6d385…` 内的同名文件一致，两者都不含 `GLAMOR_NO_BLIT` 标记 ⇒
`patches/xserver/0001-glamor-blit-fast-path.patch` 已丢失；备份
`libglamoregl.so.mwv207-orig`（md5 `03fe0a6e…`）属 724bc2 deploy，同样未打补丁。

机理：线性目标上 glamor 着色器拷贝 6.7 GB/s（0.576 ms/800×600），
`glBlitFramebuffer` 17.3 GB/s（0.222 ms）；补丁把每个 box 的着色器拷贝换成 blit。

已处理：
- `./tools/build-xglamor.sh --build` 重建，产物 md5 `6a1c6822…`，含 `GLAMOR_NO_BLIT`；
  导出符号与系统版一致（45/45，无缺失），`ldd` 依赖全解析；已 `sudo` 安装到
  `/usr/lib/xorg/modules/libglamoregl.so`。
- **需重启 X 才生效**。注意：本会话的 DSH 服务跑在该 X 会话内终端里，重启 lightdm 会一并结束它。
- `switch-mwv207.sh umd check` 新增 glamor 补丁在位检查（只警告不改动），
  避免下次部署切换后再次静默退化。

补丁恢复后仍存在的结构性差距：
- present ≈0.25 ms vs 厂商 ≈0.05 ms（厂商 DDX 用 DRI3 + EXA/2D 引擎做 dirty 区域拷贝 + scanout flip）；
- 渲染层 offscreen `build` 0.277 ms vs 厂商 ≈0.05 ms（≈5×，PE/3D 管线效率）。

验证入口：`./tools/verify-glamor-patch.sh`（只读判定 + 实测）。它会检查模块是否含
`GLAMOR_NO_BLIT`、运行中的 Xorg 是否在补丁安装之后启动、并跑 `glmark2 -b build`
与 `glxgears`。当前基线（着色器路径，未重启 X）：build **945 FPS**、glxgears **2375 FPS**、
全量 glmark2 **784**；补丁生效后预期 build ~1600–2000。

### 每帧固定开销分解（2026-11-18，着色器 present 路径下测得）

用 /tmp/roundtrip.c（64×64 pbuffer，一个三角形）把一帧拆成三段，分离出
"CPU 构建命令 / 提交 ioctl / 同步等待"：

| 模式 | 每帧 | 增量 |
|---|---|---|
| 只构建、不提交（mode 2） | 40.5 µs | — 状态+命令构建 |
| 每帧 glFlush（mode 1） | 53.6 µs | +13 µs（submit ioctl） |
| 每帧 glFinish（mode 0） | 250.4 µs | **+197 µs（GPU 完成→fence 唤醒）** |

`draw-bench 1 200`（每帧 1 个 draw）同样给出 207 µs/帧；`draw-bench 20000 5`
是 0.40 µs/draw ⇒ 这 197 µs 是**提交往返延迟**，不是 GPU 吞吐（GPU 本身很快）。

glmark2 离屏（`--off-screen`，无 X present）基线，说明固定开销占比：

| 画布 | FPS | ms/帧 |
|---|---|---|
| 100×100 | 2383 | 0.420 |
| 300×300 | 2134 | 0.469 |
| 800×600 | 1739 | 0.575 |
| 1920×1080 | 965 | 1.037 |

100×100 就要 0.42 ms，其中约 0.25 ms 是上面那三段固定开销。厂商基线（报告）
build 5982 FPS=0.167 ms，差距的大头也在这里，而不在填充率。

**待查**：KMD `mwv207_submit_ioctl` 本身是异步的（返回 dma_fence），197 µs
应来自 job 完成中断→fence 信号→`sync_wait` 唤醒这条链路，值得单独 profiling
（IRQ 是否走了 threaded handler / workqueue 延迟）。这部分不阻塞 glamor 快路径
的收益，属于下一步优化。

### X 重启验证工具（2026-11-18）

`tools/glamor-restart-verify.sh`：重启 lightdm → 等新 Xorg 并观察 12 s →
起得来就跑 `tools/verify-glamor-patch.sh` 报告实测；起不来自动
`build-xglamor.sh --rollback` 还原并再重启，日志写 /tmp/glamor-restart-*.log。
支持 `--dry-run`（只检查前置条件）。重启 lightdm 会关闭当前桌面会话，
建议从 SSH/TTY 执行。

### glamor blit 快路径的上屏前验证（2026-11-18，用自建 Xephyr）

重启 X 是不可逆的（会关掉用户桌面），所以在重启前先用自建 Xephyr 验证了补丁
本身的行为——Xephyr 的 glamor 是静态链接的，可以从打过补丁的同一份源码构建：

1. 依赖：`apt install libxcb-render-util0-dev libxcb-image0-dev libxcb-icccm4-dev
   libxcb-keysyms1-dev libxcb-util0-dev libxcb-xkb-dev libxkbcommon-x11-dev
   libxcb-xv0-dev`
2. `meson setup build-kdrive ... -Dxephyr=true`（其余选项同 build-xglamor.sh，
   注意 -Dxephyr 默认 false）→ `ninja -C build-kdrive hw/kdrive/ephyr/Xephyr`
3. `Xephyr :9 -screen 800x600 -glamor -ac -no-host-grab` + 诊断补丁 0002
   （文件旗标 /tmp/mwv207-xg-dbg 打开打印，/tmp/mwv207-xg-noblit 运行时切换回
   着色器路径）观察 `MWV207DBG: blit=N shader=M | last: BLIT ...`。

结果：
- `blit=100 shader=0 | last: BLIT 200x100 -> 800x600 nbox=1` —— **Present 那种
  "客户端 pixmap → 屏幕 pixmap、尺寸不同"的拷贝确实走 blit 快路径**；
- 200x100 pixmap 上红下蓝拷到 800x600 屏幕，读回 (100,25)=ff0000、
  (100,75)=0000ff —— **跨高度拷贝没有 y 翻转**（这是该补丁最可能出错的地方）；
- 不逐次 XSync 的微基准：blit 0.010 ms/copy vs 着色器 0.021 ms/copy（Xephyr 内，
  含宿主 present 干扰，仅作同环境 A/B 参考）。

结论：补丁在本驱动的 glamor 里会被命中且方向正确，重启 X 的风险主要只剩
"模块与发行版 ABI 不匹配导致加载失败"，而导出符号 45/45 一致已排除该项。

### glamor blit 快路径生效后的实测（2026-11-18 10:00，Xorg 重启后）

Xorg 于 10:00:22 重启，日志确认加载 `/usr/lib/xorg/modules/libglamoregl.so`
（md5 `6a1c68221fa5`，含 `GLAMOR_NO_BLIT`）。同一台机器、`vblank_mode=0`：

| 指标 | 补丁前（着色器路径） | 补丁后（blit 路径） | 变化 |
|---|---|---|---|
| `glmark2 -b build` FPS | 945 | 1477–1520（3 次） | **+56%** |
| `glmark2 -b :` 全套 Score | 784 | **1215** | +55% |
| `glxgears` FPS | 2375 | 4208–4249（3 次） | **+78%** |
| present 300×300 clear+swap | 0.344 ms | **0.104 ms** | 3.3× |
| present 800×600 swap-only | 0.904 ms | **0.488 ms** | 1.85× |
| present 800×600 clear+swap | 0.931 ms | **0.500 ms** | 1.86× |
| present 1920×1080 clear+swap | ~2.83 ms（窗口） | **1.519 ms** | — |

`tools/verify-glamor-patch.sh`：`[OK] 模块含标记`、`[OK] Xorg 启动于补丁安装之后`
→ 结果通过。`GL_DURATION=1 ./tools/gl-regression.sh` → **pass=10 fail=0**。

逐场景（自研 / 厂商，FPS）：`build` 1477–1520 / 5982；`conditionals` 1662–1794 /
7589；`function` 1167–1479 / 6882–6990；`loop` 1128–1176 / 6875–6962；
`jellyfish` 770 / 1175；`terrain` 121 / 155；`refract` 106 / 123；`shadow` 906 / 1241。

结论（差距分层已按新数据修正）：
1. **轻载场景**（build/conditionals/function/loop）从落后 ~6–8× 收窄到 **~4–6×**，
   瓶颈全部是每帧 fixed overhead（present 0.50 ms + 生成侧 ~0.2 ms），已不再有
   "一次多余的全帧拷贝"这层。
2. **重载场景**（terrain/refract）落后 **1.2–1.3×**，`jellyfish` 1.5×、`shadow` 1.4×，
   与之前判断一致：属 GPU 填充率/ALU 与窗口线性缓冲回读，不是 present。
3. 下一刀应在 `present 800×600=0.50 ms` 里继续拆：其中 blit 理论值 ~0.22 ms
   （3.8 MB / 17.3 GB/s），余下 ~0.28 ms 是 swap/合成/同步；再往下就是结构性的
   "屏幕 pixmap 再拷一次"（需 DDX 零拷贝，已评估止损）。

顺带修了回归脚本：`tests/query/run-query-tests.sh` 在 `/tmp/mwv207-sim` 被清掉后
会退回系统 DRI 目录（`/usr/lib/aarch64-linux-gnu/dri`），不再误报 FAIL。

### 更正：提交链路的 kernel 侧其实很快（2026-11-18，kprobe 实测）

上一节把帧内 ~200 µs 记为"job 完成中断→fence 信号→sync_wait 唤醒"，用 kprobe
直接量了一下提交链路（`mwv207_submit_ioctl` / `mwv207_sched_run_job` /
`mwv207_pipe_3d_submit` / `mwv207_pipe_3d_isr` / `dma_fence_signal`）：

```
356200 mwv_ioctl → 356205 mwv_run → 356206 mwv_sub3d → 356211 mwv_isr → 356213 mwv_fence
```

即 **ioctl → 调度线程 → 提交 → IRQ → fence 信号 ≈ 13 µs**，相邻两次 ioctl 间隔
也才 35–45 µs。所以那 200 µs 不在内核提交路径上，大头是**等待/唤醒**：

- `glFinish` 为空操作时也要 ~90 µs（`GL_ARB_sync` 轮询到已完成的 fence 后再
  `glFinish` 仍要 231 µs）；
- 同进程加 `usleep` 再 `glFinish`，耗时随 sleep 增加而变大
  （0/200/1000/5000 µs → 79/134/529/793 µs），这是典型的 **CPU 浅睡眠后唤醒延迟**，
  在当前桌面（firefox 占 ~80% CPU、loadavg 3.4）下被放大；
- RT 优先级 + 绑核后仍有 ~90 µs/帧。

结论：**帧内固定开销主要是调度/唤醒延迟，不是 KMD 的某个 usleep 或 IRQ 处理慢**。
要再压这一层需要减少"每帧至少一次跨进程往返"的次数（合批 / 少 sync / 直接用
submit 返回的 fence），属于 UMD 侧行为改动，收益依赖机器负载，暂不作为本轮交付。

### 厂商栈"结构性零拷贝"取证与渲染吞吐量化（2026-11-18）

回答"为什么差厂商栈那么多、参考厂商栈怎么实现"。取证对象:
`ref/vendor-1.8.0/mwv207-dev_1.8.0.uos_arm64.deb` → `/tmp/vendor-extract/usr/lib/xorg/modules/drivers/mwv207_drv.so`
（未 strip，含 DWARF）与 `/usr/src/mwv207-1.8.0.uos` KMD 源码。

**厂商 DDX 结构（与"自编 xserver"无关）**
- NEEDED 只有 libgomp/libdrm/libdrm_jmgpu/libpixman/libgbm，**无 libglamoregl**；
  DWARF 里 `jmgpu_glamor.c` 仅 1 行 ⇒ 完全没实现 glamor。
- 导入符号**无 `drmModeSetPlane`**；有 `drmModePageFlip`/`drmModeSetCrtc`/
  `drmModeAddFB`(v1)/`drmWaitVBlank`/`PixmapStartDirtyTracking`/
  `PixmapSyncDirtyHelper`/`present_screen_init`/`dri3_screen_init`。
- 自研 **EXA DDX**：`jmgpu_exa.c` 2349 行、`jmgpu_drmmode_display.c` 5091、
  `jmgpu_present.c` 492、`jmgpu_dri3.c` 176。
- 路径 = 专用 scanout BO（`jmgpuDrmModeCrtcScanOutCreate/Destroy/Free`）
  + dirty 区域 2D 引擎拷贝（`jmgpuScanOutFlip`/`jmgpuScanOutUpdate`/
  `jmgpu2dCopyAreaHW`）+ `drmModePageFlip` 异步回调。
- `jmgpuPresentFlip` 反汇编：`jmgpuPresentCheckFlip` 放行 → `jmgpuFlushInDirect`
  → `jmgpuDoPageFlip`(带 abort/flip 双回调) → 清 pending。
- `jmgpuPresentCheckFlip` 只对覆盖整屏的单窗口放行，与 X 核心门禁一致。

**厂商 2D 命令通路（我方缺的那块）**
- `jmgpu_garbage.c:950-975` `j9_handle_j9m_muriculate()` 收到
  `struct drm_jm_j2d_send_cmd` 直接转 `j9_handle_insurrecto(gdev->p2d,
  args->cmd, args->size, nowait)`。
- `jmgpu_define.c:449` 只做 `size & 0x3` 校验 + `enginelock`，然后
  `p2d->engine->ops->input_cmds(engine, pcmds, size, no_wait, from_user=1)`
  ⇒ **内核不解析 2D 命令字，编码全在用户态**。
- 我方 KMD 有 ring/fence 框架（`src/kernel-6.6/mwv207_pipe_2d.c`，
  含 `module_param(diag, ...)`），但 UMD 侧无任何 2D 命令流；
  `docs/HANDOVER-2D.md` 记录"draw 数据面寻址"仍未解出。

**厂商 DDX 不可直接复用**
`nm -D --undefined-only mwv207_drv.so` 强依赖厂商私有 ioctl：
`drm_jmgpu_bo_create/destroy/mmap/export_to_fd/import_from_fd/get_handle/
invalidate/query/read_rect/write_rect/xfer_to_dev/xfer_from_dev/clean`、
`drm_jmgpu_create/close`、`drm_jmgpu_j2d_send_cmd`/`drm_jmgpu_j2d_wait_idle`。
我方 KMD 无这些 ioctl ⇒ 直接装厂商 DDX 起不来，复刻必须自写 DDX + 补 2D 通路。

**我方 present 已在走 page flip（kprobe 实测）**
五个探针（`drm_mode_page_flip_ioctl`/`drm_mode_atomic_ioctl`/`drm_mode_setcrtc`/
`drm_mode_addfb2`/`drm_mode_dirtyfb_ioctl`）：
- 1920×1080 全屏 override-redirect 连渲 2 s（4862 帧，0.411 ms/帧）→
  `pgflip=202 addfb2=202 atomic=0 setcrtc=0 dirtyfb=0` ⇒ **每帧一次 flip**；
- 800×600 同测法 → **全 0**（走 `present_copy_region()` 整块 CopyArea）；
- 静止 → 0（无泄漏）。
X 核心门禁: `/tmp/xorg-server-21.1.16/present/present_scmd.c:61
present_check_flip()` 要求 `RegionEqual(&window->clipList, &root->winSize)`，
**非全屏窗口在 X 核心里根本不允许 flip**；`present/present.c:66
present_copy_region()` 的 CopyArea 宽高恒为整块 pixmap，update region 只当裁剪。

**present 成本归属（自写探针）**
- `/tmp/filltex.c`（全屏 textured quad 画到 FBO）：800×600 NEAREST
  **0.2481 ms (1.93 Gpx/s)**、纯色 0.1273、LINEAR 0.2779；1920×1080 NEAREST 0.8939。
  ≈ 窗口态 present 净成本（0.482 − 0.189 = 0.29 ms）⇒ **present 就是一次全屏纹理采样**。
- `glBlitFramebuffer` 800×600 = 0.2053 ms (18.70 GB/s)，1920×1080 = 0.8607 (19.27)；
  与 `patches/xserver/0001` 的 17.3 GB/s 记录吻合。
- `glReadPixels`（DMA 回读路径）= **2.7440 ms (0.70 GB/s)**、1080p 9.4997 ms
  ⇒ **DMA 引擎做 present 拷贝这条路排除**。
- `XCopyArea` 恒定 ~22 µs/op 且与面积无关（200²=0.0206 … 1600×1000=0.0221），
  X 请求异步批处理直接掩盖 ⇒ 不是 present 成本来源。

**渲染吞吐量化（真正的大头）**
| 项目 | 数值 |
|---|---|
| 通用 draw 填充 800×600 | **5.9 Gpx/s (23.6 GB/s)** |
| 通用 draw 填充 1920×1080 | 6.2 Gpx/s (24.8 GB/s) |
| `glClear` RGBA8 800×600 | **458.9 Gpx/s (1835 GB/s)** |
| 重片元（300×sin） | **0.32 Gpx/s**（5.90 → 0.32） |
| discard / colorMask off | 7.36 Gpx/s（只省 20%） |
| 深度高 overdraw | **2.8× 惩罚**（2.5153 vs 0.9069 ms） |
| blending | 免费 |
| 顶点吞吐 | 饱和 ~810 Mverts/s |
| CPU memcpy 64MB | 12.6 GB/s（GPU 通用路径 23.6 已高于它） |
| glxgears 我方/厂商 | 6392 / 17621（2.8×） |

**early-Z 两个候选位实测无效（已回滚）**
按厂商头文件（`jmgpu_toolkit.h:26/135-147` `AQ_DEPTH_CONFIG_Address 0x01400`,
`EARLY` bit16；`jmgpu_protocol.h:679-684` `EX_EARLY_DEPTH_IN_APP` 0x382 bit5）
实现了"普通 LESS 也开 early_z + 补 bit5 + discard 反例保护"：
- 5 轮交替 A/B（`glmark2 -b build:duration=2`）：2072/2074、2070/2070、
  2073/2072、2072/2075、2070/2073 ⇒ **完全无差异**；
- `/tmp/earlyz N ORDER`（近→远全被深度拒绝）无改善，bit5 打开略慢；
- `/tmp/layers 1`（20000 三角形高 overdraw）2.5133 vs 2.5106 ms ⇒ 无差异；
- `GL_DURATION=1 ./tools/gl-regression.sh` pass=10 fail=0。
⇒ JM9100 上这两点不生效（可能不做 early-Z，或 tile 化整块提交使其失效）。
零收益且带 discard/alpha 正确性风险 ⇒ 已 `git checkout` 回滚，
`mwv207_draw.c` / `mwv207_state.c` 恢复原状。

**结论**：不需要自编 xserver 核心，但确实需要**新的 DDX 模块** + 2D 命令通路
（工作量在闭源的 2D 数据面编码）。已落地且性价比最高的是 C2 线性翻转
（+21–22%）与 glamor blit 快路径（+57%）；再往上要动 2D 引擎或 X 核心。
