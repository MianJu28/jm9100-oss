# 交接文档 — JM9100 (MWV207) 开源驱动适配（2026-11-10）

> 目的：把当前进度、环境、方法学与"下一步做什么"完整交给下一个会话。
> 本文档随进度更新；未修问题权威清单在 `docs/UNRESOLVED.md`（含 2026-11-10 的 §0.1 速览）。
> VP9 专项见 `docs/HANDOVER-VP9.md`。

## 1. 当前状态（一句话）

**r19 已部署并验收通过**（`payload-42` / `24.1.2-fullstack-r19`，`umd status` 显示活动栈
`oss-24.1.2-fullstack-r19`、快照 `20261110-132539`、安装目录 219M；当前启动内核
`6.6.155-arm64-desktop-hwe`；本文早前写的 6.6.143 是升级前的内核，两个版本的 DKMS
模块都已同步重建）。
桌面零 env 下 GL 4.0 / GLES 3.2 / `Video memory: 2048MB`，glxgears 110 FPS；
OpenCL/VA-API/VDPAU 全走我们的栈；自建 KMD 在跑（srcversion `D5EEF36617A4D3FBF4AFD1D`，
含 N14 的 reset/fence 修复与 N1/N2/K12/K9-②/N9 批次，**2026-11-10 13:29 重启后已验证生效**；
再下一批 N4（DMA 异步完成 + 真 fence）/K11（中断窗口 + `pci_disable_device`）即
`0EFB7F4433EA5311B08E88C`，2026-11-10 14:36 重启后**已验证生效**（一键验收
`tests/verify-installed.sh` 22 项全过）；DKMS 包 `mwv207-oss/1.0.0`）。**VP9 Profile0 8bit 硬解已与软件逐位一致**（回归 18/18，
含真实 720p/1080p 影视内容）。**Firefox 两处渲染缺陷已根治**：字形/图标撕裂（`f8f693a`）、
标签栏"微缩 chrome"（`6ec1e17`）。剩余项见 §5。

## 2. 系统与仓库布局

| 项 | 值 |
|---|---|
| 机器 | Deepin 25 aarch64，内核 `6.6.155-arm64-desktop-hwe`（143 也装了 DKMS 包，但不是启动内核），GPU `[0731:9100]` PCI 07:00.0，HDMI-1 1920×1080@100Hz，无免密 sudo（用 `printf '666\n' \| sudo -S`） |
| 仓库 | `/home/admin/jm9100-oss`（git，远端 `git@github.com:MianJu28/jm9100-oss.git`，已推送同步） |
| 源码 | `src/mwv207-umd/`（UMD，唯一可改处；上游树 `ref/oh-mesa/.../src/gallium/{drivers,winsys}/mwv207` 是指向它的符号链接）、`src/kernel-6.6/`（KMD，已并入本仓库） |
| 已装栈 | `/usr/local/lib/mwv207-oss/`（运行中 **r20**：DRI md5 `39f38c6f5fd2f5ec64d0b90459f31e89`、VA md5 `fa63148dfb01b97461897106a4591346`、EGL_mwv207 `d038036a…`、vdpau `f10fcec15f26…`、OpenCL `a3fb2f47559b…`；`payload-41`=r16/`payload-42`=r20 可回滚），`/lib/modules/…/updates/dkms/mwv207.ko`（自建 KMD，运行中 `8DFC520FB8FCDECFBC911AA`；2026-11-10 17:55 重启后已验证） |
| 载荷 | 运行中 `payload-42`（`24.1.2-fullstack-r22`，含 I-1…I-6 + N18；2026-11-11 09:09 安装，DRI `2b62dd7a752e73c8c474efd9ece696c8`、VA `7424586ec2b6edb53c82d283d24632a6`）；**I-5 修复（`8f6e964`）需重新组装 r23 才能在安装栈生效**；**安装需用户在自己终端跑 `sudo ./switch-mwv207.sh umd install`（会重启 lightdm）**；`payload-41`（r16）为上一份可回滚点，历史快照在 `/var/lib/mwv207-oss/switch-history` |
| 快速回归副本 | `/tmp/gd3/`（测试用；`/tmp` 是 7.8G tmpfs，重启即空，记得清理大文件）。实验构建流程：改 `src/mwv207-umd` → `ninja -C ref/oh-mesa/User_source_code/mesa-24.1.2/build-full src/gallium/targets/dri/libmwv207_dri.so` → `cp` 到 `/tmp/gd3/` → 用 `LIBGL_DRIVERS_PATH=/tmp/gd3 MESA_LOADER_DRIVER_OVERRIDE=mwv207` 测 |
| 出包安装 | `./tools/make-payload.sh -v 24.1.2-fullstack-rN` → `printf '666\n' \| sudo -S ./switch-mwv207.sh umd install --force`（**install 调用常显示 interrupted，实装已完成——必须用 `status` + md5 比对确认，勿盲目重跑**） |
| 构建陷阱 | 性能实验必须用 `build-full`（`build-glvnd` 是旧二进制）；`meson setup --wipe` 会把 glvnd 从 auto 变 enabled → 必须显式 `-Dglvnd=false -Dgles1/2=enabled` |

## 3. 已完成里程碑（提交索引）

- 全功能栈（GL/VAAPI/VDPAU/rusticl）、GLES 3.2 原生点亮、自建 KMD 真机运行、
  切换脚本统一（`switch-mwv207.sh`，umd/kmd/all 三层）。
- **Firefox 图元全丢**：根因 = VS 里 RGBA32F `texelFetch` 只回 64 位
  （`nir/mwv207_lower_128bit()` 只对 FS 生效）→ 提交 `bda3749`（取样）+
  `d7c52a3`（重编译记录），最小复现
  `tests/vstf-128bit/`（`/tmp/wrreplay` 运行器，判据 = 与 llvmpipe FB_HASH 逐字节一致）。
- **Firefox 性能**：① 重编译判据误用 `ctx->dirty`（每次 submit 被重置为全量）→
  "128 位模式指纹参与着色器缓存键"（`mwv207_shader.c:985-1015`，`built_fp_*`）；②
  128 位纹理 split/merge 对 WC 显存逐标量拷贝（~100MB/s）→ 逐行 memcpy。WebGL 36→42+
  FPS（llvmpipe 28.5），CSS 页 97-99 FPS（vsync 上限）→ 提交 `d7c52a3`。
- **GitHub 概率性崩溃**：dual-source blend 的 `sampler_index`（`num_samplers`）与
  视图槽位（`num_sampler_views`）不一致 + 无判空 → SIGSEGV `get_rt_texture_coord`
  → 提交 `40d5d76`。
- **健壮性批次 1/2 + GBM/Wayland**：W6/W7/W12/E2/E4/E7/E9/E10/F.3.2/F.3.3/O4 +
  GBM EACCES（render 节点回退）→ 提交 `841e300`。玲珑 CDI 冲突修复（EGL ICD
  原地改写，**不要删厂商 `10_mwv207.json`**）。
- **A 类修复（`5dee412`，r12 已部署）**：E5 双面模板双重互换（0x528 恒取
  `stencil_back_reference[0]`）、F.3.1 反汇编 type 位序（`(bit0<<2)|bit1_2`）、
  F.4 三条（link_shader 返回 true / shared_atomic 判空 / RA 收敛上限 32）、
  O5 失败 fence（`video_cs.c` 对 fd<0 不创建 fence）。E6 复核结论：**不改**（与
  llvmpipe 同为 CPU 时钟语义）。
- 显存修正 `PIPE_CAP_VIDEO_MEMORY` = 2048（KMD 实测 2GB）；显存单位历史教训：
  该 cap 单位是 MB。
- **10bit HEVC 解码面（`f0c7605`，r13 已部署）**：`mwv207_is_video_format_supported()`
  对 P010/P016 改为按 `R16_UNORM`/`R16G16_UNORM` 的 `SAMPLER_VIEW` 支持取 AND（仍与
  profile 白名单取 AND）。真机 640×360/30 帧 Main10 解码 + `vaExportSurfaceHandle`
  （DRM_PRIME_2）导出与软件解码逐位一致；8bit HEVC/H264 回归同样逐位一致。**GL 侧
  16 位采样恒 0（C1）仍在**：2026-11-03 已定位为 0x1F76 不解码
  `TX_EXT_CONFIG.FORMAT` 码 0x32–0x36（详见 UNRESOLVED C1），10bit 帧在 GL 里
  采样/缩放仍不正确。

## 4. 关键方法学（重复踩过的坑，务必遵守）

1. **探针必须先在 llvmpipe 上跑出"符合预期"**才能用来判定我们的驱动；判据始终是
   "两驱动逐条/逐字节一致"，不要用自己手算的期望值。
2. 桌面 GL 2.0+ 入口必须 `eglGetProcAddress`（`PFNGL##U##PROC` 拼名要与 GL 头逐字
   一致；GL 1.1 入口直接调用）；`glUniform4f` 不加载会因 double 传参出垃圾 uniform；
   纹理必须设 `MIN_FILTER`（默认 mipmap → 不完整 → 采样恒 0）；探针缓冲区按格式
   尺寸分配（`uint4` 这类 typedef 在该上下文不存在）。
3. 驱动回调内**禁止**调 `texture_map/buffer_map`（SIGSEGV）；BO 读回用
   `ws->bo_wait + ws->bo_map`（见 `mwv207_query.c:245-248`）。
4. 测 FPS 前必须 kill 所有 `/usr/lib/firefox/firefox` 并用 `xprop -id <win>
   _NET_WM_PID` 杀 `Firefox Crash Reporter`，标题匹配用 `/tmp/title.sh`；直方图裁切
   用 `xwininfo -id` 的真实几何（历史教训：固定 crop 量错窗口）。
5. `pkill -f` 会杀自己的 shell；按 `readlink -f /proc/<pid>/exe` 判定。
6. core dump：`echo "/tmp/coredir/core.%e.%p" > /proc/sys/kernel/core_pattern`
   （崩溃在 GPU/Renderer 子进程，gdb 跟父进程抓不到）。
7. `glmark2` Score 波动 704-1299，**不能当回归判据**；回归用
   `tests/query/run-query-tests.sh`（需 `DRI_DIR=/tmp/gd3
   LD_LIBRARY_PATH=/usr/local/lib/mwv207-oss`）+ wrreplay 三组 + glxinfo + 火狐
   diagpage 直方图（红 2.3%/绿 3.0%/蓝 2.5%/黄 2.0%/白 9.3%）。
8. `docs/UNRESOLVED.md` 与 `api/11-FIX-FEASIBILITY.md` 的历史结论会过期，采信前实测复核。
9. 测试装置：`/tmp/fps.html`（FPS 写进 document.title）、`/tmp/css.html`、
   `/tmp/diagpage.html`、`/tmp/ffp4/user.js`、`/tmp/hist2.py`、`/tmp/ts4.sh`、
   `/tmp/swapbench`、`/tmp/wrshade/`、`/tmp/wrreplay`、`/tmp/tsprobe`。清理时保留。

## 5. 剩余工作（按优先级，2026-11-10 复核）

### 已完成（不再需要做）
- **VP9 Profile0 8bit 硬解**：单 tile / 多 tile 列 / tile rows / 分段 / show_existing /
  概率自适应 / 时间 MV / ALTREF 全部逐位一致；`tools/vp9-regression.sh` 18/18；
  Big Buck Bunny 720p/1080p 整条逐字节一致。提交链 `5c6d05e`→`f8f693a`，
  细节见 `docs/HANDOVER-VP9.md`。
- **Firefox 渲染两处根因**：字形/图标撕裂（`f8f693a`，上传暂存 BO 延迟释放）、
  标签栏"微缩 chrome"（`6ec1e17`，buffer 写入前等待 GPU）；mesa 侧
  `loader_dri3_query_buffer_age()` 恒 0（`ref/oh-mesa` `248659c`，仅本地提交）。
- **KMD 自建 + DKMS**：`mwv207-oss/1.0.0` 已注册，运行中 srcversion 与构建一致；
  切换脚本三态（builtin/oss/vendor）已实现，失败回滚 builtin。
- **N14（KMD，`22145e9`）**：3D/2D/DEC/ENC 的 `reset`/`destroy` 不再丢弃在飞 fence ——
  统一 `dma_fence_set_error(-EIO)` + signal；DEC/ENC 用 `xchg(&current_fence, NULL)`
  与轮询线程抢槽，杜绝双重 put。已在 2026-11-10 重启后生效（运行中即
  `039F972EFE3279A155EE225`），重启后 glxinfo / query 4·4 / VP9 18·18 / glxgears
  117 FPS 全过；reset 通路已用 `tests/n14-reset/` 探针实测：6 轮 TDR、
  `pipe_3d_force_complete` 命中 6/6、用户 fence 12.3 s 完成、复位后 GPU 正常。
  2D/DEC/ENC 与 destroy 路径仍未被探针覆盖，见 `docs/UNRESOLVED.md` §2.1。
- **KMD 加固批次（`53b3229` + `c01af40`，已重启验证生效）**：20 处 `BUG_ON` 收敛——用户
  提交路径上的（`mem_type` 非 VRAM/TT、`reloc->idx` 越界、DMA `cmd_size` 不符、3D 事件
  槽耗尽）返回 `-EINVAL`/`-EIO`/`-EBUSY`，寄存器访问器与 3D ring 描述符改
  `WARN_ON_ONCE` + 跳过动作（受影响的 job 由 2 s TDR 回收，N14 已保证在飞 fence 会被
  signal）；写配套测试时又抓到 **N16**——畸形 submit 会让 `mwv207_job_fini()` 对未分配的
  `cmd_dats` 空指针解引用（实测内核 oops `[#11]/[#12]`），已修。新增
  `tests/bad-submit/`（9 个用例）。2026-11-10 16:21 重启后运行中即为
  `5A5B401802B79D560151210`，`tests/verify-installed.sh … --bad-submit` 全过
  `PASS=25 FAIL=0 SKIP=1`；畸形提交 9/9 干净返回 errno、dmesg 零 oops。
- **N17（`1fc9b21`，已重启验证生效）**：`5A5B4018…` 上普通桌面负载即触发
  `refcount_t: addition on 0; use-after-free`（`mwv207_pipe_dma_submit+0x77c`）——
  N4 的异步 DMA 把给调度器的 `dma_fence_get()` 放在 `queue_work()` 之后，worker 抢跑时
  fence 已被回收。轻则引用计数污染/泄漏，重则 parent fence 被回收、job 永不完成，用户态
  停在不可中断等待（与 16:30 整屏卡死、Xorg 只能 SIGKILL 的症状吻合）。两内核与两
  initramfs 已装 `8DFC520FB8FCDECFBC911AA`；17:55 重启后按 §2.6 复核通过（PASS=25、kprobe 压测 submit/complete 2052·2052 平衡、`refcount_t` 告警 0、X 最大延迟 5 ms）。
- **KMD 结构/边界批次（`6502f19`，已重启验证生效）**：N1 sched 槽位撞车（6 槽放 7 个
  引擎，enc0 覆盖 dec1）→ `MWV207_MAX_SCHED_NR 7` + 显式下标；N2 3D `hw_submission`
  30→28（等于事件槽数会命中 `BUG_ON(event >= 30)`）；K12 超时路径 devfreq 多减 →
  新增 `mwv207_devfreq_reset_busy()` 清零；K9-② ISR 影子寄存器改
  `READ_ONCE`/`WRITE_ONCE`；N9 显示侧两处 `BUG_ON(irq == 0)` 改可回退错误。
  两内核已装、initramfs 已刷（srcversion `D5EEF36…`）。2026-11-10 13:29 重启后
  运行中即 `D5EEF36617A4D3FBF4AFD1D`，glxinfo / query 4·4 / VP9 18·18 /
  H.264 8bit 逐位一致 / `tests/n14-reset/` 6·6 / dmesg 无 WARN·BUG 全过；
  引擎计数用新增的 `tests/engines/run-engines.sh` 核对（本机 lite：
  3d=1 dec=1 enc=0 2d=1 dma=1，见 `docs/UNRESOLVED.md` §2.2）。
- **KMD 批次 3（`fbed27f`，2026-11-10 14:36 重启后验证生效）**：N4 —— `pipe_dma_commit()` 原来在 drm_sched 的
  `run_job` 线程里 `cpu_relax()` 自旋最多 5 s，再返回 `dma_fence_get_stub()`（提交即已
  signal）。改成 submit 只启动引擎 + 返回真实 fence，完成/清通道/失效缓存/signal 交给本管线
  自己的 `WQ_MEM_RECLAIM` 有序队列睡眠式轮询（上限 1500 ms < 调度器 2000 ms 超时），
  reset/destroy 按 N14 范式 `cancel_work_sync()` + xchg 以 `-EIO` 收尾；K11 —— `irq_fini()`
  在 `free_irq()` 前把 `0x009A8020/0x24` 使能位清零、按 W1C 清挂起状态（关掉 nobody-cared
  窗口），`pci_remove()` 在 `pci_clear_master()` 后补 `pci_disable_device()`。K9-① 经与前
  一版厂商源码对照确认是 W1C 标准清位、**不是缺陷**；N10 决定只文档化（见
  `docs/UNRESOLVED.md` §2.3）。
- **UMD 视频批次（`385ed81`/`a5a938b`/`4ecca0d`，载荷 r20 已安装）**：I-1 HEVC tile 缓冲按真实帧高
  分配（原先拿 `bit_depth_chroma_minus8` 当高度，8bit 时高度为 0）；I-2 H.264 路径删掉
  `set_reg(BITPLANE1_E, 0)`（与原厂 1840 行表核对：BITPLANE0/1/2 与
  CABAC/BLACKWHITE/DIR_8X8_INFER 在 word7 上逐位同名同址，是按 codec 复用的物理位，
  该句把单色标志清掉）；I-3 收回 H.264 High422/High444 声明 —— 寄存器/原厂驱动/libva
  三重证据表明本机只有 4:0:0 与 4:2:0（细节 `docs/UNRESOLVED.md` §1.5）。
  验证：HEVC 8bit/Main10 2 tile 列逐位一致、H.264 High 420 逐位一致、VP9 18/18、
  query 4/4。**I-4：H.264 High10 已定案并收回声明** —— 实测 10bit 样片会让
  DEC 管线连报 6 次 `job from 'av:h264:df0' to 'mwv207_dec0', timed out!`
  （TDR 回收、输出全 0），缺 Hantro 的 high10p 位流预转换；原厂闭源驱动也不
  声明 High10。收回后 ffmpeg 对该类流直接回退（`hwaccel initialisation
  returned error`），DEC 零超时；`end_frame` 另加 `bit_depth_luma_minus8 > 0`
  跳过分支。见 `docs/UNRESOLVED.md` §1.2。
- **I-6：被跳过的帧不再静默返回脏图（2026-11-10，`bb5cd9e`）**：VP9/H.264 里所有
  "这一帧没法解"的提前返回（参考缺失、尺寸不符、参数不支持、分配失败、分区尺寸异常）
  以前只清 `JMDEC_DEC_E` 就返回，PPU 输出面根本没编程，而 `vaSyncSurface()` 照样
  成功 —— 应用拿到的是目标 surface 的旧内容（B2 的 resize_inter 向量就是这样）。
  现在 codec 层置 `frame_skipped`，`end_frame()` 不提交、改置 `frame_failed` 并给
  `*picture->fence` 一个 fd=-1 的桩 fence，新增的 `get_decoder_fence()` 在失败时
  返回 0，Mesa 的 va 前端据此让 `vaSyncSurface()` 回 `VA_STATUS_ERROR_TIMEDOUT`
  （ffmpeg 侧 `Failed to sync surface`）。回归：VP9 P2 10bit / show_existing /
  res14 / HEVC 8 / H.264 8 全部逐位一致。
- **I-5：VP9 intra-only 之后的 inter 帧解码错（2026-11-11，`8f6e964`）**：官方
  `vp90-2-16-intra-only.webm` 上 4 个真实解码帧与 libvpx 差 15–22 阶（64×64 块级
  损坏）。三处根因：① `vp9_setup_past_independence()` 只对 key/error-resilient 生效，
  漏了 intra-only（libvpx 对 `frame_is_intra_only` 同样重置概率表、lf delta、段特征
  并把 `frame_context_idx` 归零）；② 自适应基准应取"刚重置成默认表的
  `frame_contexts[fci]`"，我们固定用帧头更新前的保存表；③ `show_existing_frame` 的
  1 字节包被解析器早返回后，调用方又把它当压缩头"解"了一遍，函数尾的
  `saved_ctx[fci] = sh->entropy` 把垃圾写进保存的概率表（重放帧也不清系数计数）。
  修后官方向量 **7 个真实解码帧全部与 libvpx 逐字节一致**，本机 20 帧 `-g 5`、
  `-frame-parallel 0` 样本 20/20 一致；只剩 3 个 `show_existing` 重放帧仍是黑图
  （独立限制：整条解码+PPU 由 `JMDEC_DEC_E` 启动，重放不启动 PPU）。
  定位工具：`tests/vaapi-client/vadec.c`（直连 libva）+ `tests/vp9-intraonly/`。
- **一键验收脚本（2026-11-10，`tests/verify-installed.sh`）**：把 KMD 版本/来源、
  glxinfo/query/引擎计数、VAAPI profile 归位、H.264-High/HEVC 8/10bit 逐位一致、
  High10 回退与 DEC 零超时、TDR 探针、dmesg 新增告警（带基线，只查本次运行新增行）
  串成一条命令；`kernel.dmesg_restrict=1` 时明确 SKIP 而非假 PASS。已在 r19 + `D5EE…`
  上预跑通过（root 全跑 `PASS=21 FAIL=0 SKIP=1`）。

### 待办（按价值排序）
1. **A5 已完成（2026-11-10，`82e7b02`）**：VP9 Profile 2（10bit 4:2:0）与软件解码逐位
   一致（64x64~1080p，含 inter 帧，零 TDR），默认声明；`MWV207_VP9_10BIT_DISABLE=1`
   可关。4:2:2/4:4:4 定案不可实现（解码器无色度采样格式选择字段）。12bit 未验证。
   这一步**只需装 r20 载荷**，不需要改内核。
2. **B3 / I-5 已完成（2026-11-11，`8f6e964`）** —— 官方 intra-only 向量上 7 个真实
   解码帧全部与 libvpx 逐字节一致，诊断脚本 `tests/vp9-intraonly/run-vp9-intraonly.sh`，
   细节见 `docs/UNRESOLVED.md` §1.7。剩下的只是 `show_existing` 重放帧的黑图问题
   （PPU 不经 `JMDEC_DEC_E` 就无法启动），属独立课题，不在 B3 判据内。
3. **B2：真正的跨尺寸参考** —— 已有"缺参考/尺寸不同即拒绝"保护（`72f9523`），且
   I-6 已修（`bb5cd9e`）：被跳过的帧现在会显式失败（`vaSyncSurface` 报错、ffmpeg 停止
   输出），不再静默出脏图。要真正**支持**跨尺寸参考，还需要自写 VAAPI 客户端验证
   "参考自身几何 + hscale/vscale"路径（ffmpeg 在分辨率变化时会重建 VA context，绕不过去）。
4. **其余 KMD 项**：N1/N2/K12/K9-②/N9 已修并重启验证（`6502f19`）；N4/K11 已修、
   **重启验证生效**（`fbed27f`，运行中 srcversion `0EFB7F4…`；判据见
   `docs/UNRESOLVED.md` §2.3）；K9-① 已核实为与厂商一致（非缺陷）；20 处 `BUG_ON`
   已收敛（`53b3229`）＋ 新发现的 N16 已修（`c01af40`），运行中已是 `5A5B4018…`、
   **重启后验证生效**（PASS=25，§2.4/§2.5）。另在 16:41 抓到由 N4 引入的引用计数缺陷
   **N17**（异步 DMA 在 `queue_work()` 之后才 `dma_fence_get()`，worker 抢跑时对已释放
   fence 做 `refcount_inc`），已修（`1fc9b21`）、两内核装为 `8DFC52…`，**17:55 重启后验证生效**
   （§2.6）；它与 16:30 那次整屏卡死（Xorg 收 SIGTERM 不退、只能 SIGKILL）症状一致。
   只剩 N10（决定只文档化）。
   **一条命令验收**：`printf '666\n' | sudo -S ./tests/verify-installed.sh 8DFC520FB8FCDECFBC911AA --bad-submit`
   （`--no-n14` 跳过 TDR 探针）。2026-11-10 14:36 重启后实跑：
   `PASS=22 FAIL=0 SKIP=1`（SKIP 为 TDR 探针后预期的 6 条 `timed out`），
   另测 glxgears 102.7 FPS、`tools/vp9-regression.sh` 18/18。
5. **vendor 态端到端验证**：`stack vendor` 会重启，尚未人工实跑；热切换已证不可行
   （`mwv207` refcnt 恒为 2，只有 reboot 才释放）。
6. **性能**：buffer 写入等待的吞吐代价只做了 glxgears 观测（110 FPS 无回退），重负载未量化；
   Firefox WebGL 42–44 FPS 的剩余瓶颈在 WebRender 帧调度。
7. **UMD r20 已安装并复核**：`payload-42`（`24.1.2-fullstack-r20`）2026-11-10 16:17 安装，
   `umd status` 显示活动栈 `oss-24.1.2-fullstack-r20`、快照 `20261110-161759`；
   `md5sum /usr/local/lib/mwv207-oss/libmwv207_dri.so` = `39f38c6f…`、
   `libmwv207_drv_video.so` = `fa63148d…`。重启后复核：`vainfo` 只剩 ConstrainedBaseline/
   Main/High + HEVC Main/Main10 + VP9Profile0 + None(VideoProc)；H.264 High10 样片
   `-hwaccel vaapi` 报 `Failed setup for format vaapi` 正确回退（DEC 零超时）；
   HEVC 8bit 640×360 逐位一致。回滚点 `payload-41`（r16）。
8. **DKMS 一致性**：改 KMD 源码后必须重跑 `tools/dkms-sync.sh`（它会 dkms build/install
   两个内核）。注意 `switch-mwv207.sh kmd stage` 会用仓库构建的 .ko 覆盖 DKMS 安装路径，
 `dkms status` 会因此重新报 `Diff between built and installed module!`——差异来自
   DKMS 的 MOK 签名与构建方式，功能等价；要恢复干净状态，再跑一次
   `dkms install -m mwv207-oss -v 1.0.0 -k <ver> --force` + `depmod -a` +
   `update-initramfs -u -k <ver>`（2026-11-10 已这样处理 6.6.155/6.6.143，两处
   解包后的模块 srcversion 均已核对一致（当次为 `0EFB7F4…`）。

### 不建议投入（沿用 2026-11-03 结论）
C-2 16 位采样编码（硬件边界）、D7 Vulkan、C-5/C-6、N10、openuuyc 单实例 UX。
原先"chrome 文字错位（用户已搁置）"一条已随 `f8f693a`/`6ec1e17` 修复，可删除。

## 6. C 类三项（2026-11-03 已拍板：全部保留声明 + 注明）

| # | 问题 | 选项 |
|---|---|---|
| C1/C3 | R16G16_UNORM/R16_SNORM/R16G16_SNORM 采样恒 0。**2026-11-03 已定位为硬件边界**：把正常纹理强制换码 0x36 也读 0，而码 7/24–26 正常，tiling/align/NORM/new-mode/bit23/31 全无效 → 0x1F76 不解码归一化 16 位码 0x32–0x36。10bit **解码面**已随 `f0c7605` 启用（P010/P016 + DRM_PRIME_2 导出逐位验证） | A 保留声明+文档注明 / B 回退只留 R16_UNORM / C 继续逆向（已证伪：非编码问题，2026-11-03） |
| C2 | 高级混合 8/15（7 种方程静默不混合） | 保留声明 = 原生 GLES 3.2 的必要代价 / 撤销则 GLES 3.2 退回 3.1 |
| ~~I-3~~ | ~~H264 High422/444 声明 vs `chroma_format_idc=1` 硬置~~ | **已定案（2026-11-10，`a5a938b`）**：硬件只有 4:0:0/4:2:0（寄存器表无采样格式选择器、原厂驱动只声明 ConstrainedBaseline/Main/High、libva 无对应 VAProfile）→ **不可实现，已收回声明**；细节见 `docs/UNRESOLVED.md` §1.5。另新增 **I-4**：H.264 High10 输出全 0（硬件 `h264_high10_support=1`，属本驱动缺陷） |

## 7. 每次交付的标准流程

1. 改 `src/mwv207-umd`（每处带 `FIX(jm9100-oss)` 注释说明"为什么"）。
2. `ninja -C ref/oh-mesa/User_source_code/mesa-24.1.2/build-full src/gallium/targets/dri/libmwv207_dri.so`
   → `cp` 到 `/tmp/gd3/` → 跑回归四件套。
3. `./tools/build-umd.sh check`（FIX 文件计数应递增）。
4. `git commit`；需要时 `tools/make-payload.sh -v …-rN` 出包 → `umd install --force`
   → **status+md5 确认实装** → 零 env 验收（glxinfo/glmark2/query tests）。
5. 更新 `docs/UNRESOLVED.md` 与 README 版本号，push。**不再导出补丁**（用户
   2026-11-03 决定，`patches/` 已删除）：改动以 git 提交为唯一记录。

## 8. 杂项备忘

- openuuyc（UU 远程被控端）：后台服务 `app-openuuyc.service` 登录即跑
  （`OpenUUYC gui --background`），持"控制中心"单实例锁
  `/run/user/1000/openuuyc-control-center-1000.lock`；桌面点击的第二实例按设计退出
  （**非故障**，用户已确认没问题）。切栈重启 lightdm 会断它的远程会话——切换前可
  先 `systemctl --user stop app-openuuyc`。
- `EGL_ICD` 原地改写逻辑在 `switch-mwv207.sh`（`VENDOR_EGL_ICD_ORIG` 备份 +
  `library_path` 指向 `libEGL_jm.so.0`）；`/etc/cdi/jmgpu.yaml` 依赖该文件存在。
- KMD 热切换**可行**（2026-11-06 实测）：`sudo ./switch-mwv207.sh kmd install`
  会停 lightdm → `rmmod`（refcnt 到 0 后能卸）→ 装入自建 `.ko` → 重新拉起 lightdm，
  桌面约几秒后恢复，桌面会话会重新登录一次。脚本另有 `kmd stage`（只放置，重启生效）
  与 `refresh_initramfs`。
- **自建 KMD 用 DKMS 注册成 `mwv207-oss` 包，随内核自动重建**（2026-11-06）：
  * 源目录 `/usr/src/mwv207-oss-1.0.0`（由 `tools/dkms-sync.sh` 从仓库
    `src/kernel-6.6` 同步，剔除 `*.o/*.ko/*.mod.c` 等编译产物）；`dkms.conf` 里
    `BUILT_MODULE_NAME=mwv207`、`DEST_MODULE_LOCATION=/updates/dkms`、`AUTOINSTALL=YES`，
    `MAKE` 用 `${kernelver}` + `${build_dir}`。
  * 产物 `/lib/modules/<kver>/updates/dkms/mwv207.ko`；因为模块名仍是 `mwv207`，
    depmod 会把它排在 `kernel/drivers/gpu/drm/mwv207/` 的内建版之前，重启即生效。
  * 内核升级后 `dkms autoinstall` 自动重编这个包；仓库源码有改动时跑一次
    `sudo ./tools/dkms-sync.sh [版本号]` 同步并重建。
  * 坑：`make -C /lib/modules/$(uname -r)/build M=`（M 为空）会触发 syncconfig，
    而 Deepin 的 headers 包没有 `scripts/kconfig`，失败时会删掉
    `include/generated/autoconf.h`，之后所有模块编译都报找不到该文件。可从 ostree
    部署快照还原（`rsync -a /ostree/deploy/deepin/deploy/<当前部署>/usr/src/linux-headers-$(uname -r)/ /usr/src/linux-headers-$(uname -r)/`）；
    `tools/dkms-sync.sh` 已在开头检查该文件并给出提示。
  * 厂商那条 DKMS 包（`mwv207/1.8.0.uos`）构建出来的是 `jmgpu.ko`，不会覆盖我们的
    模块；`jmgpu` 仍由内核命令行 `modprobe.blacklist=jmgpu` 拦住。
- 上游枚举陷阱：`p_format.h` 只在 `ref/mesa-22.3.7/...`（`find` 要 `-L`）。
- **三态"整套驱动"切换**（2026-11-06 新增 `stack` 子命令）：把内核态(KMD)与用户态(UMD)
  当成一个整体，只有三种合法状态：

  | 状态 | 内核态 | 用户态 |
  |---|---|---|
  | `builtin` | 内核内建 `mwv207.ko` | 开源载荷（Mesa 24.1.2 oss） |
  | `oss` | 自建修复 `mwv207.ko`（DKMS） | 开源载荷（VA-API 带全部 VP9 修复） |
  | `vendor` | 厂商 `jmgpu.ko` | 厂商 UMD/EGL/OpenCL |

  `sudo ./switch-mwv207.sh stack status|builtin|oss|vendor|menu`；任一步失败（含桌面未点亮、
  自检不过）自动回滚到 `builtin`（内核内建驱动）并点亮桌面；切 `vendor` 会拉看门狗，超时自动回滚到 `builtin`。
  状态检测靠 `srcversion` 三方比对（已加载 / 内建 .ko / 自建 .ko），自建版 srcversion
  另存 `/var/lib/mwv207-oss/oss-srcversion`，因此 updates 里的 .ko 被删掉时仍能认出来。

- **停 lightdm 必须放到流程最后、且与调用终端脱钩**（2026-11-06 踩坑）：本机图形会话本身
  就跑在 lightdm 下的 X 里，脚本一停 lightdm，调用它的终端/对话窗口立即被杀，切换只做
  到一半。`stack` 因此拆成两段：
  1. **阶段 1（前台）**：只做文件级动作（UMD 载荷、KMD 模块放置、depmod、initramfs），
     全程不碰桌面；失败直接退出，桌面毫发无损。
  2. **阶段 2（detached）**：`setsid nohup` 起后台进程，`sleep ${STACK_DELAY:-3}` 后
     停 lightdm → rmmod/modprobe → 起 lightdm → 自检；日志 `/var/log/mwv207-stack.log`，
     结果 `/run/mwv207-stack-result`（`ok|failed target=...`）。
  可选 `--delay <秒>` 或 `STACK_DELAY=N` 让阶段 2 更晚启动（把最后的确认/说明先输出完）。

- **切换脚本的 Xorg 日志要按实际显示号解析**（2026-11-03 修）：display 不一定是
  `:0`——`:0` 的 Xorg 崩溃后 lightdm 会起在 `:1`，日志写到 `Xorg.1.log`。原先写死
  `/var/log/Xorg.0.log`，install 等不到新日志 → 误判"软件渲染"→ 自动回滚 + 黑屏。
  现在 `xorg_log()` 从 `pgrep -a Xorg` 解析 `:N`（等待循环内每轮重解析），
  `umd verify`/`umd status`/watchdog 同样走它。
- **install 前把安装目录恢复成干净快照**：`umd install` 的快照取自"当前安装目录"，
  如果之前手工 `cp` 过库，失败回滚会"复原"这份脏状态（2026-11-03 10:44 实证：
  回滚后仍是新库）。先 `umd rollback` 回最近一次干净快照（或从备份恢复）再 install。
- **补丁流程已废止**（2026-11-03 用户决定）：`patches/` 目录与"导出补丁"步骤一并
  删除，之后不再生成任何 `*.patch` 归档；版本记录只靠 git 提交，需要对外提交时
  直接引提交哈希（或 `git format-patch` 临时生成、不入库）。
